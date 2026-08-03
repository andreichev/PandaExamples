#!/usr/bin/env python3
# Сборка атласов Neverland из тайлов Cocricot (Minecraft resource pack).
# Запуск: python3 tools/cocricot_atlas.py [путь к паку]
# После сборки удалить cooked-кеш: rm .Panda/Textures/Assets/textures/*.ptex
#
# ВНИМАНИЕ: условия Cocricot запрещают использование текстур вне Minecraft —
# сборка годится для личного прототипа; перед публикацией тайлы заменить на свои
# (достаточно поменять имена в SLOT-таблицах ниже на собственные файлы).

import os
import struct
import sys
import zlib

DEFAULT_PACK = os.path.expanduser("~/Downloads/cocricot_for1.20.4_v1.1")
SRC = DEFAULT_PACK + "/assets/minecraft/textures/block/"


def set_pack(pack):
    global SRC
    SRC = pack + "/assets/minecraft/textures/block/"
DST = os.path.join(os.path.dirname(__file__), "..", "Assets", "textures") + "/"
SCALE = 4  # 32px тайл -> 128px (nearest: пиксель-арт сохраняется, мипы глубже)

# Тинты colormap (в Minecraft траву и листву красит биом; цвета — из колормап пака).
GRASS_TINT = (121, 148, 67)
FOLIAGE_TINT = (158, 179, 86)

# Слоты атласов: индекс тайла (row * grid + col) -> файл пака (или (файл, тинт)).
# Индексы закреплены в Scripts/Neverland/src/Model/VoxelTextureMapper.hpp,
# terrain_frag.hlsl (TILE_BASE) и константах BuildingCellGrid.cpp (timber/casing/plinth).
MATERIALS = {  # base_materials_1.png, 7x7
    0: "white_concrete",      # WHITE_PLASTER + наличники окон
    1: "sandstone_top",       # BOARDS «Plaster» — тёплая штукатурка
    2: "spruce_planks",       # рамы окон + фахверк
    3: "log_birch",           # BIRCH_LOG
    4: "polished_andesite",   # TREE (legacy)
    5: "deepslate_tiles",     # SLATE
    6: ("leaves_birch", FOLIAGE_TINT),  # BIRCH_LEAVES
    28: "terracotta",         # TERRACOTTA
    30: "bricks",             # STONE_BRICKS «Bricks»
    31: "nether_bricks",      # DARK_BRICK
    43: "stone_bricks",       # SAND_STONE «Stone Block»
    44: "cobbled_deepslate",  # DARK_STONE + цоколь стен
    19: "quartz_block",       # MARBLE (мрамор)
    20: "quartz_pillar",      # COLUMN бок (каннелюры)
    21: "quartz_pillar_top",  # COLUMN верх/низ (меандр)
    # Фахверк: конкретные тайлы CTM-набора spruce_planks_sides (панели с балками).
    52: "../../optifine/ctm/spruce_planks_sides26.png",  # TIMBER_PLAIN (чистая штукатурка)
    53: "../../optifine/ctm/spruce_planks_sides5.png",   # TIMBER_FRAME (рамка-панель)
    54: "../../optifine/ctm/spruce_planks_sides0.png",   # TIMBER_CROSS (крест в рамке)
    55: "../../optifine/ctm/spruce_planks_sides1.png",   # TIMBER_DIAG_L
    56: "../../optifine/ctm/spruce_planks_sides13.png",  # TIMBER_DIAG_R
    57: "red_terracotta",     # ROOF_TILES_RED (черепица кубом)
    58: "brown_terracotta",   # ROOF_TILES_BROWN
    59: "black_terracotta",   # ROOF_TILES_DARK
}
GROUND = {  # base_ground_1.png, 6x6 (тайлы терраина: TILE_BASE в terrain_frag.hlsl)
    0: ("grass_block_top", GRASS_TINT),
    4: "dirt",
    12: "sand",
    18: "stone",
}
ROOF = {  # base_roof_1.png, 7x7 — колонки ряда 0 = цвета черепицы (roofTileFor)
    0: "red_terracotta",              # красная
    1: "granite",                     # коричневая
    2: "deepslate_tiles",             # серая
    3: "polished_blackstone_bricks",  # тёмная
}


def read_png(path):
    data = open(path, "rb").read()
    pos, width, height, bitd, color = 8, 0, 0, 0, 0
    idat, palette, trns = b"", None, None
    while pos < len(data):
        length, ctype = struct.unpack(">I4s", data[pos:pos + 8])
        chunk = data[pos + 8:pos + 8 + length]
        if ctype == b"IHDR":
            width, height, bitd, color = struct.unpack(">IIBB", chunk[:10])
        elif ctype == b"IDAT":
            idat += chunk
        elif ctype == b"PLTE":
            palette = chunk
        elif ctype == b"tRNS":
            trns = chunk
        pos += 12 + length
    raw = zlib.decompress(idat)
    ch = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[color]
    assert bitd == 8, f"{path}: unsupported bit depth {bitd}"
    stride = width * ch
    out, prev, pos = bytearray(), bytearray(stride), 0
    for _ in range(height):
        f = raw[pos]
        pos += 1
        line = bytearray(raw[pos:pos + stride])
        pos += stride
        if f == 1:
            for i in range(ch, stride):
                line[i] = (line[i] + line[i - ch]) & 0xFF
        elif f == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 0xFF
        elif f == 3:
            for i in range(stride):
                a = line[i - ch] if i >= ch else 0
                line[i] = (line[i] + ((a + prev[i]) >> 1)) & 0xFF
        elif f == 4:
            for i in range(stride):
                a = line[i - ch] if i >= ch else 0
                b = prev[i]
                c = prev[i - ch] if i >= ch else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 0xFF
        out += line
        prev = line
    rgba = bytearray(width * height * 4)
    for i in range(width * height):
        if color == 6:
            rgba[i * 4:i * 4 + 4] = out[i * 4:i * 4 + 4]
        elif color == 2:
            rgba[i * 4:i * 4 + 3] = out[i * 3:i * 3 + 3]
            rgba[i * 4 + 3] = 255
        elif color == 3:
            idx = out[i]
            rgba[i * 4:i * 4 + 3] = palette[idx * 3:idx * 3 + 3]
            rgba[i * 4 + 3] = trns[idx] if trns and idx < len(trns) else 255
        elif color == 0:
            v = out[i]
            rgba[i * 4:i * 4 + 3] = bytes([v, v, v])
            rgba[i * 4 + 3] = 255
        elif color == 4:
            v = out[i * 2]
            rgba[i * 4:i * 4 + 3] = bytes([v, v, v])
            rgba[i * 4 + 3] = out[i * 2 + 1]
    return width, height, rgba


def write_png(path, width, height, rgba):
    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)

    raw = b""
    for y in range(height):
        raw += b"\x00" + bytes(rgba[y * width * 4:(y + 1) * width * 4])
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b"")
    open(path, "wb").write(png)


def load_tile(spec):
    name, tint = spec if isinstance(spec, tuple) else (spec, None)
    # абсолютный путь — модельные текстуры проекта; с '/' — путь относительно
    # textures/block пака (CTM-тайлы); иначе — тайл пака по имени
    if os.path.isabs(name):
        path = name
    elif "/" in name:
        path = SRC + name
    else:
        path = SRC + name + ".png"
    w, h, px = read_png(path)
    h = min(h, w)  # анимированные полосы — верхний кадр
    if tint:
        tr, tg, tb = tint
        px = bytearray(px)
        for i in range(0, w * h * 4, 4):
            px[i] = px[i] * tr // 255
            px[i + 1] = px[i + 1] * tg // 255
            px[i + 2] = px[i + 2] * tb // 255
    return w, h, px


def build_atlas(filename, grid, fill, slots):
    tile = 32 * SCALE
    size = grid * tile
    canvas = bytearray(size * size * 4)

    def blit(index, spec):
        w, h, px = load_tile(spec)
        ox, oy = (index % grid) * tile, (index // grid) * tile
        for y in range(tile):
            sy = min(y * h // tile, h - 1)
            row = bytearray()
            for x in range(tile):
                sx = min(x * w // tile, w - 1)
                s = (sy * w + sx) * 4
                row += px[s:s + 4]
            off = ((oy + y) * size + ox) * 4
            canvas[off:off + len(row)] = row

    for i in range(grid * grid):
        blit(i, fill)
    for index, spec in slots.items():
        blit(index, spec)
    write_png(DST + filename, size, size, canvas)
    print("wrote", filename, f"{size}x{size}")


def model_texture_slots():
    """Слоты текстур модельных блоков из реестра tools/blockmodels_registry.json."""
    import json
    registry_path = os.path.join(os.path.dirname(__file__), "blockmodels_registry.json")
    if not os.path.exists(registry_path):
        return {}
    registry = json.load(open(registry_path))
    slots = {}
    textures_dir = os.path.join(os.path.dirname(__file__), "..", "Assets", "blockmodels", "textures")
    for name, slot in registry.get("textureSlots", {}).items():
        slots[slot] = os.path.join(textures_dir, name + ".png")
    return slots


def main():
    materials = dict(MATERIALS)
    materials.update(model_texture_slots())
    build_atlas("base_materials_1.png", 10, "smooth_stone", materials)
    build_atlas("base_ground_1.png", 6, "dirt", GROUND)
    build_atlas("base_roof_1.png", 7, "red_terracotta", ROOF)


if __name__ == "__main__":
    if len(sys.argv) > 1:
        set_pack(sys.argv[1])
    main()
