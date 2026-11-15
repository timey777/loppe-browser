/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "engine.h"
#include "engine/core/resolved_tree.h" // For UIState
#include <map>
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include "engine/core/node_flags.h"
#include "engine/component_scope/style_rule.h"
#include "engine/component_scope/style_bucket.h"
#include "engine/component_scope/component_global_style_rules.h"
#include "misc/rule_signature.h"   // loppe::RuleSignature（ruleSignatures 返回）

namespace loppe { struct CompiledStyleRules; }

enum class StyleRuleScope {
    Default,
    Global,
    Local
};

class StyleEngine : public AbstractEngine {
public:
    StyleEngine() = default;
    ~StyleEngine() = default;
    
    void initialize() override;
    void shutdown() override;
    std::type_index type() const override;
    void process(std::shared_ptr<ASTNode> root, loppe::DisplayList& outList,
                 float viewportW, float viewportH) override;
    // 当 AST 热重载或外部修改时调用。
    void onASTUpdated(std::shared_ptr<ASTNode> root) override;
    
    std::string name() const override;
    
    // 获取匹配节点的样式属性（返回 Attribute 指针）
    const Attribute* getStyleProperty(std::shared_ptr<ASTNode> node, const std::string& property) const;
    // UIState-aware overload: only consider rules that either have state == UIState::None
    // or match the provided state (rules with matching state take precedence)
    const Attribute* getStyleProperty(std::shared_ptr<ASTNode> node, const std::string& property, loppe::UIState state) const;
    
    // 获取样式属性的字符串值（便捷方法）
    std::string getStylePropertyString(std::shared_ptr<ASTNode> node, const std::string& property,
                                      const std::string& defaultValue = "") const;
    // UIState-aware overload
    std::string getStylePropertyString(std::shared_ptr<ASTNode> node, const std::string& property,
                                      const std::string& defaultValue, loppe::UIState state) const;
    
    // 检查节点是否有样式属性
    bool hasStyleProperty(std::shared_ptr<ASTNode> node, const std::string& property) const;
    
    // 清空所有样式规则
    void clearStyles();
    
    // 加载默认样式
    void loadDefaultStyles();
    
    // 重置引擎状态（清空样式规则并重新加载默认样式）
    void reset();

    // 按作用域写入规则
    void setScopedRules(std::shared_ptr<ASTNode> root, StyleRuleScope scope,
                        std::optional<int> ownerId = std::nullopt, bool replace = true);
    // 清理作用域规则（Local 时 ownerId 必填）
    void clearScopedRules(StyleRuleScope scope, std::optional<int> ownerId = std::nullopt);

    // 将 AST 节点匹配到的规则预编译为合并后的样式（base + state deltas）
    loppe::CompiledStyleRules compile(std::shared_ptr<ASTNode> ast) const;

    // 规则集签名表（每个选择器聚合一条内容指纹）。
    std::vector<loppe::RuleSignature> ruleSignatures() const;

    // 规则桶当前是按哪一份 AST 解析出来的（nullptr = 还没有）。
    bool rulesParsedFrom(const std::shared_ptr<ASTNode>& ast) const;

    static constexpr int ORDER_USER_START = 1000;
    static constexpr int ORDER_INLINE = 0x7FFFFFFF;
    
private:
    // 分桶存储
    StyleRuleBucket defaultBucket_;
    StyleRuleBucket globalBucket_;        // 根文档全局规则
    std::unordered_map<int, StyleRuleBucket> localBuckets_;
    // 组件注册的全局规则
    ComponentGlobalStyleRules componentGlobalStyleRules_;
    // 规则桶是按哪一份 AST 解析出来的
    std::shared_ptr<ASTNode> rulesSourceAST_;
};