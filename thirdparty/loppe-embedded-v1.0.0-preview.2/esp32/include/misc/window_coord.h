/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

// window_coord.h — 窗口像素坐标 → 逻辑坐标（DIP）的等比换算
#pragma once

#include "engine/core/coordinate_types.h"
#include "loppe_export.h"

namespace loppe {

/**
 * @brief 窗口坐标换算：把平台回调给的窗口像素坐标映射成逻辑坐标（DIP）。
 *
 * 平台给的鼠标/触摸坐标是像素，而布局与命中用 DIP；两者按
 * 「窗口像素尺寸 : 逻辑视口尺寸」等比缩放。用法：
 *
 *     int ww = 0, wh = 0;
 *     Window::getWindowSize(window, &ww, &wh);        // 或平台自己的取尺寸方式
 *     auto p = WindowCoord::toLogical(x, y, ww, wh, 1280.0f, 800.0f);
 *     // p.x.value / p.y.value = 窗口逻辑坐标
 *
 * 与 `coordinate_types.h` 的 `toLogicalPointDp` 不是一回事：那个按 `ContentScale`
 * 在 framebuffer 像素与 DIP 之间换算（设备像素比），本类按窗口像素尺寸与逻辑视口尺寸
 * 换算（窗口被等比缩放的场景，例如 GLFW 在 HiDPI 下报的逻辑窗口尺寸）。
 */
class LOPPE_API WindowCoord {
public:
    // 像素坐标 (px, py) → 逻辑坐标：x * logicalW / windowW，y * logicalH / windowH。
    // windowW / windowH <= 0 时该轴原样返回（窗口尺寸还拿不到，不做缩放）。
    static LogicalPointDp toLogical(double px, double py,
                                    int windowW, int windowH,
                                    float logicalW, float logicalH);
};

} // namespace loppe
