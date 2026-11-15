/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "abstract_control.h"
#include "text.h"
#include <string>
#include <vector>
#include "scene.h"
#include "misc/logger.h"

namespace loppe {

/**
 * @brief 单选按钮控件
 */
class LOPPE_API RadioButton : public AbstractControl {
public:
    RadioButton() = default;

    // 状态
    enum class State {
        Normal,
        Hover,
        Pressed,
        Disabled
    };

    void onRender(DisplayList& displayList, const ui::Rect& clip) const override;

    std::string getTypeName() const override;

protected:
    bool handleLocalMouseMove(const ControlLocalMouseMoveEvent& e) override;
    bool cursorIntent(const ControlLocalPointDp& point, CursorIntent& out) const override;
    bool handleLocalMouseButton(const ControlLocalMouseButtonEvent& e) override;

public:
    void setChecked(bool checked);
    bool isChecked() const;

    void setGroup(const std::string& group);
    const std::string& getGroup() const;

    void setText(const std::string& text); // 简单属性

protected:
    void notifyGroup();

    State state_ = State::Normal;
    bool checked_ = false;
    std::string group_;
    std::string text_;
    ui::Color hoverColor_ = ui::Color(0, 0, 0, 0);
};

} // namespace loppe
