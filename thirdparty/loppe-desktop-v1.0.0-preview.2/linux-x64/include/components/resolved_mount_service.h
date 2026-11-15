/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "abstract_control.h"
#include "engine/core/resolved_rule_scope.h"
#include "engine/core/ast_snippet.h"
#include "engine/core/ast_node.h"
#include "engine/core/resolved_node.h"
#include "engine/core/resolved_tree.h"
#include "loppe_export.h"
#include <memory>
#include <string>
#include <vector>

class DocumentRuntime;

namespace loppe {

class ControlFactory;

struct LOPPE_API MountResult {
	bool success = false;
	std::vector<std::shared_ptr<AbstractControl>> controls;

	std::shared_ptr<AbstractControl> primary() const;
	bool empty() const;
	explicit operator bool() const;
};
class ResolvedTreeView;
class LOPPE_API ResolvedMountService {
public:
	static MountResult MountResolvedSubtreeUnderId(ResolvedTreeView* rt, const std::string& parentId,
		ResolvedNode* subtree, DocumentRuntime* ctx = nullptr, ControlFactory* factory = nullptr,
		int index = -1);

	static MountResult MountAstUnderId(ResolvedTreeView* rt, const std::string& parentId,
		std::shared_ptr<ASTNode> ast, DocumentRuntime* ctx = nullptr,
		ResolvedRuleScope ruleScope = ResolvedRuleScope::Local,
		ControlFactory* factory = nullptr);

	static MountResult MountSnippetUnderId(ResolvedTreeView* rt, const std::string& parentId,
		const AstSnippet& snippet, DocumentRuntime* ctx = nullptr,
		ResolvedRuleScope ruleScope = ResolvedRuleScope::Local,
		ControlFactory* factory = nullptr);
};

} // namespace loppe