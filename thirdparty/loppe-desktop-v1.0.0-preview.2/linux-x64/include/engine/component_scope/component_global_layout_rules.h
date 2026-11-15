/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once
// ═══════════════════════════════════════════════════════════════════
// component_global_layout_rules.h — 「组件注册段」全局布局规则（独立组件）
//
// 组件注册段的全局布局规则；优先级高于文档 global、低于 local 与内联属性。
// ═══════════════════════════════════════════════════════════════════

#include <functional>
#include <memory>

struct ASTNode;
struct LayoutRuleBucket;

class ComponentGlobalLayoutRules {
public:
    // 注入的解析回调：把 @layout 元数据子树解析进目标桶。
    using ParseRuleFn = std::function<void(const std::shared_ptr<ASTNode>&, LayoutRuleBucket&)>;

    ComponentGlobalLayoutRules();
    ~ComponentGlobalLayoutRules();
    ComponentGlobalLayoutRules(const ComponentGlobalLayoutRules&) = delete;
    ComponentGlobalLayoutRules& operator=(const ComponentGlobalLayoutRules&) = delete;

    // 清空组件注册段的全部布局规则。
    void clear();

    // 注册：从 AST 解析 @layout 规则到本段；replace=true 时先清空已有规则。
    void registerFrom(const std::shared_ptr<ASTNode>& root, bool replace, const ParseRuleFn& parseRule);

    // 获取本段的布局规则桶。
    LayoutRuleBucket& bucket();
    const LayoutRuleBucket& bucket() const;

private:
    std::unique_ptr<LayoutRuleBucket> bucket_;
};
