/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "container_control.h"
#include "text.h"
#include <iostream>
#include "scene.h"
#include "misc/logger.h"

namespace loppe {

/**
 * @brief 按钮控件
 *
 * 可交互的按钮控件，可以选择性地添加文本或其他子控件
 */
class LOPPE_API Button : public ContainerControl {
public:
    // 禁止外部直接构造，使用 ControlFactory 创建
protected:
    Button() = default;
    friend class ControlFactory;

    void onRender(DisplayList& displayList, const ui::Rect& clip) const override;

public:
    std::string getTypeName() const override;

protected:
    // 事件处理：鼠标悬停/按下改变状态
    bool handleLocalMouseMove(const ControlLocalMouseMoveEvent& e) override;
    bool cursorIntent(const ControlLocalPointDp& point, CursorIntent& out) const override;
    bool handleLocalMouseButton(const ControlLocalMouseButtonEvent& e) override;
};

} // namespace loppe
