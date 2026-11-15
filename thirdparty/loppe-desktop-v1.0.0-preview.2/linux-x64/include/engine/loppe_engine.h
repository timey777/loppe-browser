/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once
// engine/ 目录汇总头（引擎服务、运行时、帧协调；各分层另有自己的汇总头）。

#include "engine/document_runtime.h"
#include "engine/document_update_plan.h"
#include "engine/update_scheduler.h"
#include "engine/compositor.h"
#include "engine/view_coordinator.h"
#include "engine/view_slot_manager.h"
#include "engine/window_paint_request.h"
#include "engine/embedded_view_coordinator.h"
#include "engine/preview_view_coordinator.h"

#include "engine/pipeline/loppe_pipeline.h"
#include "engine/event/loppe_event.h"
#include "engine/resource/loppe_resource.h"
#include "engine/damage/loppe_damage.h"
#include "engine/platform/loppe_platform.h"

#include "engine/core/loppe_core.h"
#include "engine/capability/loppe_capability.h"
#include "engine/component_scope/loppe_component_scope.h"