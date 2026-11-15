// window_host.cpp —— 窗口缝的默认实现（空操作）。
//
// 没有窗口的平台（安卓 / ESP32-S3）不需要做任何事：内核调 windowHost().Close()
// 时走到这里，什么都不发生，标签照常拆掉。

#include "window_host.h"

namespace loppe_browser {
namespace {

class NullWindowHost final : public WindowHost {
public:
    bool IsMaximized() override { return false; }
    void Minimize() override {}
    void ToggleMaximize() override {}
    void Close() override {}
    void SetDragRegions(const DipRect&, const std::vector<DipRect>&) override {}
};

NullWindowHost g_nullHost;
WindowHost* g_host = nullptr;

} // namespace

void SetWindowHost(WindowHost* host)
{
    g_host = host;
}

WindowHost& windowHost()
{
    return g_host ? *g_host : static_cast<WindowHost&>(g_nullHost);
}

} // namespace loppe_browser
