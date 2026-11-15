#pragma once
// window_frame.h —— 去掉系统标题栏（客户区铺满整个窗口），拖动/缩放照样能用。
//
// 用法（Application::build() 里，窗口已经建好）：
//     loppe_browser::InstallDesktopWindowHost();                  // 窗口能力接进内核的窗口缝
//     loppe_browser::InstallFramelessWindow(getWindowHandle());   // 失败 = 这个平台不支持，标题栏留着
//     // 拖动区由内核每帧算好后经 windowHost().SetDragRegions(...) 报进来，壳不用管。
//
// 约定：
//   - 只有 InstallFramelessWindow 返回 true 才有拖动区这回事，其余接口都是空实现。
//   - 拖动区 = dragStrip 减去 noDrag。点在里面的效果和原生标题栏一致：拖动、
//     双击最大化/还原；Windows 上还带 Aero 吸附（拖到屏幕顶端/左右边缘）。
//   - 窗口四边/四角 6dp 内是缩放区（最大化时没有）。
//   - 其余区域照常派发给界面，界面代码不用关心窗口边框。

#include <functional>
#include <vector>

#include "window_host.h"   // DipRect：与内核窗口缝共用同一个矩形类型

namespace loppe_browser {

// 去掉系统标题栏。glfwWindow = Application::getWindowHandle()。
// 返回 false = 当前平台没有实现（调用方保留系统标题栏即可）。
bool InstallFramelessWindow(void* glfwWindow);

// 把窗口动作（关窗 / 最小化 / 最大化 / 拖动区）装进内核的窗口缝。
// 在 InstallFramelessWindow 之前调一次；没有窗口的壳（安卓 / ESP32）不调，窗口缝保持空操作。
void InstallDesktopWindowHost();

// 点在没有界面元素的标题栏区域时回调：宿主在这里收起键盘焦点，
// 效果与原生窗口「点标题栏就失焦」一致。
void SetCaptionPressHandler(std::function<void()> handler);

// 指针按下（窗口 DIP 坐标 + GLFW 的 button/action）。返回 true = 这一下被窗口当
// 标题栏/边框接管了。Windows 上系统在 WM_NCHITTEST 里就分走了，这里永远返回 false；
// 只有自己实现命中测试的平台（Linux）会真的处理。壳在 onMouseButton 里调它。
bool FrameHandlePress(float xDip, float yDip, int button, int action);

// 系统会不会把窗口四角裁成圆角（Windows 的 DWM 会；Linux 的窗口是方的）。
// 外框描边按它决定拐不拐弯：画圆角线而系统不裁，四角会露出方形的底色。
bool SystemClipsWindowCorners();

} // namespace loppe_browser
