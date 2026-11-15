/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

// pointer_cast_helper.h — AbstractControl ↔ ContainerControl 安全向下转型。
#pragma once

#include <functional>
#include <string>

#include "misc/undo_redo.h"
#include <components/container_control.h>

namespace loppe {

class pointer_cast_helper {
public:
    // AbstractControl → ContainerControl 安全向下转型：非容器返回 nullptr。
    static std::shared_ptr<ContainerControl> cast(std::shared_ptr<AbstractControl> ctrl);
};

} // namespace loppe