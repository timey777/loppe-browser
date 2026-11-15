/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once
// ═══════════════════════════════════════════════════════════════════
// component_global_style_rules.h — 「组件注册段」全局样式规则（独立组件）
//
// 组件注册段的全局样式规则；优先级高于文档 global、低于 local 与内联属性。
// ═══════════════════════════════════════════════════════════════════

#include <functional>
#include <memory>

struct ASTNode;
class StyleRuleBucket;

class ComponentGlobalStyleRules {
public:
    // 注入的解析回调：把 @style 元数据子树解析进目标桶。
    using ParseRuleFn = std::function<void(const std::shared_ptr<ASTNode>&, StyleRuleBucket&)>;

    ComponentGlobalStyleRules();
    ~ComponentGlobalStyleRules();
    ComponentGlobalStyleRules(const ComponentGlobalStyleRules&) = delete;
    ComponentGlobalStyleRules& operator=(const ComponentGlobalStyleRules&) = delete;

    // 清空组件注册段的全部样式规则。
    void clear();

    // 注册：从 AST 解析 @style 规则到本段；replace=true 时先清空已有规则。
    void registerFrom(const std::shared_ptr<ASTNode>& root, bool replace, const ParseRuleFn& parseRule);

    // 获取本段的样式规则桶。
    StyleRuleBucket& bucket();
    const StyleRuleBucket& bucket() const;

private:
    std::unique_ptr<StyleRuleBucket> bucket_;
};
