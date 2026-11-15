/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "ast_node.h"
#include "loppe_export.h"
#include <memory>
#include <string>
#include <vector>

namespace loppe {

class LoppeParser;

class LOPPE_API AstSnippet {
public:
	AstSnippet() = default;
	static AstSnippet FromParsedRoot(std::shared_ptr<ASTNode> root);

	const std::shared_ptr<ASTNode>& root() const;
	std::vector<std::shared_ptr<ASTNode>> contentRoots() const;
	const std::vector<std::shared_ptr<ASTNode>>* getMeta(const std::string& key) const;
	bool hasMeta(const std::string& key) const;
	bool hasContent() const;
	size_t contentCount() const;
	AstSnippet instantiate() const;
	bool empty() const;
	explicit operator bool() const;

private:
	explicit AstSnippet(std::shared_ptr<ASTNode> root);

	std::shared_ptr<ASTNode> root_;

	friend class LoppeParser;
};

} // namespace loppe