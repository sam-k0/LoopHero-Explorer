# AGENTS.md

Guidance for AI agents (and humans) working in this repository.

## What this project is

**Loop Hero Explorer** (repo dir `LoopHero-Dumper`) is a **YYTK plugin DLL** for the
game *Loop Hero* (a GameMaker / YYC title). It is **not** a standalone application:
it is a C++ DLL loaded into the game process through the YYTK plugin system and
renders an in-game **Dear ImGui overlay** used for data mining and debugging:

- Object variable explorer (per instance)
- Global variable explorer
- Run GML commands at runtime
- FPS performance plot
- Dump sprite & object name/index tables
- Create-event recording and filtered event logging
- Editing of plain (non-array) variables

It depends on a companion core DLL, **`LoopHeroCallbackCore.dll` (LHCC)**, which
provides the `CoreReady`/`RegisterModule`/patch-install exports. Without it, the
plugin loops printing `Waiting for Core. Did you install LoopHeroCallbackCore.dll?`.

## Prerequisites & build

- Windows only: Visual Studio with the **v145** platform toolset, C++20
  (`std::format`, `std::from_chars`).
- Open `LoopHero Dumper.sln`, build **Release | x64** (Win32 also configured) →
  a DLL is produced in `Release/`.
- There is **no buildable configuration on Linux**; the code uses `Windows.h`,
  `direct.h`, DX11, and Win32 APIs. Do not attempt to compile here.
- `merge.py` is plain Python 3 (no dependencies) for merging dump files; it runs
  anywhere.

## Running / deploying

1. Build the DLL.
2. Place it next to the game executable **and** `LoopHeroCallbackCore.dll`.
3. Launch the game; the plugin registers with the core and draws the
   "Loop Hero Explorer" overlay window in-game.
4. Game console output (`PrintMessage`) is prefixed with the plugin ID
   `sam-k0.loophero.explorer`.

## Architecture

### Plugin bootstrap

- `MyPlugin.h` defines `YYSDK_PLUGIN`, includes the vendored `SDK/SDK.hpp`, and
  declares the plugin name (`gPluginName`), `VarInfo`/`InstanceInfo` structs, and
  the `Misc` helper namespace.
- `dllmain.cpp` exports `PluginEntry(YYTKPlugin*)` and `GetPluginName()` (required
  by YYTK). `PluginEntry`:
  1. Starts `LHCore::ResolveCore` on a worker thread (waits for LHCC exports).
  2. Creates `EVT_PRESENT` and `EVT_RESIZEBUFFERS` callbacks via
     `PmCreateCallback`.

### Core resolution (`LHCore.h`)

`ResolveCore` polls `PmGetExported` for `CoreReady`, `RegisterModule`,
`UnregisterModule`, `API_InstallPrePatch`, `API_InstallPostPatch`. Once ready it
registers the module and runs the install callback, which registers
`ExecuteCodeCallback` (pre) and `PostExecuteCodeCallback` (post) on GML code
execution, and imports `API_HttpGetRequest`.

### Callbacks (`dllmain.cpp`)

- `FrameCallback` (`EVT_PRESENT`): on first present, grabs the D3D11 device,
  hooks the window proc, and initializes Dear ImGui (Win32 + DX11 backends).
  Each frame it renders all UI windows, the FPS ring buffer, and the event log.
- `ResizeBuffersCallback` (`EVT_RESIZEBUFFERS`): **must** unbind and release the
  render-target view before the core resizes the swap chain, or
  `ResizeBuffers` fails. It is recreated on the next present.
- `ExecuteCodeCallback`: intercepts GML events — records create events
  (`gml_Object_*_Create_*`) into `g_createEvents`, logs non-Draw/Step events,
  and applies the user event filter.
- `PostExecuteCodeCallback`: currently a no-op stub.

### Talking to the game (GML)

All game introspection goes through **`Binds::CallBuiltin*`** in `Helpers.h`,
which wraps the SDK's `CallBuiltin` to invoke GML builtins
(`variable_instance_get`, `variable_global_get`, `typeof`, `array_get`,
`instance_id_get`, `object_get_name`, `sprite_get_name`, ...). Key helpers:

- `DCS(YYRValue)` → deep-copies a VM string into `std::string` **immediately**.
- `ReadValue`/`FetchInstanceVariables` → builds a `VarInfo` tree for an
  instance (or `INSTANCE_GLOBAL`); arrays are recursive and capped by
  `MAX_ARRAY_ITEMS` (1000) and `MAX_ARRAY_DEPTH` (8).
- `SetVariable` → parses input against the stored type and writes it back, then
  re-reads the value.
- `RunCommand` → tiny GML-call parser: supports `global.foo = expr` and
  `global.foo` sugar, numeric/string/bool args; validates with a regex before
  calling.
- `dumpObjectIDs` / `dumpSpriteIDs` → iterate `object_exists`/`sprite_exists`
  and emit `objEnum.txt` / `SpriteEnum.txt`.
- `FetchInstanceVarsDumpFile` → writes `{objIndex , {"var1", "var2"}},` lines
  consumed by `merge.py`.

## File map

| File | Role |
|------|------|
| `dllmain.cpp` | Entry point, all ImGui UI, present/resize/code callbacks (the big one) |
| `MyPlugin.h` | Plugin bootstrap, `VarInfo`/`InstanceInfo`, `Misc` helpers, `DCS` |
| `Helpers.h` | `Binds::CallBuiltin*`, value read/write/edit, `RunCommand`, dump functions |
| `LHCore.h` | Core-resolution thread, function-pointer table, install callback wiring |
| `LHObjects.h` | **Generated** `LHObjectEnum` + object name lookup (0–300) |
| `LHSprites.h` | **Generated** `LHSpriteEnum` + sprite name lookup |
| `Assets.h` | Runtime sprite load/replace/inspect helpers (paths relative to EXE dir) |
| `HttpRequests.h` | GitHub API header map helper (WIP) |
| `Filesystem.h` | `GetCurrentDir()` / `FileExists()` |
| `SDK/` | Vendored YYTK SDK (do not hand-edit; update as a unit) |
| `imgui/` | Vendored Dear ImGui + Win32/DX11 backends (do not hand-edit) |
| `merge.py` | Merges several `dump_*.txt` files into `merged.txt`, deduping vars per object |
| `LoopHero Dumper.sln` / `.vcxproj` | VS solution/project (DLL, v145) |
| `Assets/demo.png` | Screenshot shown in README |

## Conventions

- **GML access only via `Binds::CallBuiltin*`** — never call raw VM APIs
  directly from UI code.
- **VM strings are not owned.** Any `const char*` obtained from a `YYRValue`
  must be deep-copied immediately (`DCS()` / `std::string(ptr)`). Do not store
  raw pointers.
- **Generated tables are regenerated, not hand-edited.** `LHObjects.h` and
  `LHSprites.h` come from `dumpObjectIDs`/`dumpSpriteIDs` output
  (`objEnum.txt`/`SpriteEnum.txt`). If game content changes, re-dump and
  replace the headers.
- **Dump workflow:** in-game "Dump Objects/Globals" → `dump_objects.txt` /
  `dump_globals.txt`; "Dump create events" → `event_dump_N.txt`. `merge.py`
  combines multiple dump files (list them in its `files` array) into
  `merged.txt`.
- **UI state** lives in file-scope globals in `dllmain.cpp` (`g_show*`,
  `g_filter*`, filter buffers). New windows follow the same pattern: a
  `g_show*` bool toggled from the main window plus an `ImGui::Begin/End` block
  inside `FrameCallback`.
- Logging goes through `Misc::Print`, which prefixes the plugin name and routes
  to the game console via `PrintMessage`.
- The codebase mixes tabs and spaces in places; match the surrounding file's
  style when editing.

## Gotchas

- **Windows-only** — nothing compiles on Linux/macOS.
- **RTV lifetime**: `ResizeBuffersCallback` must release `g_RTV` before the
  core resizes; it is recreated in `FrameCallback`. Forgetting this breaks
  window resizing (fixed before, don't regress).
- **Array recursion**: arrays can contain themselves; `ReadValue` caps depth
  and item count. Don't raise caps without considering VM cost — reading whole
  arrays every frame is slow (see the 30-frame cache in "Cursor Object Info").
- **Instance IDs are ephemeral**: instances can die between fetch and display;
  always re-check `instance_exists` before reading/writing (see
  `FetchInstanceVariables` and the explorer refresh paths).
- `dllmain.cpp` contains large commented-out debug blocks (e.g. the
  `o_camp_statistik_Create_0` CInstance dump) — dead code kept for reference;
  don't delete casually.
- `HttpRequests.h` and the "Send HTTP request" button are experimental
  (`API_HttpGetRequest` from LHCC).
- The plugin ID string `sam-k0.loophero.explorer` is the registration key —
  changing it requires matching changes anywhere it's referenced.
