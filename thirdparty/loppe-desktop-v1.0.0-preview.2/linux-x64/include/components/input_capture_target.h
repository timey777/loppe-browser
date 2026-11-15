/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include <memory>

#include "engine/core/coordinate_types.h"
#include "components/input/input_event.h"

namespace loppe {
class AbstractControl;
class ResolvedNode;

class IInputCaptureTarget {
public:
    virtual ~IInputCaptureTarget() = default;
    virtual bool onMouseButton(const MouseButtonEvent& e) = 0;
    virtual void onMouseMove(const MouseMoveEvent& e) = 0;
    virtual ResolvedNode* capturedNode() const = 0;
    virtual bool refersToNode(ResolvedNode* node) const = 0;
    // 捕获目标对应的控件（拿不到时为 nullptr）。Scene 用它询问捕获期间的光标意愿。
    // 非拥有指针：控件由 ControlManager 持有，与捕获目标同生命周期。
    virtual AbstractControl* capturedControl() const = 0;
};

std::unique_ptr<IInputCaptureTarget> createResolvedNodeCaptureTarget(ResolvedNode* node, ResolvedNode* relativeTo);
std::unique_ptr<IInputCaptureTarget> createControlCaptureTarget(AbstractControl* control, AbstractControl* relativeTo);

} // namespace loppe