/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

// missing_glyph.h — 缺字方块兜底与不可见码点判定（纯计算）。

#include <cstdint>
#include <vector>


namespace loppe {

// Unicode East Asian Width = W/F（宽/全角）判定。
bool isEastAsianWide(uint32_t cp);

// 不可见码点：C0 控制符（除 '\t' 与 '\n'）、DEL(0x7F)、U+FEFF（BOM / 零宽不换行空格）。
// 语义：零宽（advance = 0）且不产生字形/quad —— 与 '\n' 的处理一致。
// 例外：'\n' 由调用方当换行处理；'\t' 走制表位，也不视为不可见。
bool isInvisibleCodePoint(uint32_t cp);

// 缺字的布局 advance（**设备像素整数**；调用方 / scale 转 DIP）。
//   wide  → 1em（= 字号 × scale，四舍五入到设备像素）
//   other → 该字体 .notdef 的 advance（不发明宽度），但不小于 0
// notdefAdvancePx：FT_Load_Char(cp, FT_LOAD_DEFAULT) 后 face->glyph->advance.x / 64。
int missingGlyphAdvancePx(uint32_t cp, float sizeDip, float scale, int notdefAdvancePx);

// 缺字方块位图（引擎自绘；8-bit 覆盖度，直接进字形图集）。
struct MissingGlyphImage {
    int width = 0;   // 位图宽（设备像素）
    int height = 0;  // 位图高（设备像素）
    int left = 0;    // bitmap_left 语义：位图左缘相对笔位（设备像素）
    int top = 0;     // bitmap_top 语义：位图顶相对基线（设备像素）
    int advancePx = 0; // 布局 advance = 格子宽（设备像素）
    std::vector<unsigned char> pixels; // width*height，0 或 255（空心矩形边框）
};

// ---------------------------------------------------------------------------
// 制表符 '\t'
//
// 规则：制表位从行首起每 kTabWidthEm × 字号一格；'\t' 的宽度 = 到下一个制表位
// （已经在制表位上时也前进一整格）。绘制上 '\t' 不画任何字形，只占这段空白；
// 光标的字符矩形就是这段空白，所以光标在 '\t' 上前进一格、选区能盖住它。
// 字号的量纲与 Font::size 一致（DIP）。
// ---------------------------------------------------------------------------
constexpr int kTabWidthEm = 4;

// 水平笔位 penX 处的 '\t' 应占的宽度。tolerance 吸收浮点累加误差。
float tabStopAdvance(float penX, float fontSize);

// 生成缺字方块位图：格内居中、左右留边、底边贴基线、高度 ≈ 0.72em；笔画随字号缩放（1px 起）。
MissingGlyphImage makeMissingGlyphImage(uint32_t cp, float sizeDip, float scale,
                                        int notdefAdvancePx);

} // namespace loppe
