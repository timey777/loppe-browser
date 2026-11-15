/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once
#include "abstract_control.h"
#include "panel.h"
#include "button.h"
#include "text.h"
#include "progress_bar.h"
#include "engine/core/resolved_tree.h"
#include "engine/document_runtime.h"
#include <memory>
#include "loppe_export.h"

namespace loppe {

// 所有与控件创建、绑定和 uid 映射相关的接口已封装为 ControlFactory

// Forward declare ScrollbarControl used in factory signatures
class ScrollbarControl;

class LOPPE_API ControlFactory {
public:
	ControlFactory() = default;
	virtual ~ControlFactory() = default;

	// Instance management (singleton-like). 默认会有一个全局实例。
	static void setInstance(std::unique_ptr<ControlFactory> inst);
	static std::unique_ptr<ControlFactory> releaseInstance();
	static ControlFactory* getInstance();

	std::shared_ptr<AbstractControl> CreateControlFromResolvedNode(ResolvedNode* resolvedNode);
	std::shared_ptr<AbstractControl> CreateControlTreeFromAST(ResolvedNode* resolvedNode);

	void BindControlToResolvedNode(ResolvedNode* rn, std::shared_ptr<AbstractControl> ctrl);

	// 字符串版（对外：app / 编辑器 / 模板）。
	ResolvedNode* FindNodeByUid(const std::string& uid);
	void RemoveResolvedNodeByUid(const std::string& uid, DocumentRuntime* ctx);

	// 整数版 uid 查找（无字符串转换）。
	ResolvedNode* FindNodeByUidValue(loppe::NodeUid uid);
	// 按节点直接延迟移除（不经 uid 表）。
	void RetireResolvedNode(ResolvedNode* node, bool eraseFromAst);

	// 执行延迟移除 (通常在帧末调用)
	void executePendingRemovals();

	void RegisterNodeUid(loppe::NodeUid uid, ResolvedNode* node);
	void UnregisterNodeUid(loppe::NodeUid uid);
	void ClearUidMap();
	// Create auxiliary controls that may be needed by other controls (only factory can create).
	// 虚拟：视图层可返回自定义（细/配色）滚动条；返回类型必须是 ScrollbarControl 子类。
	// 粗细由滚动条自己按宿主节点样式 scrollbar-width 现读（未写则用 ScrollbarControl::kScrollbarThickness）。
	virtual std::shared_ptr<loppe::ScrollbarControl> CreateScrollbar(loppe::ScrollbarControl::Orientation orient);

	// Lightweight factory: create bare controls without AST/ResolvedNode.
	std::shared_ptr<Text>  CreateText();
	std::shared_ptr<Panel> CreatePanel();
	std::shared_ptr<ProgressBar> CreateProgressBar();

protected:
	std::shared_ptr<AbstractControl> makeControl(ResolvedNode* resolvedNode);
	// 唯一保留的扩展点：插入额外控件类型创建。
	virtual std::shared_ptr<AbstractControl> doMakeControlFromResolvedNodeHook(ResolvedNode* resolvedNode);

private:
	static std::unique_ptr<loppe::ControlFactory> instance_;
};

} // namespace loppe