/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "panel.h"

namespace loppe {

/**
 * @brief 根控件
 *
 * 一个容器控件，可以包含其他控件，并绘制背景和边框
 */
class LOPPE_API Root : public Panel {
public:
    // 禁止外部直接构造，使用 ControlFactory 创建
protected:
    Root() = default;
    friend class ControlFactory;

public:
    std::string getTypeName() const override;
};

} // namespace loppe
