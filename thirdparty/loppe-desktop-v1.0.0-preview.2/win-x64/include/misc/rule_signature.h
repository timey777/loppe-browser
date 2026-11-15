/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "loppe_export.h"

struct ASTNode;
class Attribute;   // 全局类型

namespace loppe {

// ============================================================================
// 规则集签名 —— 「这一轮生效的样式/布局规则，和上一轮一样吗；不一样的话，变了哪几条」
// ============================================================================

// 一条规则的「身份」= 去伪类后的选择器文本；hash = 该选择器下所有规则内容的聚合指纹。
struct RuleSignature {
    std::string selector;
    uint64_t    hash = 0;
};

// 单条规则的内容指纹：由选择器 + 状态 + 属性计算；派生字段不参与。
LOPPE_API uint64_t ruleContentHash(const std::string& selector, int state,
                                   const std::map<std::string, Attribute>& properties);

// 由「(选择器, 单条规则指纹) 序列」生成签名表：按选择器聚合、按选择器排序。
// 传入顺序 = 规则生效顺序（只影响同一选择器组内）。
LOPPE_API std::vector<RuleSignature> buildRuleSignatures(
    std::vector<std::pair<std::string, uint64_t>> rules);

// 对比两轮签名表 → 「内容变了 / 新增 / 删除」的选择器（去重、按选择器排序）。
LOPPE_API std::vector<std::string> changedSelectors(const std::vector<RuleSignature>& previous,
                                                    const std::vector<RuleSignature>& current);

// 「这一轮变化的选择器」的逐节点判定器：节点是否「可能被变化的选择器命中」。
// 判定为「不受影响」⇒ 该节点该轮编译结果必然一致（判定保守：宁可多编）。
class LOPPE_API ChangedSelectors {
public:
    ChangedSelectors() = default;
    explicit ChangedSelectors(const std::vector<std::string>& selectors);

    bool empty() const;

    // 任一变化的选择器命中该节点 ⇒ 该节点的编译结果可能变，必须重编
    bool mayAffect(const std::shared_ptr<ASTNode>& node) const;

private:
    struct Entry {
        std::string selector;   // 完整选择器（去伪类）
        std::string keyValue;   // 末段「必要条件」的值
        char        keyKind = 0; // 'c' class / 'i' id / 't' tag / 0 = 无键（如 `*`，一律进完整匹配）
    };

    std::vector<Entry> entries_;
};

// 节点的「选择器输入」（type + 全部属性 + class 列表）是否与上一轮逐字段相同。
// 注意：任一节点带 localScopeId 时返回 false。
LOPPE_API bool selectorInputsUnchanged(const std::shared_ptr<ASTNode>& oldNode,
                                       const std::shared_ptr<ASTNode>& newNode);

} // namespace loppe
