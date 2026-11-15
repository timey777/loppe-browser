/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "render/display_list.h"
#include "render/renderer.h"
#include "core/coordinate_types.h"
#include "core/parse.h"
#include <memory>
#include <string>
#include <typeindex>
#include "loppe_export.h"

namespace loppe {

// 纯合成器/呈现器：仅管理 DisplayList 的提交、叠加与绘制。
class LOPPE_API Compositor {
public:
    Compositor();
    virtual ~Compositor();

    void setRenderer(Renderer* renderer);
    Renderer* getRenderer() const;

    void submitDisplayList(const DisplayList& displayList);
    void submitDisplayList(DisplayList&& displayList);
    void submitOverlayDisplayList(const DisplayList& overlay);
    void submitOverlayDisplayList(DisplayList&& overlay);

    void paintAll();
    void paintAllTo(const ui::Rect& target, bool presentAfter = false);

    // 只画正文（主控件树），不画弹层。窗口有多块视图时，正文与弹层要分两遍画：
    // 先所有视图的正文，再所有视图的弹层。
    void paintContent();
    void paintContentTo(const ui::Rect& target);

    // 只画弹层（菜单/下拉/浮窗）。正文与弹层要分两遍画：先所有视图的正文，再所有视图的弹层。
    // 目标矩形只用来平移原点（与本槽正文同一坐标系），不裁剪 —— 弹层允许盖到相邻槽上。
    void paintOverlay();
    void paintOverlayTo(const ui::Rect& target);
    // 同上，但把弹层裁在 target 矩形内：弹层不得越出自己的视图（页签里的下拉不能盖到别的页签）。
    void paintOverlayClippedTo(const ui::Rect& target);

    void clear();
    void markDirty(float x, float y, float w, float h);

    // 从 Scene 渲染一帧并提交为当前 DisplayList（清除旧内容）。
    void renderScene(class Scene* scene);

    bool saveDisplayListsToFile(const std::string& path) const;
    bool loadDisplayListsFromFile(const std::string& path);

private:
    Renderer* renderer_ = nullptr;
    std::vector<DisplayList> displayLists_;
    DisplayList overlayDisplayList_;
    bool isDirty_ = false;
};

} // namespace loppe