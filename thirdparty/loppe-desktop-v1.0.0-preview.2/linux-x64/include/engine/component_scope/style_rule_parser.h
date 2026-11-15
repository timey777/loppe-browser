/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once
// ═══════════════════════════════════════════════════════════════════
// style_rule_parser.h — @style 元数据 → 规则桶（解析组件）
//
// 把 AST 上的 @style 块解析为样式规则并写入目标桶。
// ═══════════════════════════════════════════════════════════════════

#include <memory>

struct ASTNode;
class StyleRuleBucket;

class StyleRuleParser {
public:
    // 递归解析：node 本身及其子树中的 Style/StyleBlock 块内容 → bucket
    static void parseInto(const std::shared_ptr<ASTNode>& node, StyleRuleBucket& bucket);

private:
    static void parseIntoRuleNode(const std::shared_ptr<ASTNode>& ruleNode, StyleRuleBucket& bucket);
};
