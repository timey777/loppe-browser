/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "parse.h"
#include "render/display_list.h"
#include <memory>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <optional>
#include <array>
#include <string>
#include "node_flags.h"
#include "resolved_node.h"
#include "engine/event/interaction_scope.h"

class DocumentRuntime;

namespace loppe {
    class ResolvedNode;
    class ResolvedTree;

    // 解析后树视图：只组合各文档的层级，不拥有节点，也不改动源树的结构。
    class ResolvedTreeView {
    public:
        struct Mount {
            ResolvedTree* ownerTree = nullptr;     // 槽内节点所属的源树（只作查询，不解引用生命周期）
            std::vector<ResolvedNode*> roots;      // 挂进槽的顶层节点快照
        };

    private:
        loppe::ResolvedNode* root_ = nullptr;
        // 槽节点 → 子文档挂载。
        std::unordered_map<ResolvedNode*, Mount> mounts_;
        // 交互作用域注册表（预览槽沙盒）
        InteractionScopeRegistry interactionScopes_;

    public:
        ResolvedTreeView() = default;
        ~ResolvedTreeView() = default;

        // 获取根节点
        loppe::ResolvedNode* getRoot() const;
        // 交互作用域注册表（查询：enclosingOf / isInsideIsolated）
        InteractionScopeRegistry& interactionScopes();
        const InteractionScopeRegistry& interactionScopes() const;

        // 清空视图：解除对源树根的引用，并解绑所有子文档挂载
        void clear();
        void attachSubtree(ResolvedNode* node, ResolvedNode* parent, int index = -1);

        // 把 source 的根 children 作为「引用」挂进 parent 槽，不摘源树 wrapper 的 children。
        // 同一槽重复调用会先解绑旧引用（幂等）。parent 为空时只把 source 根接管为视图根。
        void attachSubtreeFromOther(ResolvedTree& source, loppe::ResolvedNode* parent,
                                    bool shouldMarkDirty = true);

        // 解除某个槽的子文档挂载：把引用从槽里摘掉，并把节点的 parent 恢复为源树 wrapper。
        void detachMount(ResolvedNode* slot);

        // 槽上挂载的子文档源树；未挂载时返回 nullptr
        ResolvedTree* mountedOwnerTree(ResolvedNode* slot) const;

        // 若本视图树尚无根，将 source 的根接管为本视图根（首次构建合并引擎树）。
        // 已有根或 source 无根时不做任何事。
        void adoptRootFrom(ResolvedTree& source);
    };

} // namespace loppe
