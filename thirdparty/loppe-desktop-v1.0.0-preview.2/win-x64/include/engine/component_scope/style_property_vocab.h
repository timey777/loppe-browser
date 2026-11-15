/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once
// ═══════════════════════════════════════════════════════════════════
// style_property_vocab.h — DSL 属性词汇表（查询组件）
//
// DSL 属性词汇表：判断属性名是否被 @style / @layout 规则认识（未认识的属性会被忽略并告警）。
// ═══════════════════════════════════════════════════════════════════

#include <string>

namespace loppe {

// key 是否为 @style 规则认识的属性（width/height/margin/padding 等盒模型属性也算样式层）
bool isKnownStyleProperty(const std::string& key);

// key 是否为 @layout 规则认识的属性（display/position/flex-*/gap/grid-template-*）
bool isKnownLayoutProperty(const std::string& key);

} // namespace loppe
