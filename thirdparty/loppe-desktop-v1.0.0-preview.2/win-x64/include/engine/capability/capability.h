/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once
#include <functional>

#include "engine/core/coordinate_types.h"

namespace loppe{
    class ResolvedNode;

    struct DropTargetEvent {
        ResolvedNode* dragged = nullptr;
        LogicalPointDp hitPoint;
    };
}
struct Capability {
    virtual ~Capability() = default;
};
//配置被拖动的对象
struct DraggableCap : Capability {
    bool enabled = true;
};
//指定拖动手柄和被拖动的对象
struct DragHandleCap : Capability {
    // 实际被拖动的节点
    loppe::ResolvedNode* owner = nullptr;

    explicit DragHandleCap(loppe::ResolvedNode* o);
};
//指定手柄内不允许拖动的对象
struct BlockDragCap : Capability {};
//拖动时的动画效果，当拖动到被拖动的对象进入某个容器时，容器会触发onHover
struct DropTargetCap : Capability {
    std::function<void(const loppe::DropTargetEvent& event)> onDrop;
    std::function<void(const loppe::DropTargetEvent& event)> onHover;
};