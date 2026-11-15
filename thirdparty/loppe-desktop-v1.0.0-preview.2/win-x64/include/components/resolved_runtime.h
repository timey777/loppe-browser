/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "misc/stable_id.h"

namespace loppe {

class ResolvedNode;

// 节点 uid → 解析节点 的全局查找表（uid 是整数）。
class ResolvedRuntime {
public:
    ResolvedNode* findNodeByUid(loppe::NodeUid uid) const;
    void registerNode(loppe::NodeUid uid, ResolvedNode* node);
    void unregisterNode(loppe::NodeUid uid);
    void clear();

    // 请求移除节点（延迟执行，executePendingRemovals 时落地）。eraseFromAst：true = 同步摘掉该节点的 AST 兄弟项。
    void scheduleRemoval(ResolvedNode* node, bool eraseFromAst);
    void scheduleRemovalByUid(loppe::NodeUid uid, bool eraseFromAst = true);
    void executePendingRemovals();

private:
    struct PendingRemoval {
        ResolvedNode* node = nullptr;
        ResolvedNode* parent = nullptr;
        bool eraseFromAst = true;
    };

    std::unordered_map<loppe::NodeUid, ResolvedNode*> uidMap_;
    std::unordered_set<ResolvedNode*> pendingRemovalSet_;
    std::vector<PendingRemoval> pendingRemovalNodes_;
};

} // namespace loppe