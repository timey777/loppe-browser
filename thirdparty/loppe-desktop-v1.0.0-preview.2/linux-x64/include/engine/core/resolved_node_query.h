/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "resolved_node.h"
#include "loppe_export.h"
#include <string>

namespace loppe {
class ResolvedTree;
class ResolvedTreeView;

// 解析后树的节点查询
class LOPPE_API ResolvedNodeQuery {
public:
	// 在视图树中按 id 深度优先查找节点。tree 为空、树无根或 id 不存在时返回 nullptr；
	// id 为空时返回根节点。返回的节点由树持有，树清空或节点回收后不可再使用。
	static ResolvedNode* FindById(ResolvedTreeView* tree, const std::string& id);

	// 在源树中按 id 深度优先查找节点，语义与视图树重载相同。
	// 例：ResolvedNode* n = ResolvedNodeQuery::FindById(runtime->getResolvedTree(), "preview");
	static ResolvedNode* FindById(ResolvedTree* tree, const std::string& id);

	// 在视图树中按 id 查找属于指定源树的节点。各文档可有同名 id：
	// 用节点的 ownerTree() 与传入的源树比较，节点被挂进视图树后该归属不变。
	// tree / ownerTree 为空、树无根或 id 不存在时返回 nullptr。
	static ResolvedNode* FindById(ResolvedTreeView* tree, ResolvedTree* ownerTree,
	                             const std::string& id);

private:
	static ResolvedNode* FindByIdInSubtree(ResolvedNode* root, const std::string& id);
};

} // namespace loppe
