/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

// Window.h
#pragma once

#include <string>
#include "loppe_export.h"

struct GLFWwindow;

class LOPPE_API Window {
public:
    // 光标形状（数值与 loppe::Scene::CursorType 一一对应）。
    enum class CursorType { Arrow, Hand, IBeam, ResizeHorizontal, ResizeVertical };

    Window(int width, int height, const std::string& title, bool isFullScreen = false,
           bool visible = true, bool maximized = false);
    ~Window();

    // Platform init/terminate
    bool init(int width, int height, const std::string& title, bool isFullScreen, bool visible = true,
              bool maximized = false);

    // 建窗前的平台准备（DPI 感知 + GLFW 初始化；可重复调用）。
    // 需要在建窗**之前**查询显示器/工作区的宿主先调它一次（`glfwGetPrimaryMonitor` 等）。
    static void preparePlatform();
    void quit();
    // 请求关闭窗口（设置 GLFW should-close 标志，主循环随后退出）
    void requestClose();

    GLFWwindow* get() const;
    // 原生窗口句柄（Windows: HWND；其它平台/未建窗: nullptr）。纯 CPU 后端用它贴位图。
    void* getNativeWindowHandle() const;
    bool shouldClose() const;
    void swapBuffers();
    void pollEvents();
    // 显示 / 隐藏窗口（隐藏期间照常渲染；show() 会同时把窗口聚焦）
    void show();
    void hide();

    void setFramebufferSizeCallback(void (*cb)(GLFWwindow*, int, int));
    void setMouseButtonCallback(void (*cb)(GLFWwindow*, int, int, int));
    void setCursorPosCallback(void (*cb)(GLFWwindow*, double, double));
    void setCursorEnterCallback(void (*cb)(GLFWwindow*, int));
    void setScrollCallback(void (*cb)(GLFWwindow*, double, double));
    void setKeyCallback(void (*cb)(GLFWwindow*, int, int, int, int));
    void setCharCallback(void (*cb)(GLFWwindow*, unsigned int));
    void setWindowFocusCallback(void (*cb)(GLFWwindow*, int));
    void setDropCallback(void (*cb)(GLFWwindow*, int, const char**));
    void setWindowContentScaleCallback(void (*cb)(GLFWwindow*, float, float));

    static void getFramebufferSize(GLFWwindow* window, int* width, int* height);
    static void getWindowContentScale(GLFWwindow* window, float* xscale, float* yscale);
    static void getWindowSize(GLFWwindow* window, int* width, int* height);
    // GLFW 光标坐标（窗口像素）→ 窗口逻辑坐标（DIP）。
    static void toLogical(GLFWwindow* window, double px, double py,
                          float logicalW, float logicalH, float& outX, float& outY);
    void getFramebufferSize(int* width, int* height) const;
    void getWindowContentScale(float* xscale, float* yscale) const;
    void getWindowSize(int* width, int* height) const;

    static void getCursorPos(GLFWwindow* win, double* x, double* y);
    static void setCursor(GLFWwindow* win, CursorType type);
    static void updateMouseCapture(GLFWwindow* win, int button, int action);
    static bool isMouseCaptured();
    static void forceReleaseMouseCapture();

    // 当前按住的主键位掩码（bit0=左 / bit1=右 / bit2=中）。mouse_button_callback 维护，全平台有效。
    static unsigned int pressedButtonsMask();

    // 查询真实指针状态：x/y = 相对窗口客户区（像素，可与 toLogical 配套换算），
    // heldButtons = 实际按住的键位（位定义同 pressedButtonsMask）。仅按住期间逐帧调用。
    // 返回 false = 当前平台查不到（Windows 走 OS 捕获不需要查询；指针在其它屏幕）。
    static bool queryPointerState(GLFWwindow* win, double* x, double* y, unsigned int* heldButtons);

    void setClipboardString(const std::string& s);
    std::string getClipboardString() const;

private:
    GLFWwindow* m_window;
};