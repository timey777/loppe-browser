/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once
#include <memory>
#include "engine/core/coordinate_types.h"
#include "engine/core/resolved_node.h"
#include <engine/event/ui_event_system.h>
#include "components/input/input_event.h"

namespace loppe {
    class Scene;
    struct DragState {
        AbstractControl* control = nullptr;
        loppe::ResolvedNode* node = nullptr;
        Capability* cap = nullptr;
        LogicalPointDp startPos;
        loppe::ResolvedNode* hoveredDropTarget = nullptr;
        bool dragging = false;
    };
    class DragManager {
    public:
        explicit DragManager(Scene* scene);

        bool onMouseButton(const MouseButtonEvent& e);
        bool onMouseMove(const MouseMoveEvent& e);
        void cancel();

    private:
        Scene* scene_ = nullptr;

        std::optional<DragState> active_;

    private:
    };

} // namespace loppe