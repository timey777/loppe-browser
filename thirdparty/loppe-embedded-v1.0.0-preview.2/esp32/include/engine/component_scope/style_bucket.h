/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once
// ═══════════════════════════════════════════════════════════════════
// style_bucket.h — 样式规则桶与选择器键索引（StyleEngine 的数据组件）
//
// StyleRuleBucket：按来源分组的样式规则集合，支持按节点快速收集候选规则。
// StyleBucketKeyIndex：桶内的选择器键索引。
// ═══════════════════════════════════════════════════════════════════

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "engine/component_scope/style_rule.h"

struct ASTNode;

// 候选规则引用（order = 规则在桶内的原始下标，结果按其升序返回）
struct StyleRuleRef {
    int order;
    const StyleRule* rule;
};

// 桶内「选择器键」索引
class StyleBucketKeyIndex {
public:
    // 按规则的选择器键重建索引。
    void build(const std::vector<StyleRule>& rules);
    void clear();
    bool empty() const;

    // 收集与节点可能相关的候选（class/tag/id + universal），按 order 升序
    void collect(const ASTNode& node, std::vector<StyleRuleRef>& out) const;

private:
    std::unordered_map<std::string, std::vector<StyleRuleRef>> byClass_;
    std::unordered_map<std::string, std::vector<StyleRuleRef>> byTag_;
    std::unordered_map<std::string, std::vector<StyleRuleRef>> byId_;
    std::vector<StyleRuleRef> universal_;
};

// 样式规则桶：用于按来源分组存储规则（例如 default/global/组件注册段）
class StyleRuleBucket {
public:
    void addRule(StyleRule rule);
    void clear();
    bool empty() const;
    std::size_t size() const;
    // 只读遍历规则（按桶内顺序，即生效顺序）。
    const std::vector<StyleRule>& rules() const;

    // 重建规则索引。
    void rebuildIndex();

    // 收集可能与节点匹配的候选规则（可能多于实际命中），按桶内原始顺序升序返回。
    void collectCandidates(const ASTNode& node, std::vector<StyleRuleRef>& out) const;

private:
    std::vector<StyleRule> rules_;
    std::unique_ptr<StyleBucketKeyIndex> keyIndex_;   // 懒建
};
