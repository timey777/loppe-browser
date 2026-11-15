/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include <string>
#include <vector>
#include "components/uitypes.h"
#include "loppe_export.h"

namespace loppe {

class DisplayList;

// 阴影渲染质量档（进程级全局；建界面之前设置）
enum class LOPPE_API BoxShadowQuality
{
    None,
    Low,        // 4 环
    Medium,     // 8 环
    High,       // 16 环
    Gaussian
};

// 全局默认质量档（默认 Gaussian）
LOPPE_API void setBoxShadowQuality(BoxShadowQuality quality);
LOPPE_API BoxShadowQuality getBoxShadowQuality();

// 一条 CSS 口径的外阴影（`box-shadow`）：长度单位 dp
struct LOPPE_API BoxShadow
{
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    float blur = 0.0f;
    float spread = 0.0f;
    ui::Color color{0.0f, 0.0f, 0.0f, 0.0f};
};

// 一条同心环：expand = 距形状边缘的外扩量，width = 环宽，alpha = 不透明度系数
struct LOPPE_API ShadowRing
{
    float expand = 0.0f;
    float width = 0.0f;
    float alpha = 0.0f;
};

// 把 blur 拆成同心环；maxRings = 环数上限（Low/Medium/High = 4/8/16）
LOPPE_API std::vector<ShadowRing> shadowRingsFor(float blur, int maxRings);

// 解析 `box-shadow` 的值：`<offsetX> <offsetY> [blur] [spread] <color>`，逗号分隔可多条；
// 颜色支持 #RGB / #RRGGBB / #RRGGBBAA / rgb() / rgba() / transparent。
// 空串与 `none` 返回空列表；`inset`、缺颜色、无法识别的单条被忽略（打 warning）。
LOPPE_API std::vector<BoxShadow> parseBoxShadow(const std::string& value);

// 把一条阴影追加到显示列表；应在控件自身绘制之前调用。
// borderBox / cornerRadius = 控件边框盒（控件局部坐标）与其圆角。
LOPPE_API void appendBoxShadow(DisplayList& displayList, const ui::Rect& borderBox,
                               float cornerRadius, const BoxShadow& shadow);

// 多条阴影：按 CSS 顺序追加（列表第一条在最上层）
LOPPE_API void appendBoxShadows(DisplayList& displayList, const ui::Rect& borderBox,
                                float cornerRadius, const std::vector<BoxShadow>& shadows);

// 把矩形外扩到覆盖其 box-shadow（脏区用）；无阴影或解析失败时原样返回。
LOPPE_API ui::Rect boxShadowOuterRect(const ui::Rect& rect, const std::string& css);

} // namespace loppe
