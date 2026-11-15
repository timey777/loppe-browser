/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once
// ═══════════════════════════════════════════════════════════════════
// default_style_rules.h — 默认样式表（Default 桶内容）
//
// 内置控件默认样式（button/input/text 等，含 state 规则）。
// 向样式规则桶追加内置控件默认样式（button/input/text 等，含 state 规则）；不覆盖桶内已有规则。
// ═══════════════════════════════════════════════════════════════════

class StyleRuleBucket;

class DefaultStyleRules {
public:
    static void loadInto(StyleRuleBucket& bucket);
};
