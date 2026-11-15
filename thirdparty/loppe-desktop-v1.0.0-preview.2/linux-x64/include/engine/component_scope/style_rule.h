/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once
// ═══════════════════════════════════════════════════════════════════
// style_rule.h — 样式规则数据模型（纯数据 + 选择器派生）
//
// CompiledSelector：选择器单个条件（tag/class/id/universal）的编码。
// StyleRule：一条样式规则（选择器 + 状态 + 属性 + 影响标志 + 索引键）
// extractPseudoState()：拆出选择器伪类后缀（:hover/:active/:focus/:disabled）
// ═══════════════════════════════════════════════════════════════════

#include <map>
#include <string>

#include "engine/core/attribute.h"
#include "engine/core/node_flags.h"   // loppe::UIState

// 预编译的选择器条件（tag/class/id/universal）。
struct CompiledSelector {
    enum class Type { Tag, Class, Id, Universal };
    Type type = Type::Universal;
    std::string value; // tag name, class name (without .), or id (without #)
};

// 一条样式规则；requiredState 限定规则仅在对应 UI 状态下生效。
struct StyleRule {
    std::string selector; // 原始选择器字符串（复合 ".a.b" / 后代 ".a .b" 都直接存）

    loppe::UIState requiredState = loppe::UIState::None;

    std::map<std::string, Attribute> properties; // 样式属性（直接存储 Attribute）

    bool affectsLayout = false;  // 是否影响布局
    bool affectsPaint  = true;   // 是否影响绘制

    // 由 selector 派生的选择器键（复合/后代选择器取末段）。
    CompiledSelector key;

    // 由 selector 派生 key。
    void deriveSelectorFields();
};

// 从选择器尾部拆出伪类状态（outSelector=去伪类后的选择器；无伪类 → UIState::None）
void extractPseudoState(const std::string& rawSelector, std::string& outSelector, loppe::UIState& outState);
