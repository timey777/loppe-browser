#pragma once
// browser_app.h —— 窗口外壳：开窗 + 装 BrowserScene + 每帧推进页面。
//
// 用法：
//     loppe_browser::BrowserApp app("Loppe Browser", 1100, 760);
//     app.setStartupAddress("loppe");      // 可选，默认不联网：开一个开始页标签
//     return app.run(argc, argv);
//
// 想换渲染后端（CPU 渲染）就在 build.lpj 的 define 里加 LOPPE_RENDER_CPU。

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "loppe.h"

#include "misc/throttle.h"

#include "browser_host.h"
#include "browser_scene.h"
#include "platform/frame_paint.h"

struct GLFWwindow;

namespace loppe_browser {

// 每帧在「布局/重绘之前」推进页面脚本与加载结果。
class BrowserLoop : public loppe::RenderLoop {
public:
    BrowserLoop(::Application& app, BrowserScene* scene, const FrameStyleSpec& frameStyle,
                GLFWwindow* window);
    void onFrameBegin() override;
    // 视口尺寸（DIP）随每帧下发 —— 外框用它算圆角与描边
    void onBeforePaint(IDrawer* drawer, float width, float height) override;
    // 所有视图槽画完之后：给窗口最外圈画一条边框线，不进任何控件树
    void onFramePresent() override;
    void onIdle() override;

private:
    BrowserScene* scene_ = nullptr;
    FramePainter framePainter_;
    GLFWwindow* window_ = nullptr;   // 只用来查焦点 / 最大化（不建 GL 上下文也有效）
    float viewportW_ = 0.0f;
    float viewportH_ = 0.0f;
    // tick 的闸门：最多每 8ms 一次（≈120Hz）。各标签内部的 JS 泵还有自己的闸门。
    loppe::Throttle tickGate_{std::chrono::milliseconds(8)};
};

// 桌面壳：Application（窗口 + 主循环）+ BrowserHost（内核要的渲染资源与 Stage 挂载）。
class BrowserApp : public Application, public BrowserHost {
public:
    BrowserApp(std::string title, int winW, int winH);

    bool build(DocumentRuntime& runtime, loppe::Compositor& compositor, float width, float height) override;
    std::unique_ptr<loppe::RenderLoop> createRenderLoop() override;
    void onKey(const loppe::KeyEvent& e) override;
    void onMouseButton(const loppe::MouseButtonEvent& e) override;

    void setStartupAddress(std::string text);
    // config/ 的绝对目录（run 之前设；缺省 = 内核默认探测）。exe 落点不止一种布局时由 main() 探好传入。
    void setStartupConfigDir(std::string dir);

    // ── BrowserHost：内核只认这个接口（不认窗口）──
    // 注意：这些不是 Application 的虚函数重写，而是浏览器自己的抽象接口实现；
    // 引擎侧对应能力分别是 renderServices() / m_views / pendingPresentFlag()。
    IDrawer* drawer() override;
    loppe::ITextRenderer* textRenderer() override;
    loppe::Renderer* renderer() override;
    std::atomic<bool>* pendingPresentFlag() override;
    loppe::ViewSlotManager& views() override;

private:
    // 文件拖放：把窗口接成拖放目标（GLFW 的 drop 回调），拖进来的文件交给内核开新标签。
    void installFileDrop();
    void onFilesDropped(const std::vector<std::string>& files);
    static void OnFilesDropped(GLFWwindow* window, int count, const char** paths);

    std::shared_ptr<BrowserScene> scene_;
    std::string startupAddress_;   // 空 = 启动开开始页（不联网）
    std::string startupConfigDir_;
    FrameStyleSpec frameStyle_;   // config/window.cfg：窗口外框（build() 时读，createRenderLoop 交给 loop）
};

} // namespace loppe_browser
