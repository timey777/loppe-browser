/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

// glyph_atlas.h — 字形图集 shelf 打包与采样 UV 的纯计算工具。

namespace loppe {

// shelf 打包：在 [pad, pageW/H) 内为 w×h 位图找位置；成功时提交页状态并给出 (outX,outY)。
// 本页放不下返回 false，且**不改状态**（调用方可换新页重试）。
bool glyphAtlasPack(int& penX, int& penY, int& rowH, int pageW, int pageH, int pad,
                    int w, int h, int& outX, int& outY);

// 采样 UV：直接用 glyph rect 的纹素边界（不做半纹素内缩）。
// 越界安全要求调用方保证：每字形 pad>=1 纹素 + 图集页零初始化（padding 全透明）。
void glyphAtlasUV(int x, int y, int w, int h, int pageW, int pageH,
                  float& u0, float& v0, float& u1, float& v1);

} // namespace loppe
