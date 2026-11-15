/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "abstract_control.h"
#include <vector>
#include <memory>
#include <algorithm>
#include "misc/float_utils.h"
#include "scrollbar_control.h"

namespace loppe {

class Scene;

/**
 * @brief 容器控件抽象类
 * 
 * 可以包含子控件的容器类控件的基类
 */
class LOPPE_API ContainerControl : public AbstractControl {
    friend class ControlFactory;
    friend class ControlManager;
    friend class Compositor;
public:
    struct InternalAccess {
        static void removeChild(ContainerControl* container, std::shared_ptr<AbstractControl> child);
    };
public:
    // 浮点比较容差
    static constexpr float kScrollEpsilon = 0.01f;

    ContainerControl() = default;
    virtual ~ContainerControl() = default;

    bool isContainer() const override;

    /**
     * @brief 设置场景，并传播到子控件
     */
    void setScene(Scene* scene) override;

    /**
     * @brief 添加子控件
     */
    virtual void addChild(std::shared_ptr<AbstractControl> child);

protected:
    /**
     * @brief 移除子控件
     */
    virtual void removeChild(std::shared_ptr<AbstractControl> child);

    /**
     * @brief 清空所有子控件
     */
    virtual void clearChild();

public:
    /**
     * @brief 获取所有子控件
     */
    const std::vector<std::shared_ptr<AbstractControl>>& getChild() const;
    const std::shared_ptr<AbstractControl>& at(int idx) const;

    bool onKey(const KeyEvent& e) override;
    bool onChar(const CharEvent& e) override;

    // Handle mouse events for overlay elements (scrollbars) that sit above children.
    // Returns true if the overlay handled the event.
    virtual bool onOverlayMouseButton(const ParentLocalMouseButtonEvent& e);

    // 悬停回调：指针离开容器时清掉 overlay 滚动条的 hover 态。
    void onHover(bool isHovered) override;

    std::shared_ptr<ScrollbarControl> getVScrollbar() const;
    std::shared_ptr<ScrollbarControl> getHScrollbar() const;

    

    /**
     * @brief 通过ID查找子控件
     */
    std::shared_ptr<AbstractControl> findChildById(const std::string& id) const;

protected:
    bool handleLocalMouseMove(const ControlLocalMouseMoveEvent& e) override;
    bool handleLocalMouseButton(const ControlLocalMouseButtonEvent& e) override;
    bool handleLocalScroll(const ControlLocalScrollEvent& e) override;

    /**
     * @brief 现在能不能「按住拖动滚动内容」：有 viewport、内容任一轴溢出，且当前输入方式是触摸。
     */
    bool canDragScrollContent() const;

    /**
     * @brief 渲染所有子控件。
     *
     * @param parentClip 祖先链路累积下传的渲染裁剪区域（本容器局部坐标系）。
     *                   空矩形表示「祖先没有施加额外裁剪」，此时由本容器根据自身
     *                   是否为滚动容器（hasViewport()）决定裁剪策略。
     */
    void renderChildren(DisplayList& displayList, const ui::Rect& parentClip = ui::Rect()) const;

    // ── renderChildren sub-steps ──

    /**
     * @brief 解析本容器的有效裁剪区域。
     *
     * 优先级：parentClip（渲染递归传入的 clip 参数） >
     *         viewport（本容器作为滚动容器时的可见窗口，作为兜底裁剪）。
     */
    ui::Rect effectiveClipRect(const ui::Rect& parentClip) const;
    bool hasScrollableContent() const;

    /** @brief 内容区域的裁剪矩形（= bounds_ 扣除 stroke），与 viewport 无关。 */
    ui::Rect contentClipRect() const;
    /** @brief 滚动容器的裁剪矩形：在 contentClipRect 基础上，「不裁剪的轴」放开（不限长）。 */
    ui::Rect scrollClipRect() const;
    void pushContentClip(DisplayList& dl, const ui::Rect& clip) const;
    bool isChildVisible(const ui::Rect& clip, AbstractControl* child) const;
    void renderChildWithClip(DisplayList& dl, const ui::Rect& clip, AbstractControl* child) const;
    void renderScrollbarsIfNeeded(DisplayList& dl) const;
    void renderScrollChildrenDirect(DisplayList& dl, const ui::Rect& clip) const;
    void renderNonScrollChildrenDirect(DisplayList& dl, const ui::Rect& clip) const;
    void renderScrollChildrenCulled(DisplayList& dl, const ui::Rect& clip) const;
    void renderNonScrollChildrenCulled(DisplayList& dl, const ui::Rect& clip) const;

    /**
     * @brief 获取子树包围盒（所有子控件 bounds 的并集），用于子树级裁剪
     */
    ui::Rect getBoundingRect() const;
    void invalidateBoundingRect();

    std::vector<std::shared_ptr<AbstractControl>> children_;
    mutable std::shared_ptr<ScrollbarControl> vScrollbar_;
    mutable std::shared_ptr<ScrollbarControl> hScrollbar_;

    mutable ui::Rect cachedContentClip_;
    mutable ui::Rect cachedLayerClip_{};
    mutable ui::Rect cachedBoundingRect_;
    mutable bool boundingRectDirty_ = true;
    mutable float cachedScrollX_ = -1.0f;
    mutable float cachedScrollY_ = -1.0f;
    mutable bool renderDragging_ = false;

    static constexpr float kDragScrollThresholdDp = 4.0f;
    bool  dragPending_ = false;
    bool  dragPanning_ = false;
    float dragStartX_ = 0.0f;
    float dragStartY_ = 0.0f;
    float dragStartScrollX_ = 0.0f;
    float dragStartScrollY_ = 0.0f;
};

} // namespace loppe