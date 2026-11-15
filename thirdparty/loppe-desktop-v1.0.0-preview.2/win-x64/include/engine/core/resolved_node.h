/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "render/display_list.h"
#include <memory>
#include <vector>
#include <unordered_set>
#include <optional>
#include <array>
#include <string>
#include "ast_node.h"
#include "node_flags.h"
#include "computed_style.h"
#include "computed_layout.h"
#include "property_provider.h"
#include <engine/capability/capability_host.h>
#include "loppe_export.h"

class DocumentRuntime;
class UpdateScheduler;
namespace loppe
{
	class AbstractControl;
	class ResolvedTree;

	// ================= ResolvedNode =================
	class LOPPE_API ResolvedNode 
	{
	public:
		~ResolvedNode();
		
		ResolvedNode(ResolvedTree* tree);

		// 本节点所属文档的调度器。树未 attach 到 DocumentRuntime
		// 时返回 nullptr —— 调用方需判空，此时标脏不会被任何一帧消费。
		UpdateScheduler* scheduler() const;

		friend class ResolvedTree;
		friend class ResolvedTreeView;

	private:
		ResolvedNode();
		// ===== 脏标记 =====
		DirtyFlag dirtyFlags = DirtyFlag::All;
		std::weak_ptr<loppe::AbstractControl> control;
		// ===== 树结构（私有）=====
		ResolvedNode* parent;
		ResolvedNode* viewParent_ = nullptr;
		std::vector<ResolvedNode*> children;
		std::vector<ResolvedNode*> overlay_children;
		std::vector<ResolvedNode*> flow_children;


		CapabilityHost caps_;
	private:
		loppe::ComputedStyle finalStyle_;
		loppe::ComputedStyle lastComputedStyle_;
	public:
		CapabilityHost &caps();
		const CapabilityHost &caps() const;

		// ===== Accessor =====
		const class IPropertyProvider& props() const;

		// ===== AST & Control =====
		std::shared_ptr<ASTNode> astNode;
		std::string getId();
		// 树内身份（整数）：创建时分配一次，跨解析、跨编辑保持不变。
		loppe::NodeUid uidValue() const;
		void setUidValue(loppe::NodeUid uid);
		// 拥有本节点的解析树
		ResolvedTree* ownerTree() const;

		// ===== UI 状态 =====
		UIState uiState = UIState::None;
		UIState lastAppliedState = UIState::None;

		// ===== 样式 =====
		// compiled rules storage
		const loppe::ComputedStyle& getFinalStyle() const;
		loppe::ComputedStyle& finalStyle();
		CompiledStyleRules compiledRules;

		// ===== 布局 =====
		ComputedLayout layout;
		CompiledLayoutRules layoutRules;

		// ===== 树结构 =====
		ResolvedNode* ownerParent() const;
		void setOwnerParent(ResolvedNode* p);
		ResolvedNode* viewParent() const;
		void setViewParent(ResolvedNode* p);
		// 运行期上溯用的父：跨文档挂载时为槽节点，否则为所有权父。
		ResolvedNode* parentInView() const;

		const std::vector<ResolvedNode*>& getChildren();
		ResolvedNode* at(int i);
		const std::vector<ResolvedNode*> &getFlowChildren();
		const std::vector<ResolvedNode*> &getOverlayChildren();
		void clear();
		// 按给定顺序重装子节点与 flow/overlay 桶（不动子节点的 parent）。
		void setChildren(const std::vector<ResolvedNode*>& ordered);
		void destory();

		// Compiled style rules attached to this node (base + state deltas)
		const CompiledStyleRules &getCompiledStyleRules() const;
		void setCompiledStyleRules(const CompiledStyleRules &c);
		void setCompiledLayoutRules(const CompiledLayoutRules &c);

		// Recompute finalStyle based on compiledRules and current uiState
		void recomputeStyle();
		void recompileStyleAndLayout(DocumentRuntime* ctx);

		// 某个 UI 状态（Hover/Active/…）变化时，本节点的样式是否真的会变 ——
		// 没写过对应的状态规则（`.x:hover {}`）就是没变化，宿主/事件系统可以跳过重同步。
		bool stateAffectsStyle(loppe::UIState s) const;
		// 该状态规则是否改到了**可继承**属性（color / font-* / line-height / text-align）——
		// 是的话，路径上更深的后代即使自己没有状态规则，也必须跟着重同步。
		bool stateAffectsInherited(loppe::UIState s) const;

		std::weak_ptr<loppe::AbstractControl> getControl();
		void setControl(std::weak_ptr<loppe::AbstractControl> control);
		// ================= 状态控制 =================
		void setUIState(UIState newState);
		void addUIState(UIState s);
		void removeUIState(UIState s);
		bool hasUIState(UIState s) const;

		// ================= Dirty 逻辑 =================
		// 只标本节点（并沿父链插 ChildrenLayout），不级联给子节点。
		void markDirty(DirtyFlag flags);

		void propagateDirtyToParent();

		void clearDirty(DirtyFlag flags);
		bool isDirty(DirtyFlag flags) const;
		bool needsLayout() const;
		bool needsStyle() const;
		DirtyFlag getDirtyFlags() const;

		void addChildren(ResolvedNode* node);
		// 在 index 处插入子节点（index 越界/默认按 append）；按 finalStyle.overlay 归入对应桶。
		// 注：flow/overlay 桶不互相同序，index 只精确保证 children 序；桶内按近似位置插入。
		void insertChild(size_t index, ResolvedNode* node);
		// 从父节点摘除自己（children + flow/overlay 桶一起摘，并清空 parent）。
		void detachFromParent();

		// 状态规则或类表改到可继承属性（color / font-* / line-height / text-align）时调用：重算整个子树的可继承样式。
		void refreshInheritedStylesInChildren();
	private:
		void commitStyle(const loppe::ComputedStyle& prev);

		ResolvedNode* extractChild(ResolvedNode* node);
		void applyInheritedStyles();

		class IPropertyProvider* property_resolver_ = nullptr;
		ResolvedTree* tree_ = nullptr;
		loppe::NodeUid uid_ = loppe::kUidNone;

		// diff 里是否有「改到可继承属性」的状态（setUIState 用）
		bool affectsInheritedAny(UIState diff) const;
	};

}