/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "../container_control.h"

namespace loppe {

class ComboBox;

/**
 * @brief 下拉框触发按钮
 */
class ComboTrigger : public ContainerControl {
public:
    explicit ComboTrigger(ComboBox* owner);

    std::string getTypeName() const override;

    void setText(const std::string& text);
    void setTextColor(const ui::Color& color);
    void setFontSize(float size);

    // 展开箭头用的图标名（内置图标按名称引用；空 = 不画箭头）
    void setArrowIcon(const std::string& name);
    const std::string& getArrowIcon() const;

    void onRender(DisplayList& displayList, const ui::Rect& clip) const override;

protected:
    bool handleLocalMouseButton(const ControlLocalMouseButtonEvent& e) override;
    bool handleLocalHitTest(ControlLocalPointDp point) const override;

private:
    ComboBox* owner_ = nullptr;
    std::string text_;
    ui::Color textColor_ = ui::Color(0, 0, 0, 1);
    float fontSize_ = 14.0f;
    std::string arrowIcon_ = "chevron-down";   // 展开箭头：内置 SVG 图标
};

} // namespace loppe