/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

namespace loppe {

struct CssPx {
    float value = 0.0f;
};

struct LogicalPx {
    float value = 0.0f;
};

struct DeviceScaleFactor {
    float value = 1.0f;
};

struct PageScaleFactor {
    float value = 1.0f;
};

struct FramebufferSizePx {
    int width = 0;
    int height = 0;
};

struct ViewportSizeDp {
    LogicalPx width;
    LogicalPx height;
};

struct DevicePointPx {
    float x = 0.0f;
    float y = 0.0f;
};

struct LogicalPointDp {
    LogicalPx x;
    LogicalPx y;
};

struct NodeLocalPointDp {
    LogicalPx x;
    LogicalPx y;
};

struct ControlLocalPointDp {
    LogicalPx x;
    LogicalPx y;
};

struct ParentLocalPointDp {
    LogicalPx x;
    LogicalPx y;
};

NodeLocalPointDp makeNodeLocalPoint(float x, float y);

ParentLocalPointDp makeParentLocalPoint(float x, float y);

ControlLocalPointDp makeControlLocalPoint(float x, float y);

ParentLocalPointDp toParentLocalPoint(NodeLocalPointDp point);

ParentLocalPointDp toParentLocalPoint(ControlLocalPointDp point);

ControlLocalPointDp toControlLocalPoint(NodeLocalPointDp point);

struct DeviceScrollDeltaPx {
    float dx = 0.0f;
    float dy = 0.0f;
};

struct LogicalScrollDeltaDp {
    LogicalPx dx;
    LogicalPx dy;
};

struct LogicalDragOffsetDp {
    LogicalPx dx;
    LogicalPx dy;
};

LogicalDragOffsetDp makeLogicalDragOffset(float dx, float dy);

struct ControlLocalScrollDeltaDp {
    LogicalPx dx;
    LogicalPx dy;
};

struct ContentScale {
    DeviceScaleFactor x;
    DeviceScaleFactor y;
};

DeviceScaleFactor sanitizeScale(DeviceScaleFactor scale);

ContentScale sanitizeScale(ContentScale scale);

LogicalPx toLogicalPx(float physicalPx, DeviceScaleFactor scale);

ViewportSizeDp toViewportSizeDp(FramebufferSizePx framebufferSize, ContentScale scale);

LogicalPointDp toLogicalPointDp(DevicePointPx point, ContentScale scale);

LogicalScrollDeltaDp toLogicalScrollDeltaDp(DeviceScrollDeltaPx delta, ContentScale scale);

} // namespace loppe