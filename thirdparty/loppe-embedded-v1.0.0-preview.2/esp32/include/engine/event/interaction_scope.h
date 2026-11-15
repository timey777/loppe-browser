/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "engine/event/ui_event_system.h"
#include "loppe_export.h"

#include <memory>
#include <string>
#include <vector>

namespace loppe {

class AbstractControl;
class ResolvedNode;
class ResolvedTree;

/**
 * @brief 预览槽的交互作用域模式。
 *
 * - `Isolated`（默认）：事件止于槽内，不向宿主冒泡；宿主的 hover/active/focus 不记到槽内。
 * - `Shared`：事件继续冒泡到宿主；宿主的 hover/active/focus 照常记到槽内副本。
 */
enum class InteractionMode {
    Isolated,
    Shared
};

/**
 * @brief 一份文档（预览 DocumentRuntime）的交互作用域（沙盒边界）。
 *
 * 作用域按 `ownerTree`（该文档的 ResolvedTree）限定，边界按 `slotNode`（宿主视图树里的槽容器
 * 节点指针）判定；槽 id 只用于日志，允许各文档重复使用同一个 id（iframe 语义）。
 */
class LOPPE_API InteractionScope {
public:
    explicit InteractionScope(std::string slotId,
                              InteractionMode mode = InteractionMode::Isolated,
                              ResolvedTree* ownerTree = nullptr);

    // 槽的显示名，仅用于日志/诊断，不参与归属判定
    const std::string& slotId() const;

    InteractionMode mode() const;
    void setMode(InteractionMode mode);
    bool isolated() const;

    // 本作用域限定的源树（= 槽内节点 ownerTree() 的值）；未指定时为空
    ResolvedTree* ownerTree() const;
    void setOwnerTree(ResolvedTree* ownerTree);

    // 槽容器节点（属于外层文档）。
    ResolvedNode* slotNode() const;
    void setSlotNode(ResolvedNode* node);

    // 槽容器对应的控件；锚尚未建立时为空
    std::shared_ptr<AbstractControl> slotControl() const;

    // 作用域自己的事件注册表（该文档的处理器绑在这里）
    UIEventSystem& events();
    const UIEventSystem& events() const;

private:
    std::string slotId_;
    InteractionMode mode_;
    ResolvedTree* ownerTree_ = nullptr;
    ResolvedNode* slotNode_ = nullptr;
    UIEventSystem events_;
};

/**
 * @brief 视图层的交互作用域注册表（每棵 ResolvedTreeView 一份）。
 *
 * 归属判定按「槽容器节点指针」和「节点的 ownerTree()」进行（节点 id 允许跨文档重复）。
 * chainFor() 返回由内到外的作用域链。
 */
class LOPPE_API InteractionScopeRegistry {
public:
    void add(InteractionScope* scope);
    void remove(InteractionScope* scope);

    bool empty() const;
    size_t size() const;

    // 按槽显示名查（兼容旧调用）；归属判定不要用这个
    InteractionScope* find(const std::string& slotId) const;
    // 按节点的 ownerTree() 查作用域
    InteractionScope* forOwnerTree(ResolvedTree* ownerTree) const;
    // 按槽容器节点指针查作用域
    InteractionScope* forSlotNode(ResolvedNode* node) const;
    // 从 node 自身起沿祖先链收集作用域，由内到外（含 node 自身是槽容器的情况）
    std::vector<InteractionScope*> chainFor(ResolvedNode* node) const;

    // 最近的（最深的）祖先槽作用域 —— node 自身不算（槽容器归外层）
    InteractionScope* enclosingOf(ResolvedNode* node) const;

    // 是否落在某个隔离作用域内部
    bool isInsideIsolated(ResolvedNode* node) const;

    // 清空各槽作用域记录的 Active 状态。
    void clearActiveStates() const;

    // 一次手势结束 / 被打断：清空各层事件域的按下记录（宿主那份由 Scene 单独清）。
    void clearPressedControls() const;
    // 节点移除时从各层按下记录中移除该控件。
    void erasePressedControl(AbstractControl* ctrl) const;

    // 清空各槽作用域记录的 Active 与 Hover 状态。
    void clearPointerStates() const;

    // 清空各槽作用域记录的 Active 与 Focus 状态（`keep` 那个不动；传 nullptr = 全部清）。
    void clearActiveAndFocusStates(const InteractionScope* keep) const;

    // 当前持有焦点的槽作用域（没有 → nullptr）。
    InteractionScope* focusHolder() const;

private:
    std::vector<InteractionScope*> scopes_;
};

} // namespace loppe
