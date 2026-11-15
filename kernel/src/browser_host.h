#pragma once
// browser_host.h —— 内核要壳提供的那点东西：渲染后端 + 视图槽表。
//
// 用法（壳建好场景之前注入自己）：
//     scene = std::make_shared<BrowserScene>(runtime);
//     scene->setHost(this);     // this 实现 BrowserHost
//
// 桌面壳 = loppe 的 Application（BrowserApp 直接多继承它，转发五个方法）；
// 安卓 / ESP32-S3 没有 Application，由各自的 runtime 实现（SurfaceView / EmbeddedHost）。
// 不注入也能跑：内核遇到空宿主只跳过「附加 Stage 的渲染」，不会崩。

#include <atomic>

#include "loppe.h"

namespace loppe_browser {

class BrowserHost {
public:
    virtual ~BrowserHost() = default;

    // 与壳共用的渲染资源（同一个 drawer / 文字渲染器 / 帧脏标志）。
    virtual IDrawer* drawer() = 0;
    virtual loppe::ITextRenderer* textRenderer() = 0;
    // 窗口级渲染入口：所有标签的 Compositor 都挂在同一个 Renderer 上（图标纹理缓存共用一份）。
    virtual loppe::Renderer* renderer() = 0;
    // 有脏区要 present 时由引擎置位的标志；没有窗口的平台可以返回 nullptr。
    virtual std::atomic<bool>* pendingPresentFlag() = 0;

    // 窗口的视图槽表：一个标签 = 一个额外 Stage + 自己的 Compositor / ViewCoordinator，
    // 在这里 attach / detach；叠放顺序、命中测试、叠画都由它统一发。
    virtual loppe::ViewSlotManager& views() = 0;
};

} // namespace loppe_browser
