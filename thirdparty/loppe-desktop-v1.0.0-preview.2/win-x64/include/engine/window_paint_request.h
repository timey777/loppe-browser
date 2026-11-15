/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

// window_paint_request.h — 窗口级「至少出一帧」请求（无文档上下文的调用点用）
#pragma once

#include "loppe_export.h"

namespace loppe {

// 任意线程可调：请求主循环本次 tick 至少出一帧（各视图槽各自整屏重绘一次）。
// 用于拿不到文档/视图槽上下文的信号：UiDispatcher 保底、资源加载完成等。
LOPPE_API void requestWindowPaint();

// 主循环每 tick 调一次：有请求返回 true 并清零。
LOPPE_API bool consumeWindowPaintRequest();

} // namespace loppe
