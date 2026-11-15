#pragma once
// frame_paint.h —— 窗口外框：客户区最外圈一条圆角描边（+ 内侧一条高光）。
//
// 用法（BrowserLoop：每帧 present 前、所有视图槽画完之后）：
//     FramePainter painter;
//     painter.setStyle(LoadFrameStyle(configPath));                    // 启动时读一次
//     painter.paint(drawer, widthDip, heightDip, focused, maximized);   // 每帧一次
//
// 约定：
//   - 只用 IDrawer 画（CPU / GL 后端同一份代码），不需要 GL 上下文、没有纹理。
//   - 只覆盖客户区最外圈 2dp，不参与布局、不碰任何文档与控件树 —— 标签页内容不受影响。
//   - 阴影与圆角裁切归系统（Windows 的 DWM），这里只负责"看得见的那条边界"。

#include <string>

class IDrawer;

namespace loppe_browser {

enum class FrameStyle {
    None,   // 不画，只靠系统阴影与环境色分界
    Line,   // 一圈圆角描边（默认）
};

struct FrameStyleSpec {
    FrameStyle style = FrameStyle::Line;
    float widthDp = 1.0f;    // 描边宽度（dp）
    float radiusDp = 10.0f;  // 圆角半径（dp）；最大化时按 0 处理
};

// 读 window.cfg（style / width / radius）。文件不存在 = 默认一条线。
FrameStyleSpec LoadFrameStyle(const std::string& filePath);

class FramePainter {
public:
    void setStyle(const FrameStyleSpec& style);
    const FrameStyleSpec& style() const { return style_; }

    // 画这一帧的外框。drawer 必须是当前正在出屏的那一个（onFramePresent 里从
    // Application::renderServices() 取），尺寸用 DIP 口径。
    void paint(IDrawer* drawer, float widthDip, float heightDip, bool focused, bool maximized);

private:
    FrameStyleSpec style_;
};

} // namespace loppe_browser
