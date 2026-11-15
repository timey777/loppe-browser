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
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <array>
#include <cstddef>
#include "node_flags.h"
#include "resolved_node.h"
#include "diff/resolved_tree_differ.h"

class DocumentRuntime;

namespace loppe {
    class ResolvedNode;
    // 解析后树（节点容器 + 内存所有权 + 公开 API）。
    class ResolvedTree {
    public:
        ResolvedTree();
        ~ResolvedTree();

        // 从 AST 构建解析后树
        // 可选传入 DocumentRuntime*，如果提供则在创建节点时使用其中的 Style/Layout 引擎预编译样式规则并挂载到节点
        void buildFromAST(std::shared_ptr<ASTNode> ast, DocumentRuntime* ctx = nullptr);

        // 增量更新：比较新旧 AST，只标记变化的节点为脏
        void updateFromAST(std::shared_ptr<ASTNode> newAst);
        // Overload: 提供 DocumentRuntime 以便在更新期间重新编译样式和布局规则
        void updateFromAST(std::shared_ptr<ASTNode> newAst, DocumentRuntime* ctx);

        // 获取根节点
        loppe::ResolvedNode* getRoot() const;

        // 本树所属文档；由 DocumentRuntime 构造时注入
        void setRuntime(DocumentRuntime* rt);
        DocumentRuntime* getRuntime() const;

        // 收集所有需要重新布局的节点
        void collectDirtyNodes(std::vector<loppe::ResolvedNode*>& outNodes, loppe::DirtyFlag filter);

        // 清空整个树
        void clear();

        // 递归构建（公开）：将 parent 参数放到最后，默认 nullptr；可传入 DocumentRuntime 以使用 Style/Layout 引擎
        loppe::ResolvedNode* buildNodeRecursive(std::shared_ptr<ASTNode> ast, DocumentRuntime* ctx = nullptr, loppe::ResolvedNode* parent = nullptr);

        // 宿主把**当前文档全文**交给树。每次编辑后（notifyASTUpdated 之前）调一次。
        void setSourceText(std::string text);

        // 回收已脱离树的节点。
        void reclaimUnreachable();

        // 立即释放一棵**已经完全脱离树**的子树（注销 uid、断指针、释放内存）。
        // notifySinks=false 用于「通知已经在排队时递归做过了」的场景，避免重复通知。
        void releaseSubtree(loppe::ResolvedNode* node, bool notifySinks);

        // 当前 ownedNodes_ 里挂着多少个节点
        std::size_t ownedNodeCount() const;

        // 上一轮更新实际编译了几个节点（0 = 全部复用；节点总数 = 全量重编）
        std::size_t lastCompileCount() const;

    private:
        loppe::ResolvedNode* root_ = nullptr;
        std::vector<std::unique_ptr<loppe::ResolvedNode>> ownedNodes_;
        std::unordered_map<loppe::ResolvedNode*, std::size_t> ownedIndex_;

        // 增量更新器（组合）
        ResolvedTreeDiffer differ_;

        // 本树所属文档（不拥有，仅反查）
        DocumentRuntime* runtime_ = nullptr;

        // 两个 updateFromAST 入口的公共实现
        void applyUpdate(const std::shared_ptr<ASTNode>& newAst, DocumentRuntime* ctx);

        loppe::ResolvedNode* createNode(std::shared_ptr<ASTNode> ast);
        void recycleNode(loppe::ResolvedNode* node);
        void releaseOwnedNode(loppe::ResolvedNode* node);

        // 递归收集脏节点
        void collectDirtyNodesRecursive(loppe::ResolvedNode* node, std::vector<loppe::ResolvedNode*>& outNodes, DirtyFlag filter);
    };

}