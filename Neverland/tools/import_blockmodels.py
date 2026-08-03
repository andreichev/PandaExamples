#!/usr/bin/env python3
# Импорт Minecraft block models в Neverland.
#
#   python3 tools/import_blockmodels.py [--pack путь_к_паку] [имя_модели ...]
#
# Названные модели копируются из пака (assets/minecraft/models/block/<имя>.json)
# в Assets/blockmodels/src/. Затем ВСЕ модели из src/ конвертируются:
#   - текстурам выделяются свободные слоты base_materials_1 (реестр append-only),
#   - PNG текстур копируются в Assets/blockmodels/textures/,
#   - генерируется Scripts/Neverland/src/Model/BlockModelsGenerated.inl,
#   - пересобираются атласы (cocricot_atlas.main) — затем удалить .ptex-кеш.
# После импорта пересобрать скрипты (build_scripts.sh).
#
# Ограничения v1: модель обязана иметь собственные elements (parent игнорируется,
# кроме карты textures родителя, если его json лежит рядом в src/); неквадратные
# текстуры кропаются до верхнего квадрата.

import json
import os
import shutil
import sys

TOOLS = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(TOOLS)
SRC_DIR = os.path.join(ROOT, "Assets", "blockmodels", "src")
TEX_DIR = os.path.join(ROOT, "Assets", "blockmodels", "textures")
REGISTRY_PATH = os.path.join(TOOLS, "blockmodels_registry.json")
GENERATED_PATH = os.path.join(
    ROOT, "Scripts", "Neverland", "src", "Model", "BlockModelsGenerated.inl"
)

# Занятые слоты base_materials_1 (см. cocricot_atlas.MATERIALS + константы мешера).
def _reserved_slots():
    sys.path.insert(0, TOOLS)
    import cocricot_atlas
    return set(cocricot_atlas.MATERIALS.keys())


RESERVED_SLOTS = _reserved_slots()
GRID = 10

FACE_ORDER = ["down", "up", "north", "south", "west", "east"]

# Отображаемые имена (в игре — свои названия, не ванильные). Fallback — Title Case.
DISPLAY_NAMES = {
    "oak_fence": "Wooden Fence",
    "acacia_fence": "Garden Fence",
    "cherry_fence": "Ornate Fence",
    "nether_brick_fence": "Brick Fence",
    "iron_bars": "Iron Railing",
    "stone_brick_wall": "Stone Parapet",
    "sandstone_wall": "Sand Parapet",
    "end_stone_brick_wall": "Balustrade",
    "glass_pane": "Lattice Window",
    "white_pane": "French Window",
    "brown_pane": "Wooden Window",
    "black_pane": "Iron Window",
    "lime_pane": "Stained Window",
    "trellis_pane": "Trellis",
    "oak_door": "Panel Door",
    "spruce_door": "Barn Door",
    "birch_door": "White Door",
    "oak_fence_gate": "Wooden Gate",
    "spruce_fence_gate": "Dark Gate",
    "dark_oak_fence_gate": "Louvre Gate",
    "oak_trapdoor_open": "Oak Shutter",
    "spruce_trapdoor_open": "Dark Shutter",
    "torch": "Torch",
    "wall_torch": "Wall Torch",
    "lantern": "Street Lantern",
    "chain": "Chain",
    "black_candle_one_candle": "Candle",
    "ladder": "Ladder",
    "vine": "Ivy",
    "sandstone_stairs_top": "Sand Cornice",
    "cornice_white": "White Cornice",
    "spruce_stairs": "Wood Stairs",
    "stone_brick_stairs": "Stone Stairs",
    "brick_stairs": "Brick Stairs",
    "marble_stairs": "Marble Stairs",
    "roof_red_stairs": "Red Roof",
    "roof_brown_stairs": "Brown Roof",
    "roof_dark_stairs": "Dark Roof",
}


def display_name(name):
    return DISPLAY_NAMES.get(name, name.replace("_", " ").title())


# Категории меню: 0 Fences, 1 Windows, 2 Doors, 3 Forms, 4 Decor.
def category_for(name):
    if name.startswith("roof_"):
        return 6  # черепичные крыши
    if name.startswith("cornice") or name.startswith("baluster") or "stairs_top" in name:
        return 5  # карнизы и лепнина
    if "pane" in name:
        return 1
    if "door" in name and "trapdoor" not in name:
        return 2
    if "fence" in name or "bars" in name or "_wall" in name:
        return 0
    if "slab" in name or "stairs" in name:
        return 3
    return 4


def centroid_shift(boxes):
    """Объёмо-взвешенное смещение центроида боксов от центра ячейки (x, z)."""
    total = 0.0
    cx = cz = 0.0
    for b in boxes:
        vol = max(1e-6, (b["to"][0] - b["from"][0]) * (b["to"][1] - b["from"][1]) *
                  (b["to"][2] - b["from"][2]))
        cx += vol * ((b["from"][0] + b["to"][0]) * 0.5 - 0.5)
        cz += vol * ((b["from"][2] + b["to"][2]) * 0.5 - 0.5)
        total += vol
    return cx / total, cz / total


def back_base(boxes):
    """Индекс «тяжёлой» стороны в k-последовательности [N, W, S, E]."""
    cx, cz = centroid_shift(boxes)
    if abs(cx) >= abs(cz):
        return 3 if cx > 0 else 1  # E : W
    return 2 if cz > 0 else 0      # S : N


def diag_base(boxes):
    """Индекс диагонали смещения в последовательности [NE, NW, SW, SE]."""
    cx, cz = centroid_shift(boxes)
    if cz < 0:
        return 0 if cx > 0 else 1  # NE : NW
    return 3 if cx > 0 else 2      # SE : SW


def side_base_rotation(boxes):
    """Базовая ориентация side-секции по центроиду боксов: 0 север, 1 восток, 2 юг,
    3 запад — число поворотов приведения к северу совпадает с индексом."""
    cx = sum((b["from"][0] + b["to"][0]) * 0.5 for b in boxes) / len(boxes) - 0.5
    cz = sum((b["from"][2] + b["to"][2]) * 0.5 for b in boxes) / len(boxes) - 0.5
    if abs(cx) >= abs(cz):
        return 1 if cx > 0 else 3
    return 2 if cz > 0 else 0


def parse_args():
    pack = os.path.expanduser("~/Downloads/cocricot_for1.20.4_v1.1")
    names = []
    args = sys.argv[1:]
    while args:
        arg = args.pop(0)
        if arg == "--pack":
            pack = args.pop(0)
        else:
            names.append(arg)
    return pack, names


def load_registry():
    if os.path.exists(REGISTRY_PATH):
        return json.load(open(REGISTRY_PATH))
    return {"models": {}, "textureSlots": {}}


def resolve_texture_name(ref, textures):
    # "#key" -> textures["key"]; циклы/пропуски -> None
    seen = set()
    while isinstance(ref, str) and ref.startswith("#"):
        key = ref[1:]
        if key in seen or key not in textures:
            return None
        seen.add(key)
        ref = textures[key]
    if not isinstance(ref, str):
        return None
    # "minecraft:block/name" / "block/name" -> "name"
    ref = ref.split(":", 1)[-1]
    return ref.split("/")[-1]


def auto_uv(face_name, from_px, to_px):
    x0, y0, z0 = from_px
    x1, y1, z1 = to_px
    if face_name in ("down", "up"):
        return [x0, z0, x1, z1]
    if face_name in ("north", "south"):
        return [x0, 16 - y1, x1, 16 - y0]
    return [z0, 16 - y1, z1, 16 - y0]


def convert_model(path, registry, pack_block_textures, dry_id=False):
    model = json.load(open(path))
    name = os.path.splitext(os.path.basename(path))[0]
    # Parent-цепочка в src/: elements берутся у ближайшего предка с геометрией,
    # textures мержатся по цепочке (ребёнок переопределяет).
    textures = {}
    elements = None
    node, guard = model, 0
    while node is not None and guard < 8:
        for key, value in node.get("textures", {}).items():
            textures.setdefault(key, value)
        if elements is None and node.get("elements"):
            elements = node["elements"]
        parent = node.get("parent", "")
        parent_name = parent.split(":", 1)[-1].split("/")[-1] if parent else ""
        parent_path = os.path.join(SRC_DIR, parent_name + ".json")
        node = json.load(open(parent_path)) if parent_name and os.path.exists(parent_path) else None
        guard += 1
    if not elements:
        print(f"SKIP {name}: нет elements по parent-цепочке")
        return None

    used_textures = set()
    boxes = []
    for element in elements:
        from_px = element["from"]
        to_px = element["to"]
        rotation = element.get("rotation") or {}
        angle = float(rotation.get("angle", 0.0))
        axis = {"x": 0, "y": 1, "z": 2}[rotation.get("axis", "y")]
        origin = rotation.get("origin", [8, 8, 8])
        faces = []
        for face_name in FACE_ORDER:
            face = element.get("faces", {}).get(face_name)
            if face is None:
                faces.append(None)
                continue
            texture = resolve_texture_name(face.get("texture", ""), textures)
            if texture is None:
                faces.append(None)
                continue
            used_textures.add(texture)
            uv = face.get("uv") or auto_uv(face_name, from_px, to_px)
            faces.append({
                "texture": texture,
                "uv": [v / 16.0 for v in uv],
                "rot": (int(face.get("rotation", 0)) // 90) % 4,
            })
        if not any(faces):
            continue
        boxes.append({
            "from": [v / 16.0 for v in from_px],
            "to": [v / 16.0 for v in to_px],
            "angle": angle,
            "axis": axis,
            "origin": [v / 16.0 for v in origin],
            "faces": faces,
        })
    if not boxes:
        print(f"SKIP {name}: пустая геометрия")
        return None

    # Текстуры: слот из реестра или новый свободный; PNG копируется в проект.
    for texture in sorted(used_textures):
        if texture in registry["textureSlots"]:
            continue
        taken = RESERVED_SLOTS | set(registry["textureSlots"].values())
        free = [s for s in range(GRID * GRID) if s not in taken]
        if not free:
            raise SystemExit(f"нет свободных слотов атласа для текстуры {texture}")
        registry["textureSlots"][texture] = free[0]
        source = os.path.join(pack_block_textures, texture + ".png")
        if not os.path.exists(source):
            raise SystemExit(f"{name}: текстура {texture}.png не найдена в паке")
        os.makedirs(TEX_DIR, exist_ok=True)
        shutil.copyfile(source, os.path.join(TEX_DIR, texture + ".png"))
        print(f"  текстура {texture} -> слот {free[0]}")

    return {"name": name, "boxes": boxes}


def emit_generated(models, registry):
    lines = [
        "// Автогенерация tools/import_blockmodels.py — НЕ редактировать руками.",
        "// Источник: Assets/blockmodels/src/*.json (Minecraft block model);",
        "// тайлы текстур — слоты base_materials_1 (tools/blockmodels_registry.json).",
        "",
    ]
    f3 = lambda v: f"{v:.6f}f"

    def emit_boxes(array_name, boxes):
        lines.append(f"constexpr BlockModelBox {array_name}[] = {{")
        for box in boxes:
            faces = []
            for face in box["faces"]:
                if face is None:
                    faces.append("{0, 0, 0, 0.f, 0.f, 0.f, 0.f}")
                    continue
                slot = registry["textureSlots"][face["texture"]]
                u0, v0, u1, v1 = face["uv"]
                faces.append(
                    f"{{1, {slot}, {face['rot']}, {f3(u0)}, {f3(v0)}, {f3(u1)}, {f3(v1)}}}"
                )
            lines.append(
                "    {{" + ", ".join(f3(v) for v in box["from"]) + "}, {" +
                ", ".join(f3(v) for v in box["to"]) + "}, " +
                f3(box["angle"]) + f", {box['axis']}, {{" +
                ", ".join(f3(v) for v in box["origin"]) + "},"
            )
            lines.append("     {" + ",\n      ".join(faces) + "}},")
        lines.append("};")
        lines.append("")

    for i, model in enumerate(models):
        emit_boxes(f"MODEL_BOXES_{i}", model["boxes"])
        if model["kind"] == 1:
            emit_boxes(f"MODEL_SIDE_BOXES_{i}", model["sideBoxes"])
        if model["kind"] == 3:
            if model["innerBoxes"]:
                emit_boxes(f"MODEL_INNER_BOXES_{i}", model["innerBoxes"])
            if model["outerBoxes"]:
                emit_boxes(f"MODEL_OUTER_BOXES_{i}", model["outerBoxes"])
    lines.append(f"constexpr size_t BLOCK_MODEL_COUNT = {len(models)};")
    if models:
        lines.append("constexpr BlockModelData BLOCK_MODELS[] = {")
        for i, model in enumerate(models):
            side = (f"{len(model['sideBoxes'])}, MODEL_SIDE_BOXES_{i}"
                    if model["kind"] == 1 else "0, nullptr")
            inner = (f"{len(model['innerBoxes'])}, MODEL_INNER_BOXES_{i}"
                     if model["kind"] == 3 and model["innerBoxes"] else "0, nullptr")
            outer = (f"{len(model['outerBoxes'])}, MODEL_OUTER_BOXES_{i}"
                     if model["kind"] == 3 and model["outerBoxes"] else "0, nullptr")
            lines.append(
                f"    {{{model['id']}, \"{display_name(model['name'])}\", {model['kind']}, "
                f"{model['category']}, {model.get('sideBase', 0)}, "
                f"{len(model['boxes'])}, MODEL_BOXES_{i}, {side}, "
                f"{model.get('backBase', 0)}, {model.get('innerDiagBase', 0)}, "
                f"{model.get('outerDiagBase', 0)}, {inner}, {outer}}},"
            )
        lines.append("};")
    else:
        lines.append("constexpr BlockModelData BLOCK_MODELS[1] = {}; // пусто")
    lines.append("")
    open(GENERATED_PATH, "w").write("\n".join(lines))
    print("wrote", os.path.relpath(GENERATED_PATH, ROOT))


def main():
    pack, names = parse_args()
    pack_models = os.path.join(pack, "assets", "minecraft", "models", "block")
    pack_textures = os.path.join(pack, "assets", "minecraft", "textures", "block")
    os.makedirs(SRC_DIR, exist_ok=True)

    def copy_with_parents(name):
        source = os.path.join(pack_models, name + ".json")
        if not os.path.exists(source):
            raise SystemExit(f"модель {name}.json не найдена в паке")
        shutil.copyfile(source, os.path.join(SRC_DIR, name + ".json"))
        print("добавлена модель", name)
        parent = json.load(open(source)).get("parent", "")
        if parent:
            parent_name = parent.split(":", 1)[-1].split("/")[-1]
            parent_file = os.path.join(pack_models, parent_name + ".json")
            if os.path.exists(parent_file) and                not os.path.exists(os.path.join(SRC_DIR, parent_name + ".json")):
                copy_with_parents(parent_name)

    for name in names:
        copy_with_parents(name)

    registry = load_registry()
    converted_by_name = {}
    for filename in sorted(os.listdir(SRC_DIR)):
        if not filename.endswith(".json"):
            continue
        converted = convert_model(os.path.join(SRC_DIR, filename), registry, pack_textures, dry_id=True)
        if converted:
            converted_by_name[converted["name"]] = converted

    # Пары X_post + X_side → connected-модель X; X_bottom_left + X_top_left → tall-модель X
    # (двухъячеечная дверь: боксы верха смещаются на +1 по Y). Части не публикуются.
    models = []
    used_parts = set()
    for name, part in sorted(converted_by_name.items()):
        if name.endswith("_post"):
            base = name[:-5]
            side = converted_by_name.get(base + "_side")
            if side is None:
                continue
            used_parts.update({name, base + "_side"})
            models.append({
                "name": base, "kind": 1, "boxes": part["boxes"],
                "sideBoxes": side["boxes"],
                "sideBase": side_base_rotation(side["boxes"]),
            })
        elif name.endswith("_bottom_left"):
            base = name[:-len("_bottom_left")]
            top = converted_by_name.get(base + "_top_left")
            if top is None:
                continue
            used_parts.update({name, base + "_top_left"})
            top_boxes = []
            for box in top["boxes"]:
                lifted = dict(box)
                lifted["from"] = [box["from"][0], box["from"][1] + 1.0, box["from"][2]]
                lifted["to"] = [box["to"][0], box["to"][1] + 1.0, box["to"][2]]
                lifted["origin"] = [box["origin"][0], box["origin"][1] + 1.0, box["origin"][2]]
                top_boxes.append(lifted)
            models.append({
                "name": base, "kind": 2, "boxes": part["boxes"] + top_boxes, "sideBoxes": [],
            })
    part_suffixes = ("_post", "_side", "_side_tall", "_inventory", "_bottom_left", "_top_left",
                     "_bottom_right", "_top_right", "_left_open", "_right_open")
    for name, single in sorted(converted_by_name.items()):
        if name in used_parts or name.endswith(part_suffixes) or name.startswith("template_"):
            continue
        if name.endswith("_inner") or name.endswith("_outer"):
            base = name[:name.rfind("_")]
            if base in converted_by_name:
                continue # уйдёт в kind 3 базовой модели
        entry = {"name": name, "kind": 0, "boxes": single["boxes"], "sideBoxes": []}
        inner = converted_by_name.get(name + "_inner")
        outer = converted_by_name.get(name + "_outer")
        if inner is not None or outer is not None:
            entry["kind"] = 3
            entry["backBase"] = back_base(single["boxes"])
            entry["innerBoxes"] = inner["boxes"] if inner else []
            entry["innerDiagBase"] = diag_base(inner["boxes"]) if inner else 0
            entry["outerBoxes"] = outer["boxes"] if outer else []
            entry["outerDiagBase"] = diag_base(outer["boxes"]) if outer else 0
        models.append(entry)
    for model in models:
        model["category"] = category_for(model["name"])

    # Стабильные id публикуемым моделям.
    for model in models:
        if model["name"] not in registry["models"]:
            used_ids = set(registry["models"].values())
            next_id = 1
            while next_id in used_ids:
                next_id += 1
            if next_id > 255:
                raise SystemExit("превышен лимит 255 моделей (id — u8 в params[0])")
            registry["models"][model["name"]] = next_id
        model["id"] = registry["models"][model["name"]]
    models.sort(key=lambda m: m["id"])

    json.dump(registry, open(REGISTRY_PATH, "w"), indent=1, ensure_ascii=False)
    emit_generated(models, registry)

    import cocricot_atlas
    cocricot_atlas.set_pack(pack)
    cocricot_atlas.main()
    print("готово: пересоберите скрипты и удалите .Panda/Textures/Assets/textures/*.ptex")


if __name__ == "__main__":
    main()
