import re
from collections import defaultdict


def parse_map(text):
    result = defaultdict(list)

    # matches: {7 , {"a", "b"}}
    pattern = re.findall(r'\{\s*(\d+)\s*,\s*\{\s*([^}]*?)\s*\}\s*\}', text)

    for key, values in pattern:
        obj_index = int(key)

        # extract quoted strings
        vars = re.findall(r'"([^"]*)"', values)

        result[obj_index].extend(vars)

    print("Parsed map:", len(result), "objects")

    return result


def merge_maps(map_list):
    merged = defaultdict(set)

    for m in map_list:
        for obj_index, vars in m.items():
            merged[obj_index].update(vars)

    return merged


def write_map(merged, filename):
    with open(filename, "w") as f:
        for obj_index in sorted(merged.keys()):
            vars = sorted(merged[obj_index])

            var_list = ', '.join(f'"{v}"' for v in vars)

            line = f"{{{obj_index} , {{{var_list}}}}},\n"
            f.write(line)


# ---- MAIN ----

files = [
    "mmerged.txt",
    "carddeck.txt",
    "gamestart.txt"
]

maps = []

for file in files:
    with open(file, "r") as f:
        text = f.read()
        maps.append(parse_map(text))

merged = merge_maps(maps)

write_map(merged, "merged.txt")

print("Merged successfully!")
