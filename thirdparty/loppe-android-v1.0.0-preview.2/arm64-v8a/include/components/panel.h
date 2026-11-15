/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "container_control.h"

namespace loppe {

/**
 * @brief 面板控件
 *
 * 一个容器控件，可以包含其他控件，并绘制背景和边框
 */
class LOPPE_API Panel : public ContainerControl {
public:
    // 禁止外部直接构造，使用 ControlFactory 创建
protected:
    Panel() = default;
    friend class ControlFactory;

    void onRender(DisplayList& displayList, const ui::Rect& clip) const override;

public:
    std::string getTypeName() const override;

    // 内边距（四参顺序与 AbstractControl::setPadding 一致：左、上、右、下）
    void setPadding(float padding);
    void setPadding(float left, float top, float right, float bottom) override;

    float getPaddingTop() const;
    float getPaddingRight() const;
    float getPaddingBottom() const;
    float getPaddingLeft() const;

private:
    float paddingTop_ = 0.0f;
    float paddingRight_ = 0.0f;
    float paddingBottom_ = 0.0f;
    float paddingLeft_ = 0.0f;
};

} // namespace loppe
