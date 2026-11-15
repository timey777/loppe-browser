// ===== 设计说明 =====
//
// 【为什么把 tick() 放在 onFrameBegin，而不是 onIdle】onIdle 只在「这一帧没东西要画」
// 时执行；页面正在重绘时它根本不会被调用，JS 的定时器/Promise 就会饿死。onFrameBegin
// 是每帧必经点，且早于 onApplyUpdates —— 这一帧标下的脏区，同一帧就能落地。
//
// 【为什么要 8ms 闸门】主循环在空闲时是毫秒级转的（onIdle 睡 1~4ms）。每转都跑一次 JS
// 泵会让 QuickJS 空转几百次/秒，纯浪费；8ms ≈ 120Hz，比人眼快，也比任何 UI 事件密。
// 闸门用引擎的 loppe::Throttle（misc/throttle.h）；标签内部还有一层按前台/后台分频的闸门，
// 见 browser_scene.cpp「为什么后台标签要降频」。
//
// 【为什么不常驻 60fps 重绘】引擎只在有脏区时 present。浏览器空闲时应该是 0 帧，
// 而不是「一直在画同一张图」。

#include "browser_app.h"

#include <chrono>
#include <thread>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "render/render_loop.h"
#include "engine/event/ui_event_system.h"

#include "platform/window_frame.h"
#include "shell_icons.h"

namespace loppe_browser {

namespace {
// GLFW 的 drop 回调没有 user data 参数，只能靠一个文件内静态指针回指应用。
// 浏览器只有一个窗口，取巧但不含糊。
BrowserApp* s_dropTarget = nullptr;
} // namespace

BrowserLoop::BrowserLoop(::Application& app, BrowserScene* scene, const FrameStyleSpec& frameStyle,
                         GLFWwindow* window)
    : loppe::RenderLoop(app), scene_(scene), window_(window)
{
    framePainter_.setStyle(frameStyle);
}

void BrowserLoop::onFrameBegin()
{
    if (!scene_) return;
    if (!tickGate_.tryAcquire()) return;   // 8ms 闸门（见文件顶部）
    scene_->tick();
}

// 视口尺寸（DIP）在 onBeforePaint 里拿：同一帧、就在 paintAll 之前，比自己去问窗口准。
void BrowserLoop::onBeforePaint(IDrawer* drawer, float width, float height)
{
    (void)drawer;
    viewportW_ = width;
    viewportH_ = height;
}

// 窗口外框：所有视图槽画完之后、present 之前。只覆盖客户区最外圈一条线，跟控件树无关。
// 走 IDrawer（CPU 光栅后端没有 GL，见 frame_paint.cpp 顶部说明）。
void BrowserLoop::onFramePresent()
{
    IDrawer* drawer = app().renderServices().drawer;
    if (!drawer) return;

    const bool focused = window_ && glfwGetWindowAttrib(window_, GLFW_FOCUSED) != 0;
    const bool maximized = window_ && glfwGetWindowAttrib(window_, GLFW_MAXIMIZED) != 0;

    framePainter_.paint(drawer, viewportW_, viewportH_, focused, maximized);
}

void BrowserLoop::onIdle()
{
    // 有页面在加载时转快一点（用户正盯着状态栏），否则让出 CPU。
    const bool busy = scene_ && scene_->isLoading();
    std::this_thread::sleep_for(std::chrono::milliseconds(busy ? 2 : 6));
}

BrowserApp::BrowserApp(std::string title, int winW, int winH)
    : Application(std::move(title), static_cast<float>(winW), static_cast<float>(winH))
{
    ResourceManager::getInstance().setResourceRoot(FileSystem::getPath(""));
    ResourceManager::getInstance().loadSystemDefaultFont("Helvetica", 14.0f);
}

void BrowserApp::setStartupAddress(std::string text)
{
    if (!text.empty()) startupAddress_ = std::move(text);
}

void BrowserApp::setStartupConfigDir(std::string dir)
{
    startupConfigDir_ = std::move(dir);
}

bool BrowserApp::build(DocumentRuntime& runtime, loppe::Compositor& compositor, float width, float height)
{
    (void)compositor;
    (void)width;
    (void)height;
    loppe::Logger::setEnabled(true);

    // 外壳图标（刷新 / 关闭 / 转圈帧）必须在壳与标签建树之前注册：icon 控件只认注册名，
    // 未注册的名字渲染端会缓存成「无纹理」，之后再注册也不会自愈。
    RegisterShellIcons();

    scene_ = std::make_shared<BrowserScene>(runtime);
    scene_->setInitialAddress(startupAddress_);
    scene_->setConfigDir(startupConfigDir_);
    scene_->setHost(this);      // 各标签的 Stage 要登记进窗口的视图槽表（BrowserHost）

    // 去掉系统标题栏：客户区铺满窗口，拖动/吸附/双击最大化/边缘缩放仍交给系统
    // （标题栏由 ui/main.lui 自己画，拖动区每帧由 BrowserScene::updateDragRegions 报过去）
    // 先把窗口动作接进内核的窗口缝，再把系统标题栏摘掉。
    InstallDesktopWindowHost();
    if (!InstallFramelessWindow(getWindowHandle())) {
        LOPPE_WARN("[win] 这个平台没有无边框实现，保留系统标题栏");
    }
    // 点在没有控件的标题栏上：那次点击被系统当拖动吃掉了，界面收不到 —— 焦点在这里收
    SetCaptionPressHandler([this]() {
        if (scene_) scene_->clearAllFocus();
    });

    installFileDrop();      // 拖一个 .js / .lui 进窗口 = 新标签里打开它

    // 窗口外框（config/window.cfg）：一条圆角描边。读不到 = 默认一条线。
    frameStyle_ = LoadFrameStyle(scene_->ResolveConfigPath("config/window.cfg"));
    // 圆角得系统替窗口裁（Windows 的 DWM）；系统不裁的平台画圆角线会在四角露出方底，
    // 直接画直的。
    if (!SystemClipsWindowCorners()) frameStyle_.radiusDp = 0.0f;

    loppe::Stage::getInstance().replaceScene(scene_);
    return true;
}

// ── 文件拖放 ──

// 窗口接成拖放目标。两个平台都由 GLFW 实现（Windows 走 WM_DROPFILES，X11 走 XDND），
// 所以这里只需要登记回调，不用碰窗口过程。引擎的 Application 没有注册 drop 回调，
// 挂在这里不会和别处抢。
void BrowserApp::installFileDrop()
{
    GLFWwindow* win = getWindowHandle();
    if (!win) return;
    s_dropTarget = this;
    glfwSetDropCallback(win, &BrowserApp::OnFilesDropped);
}

void BrowserApp::OnFilesDropped(GLFWwindow* window, int count, const char** paths)
{
    (void)window;
    if (!s_dropTarget || count <= 0 || !paths) return;

    std::vector<std::string> files;
    files.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        if (paths[i] && paths[i][0] != '\0') files.emplace_back(paths[i]);
    }
    if (!files.empty()) s_dropTarget->onFilesDropped(files);
}

void BrowserApp::onFilesDropped(const std::vector<std::string>& files)
{
    if (scene_) scene_->openLocalFiles(files);   // 内核下一帧开标签
}

// ── BrowserHost：把引擎的窗口级能力接到内核接口上 ──

IDrawer* BrowserApp::drawer() { return renderServices().drawer; }
loppe::ITextRenderer* BrowserApp::textRenderer() { return renderServices().textRenderer; }
loppe::Renderer* BrowserApp::renderer() { return renderServices().renderer; }
std::atomic<bool>* BrowserApp::pendingPresentFlag() { return Application::pendingPresentFlag(); }
loppe::ViewSlotManager& BrowserApp::views() { return m_views; }

std::unique_ptr<loppe::RenderLoop> BrowserApp::createRenderLoop()
{
    return std::make_unique<BrowserLoop>(*this, scene_.get(), frameStyle_, getWindowHandle());
}

void BrowserApp::onKey(const loppe::KeyEvent& e)
{
    if (!scene_) return;
    if (e.action != 1) return;          // 只认按下（GLFW：1 = press）
    scene_->handleKey(e.key, e.mods);   // mods 用来认 Ctrl+T / Ctrl+W / Ctrl+Tab
}

void BrowserApp::onMouseButton(const loppe::MouseButtonEvent& e)
{
    if (e.action != 1) return;

    // 标题栏空白与窗口四边的按下归窗口（拖动 / 缩放 / 双击最大化）。Windows 上系统在
    // 命中测试里就分走了，这里恒返回 false；只有自己判命中的平台（Linux）会真的接手。
    (void)FrameHandlePress(e.point.x.value, e.point.y.value, e.button, e.action);

    // 键盘焦点跟几何走：点进哪个槽，键盘就归哪个 Stage（页面槽 = 该标签；壳槽 = 窗口框）。
    // 鼠标事件本身的派发按 viewRect 命中（引擎回调里做），这里只管键盘归属。
    loppe::Stage* const previous = loppe::Stage::active();
    loppe::Stage::setActive(m_views.hitStage(e.point.x.value, e.point.y.value));

    // 换了归属，上一份界面的焦点得自己收：引擎只在「被点中的那份文档」里清焦点，
    // 别的 Stage 里那个输入框会一直挂着 Focus 状态 —— 外发光不灭、光标停在最后一次闪烁，
    // 而且它已经不在前台、不再 tick，看着就是"卡住了"。
    if (previous && previous != loppe::Stage::active()) {
        if (auto scene = previous->currentScene()) {
            if (auto* ui = scene->getUIEventSystem()) ui->updateFocus(nullptr);
        }
    }
}

} // namespace loppe_browser
