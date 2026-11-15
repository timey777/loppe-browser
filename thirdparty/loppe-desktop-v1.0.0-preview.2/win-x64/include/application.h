/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

// Application.h
#pragma once

#include <memory>
#include <string>
#include <atomic>
#include "loppe_export.h"
#include "engine/core/coordinate_types.h"
#include "engine/view_slot_manager.h"
#include "components/input/input_event.h"
struct GLFWwindow;

class Window;
class IDrawer;

struct ASTNode;

namespace loppe {
    class Scene;
    class Compositor;
    class ViewCoordinator;
    class Panel;
    class Input;
    class MainScene;
    class DisplayList;
    class RenderLoop;
    class Stage;
    class Renderer;
    class ITextRenderer;
}
class DocumentRuntime;
class LOPPE_API Application {
public:
    Application(std::string title,
    float winW,
    float winH);
    virtual ~Application();
    int run(int argc, char* argv[]);

    // 请求关闭窗口：主循环随之退出（供 --selftest 等自动退出）。
    void requestClose();

    // 窗口视图槽管理器：附加视图的注册/移除与窗口级操作的入口。
    loppe::ViewSlotManager& views();

    // ── 窗口级绘图资源（建附加文档 / 追加绘制用）── 仅在 run() 主循环期间有效。
    struct RenderServices {
        IDrawer* drawer = nullptr;                    // 2D 绘制入口（自绘叠层用）
        loppe::ITextRenderer* textRenderer = nullptr; // 文本测量/绘制（建文本栈用）
        loppe::Renderer* renderer = nullptr;          // 场景渲染入口（各槽共用）
    };
    RenderServices renderServices() const;

    // GLFW callback wrappers
    static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
    static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
    static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos);
    static void cursor_enter_callback(GLFWwindow* window, int entered);
    static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
    static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void char_callback(GLFWwindow* window, unsigned int codepoint);
    static void window_focus_callback(GLFWwindow* window, int focused);
    static void window_content_scale_callback(GLFWwindow* window, float xscale, float yscale);
    static void drop_callback(GLFWwindow* window, int count, const char** paths);

    // 以给定窗口坐标投递一次鼠标移动（回调与按住期轮询共用的唯一出口）。
    static void deliverMouseMove(GLFWwindow* window, double xd, double yd);
    // 按住期间逐帧兜底：补发窗口外移动、合成窗口外丢失的释放。
    void pollCapturedPointer();

private:
    static Application* s_instance;
    friend class loppe::RenderLoop;
    // non-copyable
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    std::string m_Title;
    float m_winW;
    float m_winH;
    float m_scaleX{ 1.0f };
    float m_scaleY{ 1.0f };
    DocumentRuntime* g_documentRuntime;
    std::unique_ptr<Window> m_window;
    std::unique_ptr<class IGraphicsContext> m_graphicsContext;
    std::shared_ptr<ASTNode> m_ast;
    std::unique_ptr<loppe::Renderer> m_renderer;

    // 壳（第 0 个视图槽）的一整套：四件套引擎 + 文档运行时 + present 标志 + 合成器 + 协调器。
    struct ShellSlot;
    std::unique_ptr<ShellSlot> m_shell;

    std::unique_ptr<loppe::RenderLoop> m_renderLoop;

    bool m_pendingResize = false;
    loppe::ViewportSizeDp m_pendingViewportSize{ { 0.0f }, { 0.0f } };

    void applyViewport(const loppe::FramebufferSizePx& framebufferSize,
                       const loppe::ContentScale& contentScale);

    void syncViewportWithWindow();

    bool tryApplyLiveResize();
    void presentFrame();

protected:
    std::atomic<bool>* m_pendingPresent = nullptr;

    // 取附加视图共用的 present 标志；仅 build() 之后、run() 主循环期间有效。
    std::atomic<bool>* pendingPresentFlag() const;

    // 视图槽管理器（与 views() 返回同一对象）。
    loppe::ViewSlotManager m_views;

    // 建窗前引擎向宿主询问的初始窗口状态。
    struct StartupWindowState {
        int width = 0;          // 客户区宽（屏幕坐标）；<= 0 = 用构造函数的 winW
        int height = 0;         // 客户区高；<= 0 = 用构造函数的 winH
        bool maximized = false; // 创建即最大化
        bool visible = false;   // 默认隐藏：首帧画完由引擎显示
    };
    // 初始窗口状态：尺寸 / 是否最大化 / 初始是否可见。
    // 默认 = 构造函数给的尺寸、不最大化、隐藏。覆写时窗口还没建，但 GLFW 已初始化
    // （可以查显示器工作区）。**最大化窗口的还原尺寸也是这里给的**：建窗参数里的宽高
    // 就是用户点「还原」时回到的大小。
    virtual StartupWindowState startupWindow();

    virtual bool build(DocumentRuntime& runtime, loppe::Compositor& compositor, float width, float height);

    // 创建每帧渲染循环。默认返回内置实现；子类可覆写以叠加额外渲染（例如 Dear ImGui 属性检查器）。
    virtual std::unique_ptr<loppe::RenderLoop> createRenderLoop();

    // 暴露窗口句柄，便于叠加层初始化（例如 ImGui 需要 GLFWwindow*）。
    GLFWwindow* getWindowHandle() const;

    // 子类覆写以拦截/扩展窗口与输入事件；事件坐标为逻辑坐标(DIP)。默认空实现。
    virtual void onMouseButton(const loppe::MouseButtonEvent& e);
    virtual void onMouseMove(const loppe::MouseMoveEvent& e);
    virtual void onMouseEnter(int entered);
    virtual void onScroll(const loppe::ScrollEvent& e);
    virtual void onKey(const loppe::KeyEvent& e);
    virtual void onChar(const loppe::CharEvent& e);
    virtual void onFocusChanged(int focused);
    virtual void onContentScaleChanged(float xscale, float yscale);
    virtual void onFramebufferResized(int widthPx, int heightPx);
    virtual void onDropFiles(const std::string& path);
};