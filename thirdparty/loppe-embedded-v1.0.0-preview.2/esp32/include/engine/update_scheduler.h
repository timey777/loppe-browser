/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

// update_scheduler.h
#pragma once

#include <unordered_set>
#include <vector>

#include "document_update_plan.h"
#include "components/uitypes.h"   // loppe::ui::Rect

namespace loppe {
    class ResolvedNode;
}
#include "loppe_export.h"

// 一份文档挂起的更新集合。每个 DocumentRuntime 自带一个实例（值成员）。
// 反查点（ResolvedNode / Scene / PipelineGuard / ViewCoordinator）拿不到所属文档时
// 返回 nullptr 或各自的兜底实例。
class LOPPE_API UpdateScheduler {
public:
    UpdateScheduler();
    ~UpdateScheduler();

    UpdateScheduler(const UpdateScheduler&) = delete;
    UpdateScheduler& operator=(const UpdateScheduler&) = delete;

    // 诊断入口：反查点发现对象没有所属文档时调用；同一进程内只输出一次。
    static void warnMissingOwner(const char* who);

    // 线程约定：以下接口都只允许 UI 线程调用；后台线程要影响界面 → 走 UiDispatcher。

    // 1. 节点请求更新 (通常由 ResolvedNode 内部调用)
    void requestUpdate(loppe::ResolvedNode* node);

    // 移除节点时，从所有活跃实例里清掉相关引用。
    static void notifyNodeRemoved(loppe::ResolvedNode* node);

    // 2. 请求纯重绘。整屏是兜底语义：同一帧已有局部矩形时按矩形刷。
    void requestPaint();

    // 2a. 请求必须整屏的纯重绘，不接受矩形降级。
    void requestFullPaint();

    // 2b. 请求局部重绘（文档局部坐标）。同一帧累加；超过上限退化成整屏。
    void requestPaintRect(const loppe::ui::Rect& rect);

    // 3. 请求重排+重绘（样式更新）
    void requestRepaint();

    // 4. 请求布局更新：仅重排 + 同步 bounds，用于拖拽等纯布局变化
    void requestLayoutUpdate();

    // 5. 请求 AST 重建+重绘
    void requestASTRepaint();

    // 生成本帧挂起的更新计划。**纯产出，不改状态**：可重复调用、可用于诊断，
    // 返回值只描述本实例所属的那份文档。
    DocumentUpdatePlan makePendingPlan();

    // 确认一份计划已落地并清掉它覆盖的挂起状态；执行期间新排的请求归下一帧。
    // ASTRepaint 是最高优先级（全量重建），覆盖这一帧所有低优先级请求 → 全部清空。
    // 其余档位按 plan 的快照精确清，保留 plan 生成之后新排进来的请求。
    void acknowledge(const DocumentUpdatePlan& plan);

    // 丢弃全部挂起请求（工具 / 测试起点用），不产生也不执行任何计划。
    void discardPending();

private:
    enum class FrameAction {
        None = 0,
        PaintOnly,
        PartialUpdate,
        LayoutOnly,
        Repaint,
        FullLayout,
        ASTRepaint
    };

    FrameAction analyzeFrameAction();
    const std::vector<loppe::ResolvedNode*>& getDirtyNodes() const;
    void clear();
    void eraseNode(loppe::ResolvedNode* node);

    static std::unordered_set<UpdateScheduler*>& liveInstances();

    std::unordered_set<loppe::ResolvedNode*> m_dirtyNodesSet;
    mutable std::vector<loppe::ResolvedNode*> m_dirtyNodesVec;
    bool m_hasPaintRequest = false;
    bool m_paintFull = false;
    bool m_paintMustFull = false;
    std::vector<loppe::ui::Rect> m_paintRects;
    bool m_hasRepaintRequest = false;
    bool m_hasLayoutRequest = false;
    bool m_hasASTRepaintRequest = false;
};
