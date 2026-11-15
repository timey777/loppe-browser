/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "misc/rule_signature.h"
#include "misc/stable_id.h"

class DocumentRuntime;
struct ASTNode;

namespace loppe {
    class ResolvedTree;
    class ResolvedNode;

    // 增量更新器：把「旧树 + 新 AST + 文档全文」推成新版本。
    class ResolvedTreeDiffer {
    public:
        explicit ResolvedTreeDiffer(ResolvedTree& tree);

        // 宿主交来当前文档全文（每次编辑后、notifyASTUpdated 之前）。
        void setSourceText(std::string text);

        // 全量重建后重置源码坐标系。
        void resetCoordinates();
        // 全量重建后记录规则集基线（须在首次增量更新之前调用）。
        void captureRuleBaseline(DocumentRuntime* ctx);
        // 清空基线（ResolvedTree::clear()）
        void clearBaseline();

        // 开一轮更新：算这次编辑的坐标变换（用两版全文）
        void beginUpdate();

        // 增量更新。newAst 必须**已经分配好源码身份**（StableId::assignSourceUids）。
        // ctx 为空 = 不做任何编译，也不更新签名基线。
        void apply(const std::shared_ptr<ASTNode>& newAst, DocumentRuntime* ctx);

        // 本轮真的走了 compile() 的节点数（0 = 全部复用）
        void resetCompileCount();
        void noteCompiledNode();
        std::size_t lastCompileCount() const;

    private:
        void updateNode(ResolvedNode* renderNode, const std::shared_ptr<ASTNode>& newAst,
                        DocumentRuntime* ctx, bool ancestorRecompiled);

        bool diffChildren(ResolvedNode* renderNode, const std::shared_ptr<ASTNode>& newAst,
                          DocumentRuntime* ctx, bool ancestorRecompiled);

        bool isSameRange(const std::shared_ptr<ASTNode>& oldAst, const std::shared_ptr<ASTNode>& newAst) const;

        void retireSubtree(ResolvedNode* node);

        void refreshChangedSelectors(DocumentRuntime* ctx, const std::shared_ptr<ASTNode>& newAst);

        ResolvedTree& tree_;

        std::string sourceText_;
        std::string lastSourceText_;
        SourceRelocation relocation_;

        bool ruleReuseEnabled_ = false;
        std::vector<RuleSignature> lastStyleSigs_;
        std::vector<RuleSignature> lastLayoutSigs_;
        ChangedSelectors changedSelectors_;

        std::size_t lastCompileCount_ = 0;
    };

} // namespace loppe
