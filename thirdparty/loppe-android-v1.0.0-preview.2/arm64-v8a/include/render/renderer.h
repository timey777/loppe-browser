/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "graphics_types.h"
#include "display_list.h"
#include <memory>
#include <map>
#include "loppe_export.h"

class IDrawer;

namespace loppe {

class ITextRenderer;

// 高层渲染器：把 DisplayList 转换为底层绘制调用。
class LOPPE_API Renderer {
public:
    // 便捷构造：drawer 须同时实现 ITextRenderer。
    explicit Renderer(IDrawer* drawer);
    // 分别指定几何绘制器与文字引擎。
    Renderer(IDrawer* drawer, ITextRenderer* textRenderer);
    ~Renderer();

    // 设置底层绘制器
    void setDrawer(IDrawer* drawer);
    IDrawer* getDrawer() const;

    // 设置/获取文字引擎
    void setTextRenderer(ITextRenderer* textRenderer);
    ITextRenderer* getTextRenderer() const;

    // 渲染 DisplayList
    void render(const DisplayList& displayList);
    void render2(const DisplayList& displayList);
    // 渲染到指定目标矩形（目标以当前画布坐标系为准，支持偏移与裁剪）
    void render(const DisplayList& displayList, const ui::Rect& target);
    // 只把原点平移到 target，不额外裁剪（弹层用：允许画到目标矩形之外）
    void renderTranslated(const DisplayList& displayList, const ui::Rect& target);
private:
    struct ClipState {
        render::Rect rect{};
        float cornerRadius = 0.0f;
        bool hasClip = false;
    };

    // 转换颜色格式
    render::Color convertColor(const ui::Color& color);

    // 渲染单个命令
    void renderCommand(const DrawCommand& cmd);

    // 渲染矩形
    void renderRect(const DrawCommand& cmd);
    void renderGaussianShadow(const DrawCommand& cmd);

    // 渲染文本
    void renderText(const DrawCommand& cmd);
    void renderTextLayout(const DrawCommand& cmd);

    // 渲染图片
    void renderImage(const DrawCommand& cmd);

    // 渲染 SVG 图标
    void renderSvgIcon(const DrawCommand& cmd);

    void renderPushClip(const DrawCommand& cmd);
    void renderPopClip();

    void renderPushTransform(const DrawCommand& cmd);
    void renderPopTransform();

    // 获取或生成 SVG 图标纹理
    ImageHandle getOrCreateSvgTexture(const std::string& name, int width, int height);

    IDrawer* drawer_;
    ITextRenderer* textRenderer_ = nullptr;
    std::map<std::string, ImageHandle> svgCache_;
    std::vector<ClipState> clipStateStack_;
};

} // namespace loppe