/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "engine/view_coordinator.h"
#include "engine/event/interaction_scope.h"
#include "engine/core/resolved_tree.h"
#include "engine/core/resolved_tree_view.h"
#include "engine/core/resolved_node_query.h"
#include "engine/document_runtime.h"
#include "engine/update_scheduler.h"
#include "components/container_control.h"
#include "components/scene.h"
#include "misc/stable_id.h"
#include "misc/logger.h"
#include <vector>

namespace loppe {

// ===================================================================
// PreviewViewCoordinator — 预览视图专用的 ViewCoordinator 子类
// ===================================================================
class LOPPE_API PreviewViewCoordinator : public ViewCoordinator {
public:
    explicit PreviewViewCoordinator(
        DocumentRuntime* context,
        ViewportSizeDp initialViewportSize,
        SceneResolver sceneResolver = SceneResolver(),
        std::string previewSlotId = "preview",
        InteractionMode interactionMode = InteractionMode::Isolated);

    ~PreviewViewCoordinator() override;

    // 挂载目标文档：槽节点按该文档的源树定位，各文档可以重复使用同一个槽 id（iframe 语义）。
    // 未设置时回退为「宿主视图树全局按 id 查找」（单层预览）。
    DocumentRuntime* _target = nullptr;
    void set(DocumentRuntime* target);

    // 交互作用域（沙盒/共享）
    InteractionScope& interactionScope();
    void setInteractionMode(InteractionMode m);
    InteractionMode interactionMode() const;

protected:
    void applyASTRebuild() override;

private:
    std::string previewSlotId_;
    std::unique_ptr<InteractionScope> scope_;
};

} // namespace loppe