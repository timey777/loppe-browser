/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include <memory>
#include <vector>
#include "loppe_export.h"

class DocumentRuntime;

namespace loppe {
    class Scene;
    class ResolvedNode;
    class AbstractControl;
    class ContainerControl;
}

namespace loppe {

// Anchor points for syncing a resolved-tree subtree to UI controls.
struct LOPPE_API SyncScope {
    // Walk the resolved tree starting from this node (required).
    ResolvedNode* resolvedRoot = nullptr;

    // Collect existing controls starting from this control.
    // When nullptr, controls are collected from Scene top-level via ControlManager.
    std::shared_ptr<AbstractControl> controlRoot;

    // Where to attach newly-created orphan controls that have no parent control.
    // When nullptr, orphans are added to the Scene root via ControlManager.
    ContainerControl* orphanContainer = nullptr;
};

// 同步请求：描述「同步什么」，由调用方构造后传入 TreeSynchronizer。
// 形状（subtree/nodes）× 深度（mode）两个维度，见各工厂函数。
struct LOPPE_API SyncRequest {
    enum class Mode {
        Structural,   // 结构同步：创建/更新/删除控件，使控件树与 scope 子树一致（挂载、AST 重建）
        Full,         // 全属性同步：只更新已存在控件的全部属性，不增删控件（样式重同步）
        LayoutOnly,   // 轻量同步：只更新 bounds/scroll/viewport，不碰属性与文本排版（resize）
    };

    Mode mode = Mode::Full;

    // 子树形状：按 scope 定义的子树同步（resolvedRoot 是**子树根**，不是单个节点）。
    SyncScope scope;

    // 离散形状：逐个同步这批节点（节点必须已绑控件，没控件的会被跳过——
    // 它们归下一次结构同步管）。与 scope 二选一。
    std::vector<ResolvedNode*> nodeList;

    // ---- 工厂：形状在前，mode 在后 ----
    static SyncRequest subtreeStructural(SyncScope s);         // 一棵子树，增删改控件（挂载/AST 重建）
    static SyncRequest subtreeFull(SyncScope s);               // 一棵子树，全部属性（样式重同步）
    static SyncRequest subtreeLayoutOnly(SyncScope s);         // 一棵子树，纯布局（resize / 拖拽）
    static SyncRequest nodesFull(std::vector<ResolvedNode*> ns); // 就这几个节点，全部属性
};

// 把 Scene 的控件树与 ResolvedTree 同步的工具；同步范围与深度由 SyncRequest 描述。同步不负责渲染、标脏或结算脏位。
class LOPPE_API TreeSynchronizer {
public:
    TreeSynchronizer() = default;
    ~TreeSynchronizer() = default;

    // 唯一的同步入口。
    //   Structural            → 按 request.scope 增删改控件；
    //   Full / LayoutOnly     → scope 给了就整树遍历，nodeList 给了就逐节点。
    // 返回前所有同步到的节点上的 Style/Paint 脏已被清除（LayoutOnly 路径不碰脏位）。
    void sync(::DocumentRuntime* ctx, Scene* scene, const SyncRequest& request);

    // 确保 node 及其祖先链都有控件并逐级挂载（要求场景根已在 ControlManager 登记，否则返回 nullptr）。已就绪时为空操作。
    // 返回 node 的控件；链顶未就绪或任一步失败返回 nullptr，调用方必须整趟放弃。
    std::shared_ptr<AbstractControl> ensureControlWithAncestors(
        ResolvedNode* node, Scene* scene) const;

private:
    // ---- 内部原语 ----

    // 全属性同步单个控件（bind + 属性 + Image/Input 特殊处理），清 Style|Paint 脏。
    void updateControl(const std::shared_ptr<AbstractControl>& ctrl, ResolvedNode* rn);

    // 轻量同步单个控件：bounds/scroll/viewport only，不碰脏位。
    void updateControlLayoutOnly(const std::shared_ptr<AbstractControl>& ctrl, ResolvedNode* rn);

    // Structural 的实现体（增删改控件）。
    void structuralSync(::DocumentRuntime* ctx, Scene* scene, const SyncScope& scope);

    void updateExistingControls(::DocumentRuntime* ctx, Scene* scene, const SyncScope& scope, bool layoutOnly);
};

} // namespace loppe
