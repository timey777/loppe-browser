// ===== 设计说明 =====
//
// 【为什么只画一条线】Chrome 的窗框从来不是自己画的：Windows 上靠 DWM —— 窗口保留
// WS_THICKFRAME + WS_CAPTION（window_frame.cpp 只把非客户区设成 0），系统就给阴影、圆角和
// 边缘缩放；Chrome 自己只画标题栏顶部那条 1px（COLOR_FRAME_ACTIVE）。这里照抄这个分工：
// 能借系统的全借（见 window_frame.cpp 的 ApplyDwmFrame），只补系统给不了的那一条边界线。
// 早先试过"磨砂 / 新拟物"（8 层环 + 噪点纹理），在 CPU 光栅后端上调不出真磨砂，删了。
//
// 【为什么用 IDrawer 而不是 GL】桌面壳跑的是 CPU 光栅后端（main.cpp 里
// SetRenderBackend(RenderBackend::Cpu)），没有 GL 上下文。IDrawer 两个后端都有，画法统一，
// 也不用管 GL 状态保存/恢复。
//
// 【画在哪一层】RenderLoop::onFramePresent：所有视图槽 paintAll 之后、present 之前。
// 画在内容之上、不进控件树、不参与布局，所以壳与页签文档都不知道它的存在。
// 引擎每次 present 前都清屏 + 全窗重组，半透明不会一帧帧累积。
//
// 【为什么是两层】描边画在外沿、再贴一条内高光，是"有厚度的窗框"最省的做法；
// 只画一条实线会像贴纸。两条就够，再多就开始糊。
//
// 【为什么圆心要跟着内缩】drawRoundedRect 的描边完全落在盒子内侧，所以每层的盒子都要
// 内缩、圆角半径同步减 —— 否则角上会比直边厚一倍。
//
// 【非活动窗口与最大化】非活动换成浅色（跟系统窗口一致）；最大化贴屏幕边，圆角与描边
// 都不画 —— 那时系统自己会画。

#include "platform/frame_paint.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <string>

#include "render/drawer.h"

namespace loppe_browser {
namespace {

constexpr float kInnerWidth = 1.0f;   // 内侧高光的宽度（dp），紧贴外沿描边的里侧

render::Color Rgb(float r, float g, float b)
{
    return render::Color{ r, g, b, 1.0f };
}

// 一圈圆角描边：盒子内缩 inset，线宽 width（描边完全落在盒子内侧）。
void Ring(IDrawer* d, float w, float h, float radius, float inset, float width,
          const render::Color& color, float alpha)
{
    if (!d || width <= 0.0f || alpha <= 0.0f) return;
    const float cw = w - inset * 2.0f;
    const float ch = h - inset * 2.0f;
    if (cw <= 0.5f || ch <= 0.5f) return;

    DrawStyle style;
    style.strokeColor = color;
    style.fillColor.a = 0.0f;   // 只描边：DrawStyle 默认 fill 是黑色不透明，不清掉会填黑整块
    style.opacity = std::min(1.0f, alpha);
    style.cornerRadius = std::max(0.0f, radius - inset);
    style.strokeWidths.top = width;
    style.strokeWidths.right = width;
    style.strokeWidths.bottom = width;
    style.strokeWidths.left = width;
    d->drawRoundedRect(inset, inset, cw, ch, style.cornerRadius, style.cornerRadius, style);
}

std::string Trim(const std::string& text)
{
    const size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

std::string ToLower(std::string text)
{
    for (char& c : text) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return text;
}

} // namespace

FrameStyleSpec LoadFrameStyle(const std::string& filePath)
{
    FrameStyleSpec spec;
    if (filePath.empty()) return spec;

    std::ifstream in(filePath);
    if (!in) return spec;

    std::string line;
    while (std::getline(in, line)) {
        const size_t comment = line.find('#');
        if (comment != std::string::npos) line.erase(comment);
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        const std::string key = ToLower(Trim(line.substr(0, eq)));
        const std::string value = Trim(line.substr(eq + 1));
        if (value.empty()) continue;

        if (key == "style") {
            const std::string v = ToLower(value);
            spec.style = (v == "none") ? FrameStyle::None : FrameStyle::Line;
        } else if (key == "width") {
            spec.widthDp = std::max(0.0f, static_cast<float>(std::atof(value.c_str())));
        } else if (key == "radius") {
            spec.radiusDp = std::max(0.0f, static_cast<float>(std::atof(value.c_str())));
        }
    }
    return spec;
}

void FramePainter::setStyle(const FrameStyleSpec& style)
{
    style_ = style;
}

void FramePainter::paint(IDrawer* drawer, float widthDip, float heightDip, bool focused, bool maximized)
{
    if (!drawer) return;
    if (style_.style == FrameStyle::None) return;
    if (widthDip <= 1.0f || heightDip <= 1.0f) return;
    if (maximized) return;   // 最大化贴屏幕边：系统不画边框，我们也不画

    float width = std::min(style_.widthDp, std::min(widthDip, heightDip) * 0.25f);
    if (width <= 0.0f) return;
    const float radius = std::min(style_.radiusDp, std::min(widthDip, heightDip) * 0.25f);
    const float dim = focused ? 1.0f : 0.45f;

    drawer->resetClip();
    drawer->setClipCornerRadius(0.0f);

    // ① 外沿描边：边界本身。活动 / 非活动只换颜色深浅，跟系统窗口一样
    const render::Color line = focused ? Rgb(0.722f, 0.761f, 0.831f)    // #b8c2d4
                                       : Rgb(0.847f, 0.867f, 0.902f);   // #d8dde6
    Ring(drawer, widthDip, heightDip, radius, 0.0f, width, line, 1.0f);

    // ② 内侧 1px 高光：给出"窗框有厚度"的转折，没有它就只是一条贴纸
    Ring(drawer, widthDip, heightDip, radius, width, kInnerWidth,
         Rgb(1.0f, 1.0f, 1.0f), 0.55f * dim);

    drawer->resetClip();   // 别把裁剪留给后面的绘制者
}

} // namespace loppe_browser
