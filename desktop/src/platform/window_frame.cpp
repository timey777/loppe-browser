// ===== 设计说明 =====
//
// 【为什么动 Win32 而不是加一个引擎的 glfwWindowHint】GLFW 的 GLFW_DECORATED=false 只是把
// WS_CAPTION 摘掉：拖动、双击最大化、Aero 吸附、边缘缩放会一起消失，等于把四个系统行为自己
// 重写一遍。这里反过来做——**样式全留着**（WS_CAPTION | WS_THICKFRAME | WS_MAXIMIZEBOX…），
// 只把非客户区高度改成 0（WM_NCCALCSIZE 返回 0）：系统照旧认为「上面有个标题栏」，
// 于是拖动/吸附/缩放/阴影/圆角都不用自己实现，只是那条标题栏看不见、由我们自己画。
//
// 【为什么要单独调 DwmSetWindowAttribute】非客户区归零也把系统画的那一圈边框像素带走了，
// 剩下阴影还在、圆角没了。Win11 上用 DWMWA_WINDOW_CORNER_PREFERENCE 把圆角裁切要回来，
// DWMWA_BORDER_COLOR 顺带设成与界面描边同色。Win10 上这两条失败，保持直角 —— 不补偿。
//
// 【HTCAPTION 与界面点击的冲突怎么解】命中测试返回 HTCAPTION 的点，界面收不到鼠标事件。
// 所以宿主每帧把「标签、按钮」的矩形报进来：落在这些矩形里一律 HTCLIENT，剩下的标题栏
// 空白才当拖动区。列表只有几个元素，命中测试里线性扫一遍即可（鼠标移动频率 × 8 次比较）。
//
// 【最大化为什么直接取工作区】最大化时系统给的窗口矩形 = 屏幕矩形 + 一圈看不见的边框，
// 客户区 = 窗口矩形（我们把非客户区设成 0）就会比屏幕大出一圈，内容四边被裁掉。
// 这里不做「窗口矩形减边框」的算术：**WM_NCCALCSIZE 里 GetWindowRect 拿到的还是上一次的
// 矩形**（新矩形就在参数里，窗口尚未真正 set），减出来会得到「旧客户区 + 偏移」——
// 表现为窗口标题栏状态是最大化、内容却停在原地。直接取监视器 rcWork，一次到位，
// 有任务栏 / 竖排任务栏 / 多显示器都跟着系统走。
//
// 【为什么不改 GLFW 的 WndProc 而是自己挂一层】GLFW 的窗口过程还管着尺寸/焦点/DPI 回调，
// 替换掉它等于把引擎的地基抽了。用 SetWindowLongPtr 存一份旧过程、除了两条消息全部转发，
// 是最小侵入；窗口销毁时（WM_NCDESTROY）把旧过程装回去。
//
// ── Linux（GLFW + X11）──
//
// 【为什么不能照抄 Windows 的做法】X11 没有「保留样式、只把非客户区设成 0」这条路：
// 有没有装饰栏是窗口管理器按 _MOTIF_WM_HINTS 决定的，客户端只能在 GLFW_DECORATED 上二选一。
// 于是这边真正把装饰栏摘掉，拖动/缩放改成**请窗口管理器代劳**（_NET_WM_MOVERESIZE）。
//
// 【为什么不自己挪窗口】自己 setWindowPos 跟着指针走，等于把「拖动」重写一遍：会掉帧、
// 没有贴边吸附、没有拖出最大化还原。_NET_WM_MOVERESIZE 是 EWMH 给的标准入口（GTK/Chromium
// 的 CSD 都走它）：发一条 ClientMessage，窗口管理器接管指针，之后的表现和原生标题栏一样。
//
// 【双击最大化只能自己做】窗口管理器只认自己的标题栏，客户端发的 MOVE 请求里没有"双击"。
// 所以第二次按下落在同一处（400ms / 8dp 内）就地切换最大化，不再发 MOVE。
//
// 【命中测试为什么在按下时做】Windows 是 WM_NCHITTEST 由系统问；这边没有这个口子，
// 壳在 onMouseButton 里把按下的 DIP 坐标交进来，平台层按同一份拖动区/边框宽度判一次。
// 边界用 6dp，与 Windows 那边一致；引擎会把这次按下照常派发给界面 —— 落在标题栏空白或
// 窗口边缘上，界面那边没有可点的东西，不影响。

#include "platform/window_frame.h"

#if defined(_WIN32)

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>   // DragAcceptFiles（WIN32_LEAN_AND_MEAN 不带它）

// winuser.h 把 IsMaximized 定义成宏（IsMaximized(hwnd) → IsZoomed(hwnd)），
// 会吃掉下面适配器里的同名虚函数 —— 先撤掉这个宏。
#ifdef IsMaximized
#undef IsMaximized
#endif

namespace loppe_browser {
namespace {

WNDPROC g_oldProc = nullptr;
HWND g_hwnd = nullptr;
int g_dpi = 96;
DipRect g_dragStrip{};
std::vector<DipRect> g_noDrag;
std::function<void()> g_captionPress;

float Scale() { return static_cast<float>(g_dpi) / 96.0f; }

bool Contains(const DipRect& r, float x, float y)
{
    return r.w > 0.0f && r.h > 0.0f && x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

// GetDpiForWindow 是 Win10 1607+ 的导出；老系统上取不到就按 96 算。
int DpiOf(HWND hwnd)
{
    using PFN_GetDpiForWindow = UINT(WINAPI*)(HWND);
    static PFN_GetDpiForWindow fn = reinterpret_cast<PFN_GetDpiForWindow>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
    if (fn) {
        const UINT dpi = fn(hwnd);
        if (dpi != 0) return static_cast<int>(dpi);
    }
    HDC dc = GetDC(hwnd);
    const int dpi = dc ? GetDeviceCaps(dc, LOGPIXELSX) : 96;
    if (dc) ReleaseDC(hwnd, dc);
    return dpi > 0 ? dpi : 96;
}

int MetricForDpi(int index)
{
    using PFN_GetSystemMetricsForDpi = int(WINAPI*)(int, UINT);
    static PFN_GetSystemMetricsForDpi fn = reinterpret_cast<PFN_GetSystemMetricsForDpi>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetSystemMetricsForDpi"));
    if (fn) return fn(index, static_cast<UINT>(g_dpi));
    const int v = GetSystemMetrics(index);
    return MulDiv(v, g_dpi, 96);
}

int FrameBorderX() { return MetricForDpi(SM_CXSIZEFRAME) + MetricForDpi(SM_CXPADDEDBORDER); }
int FrameBorderY() { return MetricForDpi(SM_CYSIZEFRAME) + MetricForDpi(SM_CXPADDEDBORDER); }

// 客户区 = 整个窗口；最大化/最小化时贴到监视器工作区（避开任务栏，不超出屏幕）。
LRESULT CalcSize(HWND hwnd, LPARAM lParam)
{
    auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
    if (IsZoomed(hwnd) || IsIconic(hwnd)) {
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        if (mon && GetMonitorInfoW(mon, &mi)) {
            params->rgrc[0] = mi.rcWork;
        } else {
            // 取不到监视器信息（极老系统）：退回「rgrc[0] 减一圈边框」，
            // 这是系统本来给的窗口矩形，减完正好落在屏幕内。
            params->rgrc[0].left += FrameBorderX();
            params->rgrc[0].top += FrameBorderY();
            params->rgrc[0].right -= FrameBorderX();
            params->rgrc[0].bottom -= FrameBorderY();
        }
    }
    return 0;   // 非客户区高度 = 0
}

LRESULT HitTest(HWND hwnd, LPARAM lParam)
{
    POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
    if (!ScreenToClient(hwnd, &pt)) return HTCLIENT;

    RECT rc{};
    GetClientRect(hwnd, &rc);
    const int grip = static_cast<int>(6.0f * Scale() + 0.5f);

    // 先给边缘/角落（最大化时系统不让拖边框）
    if (!IsZoomed(hwnd)) {
        const bool left = pt.x < grip;
        const bool right = pt.x >= rc.right - grip;
        const bool top = pt.y < grip;
        const bool bottom = pt.y >= rc.bottom - grip;
        if (top && left) return HTTOPLEFT;
        if (top && right) return HTTOPRIGHT;
        if (bottom && left) return HTBOTTOMLEFT;
        if (bottom && right) return HTBOTTOMRIGHT;
        if (left) return HTLEFT;
        if (right) return HTRIGHT;
        if (top) return HTTOP;
        if (bottom) return HTBOTTOM;
    }

    const float x = static_cast<float>(pt.x) / Scale();
    const float y = static_cast<float>(pt.y) / Scale();
    if (Contains(g_dragStrip, x, y)) {
        for (const DipRect& r : g_noDrag) {
            if (Contains(r, x, y)) return HTCLIENT;
        }
        return HTCAPTION;
    }
    return HTCLIENT;
}

// 把窗框外观交回 DWM：圆角裁切 + 系统边框色。Win11 才有这两条，Win10 上它们返回失败 ——
// 失败就保持系统默认（直角、无自定义边框色），不做任何自绘补偿。
// 动态取 dwmapi 的入口，不为它新增链接项。
void ApplyDwmFrame(HWND hwnd)
{
    using PFN_DwmSetWindowAttribute = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
    static PFN_DwmSetWindowAttribute fn = []() -> PFN_DwmSetWindowAttribute {
        HMODULE mod = LoadLibraryW(L"dwmapi.dll");
        if (!mod) return nullptr;
        return reinterpret_cast<PFN_DwmSetWindowAttribute>(
            GetProcAddress(mod, "DwmSetWindowAttribute"));
    }();
    if (!fn) return;

    // 属性号按 dwmapi.h 的值写死：老 SDK 里没有这两个枚举，但运行时是系统的。
    constexpr DWORD kCornerPreference = 33;   // DWMWA_WINDOW_CORNER_PREFERENCE
    constexpr DWORD kBorderColor = 34;        // DWMWA_BORDER_COLOR

    const int round = 2;                      // DWMWCP_ROUND
    fn(hwnd, kCornerPreference, &round, sizeof(round));

    // 与界面里那条描边同色（frame_paint.cpp 的 #b8c2d4）。最大化时 DWM 自己藏起来。
    const COLORREF border = RGB(0xb8, 0xc2, 0xd4);
    fn(hwnd, kBorderColor, &border, sizeof(border));
}

LRESULT CALLBACK FrameProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_NCCALCSIZE:
        // 只接管「窗口区 → 客户区」这一向；反向（客户区 → 窗口区）交给系统算，
        // 那是 glfwSetWindowSize 之类按客户区尺寸反推窗口尺寸时用的。
        if (wParam != TRUE) break;
        return CalcSize(hwnd, lParam);
    case WM_NCHITTEST:
        return HitTest(hwnd, lParam);
    case WM_NCLBUTTONDOWN:
        // 压在拖动区上的点击：系统会接管（拖动 / 双击最大化），界面永远收不到它。
        // 界面里的焦点在这里收，跟原生窗口「点标题栏就失焦」一样。不 return，照常交给系统。
        if (g_captionPress) g_captionPress();
        break;
    case WM_DPICHANGED:
        g_dpi = HIWORD(wParam);
        break;
    case WM_NCDESTROY:
        if (g_oldProc) SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_oldProc));
        g_oldProc = nullptr;
        g_hwnd = nullptr;
        break;
    default:
        break;
    }
    return CallWindowProcW(g_oldProc ? g_oldProc : DefWindowProcW, hwnd, msg, wParam, lParam);
}

} // namespace

bool InstallFramelessWindow(void* glfwWindow)
{
    if (!glfwWindow) return false;
    HWND hwnd = glfwGetWin32Window(static_cast<GLFWwindow*>(glfwWindow));
    if (!hwnd) return false;
    if (g_hwnd == hwnd) return true;

    g_hwnd = hwnd;
    g_dpi = DpiOf(hwnd);

    // 样式补齐：拖动/吸附/双击最大化/边缘缩放全都靠这几个位（不写就只剩一个不能动的窗口）
    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    style |= WS_CAPTION | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU;
    SetWindowLongPtrW(hwnd, GWL_STYLE, style);

    g_oldProc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(FrameProc)));

    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

    // 非客户区归零之后系统不再画窗框，圆角与边框色得单独要回来（Win11 生效）
    ApplyDwmFrame(hwnd);

    // 文件拖放：让这个窗口接收 WM_DROPFILES（GLFW 建窗时也会设一次，重复设是幂等的；
    // 写明在这里，换 GLFW 版本时不会静默丢功能）。回调本身在 browser_app 里挂。
    DragAcceptFiles(hwnd, TRUE);
    return g_oldProc != nullptr;
}

void SetDragRegions(const DipRect& dragStrip, const std::vector<DipRect>& noDrag)
{
    g_dragStrip = dragStrip;
    g_noDrag = noDrag;
}

void SetCaptionPressHandler(std::function<void()> handler)
{
    g_captionPress = std::move(handler);
}

// 命中的分配全在 WM_NCHITTEST 里由系统做完了，界面的按下不需要再判一次。
bool FrameHandlePress(float, float, int, int)
{
    return false;
}

bool SystemClipsWindowCorners()
{
    return true;   // DWM 裁圆角
}

bool WindowIsMaximized()
{
    return g_hwnd && IsZoomed(g_hwnd) != FALSE;
}

void WindowMinimize()
{
    if (g_hwnd) ShowWindow(g_hwnd, SW_MINIMIZE);
}

void WindowToggleMaximize()
{
    if (!g_hwnd) return;
    ShowWindow(g_hwnd, IsZoomed(g_hwnd) ? SW_RESTORE : SW_MAXIMIZE);
}

void WindowClose()
{
    if (g_hwnd) PostMessageW(g_hwnd, WM_CLOSE, 0, 0);
}

// 把上面这几个动作接进内核的窗口缝：内核（kernel/src/browser_scene.cpp）只认 WindowHost，
// 不认识 Win32 —— 安卓 / ESP32 那两个壳不装宿主，走内核里的空实现。
void InstallDesktopWindowHost()
{
    class DesktopWindowHost final : public WindowHost {
    public:
        bool IsMaximized() override { return WindowIsMaximized(); }
        void Minimize() override { WindowMinimize(); }
        void ToggleMaximize() override { WindowToggleMaximize(); }
        void Close() override { WindowClose(); }
        void SetDragRegions(const DipRect& dragStrip, const std::vector<DipRect>& noDrag) override
        {
            ::loppe_browser::SetDragRegions(dragStrip, noDrag);
        }
    };
    static DesktopWindowHost host;
    SetWindowHost(&host);
}

} // namespace loppe_browser

#elif defined(__linux__)

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <chrono>
#include <cmath>
#include <cstdio>

namespace loppe_browser {
namespace {

GLFWwindow* g_win = nullptr;
DipRect g_dragStrip{};
std::vector<DipRect> g_noDrag;
std::function<void()> g_captionPress;

// 双击判定：上一次落在拖动区里的按下（时间 + 位置）
bool g_hasLastPress = false;
std::chrono::steady_clock::time_point g_lastPressAt;
float g_lastPressX = 0.0f;
float g_lastPressY = 0.0f;

constexpr float kResizeGripDip = 6.0f;
constexpr int kDoubleClickMs = 400;

// EWMH 的 _NET_WM_MOVERESIZE 方向码：0..7 从左上顺时针，8 = 整体移动
enum : int {
    kDirTopLeft = 0, kDirTop = 1, kDirTopRight = 2, kDirRight = 3,
    kDirBottomRight = 4, kDirBottom = 5, kDirBottomLeft = 6, kDirLeft = 7,
    kDirMove = 8,
};

bool Contains(const DipRect& r, float x, float y)
{
    return r.w > 0.0f && r.h > 0.0f && x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

bool WindowIsMaximized()
{
    return g_win && glfwGetWindowAttrib(g_win, GLFW_MAXIMIZED) != 0;
}

// 窗口尺寸换成 DIP：引擎把窗口像素按比例映射成 DIP（Window::toLogical），
// 这里用同一口径（content scale），否则高 DPI 下四边框会判偏。
bool WindowSizeDip(float* w, float* h)
{
    if (!g_win) return false;
    int ww = 0, wh = 0;
    glfwGetWindowSize(g_win, &ww, &wh);
    if (ww <= 0 || wh <= 0) return false;
    float sx = 1.0f, sy = 1.0f;
    glfwGetWindowContentScale(g_win, &sx, &sy);
    if (sx <= 0.0f) sx = 1.0f;
    if (sy <= 0.0f) sy = 1.0f;
    *w = static_cast<float>(ww) / sx;
    *h = static_cast<float>(wh) / sy;
    return true;
}

// 落在四边/四角 6dp 内返回对应的方向码，否则 -1
int ResizeDirectionAt(float x, float y, float w, float h)
{
    if (WindowIsMaximized()) return -1;   // 最大化时不缩放（跟 Windows 一致）

    const bool left = x < kResizeGripDip;
    const bool right = x >= w - kResizeGripDip;
    const bool top = y < kResizeGripDip;
    const bool bottom = y >= h - kResizeGripDip;

    if (top && left) return kDirTopLeft;
    if (top && right) return kDirTopRight;
    if (bottom && left) return kDirBottomLeft;
    if (bottom && right) return kDirBottomRight;
    if (left) return kDirLeft;
    if (right) return kDirRight;
    if (top) return kDirTop;
    if (bottom) return kDirBottom;
    return -1;
}

// 有的窗口管理器不实现 _NET_WM_MOVERESIZE（i3 这类平铺式就是），装饰栏又已经摘掉了，
// 那边只能用它自己的方式挪窗口（Alt+拖动）。启动时报一句，省得当成拖动坏了。
bool WmSupportsMoveResize()
{
    Display* dpy = glfwGetX11Display();
    if (!dpy) return false;
    const Atom supported = XInternAtom(dpy, "_NET_SUPPORTED", True);
    const Atom want = XInternAtom(dpy, "_NET_WM_MOVERESIZE", False);
    if (supported == None) return false;

    Atom actualType = None;
    int actualFormat = 0;
    unsigned long count = 0, remaining = 0;
    unsigned char* data = nullptr;
    if (XGetWindowProperty(dpy, DefaultRootWindow(dpy), supported, 0, 4096, False, XA_ATOM,
                           &actualType, &actualFormat, &count, &remaining, &data) != Success) {
        return false;
    }

    bool found = false;
    if (data && actualFormat == 32) {
        const Atom* atoms = reinterpret_cast<const Atom*>(data);
        for (unsigned long i = 0; i < count; ++i) {
            if (atoms[i] == want) { found = true; break; }
        }
    }
    if (data) XFree(data);
    return found;
}

// 请窗口管理器接管这次按下：MOVE 走拖动，SIZE_* 走缩放。指针释放是必须的 ——
// 我们自己抓着它，窗口管理器抢不到，请求会被静默忽略。
void SendMoveResize(int direction)
{
    Display* dpy = glfwGetX11Display();
    const Window xwin = glfwGetX11Window(g_win);
    if (!dpy || !xwin) return;

    XUngrabPointer(dpy, CurrentTime);

    Window root = DefaultRootWindow(dpy);
    Window child = 0;
    int rootX = 0, rootY = 0, winX = 0, winY = 0;
    unsigned int mask = 0;
    XQueryPointer(dpy, xwin, &root, &child, &rootX, &rootY, &winX, &winY, &mask);

    XEvent ev{};
    ev.xclient.type = ClientMessage;
    ev.xclient.send_event = True;
    ev.xclient.display = dpy;
    ev.xclient.window = xwin;
    ev.xclient.message_type = XInternAtom(dpy, "_NET_WM_MOVERESIZE", False);
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = rootX;      // 指针的根坐标
    ev.xclient.data.l[1] = rootY;
    ev.xclient.data.l[2] = direction;
    ev.xclient.data.l[3] = 1;          // 左键
    ev.xclient.data.l[4] = 1;          // 来源：应用自己发起的
    XSendEvent(dpy, root, False, SubstructureRedirectMask | SubstructureNotifyMask, &ev);
    XFlush(dpy);
}

bool IsDoubleClick(float x, float y)
{
    const auto now = std::chrono::steady_clock::now();
    const auto age = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_lastPressAt).count();
    const bool again = g_hasLastPress && age <= kDoubleClickMs &&
                       std::abs(x - g_lastPressX) <= 8.0f && std::abs(y - g_lastPressY) <= 8.0f;

    g_lastPressAt = now;
    g_lastPressX = x;
    g_lastPressY = y;
    g_hasLastPress = !again;   // 三下连击按「单击 + 双击」算，不再叠加
    return again;
}

void WindowMinimize()
{
    if (g_win) glfwIconifyWindow(g_win);
}

void WindowToggleMaximize()
{
    if (!g_win) return;
    if (WindowIsMaximized()) glfwRestoreWindow(g_win);
    else glfwMaximizeWindow(g_win);
}

void WindowClose()
{
    if (g_win) glfwSetWindowShouldClose(g_win, GLFW_TRUE);
}

} // namespace

bool InstallFramelessWindow(void* glfwWindow)
{
    GLFWwindow* win = static_cast<GLFWwindow*>(glfwWindow);
    if (!win) return false;
    g_win = win;
    glfwSetWindowAttrib(win, GLFW_DECORATED, GLFW_FALSE);
    if (!WmSupportsMoveResize()) {
        std::fprintf(stderr,
                     "[win] 这个窗口管理器不实现 _NET_WM_MOVERESIZE：标题栏拖不动，"
                     "用窗口管理器自己的方式移动窗口（如 Alt+拖动）\n");
    }
    return glfwGetWindowAttrib(win, GLFW_DECORATED) == GLFW_FALSE;
}

bool FrameHandlePress(float xDip, float yDip, int button, int action)
{
    if (!g_win || button != 0 || action != 1) return false;   // 只认左键按下

    float w = 0.0f, h = 0.0f;
    if (WindowSizeDip(&w, &h)) {
        const int dir = ResizeDirectionAt(xDip, yDip, w, h);
        if (dir >= 0) {
            SendMoveResize(dir);
            return true;
        }
    }

    if (!Contains(g_dragStrip, xDip, yDip)) return false;
    for (const DipRect& r : g_noDrag) {
        if (Contains(r, xDip, yDip)) return false;   // 标签、按钮自己处理
    }

    if (IsDoubleClick(xDip, yDip)) {
        WindowToggleMaximize();
        return true;
    }
    if (g_captionPress) g_captionPress();
    SendMoveResize(kDirMove);
    return true;
}

bool SystemClipsWindowCorners()
{
    return false;   // 无装饰栏的 X11 窗口是方的，没人裁圆角
}

void SetDragRegions(const DipRect& dragStrip, const std::vector<DipRect>& noDrag)
{
    g_dragStrip = dragStrip;
    g_noDrag = noDrag;
}

void SetCaptionPressHandler(std::function<void()> handler)
{
    g_captionPress = std::move(handler);
}

// 窗口动作接进内核的窗口缝：与 Windows 同一套 WindowHost，实现换成 GLFW。
void InstallDesktopWindowHost()
{
    class DesktopWindowHost final : public WindowHost {
    public:
        bool IsMaximized() override { return WindowIsMaximized(); }
        void Minimize() override { WindowMinimize(); }
        void ToggleMaximize() override { WindowToggleMaximize(); }
        void Close() override { WindowClose(); }
        void SetDragRegions(const DipRect& dragStrip, const std::vector<DipRect>& noDrag) override
        {
            ::loppe_browser::SetDragRegions(dragStrip, noDrag);
        }
    };
    static DesktopWindowHost host;
    SetWindowHost(&host);
}

} // namespace loppe_browser

#else

// 其它平台（安卓 / ESP32 没有窗口）：空实现，调用方看到 false 就保留系统标题栏。
namespace loppe_browser {
bool InstallFramelessWindow(void*) { return false; }
void InstallDesktopWindowHost() {}   // 窗口缝保持空操作（没有窗口可管）
void SetCaptionPressHandler(std::function<void()>) {}
bool FrameHandlePress(float, float, int, int) { return false; }
bool SystemClipsWindowCorners() { return false; }
} // namespace loppe_browser

#endif
