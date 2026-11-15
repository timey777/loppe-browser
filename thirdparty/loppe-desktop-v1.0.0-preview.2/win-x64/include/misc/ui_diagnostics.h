/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include <vector>

#include "engine/core/coordinate_types.h"

namespace loppe {

class ResolvedNode;
class UIEventSystem;

namespace diagnostics {

void logInputMapping(const char* source,
                     const char* action,
                     DevicePointPx physicalPoint,
                     ContentScale scale,
                     LogicalPointDp logicalPoint,
                     const ResolvedNode* hitNode);

void logHitResult(const char* source,
                  LogicalPointDp point,
                  int button,
                  int action,
                  const std::vector<ResolvedNode*>& path,
                  const ResolvedNode* hitNode);

void logSceneUiState(const char* source,
                     UIEventSystem* ui,
                     const ResolvedNode* captureNode);

void logQueuedNodeUpdate(const char* source, const ResolvedNode* node);

void logSchedulerDecision(const char* source,
                          const char* frameAction,
                          const std::vector<ResolvedNode*>& dirtyNodes,
                          bool hasPaintRequest,
                          bool hasRepaintRequest,
                          bool hasAstRepaintRequest);

void logViewportEvent(const char* source, ViewportSizeDp viewportSize);

void logPartialSync(const char* source, const std::vector<ResolvedNode*>& nodes);

void logNodeSnapshot(const char* source, const char* label, const ResolvedNode* node);

} // namespace diagnostics
} // namespace loppe