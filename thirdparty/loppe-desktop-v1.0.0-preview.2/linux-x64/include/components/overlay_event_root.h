/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "container_control.h"
#include "engine/core/coordinate_types.h"

namespace loppe {

class OverlayEventRoot : public ContainerControl {
public:
    OverlayEventRoot() = default;

    std::string getTypeName() const override;

    void onRender(DisplayList& displayList, const ui::Rect& clip) const override;

    // Dispatch mouse events using absolute screen coordinates.
    bool dispatchMouseMove(const MouseMoveEvent& e);
    bool dispatchMouseButton(const MouseButtonEvent& e);

    // 是否有可见的弹层盖住该点（场景坐标，与 dispatchMouse* 的入参同一口径）。
    bool hitTest(const LogicalPointDp& point) const;

private:
};

} // namespace loppe