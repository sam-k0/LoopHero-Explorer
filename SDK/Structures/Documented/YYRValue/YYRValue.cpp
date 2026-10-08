#include "YYRValue.hpp"
#include "../CDynamicArray/CDynamicArray.hpp"
#include "../RefThing/RefThing.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

// Loop Hero.exe runner functions (RVAs below). Strings and arrays must be created, copied and freed by the runner:
// strings live on its heap, and arrays are GC objects whose refcount is not at offset 0.
namespace RunnerRValue
{
	using FnFree = void(__cdecl*)(RValue* Value);								// FREE_RValue__Pre
	using FnCreateString = void(__cdecl*)(RValue* Value, const char* String);	// YYCreateString
	using FnAssign = RValue*(__fastcall*)(RValue* This, void* Edx, const RValue* Other); // RValue::operator=, __thiscall called via __fastcall

	struct Helpers
	{
		FnFree Free = nullptr;
		FnCreateString CreateString = nullptr;
		FnAssign Assign = nullptr;
	};

	static bool Matches(uintptr_t Address, const short* Pattern, size_t Length)
	{
		const unsigned char* Bytes = reinterpret_cast<const unsigned char*>(Address);
		for (size_t i = 0; i < Length; i++)
		{
			if (Pattern[i] != -1 && Bytes[i] != Pattern[i])
				return false;
		}
		return true;
	}

	static Helpers Resolve()
	{
		static const short FreePattern[] = { 0x56, 0x8B, 0x74, 0x24, 0x08, 0xB8, 0xFF, 0xFF, 0xFF, 0x00, 0x23, 0x46, 0x0C, 0x83, 0xF8, 0x01 };
		static const short CreateStringPattern[] = { 0x55, 0x8B, 0xEC, 0x6A, 0xFF, 0x68, -1, -1, -1, -1, 0x64, 0xA1, 0x00, 0x00, 0x00, 0x00 };
		static const short AssignPattern[] = { 0x55, 0x89, 0xE5, 0x53, 0x57, 0x56, 0x83, 0xE4, 0xF8, 0x83, 0xEC, 0x18, 0x8B, 0x45, 0x08, 0x89 };

		Helpers Result;
		uintptr_t Base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));

		uintptr_t Free = Base + 0x00002070;
		uintptr_t CreateString = Base + 0x0114DC80;
		uintptr_t Assign = Base + 0x000020D0;

		if (Matches(Free, FreePattern, 16) && Matches(CreateString, CreateStringPattern, 16) && Matches(Assign, AssignPattern, 16))
		{
			Result.Free = reinterpret_cast<FnFree>(Free);
			Result.CreateString = reinterpret_cast<FnCreateString>(CreateString);
			Result.Assign = reinterpret_cast<FnAssign>(Assign);
		}

		return Result;
	}

	static const Helpers& Get()
	{
		static const Helpers s_Helpers = Resolve();
		return s_Helpers;
	}
}

static bool KindNeedsFree(int Kind)
{
	return ((1u << (Kind & 0x1F)) & 0x46u) != 0;
}

// Value must already be initialized, its old contents are released.
static void AssignRValue(RValue* Value, const RValue* Other)
{
	if (Value == Other)
		return;

	const auto& Runner = RunnerRValue::Get();
	if (Runner.Assign)
	{
		Runner.Assign(Value, nullptr, Other);
		return;
	}

	// Unknown game build: never free (leaks, but can't free runner memory), still hold string refs.
	*Value = *Other;
	Value->Flags = 0;
	if ((Value->Kind & 0xFFFFFF) == VALUE_STRING && Value->String)
		Value->String->Inc();
}

static void ReleaseRValue(RValue* Value)
{
	const auto& Runner = RunnerRValue::Get();
	if (Runner.Free && KindNeedsFree(Value->Kind))
		Runner.Free(Value);

	Value->Kind = VALUE_UNSET;
	Value->Flags = 0;
	Value->I64 = 0;
}

static void CreateStringRValue(RValue* Value, const char* String)
{
	const auto& Runner = RunnerRValue::Get();
	if (Runner.CreateString)
	{
		Runner.CreateString(Value, String);
		return;
	}

	Value->Kind = VALUE_STRING;
	Value->Flags = 0;
	Value->String = RefString::Alloc(String, static_cast<int>(strlen(String) + 1));
}

YYRValue::YYRValue() noexcept(true)
{
	// Just set it to unset and zero out the whole 8-byte space.
	// Check it on https://godbolt.org/, it's true!
	this->Kind = VALUE_UNSET;
	this->Flags = 0;
	this->Real = 0.0;
}

YYRValue::YYRValue(const double& Value) noexcept(true)
{
	this->Kind = VALUE_REAL;
	this->Flags = 0;
	this->Real = Value;
}

YYRValue::YYRValue(const float& Value) noexcept(true)
{
	this->Kind = VALUE_REAL;
	this->Flags = 0;
	this->Real = static_cast<double>(Value);
}

YYRValue::YYRValue(const bool& Value) noexcept(true)
{
	this->Kind = VALUE_BOOL;
	this->Flags = 0;
	this->Real = static_cast<double>(Value); // A bool is really just a 0 or a 1, so I can freely cast it to an integer.
}

YYRValue::YYRValue(const long long& Value) noexcept(true)
{
	this->Kind = VALUE_INT64;
	this->Flags = 0;
	this->I64 = Value;
}

YYRValue::YYRValue(const char* Value) noexcept(true)
{
	this->Kind = VALUE_UNSET;
	this->Flags = 0;
	this->I64 = 0;
	CreateStringRValue(this, Value ? Value : "");
}

YYRValue::YYRValue(const std::string& Value) noexcept(true)
{
	this->Kind = VALUE_UNSET;
	this->Flags = 0;
	this->I64 = 0;
	CreateStringRValue(this, Value.c_str());
}

YYRValue::YYRValue(const YYRValue& Value) noexcept(true)
{
	this->Kind = VALUE_UNSET;
	this->Flags = 0;
	this->I64 = 0;
	AssignRValue(this, &Value);
}

YYRValue::YYRValue(YYRValue&& Value) noexcept(true)
{
	this->I64 = Value.I64;
	this->Flags = Value.Flags;
	this->Kind = Value.Kind;
	Value.Kind = VALUE_UNSET;
	Value.Flags = 0;
	Value.I64 = 0;
}

YYRValue::YYRValue(const RValue& Value) noexcept(true)
{
	this->Kind = VALUE_UNSET;
	this->Flags = 0;
	this->I64 = 0;
	AssignRValue(this, &Value);
}

YYRValue& YYRValue::operator=(const YYRValue& Value) noexcept(true)
{
	AssignRValue(this, &Value);
	return *this;
}

YYRValue& YYRValue::operator=(YYRValue&& Value) noexcept(true)
{
	if (this != &Value)
	{
		ReleaseRValue(this);
		this->I64 = Value.I64;
		this->Flags = Value.Flags;
		this->Kind = Value.Kind;
		Value.Kind = VALUE_UNSET;
		Value.Flags = 0;
		Value.I64 = 0;
	}
	return *this;
}

YYRValue::~YYRValue() noexcept(true)
{
	ReleaseRValue(this);
}

YYRValue::operator int() const noexcept(true)
{
	return static_cast<int>(operator double());
}

YYRValue::operator double() const noexcept(true)
{
	switch (Kind)
	{
	case VALUE_REAL:
	case VALUE_BOOL: /* Fallthrough */
		return Real;
	case VALUE_INT32:
		return static_cast<double>(I32);
	case VALUE_INT64:
		return static_cast<double>(I64);
	default:
		return 0.0;
	}
}

YYRValue::operator float() const noexcept(true)
{
	return static_cast<float>(operator double());
}

YYRValue::operator bool() const noexcept(true)
{
	return operator double() > 0.5;
}

YYRValue::operator const char* () const noexcept(true)
{
	if (Kind == VALUE_STRING)
	{
		if (String)
		{
			return String->Get();
		}
	}

	return nullptr;
}

YYRValue::operator std::string() const noexcept(true)
{
	if (Kind == VALUE_STRING)
	{
		if (String)
		{
			const char* pString = String->Get();
			
			if (pString)
				return std::string(pString);
		}
	}

	return "";
}

YYRValue::operator RefString* () const noexcept(true)
{
	if (Kind == VALUE_STRING)
		return String;
	// else
	return nullptr;
}

YYRValue::operator YYObjectBase* () const noexcept(true)
{
	if (Kind == VALUE_OBJECT)
		return Object;

	return nullptr;
}

YYRValue& YYRValue::operator+=(const double& Value)
{
	switch (Kind)
	{
	case VALUE_REAL: /* Fallthrough */
	case VALUE_INT32:
	case VALUE_INT64:
	case VALUE_BOOL:
		*this = static_cast<double>(*this); // Convert this YYRValue to a value holding a double
		this->Real += Value;
		break; // No throwing today!
	default:
		break;
	}

	return *this;
}

YYRValue& YYRValue::operator-=(const double& Value)
{
	switch (Kind)
	{
	case VALUE_REAL: /* Fallthrough */
	case VALUE_INT32:
	case VALUE_INT64:
	case VALUE_BOOL:
		*this = static_cast<double>(*this); // Convert this YYRValue to a value holding a double
		this->Real -= Value;
		break; // No throwing today!
	default:
		break;
	}

	return *this;
}

YYRValue& YYRValue::operator*=(const double& Value)
{
	switch (Kind)
	{
	case VALUE_REAL: /* Fallthrough */
	case VALUE_INT32:
	case VALUE_INT64:
	case VALUE_BOOL:
		*this = static_cast<double>(*this); // Convert this YYRValue to a value holding a double
		this->Real *= Value;
		break; // No throwing today!
	default:
		break;
	}

	return *this;
}

YYRValue& YYRValue::operator/=(const double& Value)
{
	switch (Kind)
	{
	case VALUE_REAL: /* Fallthrough */
	case VALUE_INT32:
	case VALUE_INT64:
	case VALUE_BOOL:
		*this = static_cast<double>(*this); // Convert this YYRValue to a value holding a double
		this->Real /= Value;
		break; // No throwing today!
	default:
		break;
	}

	return *this;
}