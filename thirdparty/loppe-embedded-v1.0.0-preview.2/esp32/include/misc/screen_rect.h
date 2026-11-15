/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "components/uitypes.h"
#include "loppe_export.h"

namespace loppe {

class AbstractControl;
class ResolvedNode;

// 把排版坐标换算成屏幕坐标：屏幕位置 = 自身 bounds + 自身 visualOffset + 祖先 (visualOffset − scroll) 逐级累加。
class LOPPE_API ScreenRect {
public:
    // 控件在屏幕上的外框（不含 clip/滚动视口裁剪）
    static ui::Rect ofControl(const AbstractControl* ctrl);
    // 节点在屏幕上的外框（走**排版树**的父子链；滚动口径同上）。
    static ui::Rect ofNode(ResolvedNode* node);
    // 同上，但用指定的排版外框（节点当前外框已被重排改写、手上有更早的快照时用）。
    static ui::Rect ofNodeBox(ResolvedNode* node, const ui::Rect& borderBox);
    // 给定排版外框 + 控件（用于「某个已知 bounds 要换到屏幕坐标」的场景）
    static ui::Rect ofBounds(const ui::Rect& bounds, const AbstractControl* ctrl);
};

} // namespace loppe
