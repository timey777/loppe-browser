/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include <vector>

#include "components/uitypes.h"   // loppe::ui::Rect

namespace loppe {
class ResolvedNode;
class Scene;
}

// 这一帧是谁触发的（只影响日志/自检里的入口名，不改变执行内容）。
enum class FrameReason {
    Frame,      // 主循环取走待办计划
    Resize,     // 视口尺寸变化
};

enum class DocumentUpdateKind {
    None = 0,
    PaintOnly,      // 仅重绘，不重新计算样式/布局
    PartialUpdate,  // 局部样式/布局同步（仅指定脏节点）
    LayoutOnly,     // 仅重排 + 同步 bounds（布局专用，如拖拽），不重建样式/文本
    Repaint,        // 全量样式/布局同步
    FullLayout,     // 全量重排（有节点触发了流式布局改变）
    ASTRepaint,     // AST 结构更新，全量重建
};

// 一份文档本帧要执行的更新计划。
struct DocumentUpdatePlan {
    DocumentUpdateKind kind = DocumentUpdateKind::None;
    std::vector<loppe::ResolvedNode*> dirtyNodes;
    // 动画产生的局部重绘矩形（文档局部坐标）。空 = 整屏重画；非空时只刷这些块。
    std::vector<loppe::ui::Rect> paintRects;
    // 本帧有控件请求了必须整屏的重绘（不接受矩形降级）。
    bool mustFullPaint = false;
    // 本帧有整屏兜底请求；已有精确矩形/脏节点时按它们刷。
    bool fullPaint = false;

    bool hasWork() const;
    // 重排类：整棵树的几何可能变化。
    bool isRelayoutKind() const;
};

// 一次 pass 的输出：本趟落地影响的场景；非空表示该场景帧末需要重新生成 display list。
struct PassImpact {
    loppe::Scene* scene = nullptr;
};
