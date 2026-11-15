#pragma once
// window_host.h —— 窗口动作的「缝」：内核不认识窗口，壳来提供能力。
//
// 用法（壳启动时调一次，之后内核自己用）：
//     loppe_browser::SetWindowHost(&myHost);     // 传 nullptr = 恢复空实现
//
// 内核里问窗口只有四件事：关窗口、最小化、最大化开关、报拖动区。
// 桌面壳接 src/platform/window_frame.cpp（真窗口）；安卓 / ESP32-S3 没有窗口，
// 不装宿主就是空实现 —— 关最后一个标签只拆标签，不会去关一个不存在的窗口。

#include <vector>

namespace loppe_browser {

// 一个矩形（DIP，客户区左上角为原点）
struct DipRect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

class WindowHost {
public:
    virtual ~WindowHost() = default;

    virtual bool IsMaximized() = 0;
    virtual void Minimize() = 0;
    virtual void ToggleMaximize() = 0;
    virtual void Close() = 0;

    // 可拖动的横条 + 其中不能拖的子区域（标签、按钮）。传空 = 整个窗口都不能拖。
    virtual void SetDragRegions(const DipRect& dragStrip, const std::vector<DipRect>& noDrag) = 0;
};

// 装/卸宿主。传进来的对象要活得比内核久（壳里用静态对象）。
void SetWindowHost(WindowHost* host);

// 永远返回有效引用：没装宿主时是空实现，调用方不用判空。
WindowHost& windowHost();

} // namespace loppe_browser
