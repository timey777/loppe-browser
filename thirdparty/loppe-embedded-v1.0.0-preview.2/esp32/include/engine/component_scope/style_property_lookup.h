/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once
// ═══════════════════════════════════════════════════════════════════
// style_property_lookup.h — 属性查询（查询组件）
//
// 读取优先级：内联属性 > local > 组件注册段 > 文档 global > default；
// 同桶内 state 规则优先于普通规则；未命中且属性可继承时沿父链继承。
// ═══════════════════════════════════════════════════════════════════

#include <memory>
#include <string>
#include <unordered_map>

#include "engine/component_scope/style_bucket.h"   // StyleRuleBucket / StyleRuleRef
#include "engine/core/node_flags.h"                // loppe::UIState

struct ASTNode;

class StylePropertyLookup {
public:
    struct Buckets {
        const StyleRuleBucket* defaultB   = nullptr;
        const StyleRuleBucket* globalB    = nullptr;
        const StyleRuleBucket* componentB = nullptr;
        const std::unordered_map<int, StyleRuleBucket>* locals = nullptr;
    };

    static const Attribute* find(const std::shared_ptr<ASTNode>& node, const std::string& property,
                                 loppe::UIState state, const Buckets& buckets);
};
