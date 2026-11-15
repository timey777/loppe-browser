/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

// 按住期真实指针状态查询的跨 TU 出口。实现见 window_pointer_poll_x11.cpp
// （Xlib 的 Window typedef 与引擎 class Window 同名，实现不能与 Window.cpp 同 TU）。
#pragma once

struct GLFWwindow;

// 查询真实指针状态：x/y = 相对窗口客户区（像素），heldButtons = 实际按住的键位
// （bit0=左 / bit1=右 / bit2=中，与 Window::pressedButtonsMask 同一口径）。
// 仅 Linux 有实现；Windows 没有实现（Window::queryPointerState 不会调用到这里）。
// 返回 false = 查不到（指针在其它屏幕）。
bool loppeQueryX11PointerState(GLFWwindow* win, double* x, double* y, unsigned int* heldButtons);
