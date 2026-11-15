/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once
// ═══════════════════════════════════════════════════════════════════
// component_instantiator.h — 最小组件模板实例化器
//
// 把一段 AstSnippet 当「组件模板」，运行期实例化并挂到宿主指定父节点下。
//
// 前置条件：模板内部 id 由调用方保证唯一；同一模板多次实例化时不要硬编码 id（可用参数注入）。
// ═══════════════════════════════════════════════════════════════════

#include <string>
#include <vector>

#include "components/element_view.h"
#include "components/resolved_mount_service.h"
#include "components/scene.h"
#include "components/container_control.h"
#include "engine/core/ast_snippet.h"
#include "engine/core/resolved_builder.h"
#include "engine/core/resolved_node.h"
#include "engine/core/resolved_node_query.h"
#include "engine/core/tree_synchronizer.h"
#include "engine/update_scheduler.h"
#include "misc/logger.h"

namespace loppe {

class ComponentInstantiator {
public:
    // 用一段 AstSnippet 当模板实例化并挂到 parentId 下（顶层内容节点成为 parent 的子节点）。
    // index：第一个顶层内容节点在 parent 子节点中的插入位置（-1=append，0=最前）。
    // ruleScope：Local（默认，实例隔离）或 Global（样式/layout 注册为文档全局，供拆文件/多次 spawn 共享）。
    //
    // 返回值是挂载出来的元素视图（按顶层内容节点顺序），也是「成败」的唯一表示：
    //     if (!SpawnElementViewUnderId(...)) { /* 挂载失败：parent 不存在 / 子树构建失败 */ }
    //     if (SpawnElementViewsUnderId(...).empty()) { ... }
    // 只关心成败、不需要句柄时用 explicit operator bool / empty() 判断即可。
    static ElementView SpawnElementViewUnderId(Scene* scene, const std::string& parentId,
                                               const AstSnippet& snippet, DocumentRuntime* ctx,
                                               int index = -1,
                                               ResolvedRuleScope ruleScope = ResolvedRuleScope::Local);

    static std::vector<ElementView> SpawnElementViewsUnderId(Scene* scene, const std::string& parentId,
                                                             const AstSnippet& snippet,
                                                             DocumentRuntime* ctx, int index = -1,
                                                             ResolvedRuleScope ruleScope = ResolvedRuleScope::Local);
};

} // namespace loppe
