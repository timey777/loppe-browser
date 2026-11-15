/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "ast_snippet.h"
#include "ast_node.h"
#include "resolved_rule_scope.h"
#include "resolved_node.h"
#include "loppe_export.h"
#include <memory>

class DocumentRuntime;

namespace loppe {

class LOPPE_API ResolvedBuilder {
public:
	static ResolvedNode* BuildResolvedSubtreeFromAST(std::shared_ptr<ASTNode> ast,
		DocumentRuntime* ctx = nullptr,
		ResolvedRuleScope ruleScope = ResolvedRuleScope::Local);
	static ResolvedNode* BuildResolvedSubtreeFromSnippet(const AstSnippet& snippet,
		DocumentRuntime* ctx = nullptr,
		ResolvedRuleScope ruleScope = ResolvedRuleScope::Local);
};

} // namespace loppe