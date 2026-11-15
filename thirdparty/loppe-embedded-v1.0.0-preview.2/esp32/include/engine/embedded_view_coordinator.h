/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "engine/damage/damage_tracker.h"
#include "engine/view_coordinator.h"

#include <vector>

class DocumentRuntime;

namespace loppe {

// 带脏区记账的 ViewCoordinator（嵌入式局部刷屏宿主用）。
//
// 用法（自建主循环的宿主：只重刷变了的地方）：
//     EmbeddedViewCoordinator host(&runtime, viewport);
//     ... 每帧：apply(plan) / handleResize(vp) ...
//     const auto upd = host.takeDirtyUpdate(); // full + rects 一次取走并清空（每帧恰好一次）
//     if (upd.full) repaintAll(); else repaint(upd.rects);
//
// 宿主自己的叠层（光标/状态文字…）需自己标脏并并进来。
// 没有局部刷新能力的平台可 setDirtyTracking(false) 关闭记账。
class LOPPE_API EmbeddedViewCoordinator : public ViewCoordinator {
public:
    EmbeddedViewCoordinator(
        DocumentRuntime* context,
        ViewportSizeDp initialViewportSize,
        SceneResolver sceneResolver = SceneResolver());

    // 执行一帧更新，返回本帧影响的 Scene。
    PassImpact runPass(const DocumentUpdatePlan& plan, PassReason reason) override;

    // ===== 脏区入口 =====
    void setDirtyTracking(bool on);
    bool dirtyTracking() const;
    bool needsFullRedraw() const;

    // 本帧脏区（一次取走并清空，推荐入口）：full = 整屏重画，否则只刷 rects 那几块
    using DirtyUpdate = DamageTracker::Update;
    DirtyUpdate takeDirtyUpdate();

    // 兼容入口：取走矩形并清空记录（语义同 takeDirtyUpdate().rects）
    std::vector<loppe::ui::Rect> consumeDirtyRects();

    // 只读的脏区累加器（引擎内部核对用）
    const ui::DirtyRegion& dirtyRegion() const;

private:
    DamageTracker damage_;
};

} // namespace loppe
