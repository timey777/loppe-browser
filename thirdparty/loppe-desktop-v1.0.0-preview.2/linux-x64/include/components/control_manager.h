/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "loppe_export.h"
#include "render/display_list.h"
#include "abstract_control.h"
#include "overlay_event_root.h"
#include <vector>
#include <memory>
#include <algorithm>

namespace loppe {

class Scene;
class SceneRender;
class ResolvedTree;

/**
 * @brief 控件管理器 - 管理场景中的所有控件及其生命周期和渲染顺序
 */
class LOPPE_API ControlManager {
public:
    ControlManager(Scene* scene);
    ~ControlManager();

    /**
     * @brief 渲染场景中的所有内容到 DisplayList（包含主控件树与覆盖层）
     */
    void render(DisplayList& displayList);

    /**
     * @brief 只渲染主控件树（不含覆盖层），用于窗口级「先画所有视图的正文」这一遍
     */
    void renderContent(DisplayList& displayList);

    /**
     * @brief 只渲染覆盖层（菜单/下拉/浮窗），用于窗口级最后一遍
     */
    void renderOverlay(DisplayList& displayList);

    /**
     * @brief 添加一个根控件
     */
    void addRootControl(std::shared_ptr<AbstractControl> control);

    // 控件是否已被登记为场景根控件（只查询，不修改状态）
    bool isRootRegistered(const std::shared_ptr<AbstractControl>& control) const;

    /**
     * @brief 移除一个根控件
     */
    void removeRootControl(std::shared_ptr<AbstractControl> control);

    /**
     * @brief 添加覆盖层控件
     */
    void addOverlayControl(std::shared_ptr<AbstractControl> control);

    /**
     * @brief 移除覆盖层控件
     */
    void removeOverlayControl(std::shared_ptr<AbstractControl> control);

    /**
     * @brief 清空所有控件
     */
    void clear();

    /**
     * @brief 查找控件（宿主视角，语义不变）
     *
     * 全局按 id 深度优先查找；隔离沙盒（预览槽挂进来的副本）整棵子树跳过，
     * 各文档有同名 id 时只返回宿主那一份。
     */
    std::shared_ptr<AbstractControl> findControlById(const std::string& id) const;

    // 按 (ownerTree, id) 查找控件；不跳过隔离沙盒控件。ownerTree/id 为空或无匹配返回 nullptr。
    std::shared_ptr<AbstractControl> findControlById(ResolvedTree* ownerTree,
                                                     const std::string& id) const;

    /**
     * @brief 获取所有普通控件
     */
    const std::vector<std::shared_ptr<AbstractControl>>& getControls() const;

    /**
     * @brief 获取覆盖层根节点
     */
    std::shared_ptr<OverlayEventRoot> getOverlayRoot() const;

    // 动画更新队列与动画重绘请求。
    // 加入动画 tick 队列：每帧 update()，与是否需要重绘无关。
    void addAnimatedControl(AbstractControl* ctrl);
    // 从动画 tick 队列移除。
    void removeAnimatedControl(AbstractControl* ctrl);
    // 本帧动画产生了需要重绘的变化时请求整屏重绘。
    void requestAnimationPaint();
    // 局部重绘请求（屏幕坐标）；矩形数超过上限时自动退化为整屏请求。
    void requestAnimationPaintRect(const ui::Rect& rect);
    // 取走本帧的局部重绘矩形；返回 true = 本帧必须整屏（调用方走 requestFullPaint()）。
    bool takeAnimationPaintRects(std::vector<ui::Rect>& out);
    // 执行本帧 tick（队列取走即清）；返回 true = 本帧需要重绘（调用方据此出帧）。
    bool processAnimatedControls();

    // 辅助方法：设置 Scene 上下文（通常只在初始化时调用）
    void setScene(Scene* scene);

private:
    Scene* scene_;
    std::vector<std::shared_ptr<AbstractControl>> controls_;
    std::shared_ptr<OverlayEventRoot> overlayRoot_;
    std::vector<AbstractControl*> animatedControls_;
    bool animationPaintPending_ = false;   // 本帧动画是否产生了需要重绘的变化
    static constexpr size_t kMaxAnimationPaintRects = 16;
    bool animationPaintMustFull_ = false;  // 本帧有控件要求**必须整屏**（requestAnimationPaint() 置真）
    std::vector<ui::Rect> animationPaintRects_;   // 本帧局部重绘矩形（屏幕坐标）

    void clearSceneForControl(std::shared_ptr<AbstractControl> control);
    void registerControlRecursive(std::shared_ptr<AbstractControl> control);
};

} // namespace loppe