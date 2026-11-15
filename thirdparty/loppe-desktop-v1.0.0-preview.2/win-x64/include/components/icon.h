/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include <string>

#include "abstract_control.h"
#include "engine/core/resolved_node.h"
#include "misc/logger.h"

namespace loppe {

/**
 * @brief 图标控件（SVG）
 *
 * DSL：icon(name="close", class="...")
 *   - name：svg::RegisterIcon 注册名（应用启动时注册）
 *   - 着色：CSS `color`（解析自 textColor）；hover 换色随样式层自动生效
 */
class LOPPE_API Icon : public AbstractControl {
public:
    // 禁止外部直接构造，使用 ControlFactory 创建
protected:
    Icon() = default;
    explicit Icon(const std::string& name);
    friend class ControlFactory;

    // 同步布局派生属性：注册名（DSL name 属性）与着色（CSS color）
    void OnSyncFromResolvedNode(const ResolvedNode& rn, ControlSync mask) override;

    void onRender(DisplayList& displayList, const ui::Rect& clip) const override;

public:
    std::string getTypeName() const override;

    void setName(const std::string& name);
    const std::string& getName() const;

private:
    std::string name_;
    ui::Color color_;   // CSS color（alpha=0 → 同 Text：默认黑）
};

} // namespace loppe
