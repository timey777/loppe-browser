/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "compositor.h"
#include "core/tree_synchronizer.h"
#include "document_update_plan.h"
#include "components/uitypes.h"
#include "core/coordinate_types.h"
#include "core/node_flags.h"
#include "update_scheduler.h"
#include "update_step_guard.h"
#include "misc/perf_timer.h"

#include <functional>
#include <unordered_set>
#include <utility>
#include <vector>

class DocumentRuntime;

namespace loppe {

class ResolvedNode;
class ResolvedTree;

// ===================================================================
// ViewCoordinator — 单视图更新协调器（基类，不再 final）
//
// 单视图更新协调器：把一份 DocumentUpdatePlan 落地成控件状态，返回本趟影响的 Scene。
// 不做脏区记账；需要局部刷新的宿主请用子类 EmbeddedViewCoordinator（engine/embedded_view_coordinator.h）。
// runPass 为 virtual：子类可整段覆盖以定制一帧的执行方式。
// ===================================================================
class LOPPE_API ViewCoordinator {
public:
    using SceneResolver = std::function<Scene*()>;

    // 宿主/预览样式联动模式（视图协调层策略；默认完全隔离）
    enum class StyleLinkMode {
        Isolated,        // 宿主 ↔ 预览 互不影响
        HostToPreview,   // 宿主 影响 预览
        PreviewToHost,   // 预览 影响 宿主
        Bidirectional    // 双向
    };

    void setStyleLinkMode(StyleLinkMode m);
    StyleLinkMode getStyleLinkMode() const;

    ViewCoordinator(
        DocumentRuntime* context,
        ViewportSizeDp initialViewportSize,
        SceneResolver sceneResolver = SceneResolver());

    virtual ~ViewCoordinator();

    void bind(Scene* scene);

    // 全部活着的协调器（含不在槽表里的嵌套文档）；协调器自行注册/注销，宿主无需手工登记。
    static std::vector<ViewCoordinator*>& liveCoordinators();

    // 本文档的调度器；宿主可用 setScheduler 改指。
    UpdateScheduler* scheduler() const;
    void setScheduler(UpdateScheduler* s);

    // ===== 核心入口 (virtual) =====

    // 一次 pass 的入口：把一份 DocumentUpdatePlan 落地成控件状态，返回它影响到的 Scene。
    using PassReason = FrameReason;
    virtual PassImpact runPass(const DocumentUpdatePlan& plan, PassReason reason);
    virtual PassImpact apply(const DocumentUpdatePlan& plan);

    virtual void handleResize(ViewportSizeDp viewportSize);

    void setViewport(ViewportSizeDp vp);
    void setScene(Scene* s);
    void setSceneResolver(SceneResolver sr);

protected:
    // ===== 子类可覆盖的更新步骤 =====

    virtual void applyASTRebuild();
    virtual void applyStyleUpdate();
    virtual void applyLayoutOnly();
    virtual void applyPartialStyleUpdate(const std::vector<ResolvedNode*>& nodes);

    // AST 重建趟的当趟补偿：把 onControlsReady() 里排出的更新（setText / setEnabled …）
    // 同一趟内取走执行，让首帧画面就是回调后的完整状态。applyASTRebuild 在回调返回后
    // 自动调用，子类无需手动调。
    void applyControlsReadyFollowups();

    // 本 pass 的统一收尾（自检）。
    void finishPass(const char* entry);

    // ===== 子类可复用的一帧组成步骤 =====

    // 本帧已同步过的节点集合。
    using SyncedNodes = std::unordered_set<ResolvedNode*>;

    // 「重排类先试局部」那一趟的结果
    enum class RelayoutOutcome {
        Skipped,   // 没试：非重排类，或 mustFullPaint 已表态别降级
        Applied,   // 试排成功：只同步了被摸到的节点，本帧已在局部落地
        Failed     // 试过了、局部不成立：调用方必须走全量
    };

    // 对重排类计划先尝试局部重排；Applied 时把 entry 改写为 "local-relayout"。
    RelayoutOutcome tryLocalRelayout(const DocumentUpdatePlan& plan, SyncedNodes& synced,
                                     const char*& entry);
    // 按 plan.kind 执行对应的全量更新。
    void applyByKind(const DocumentUpdatePlan& plan, SyncedNodes& synced);
    // 结算：本帧带进来的 Style 脏谁还没被同步，不看 kind 一律补上。
    // 返回本步补同步的节点（调用方要记账时用得上；样式同步不改几何）。
    std::vector<ResolvedNode*> settlePendingStyleDirty(const DocumentUpdatePlan& plan,
                                                       SyncedNodes& synced);

    // ===== 内部工具 =====

    Scene* resolveScene() const;
    bool hasViewport() const;
    bool canRunPipeline(Scene* scene) const;

    // ===== 成员 (protected 以便子类访问) =====

    TreeSynchronizer   m_treeSynchronizer;
    DocumentRuntime*   context_      = nullptr;
    ViewportSizeDp     viewportSize_{};
    StyleLinkMode      styleLinkMode_ = StyleLinkMode::Isolated;
    // 一步更新的围栏：整体生效或整体放弃
    UpdateStepGuard    stepGuard_;

private:
    // 同步一批节点。
    void syncNodes(const std::vector<ResolvedNode*>& nodes, SyncedNodes& synced);

    // 把当前视口同步给场景的内置软键盘（贴底、按逻辑分辨率缩放）。
    void syncSoftKeyboardViewport(Scene* scene) const;

    Scene*             boundScene_   = nullptr;
    SceneResolver      sceneResolver_;
    // applyControlsReadyFollowups 执行中：回调引发的嵌套 AST 趟不再递归补偿，由最外层循环兜住
    bool               inControlsReadyFollowups_ = false;
};

} // namespace loppe
