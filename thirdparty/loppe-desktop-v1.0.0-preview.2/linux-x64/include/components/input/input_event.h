/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "engine/core/coordinate_types.h"
#include "engine/core/input_source.h"

namespace loppe {

// 指针按键编号（取值与 GLFW 一致）。判键就写 mouse_button::kLeft 这类名字，别写裸 0/1/2。
namespace mouse_button
{
    constexpr int kLeft = 0;     // 主键：只有它会派发 Click
    constexpr int kRight = 1;    // 次键：上下文菜单
    constexpr int kMiddle = 2;
}

// 指针按键事件。坐标空间由模板参数决定（点位用哪个空间，就取哪个别名）。
template <typename PointT>
struct MouseButtonEventT {
    PointT point{};
    int button = mouse_button::kLeft;   // 按键编号，见 mouse_button::kXxx
    int action = 0;   // GLFW：1 = 按下，0 = 抬起
    int mods = 0;     // GLFW 修饰键位（SHIFT / CONTROL / ALT / SUPER）
    // 这次事件的来源（触摸/鼠标）：Unspecified = 不改 Scene 当前来源。
    // 同一场景的指针事件来自多种设备时，在这里逐事件写清；单设备宿主设一次 setInputSource 即可。
    InputSource source = InputSource::Unspecified;
};

// 指针移动事件。
template <typename PointT>
struct MouseMoveEventT {
    PointT point{};
    InputSource source = InputSource::Unspecified;   // 同上
};

// 滚轮事件：滚动增量（逻辑 DIP）。
template <typename DeltaT>
struct ScrollEventT {
    DeltaT delta{};
};

// 键盘按键事件（key / scancode / action / mods 均为 GLFW 原生编码）。
struct KeyEvent {
    int key = 0;
    int scancode = 0;
    int action = 0;   // GLFW：1 = 按下，0 = 抬起，2 = 重复
    int mods = 0;
};

// 文本输入事件：一个 UTF-32 码点。
struct CharEvent {
    unsigned int codepoint = 0;
};

// 窗口/场景逻辑坐标（LogicalPointDp）
using MouseButtonEvent = MouseButtonEventT<LogicalPointDp>;
using MouseMoveEvent = MouseMoveEventT<LogicalPointDp>;
using ScrollEvent = ScrollEventT<LogicalScrollDeltaDp>;

// 控件本地坐标（ControlLocalPointDp）
using ControlLocalMouseButtonEvent = MouseButtonEventT<ControlLocalPointDp>;
using ControlLocalMouseMoveEvent = MouseMoveEventT<ControlLocalPointDp>;
using ControlLocalScrollEvent = ScrollEventT<ControlLocalScrollDeltaDp>;

// 父控件本地坐标（ParentLocalPointDp）
using ParentLocalMouseButtonEvent = MouseButtonEventT<ParentLocalPointDp>;
using ParentLocalMouseMoveEvent = MouseMoveEventT<ParentLocalPointDp>;

} // namespace loppe
