#!/usr/bin/env python3
"""生成工程的**中文字库**：把界面要显示的字渲染成位图，写进 main/embedded_font.cpp。

字库里没有的字，屏上会画成 `?`；字号也是按套生成的（`.lui` 里用了没生成的字号同样画不出来）。

**构建时会自动跑一次**（build.lpj 的 `build` 第一条命令 = 本脚本 + `--if-changed`）：
脚本比对输入指纹（字体文件 / 字符表 / 字号 / 本脚本），没变就跳过 —— 改了文案或字号，
下次构建自动重生成，不用手动敲下面这些命令。

用法（工程根目录；电脑上要有 Pillow：`pip install pillow`）：

    python tools/generate_font.py                          # 用下面的默认值（改完 font_chars.txt 立即重生成）
    python tools/generate_font.py --if-changed             # 输入没变就跳过（构建时用的就是这条）
    python tools/generate_font.py --chars "温度 湿度"        # 临时补几个字
    python tools/generate_font.py --sizes 12,14,16          # 额外加字号（默认自动取 main.lui 用到的）
    python tools/generate_font.py --font D:/fonts/xxx.ttf   # 指定字体

默认：字体取电脑上第一个能用的中文字体；字符 = ASCII + main/font_chars.txt（+ `--chars`）；
字号 = main/main.lui 里出现的 font-size（+ `--sizes`）；输出 main/embedded_font.cpp。
"""

from __future__ import annotations

import argparse
import hashlib
import re
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

PROJECT_ROOT = Path(__file__).resolve().parents[1]

# 电脑上的中文字体（按顺序取第一个存在的；都没有就自己用 --font 指定 .ttf/.ttc）
FONT_CANDIDATES = (
    Path("C:/Windows/Fonts/msyh.ttc"),      # 微软雅黑（常规体）
    Path("C:/Windows/Fonts/msyhbd.ttc"),    # 微软雅黑（粗体）
    Path("C:/Windows/Fonts/simhei.ttf"),    # 黑体
    Path("/System/Library/Fonts/PingFang.ttc"),
    Path("/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc"),
)
DEFAULT_FONT = next((p for p in FONT_CANDIDATES if p.is_file()), FONT_CANDIDATES[0])
DEFAULT_CHARS_FILE = PROJECT_ROOT / "main" / "font_chars.txt"
DEFAULT_OUTPUT = PROJECT_ROOT / "main" / "embedded_font.cpp"

# 模板不带预置文案：要显示的中文写进 main/font_chars.txt（ASCII 会自动带上，不用写）。
DEFAULT_EXTRA = ""
# 字号默认从界面里扫（main.lui 的 `font-size: Npx`）；扫不到（没有 .lui / 没写字号）才退回这套。
DEFAULT_SIZES = "11,12,13,14,15"

# 界面文件：字库必须正好覆盖它用到的 font-size（引擎按字号精确匹配，不插值）。
DEFAULT_LUI = PROJECT_ROOT / "main" / "main.lui"
# 生成文件里的输入指纹行：`--if-changed` 拿它比对「输入有没有变」
FINGERPRINT_RE = re.compile(r"//\s*font-inputs:\s*([0-9a-f]{64})")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--font", type=Path, default=DEFAULT_FONT, help="中文字体文件（.ttf/.ttc）")
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT, help="生成的 .cpp（默认 main/embedded_font.cpp）")
    parser.add_argument("--chars", default="", help="额外要加的字（直接写一串中文）")
    parser.add_argument("--chars-file", type=Path, default=None,
                        help=f"字符表文件（默认 {DEFAULT_CHARS_FILE}）")
    parser.add_argument("--sizes", default=None,
                        help=f"逗号分隔的字号；不给 = 用 {DEFAULT_LUI} 里的 font-size（扫不到才用 {DEFAULT_SIZES}）")
    parser.add_argument("--if-changed", action="store_true",
                        help="输入（字体/字符表/字号/脚本）没变就跳过生成（构建时用；按生成文件里的指纹判断）")
    return parser.parse_args()


def chars_file(args: argparse.Namespace) -> Path:
    return args.chars_file or DEFAULT_CHARS_FILE


def read_chars_file(path: Path) -> str:
    if not path.is_file():
        raise SystemExit(f"字符表不存在: {path}\n  把界面上要显示的中文写进这个文件（纯字符表，一行也行）")
    return path.read_text(encoding="utf-8")


def parse_sizes(text: str) -> list[int]:
    try:
        sizes = sorted({int(value.strip()) for value in text.split(",") if value.strip()})
    except ValueError as exc:
        raise SystemExit("sizes must be a comma-separated list of integers") from exc
    if not sizes or any(size <= 0 for size in sizes):
        raise SystemExit("sizes must contain positive integers")
    return sizes


def lui_sizes() -> list[int]:
    """界面（main.lui）里用到的字号：`font-size: Npx`（写在注释里的也算，只多生成一套、不会缺字形）。"""
    if not DEFAULT_LUI.is_file():
        return []
    text = DEFAULT_LUI.read_text(encoding="utf-8")
    return sorted({int(found) for found in re.findall(r"font-size\s*:\s*(\d+)\s*px", text)})


def resolve_sizes(args: argparse.Namespace) -> tuple[list[int], str]:
    """字号 = `--sizes`（可省）+ 界面里用到的。缺一套字号 = 那些字画不出来，所以取并集、不取交集。"""
    explicit = parse_sizes(args.sizes) if args.sizes else []
    from_lui = lui_sizes()
    sizes = sorted(set(explicit) | set(from_lui))
    if sizes:
        return sizes, f"--sizes {explicit or '（未给）'} + {DEFAULT_LUI.name} {from_lui or '（没有 font-size）'}"
    return parse_sizes(DEFAULT_SIZES), f"默认 {DEFAULT_SIZES}（{DEFAULT_LUI.name} 不存在或没写 font-size）"


def input_fingerprint(args: argparse.Namespace, sizes: list[int]) -> str:
    """字库内容的**全部输入**：生成逻辑 + 字体文件 + 字符表 + 字号 + `--chars` 补的字。
    任何一样变了就换指纹 → 重新生成（往字符表加字、在 .lui 里改 font-size 都属于这类）。"""
    digest = hashlib.sha256()

    def put(data: bytes) -> None:
        digest.update(len(data).to_bytes(8, "little"))   # 带长度，避免拼接歧义
        digest.update(data)

    put(Path(__file__).read_bytes())
    put(args.font.read_bytes())
    put(read_chars_file(chars_file(args)).encode("utf-8"))
    put(",".join(str(size) for size in sizes).encode("ascii"))
    put(args.chars.encode("utf-8"))
    return digest.hexdigest()


def outputs_fingerprint(output: Path) -> str:
    """上次生成时记在文件里的指纹（没有/不是本脚本生成的 → 空串）。"""
    if not output.is_file():
        return ""
    found = FINGERPRINT_RE.search(output.read_text(encoding="utf-8"))
    return found.group(1) if found else ""


def collect_codepoints(args: argparse.Namespace) -> list[int]:
    chars = [chr(i) for i in range(32, 127)]        # ASCII 一律带上（界面里的英文/数字/符号）
    chars.extend(DEFAULT_EXTRA)
    chars.extend(args.chars)
    # 读字符表：`#` 开头的行是说明注释，不算字符
    for line in read_chars_file(chars_file(args)).splitlines():
        if line.lstrip().startswith("#"):
            continue
        chars.extend(line)
    chars.append("?")
    return sorted({ord(ch) for ch in chars})


def pack_bitmap(image: Image.Image) -> bytes:
    width, height = image.size
    packed = bytearray()
    for y in range(height):
        for x in range(0, width, 2):
            high = (image.getpixel((x, y)) * 15 + 127) // 255
            low = 0
            if x + 1 < width:
                low = (image.getpixel((x + 1, y)) * 15 + 127) // 255
            packed.append((high << 4) | low)
    return bytes(packed)


def render_glyph(font: ImageFont.FreeTypeFont, codepoint: int) -> tuple[bytes, int, int, int, int, int]:
    origin_x = 32
    baseline = 32
    canvas = Image.new("L", (128, 128), 0)
    draw = ImageDraw.Draw(canvas)
    draw.text((origin_x, baseline), chr(codepoint), font=font, anchor="ls", fill=255)
    bbox = canvas.getbbox()
    advance = max(1, round(font.getlength(chr(codepoint))))
    if bbox is None:
        return b"", 0, 0, 0, 0, advance

    glyph = canvas.crop(bbox)
    if glyph.width % 2:
        padded = Image.new("L", (glyph.width + 1, glyph.height), 0)
        padded.paste(glyph, (0, 0))
        glyph = padded
    bearing_x = bbox[0] - origin_x
    bearing_y = baseline - bbox[1]
    return pack_bitmap(glyph), glyph.width, glyph.height, bearing_x, bearing_y, advance


def byte_list(values: bytes) -> str:
    return ",".join(f"0x{value:02X}" for value in values)


def generate(args: argparse.Namespace) -> None:
    if not args.font.is_file():
        raise SystemExit(f"找不到字体文件: {args.font}\n  用 --font 指定一个电脑上的中文 .ttf/.ttc"
                         f"（Windows 常见: C:/Windows/Fonts/msyh.ttc）")
    sizes, size_source = resolve_sizes(args)
    fingerprint = input_fingerprint(args, sizes)
    if args.if_changed and outputs_fingerprint(args.output) == fingerprint:
        print(f"font inputs unchanged, skip: {args.output}")
        return

    codepoints = collect_codepoints(args)
    tables = []
    for size in sizes:
        font = ImageFont.truetype(str(args.font), size)
        glyphs = [render_glyph(font, codepoint) for codepoint in codepoints]
        data = bytearray()
        offsets = [0]
        for glyph in glyphs:
            data.extend(glyph[0])
            offsets.append(len(data))
        tables.append((size, glyphs, bytes(data), offsets, max(1, round(size * 1.2))))

    fallback_index = codepoints.index(ord("?"))
    lines = [
        '#include "embedded_font.h"',
        "#include <algorithm>",
        "#include <cstddef>",
        "#include <cstdint>",
        "",
        "namespace loppe_embedded {",
        "",
        f"// Generated by {Path(__file__).name}; do not edit by hand.",
        f"// font-inputs: {fingerprint}",
        "struct EmbeddedGlyphInfo {",
        "    uint32_t offset;",
        "    uint16_t width;",
        "    uint16_t height;",
        "    int16_t bearingX;",
        "    int16_t bearingY;",
        "    uint16_t advance;",
        "};",
        "",
        "static const char32_t kEmbeddedCodepoints[] = {",
        "    " + ", ".join(f"0x{codepoint:04X}u" for codepoint in codepoints),
        "};",
        "",
    ]

    for size, glyphs, data, offsets, _ in tables:
        suffix = str(size)
        lines.extend([
            f"static const uint8_t kEmbeddedData{suffix}[] = {{",
            "    " + byte_list(data),
            "};",
            f"static const uint32_t kEmbeddedOffsets{suffix}[] = {{",
            "    " + ", ".join(str(value) for value in offsets),
            "};",
            f"static const EmbeddedGlyphInfo kEmbeddedGlyphs{suffix}[] = {{",
        ])
        lines.extend(
            f"    {{{offsets[index]}, {glyph[1]}, {glyph[2]}, {glyph[3]}, {glyph[4]}, {glyph[5]}}},"
            for index, glyph in enumerate(glyphs)
        )
        lines.extend(["};", ""])

    lines.extend([
        "static int findGlyph(char32_t ch) {",
        "    for (std::size_t i = 0; i < sizeof(kEmbeddedCodepoints) / sizeof(kEmbeddedCodepoints[0]); ++i)",
        "        if (kEmbeddedCodepoints[i] == ch) return static_cast<int>(i);",
        f"    return {fallback_index};",
        "}",
        "",
        "const uint8_t* embeddedGlyphLookup(char32_t ch) {",
        "    const int index = findGlyph(ch);",
        f"    return kEmbeddedData{sizes[0]} + kEmbeddedOffsets{sizes[0]}[index];",
        "}",
        "",
        "int embeddedGlyphAdvance(char32_t ch) {",
        "    const int index = findGlyph(ch);",
        f"    return kEmbeddedGlyphs{sizes[0]}[index].advance;",
        "}",
        "",
        "static bool embeddedGlyphLookupSized(char32_t ch, int pixelSize, BitmapGlyph* out) {",
        "    if (!out) return false;",
        f"    pixelSize = std::max({sizes[0]}, std::min({sizes[-1]}, pixelSize));",
        "    const int index = findGlyph(ch);",
        "    switch (pixelSize) {",
    ])
    for size, _, _, _, _ in tables:
        lines.extend([
            f"    case {size}: {{",
            f"        const auto& glyph = kEmbeddedGlyphs{size}[index];",
            f"        out->bitmap = kEmbeddedData{size} + glyph.offset;",
            "        out->width = glyph.width;",
            "        out->height = glyph.height;",
            "        out->rowBytes = (glyph.width + 1) / 2;",
            "        out->bearingX = glyph.bearingX;",
            "        out->bearingY = glyph.bearingY;",
            "        out->advance = glyph.advance;",
            "        out->bitsPerPixel = 4;",
            "        return true;",
            "    }",
        ])
    lines.extend([
        "    default: return false;",
        "    }",
        "}",
        "",
        "static int embeddedLineHeightSized(int pixelSize) {",
        "    switch (pixelSize) {",
    ])
    for size, _, _, _, line_height in tables:
        lines.append(f"    case {size}: return {line_height};")
    lines.extend([
        "    default: return std::max(1, (pixelSize * 6 + 4) / 5);",
        "    }",
        "}",
        "",
        "const BitmapFont kEmbeddedFont{",
        '    "embedded-grayscale-4bpp",',
        f"    {sizes[0]},",
        "    0,",
        "    0,",
        f"    {tables[0][4]},",
        "    0,",
        "    0,",
        "    4,",
        "    embeddedGlyphLookup,",
        "    embeddedGlyphAdvance,",
        "    embeddedGlyphLookupSized,",
        "    embeddedLineHeightSized",
        "};",
        "",
        "} // namespace loppe_embedded",
        "",
    ])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(lines), encoding="utf-8", newline="\n")
    print(f"generated {len(codepoints)} glyphs at sizes {sizes} ({size_source}) -> {args.output}")


if __name__ == "__main__":
    generate(parse_args())
