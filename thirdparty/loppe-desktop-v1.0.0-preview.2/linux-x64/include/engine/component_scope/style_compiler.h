/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once
// ═══════════════════════════════════════════════════════════════════
// style_compiler.h — 桶组合 → CompiledStyleRules（编译组件）
//
// 按层叠顺序把各来源样式桶编译为节点的预编译样式（内联属性权重最高）。
// userOrderStart/inlineOrder 为层叠权重基准值。
// ═══════════════════════════════════════════════════════════════════

#include <memory>

struct ASTNode;
class StyleRuleBucket;
namespace loppe { struct CompiledStyleRules; }

class StyleCompiler {
public:
    struct Buckets {
        const StyleRuleBucket* defaultB   = nullptr;
        const StyleRuleBucket* globalB    = nullptr;   // 根文档全局
        const StyleRuleBucket* componentB = nullptr;   // 组件注册段
        const StyleRuleBucket* localB     = nullptr;   // 无本地桶 → nullptr
    };

    static loppe::CompiledStyleRules compile(const std::shared_ptr<ASTNode>& ast,
                                             const Buckets& buckets,
                                             int userOrderStart, int inlineOrder);
};
