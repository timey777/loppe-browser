/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once
// ═══════════════════════════════════════════════════════════════════
// selector_matcher.h — 选择器完整匹配（匹配组件）
//
// matches()：复杂选择器（空格分隔的后代链）的完整匹配。
// ═══════════════════════════════════════════════════════════════════

#include <memory>
#include <string>

struct ASTNode;

class SelectorMatcher {
public:
    static bool matches(const std::shared_ptr<ASTNode>& node, const std::string& selector);

private:
    static bool matchesSimple(const std::shared_ptr<ASTNode>& node, const std::string& selector);
};
