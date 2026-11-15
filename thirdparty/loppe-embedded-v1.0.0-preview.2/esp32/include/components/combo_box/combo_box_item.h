/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "../abstract_control.h"
#include <functional>

namespace loppe {

/**
 * @brief 下拉框项控件
 * 
 * 用于显示下拉框中的单个选项
 */
class ComboBoxItem : public AbstractControl {
public:
    ComboBoxItem(const std::string& text, int index, std::function<void(int)> onClick);

    void onRender(DisplayList& displayList, const ui::Rect& clip) const override;
    
    std::string getTypeName() const override;

protected:
    bool handleLocalMouseMove(const ControlLocalMouseMoveEvent& e) override;
    bool handleLocalMouseButton(const ControlLocalMouseButtonEvent& e) override;

public:
    std::string getText() const;

    void setSelected(bool selected);
    bool isSelected() const;

private:
    std::string text_;
    int index_;
    std::function<void(int)> onClick_;
    bool isHovered_ = false;
    bool isSelected_ = false;
};

} // namespace loppe