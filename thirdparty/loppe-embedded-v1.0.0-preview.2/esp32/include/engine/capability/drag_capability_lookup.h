/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include <vector>

#include "capability.h"

namespace loppe {

class ResolvedNode;

struct DragActivationTarget {
    ResolvedNode* node = nullptr;
    ::DraggableCap* cap = nullptr;
};

struct DropTargetMatch {
    ResolvedNode* node = nullptr;
    ::DropTargetCap* cap = nullptr;
};

DragActivationTarget findDragActivationTarget(const std::vector<ResolvedNode*>& path);
DropTargetMatch findDropTargetMatch(const std::vector<ResolvedNode*>& path, ResolvedNode* draggedNode);

} // namespace loppe