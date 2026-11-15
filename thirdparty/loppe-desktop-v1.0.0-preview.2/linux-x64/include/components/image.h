/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include <string>

#include "abstract_control.h"
#include "engine/core/resolved_node.h"
#include "engine/resource/resource_manager.h"
#include "misc/logger.h"

namespace loppe {

/**
 * @brief 图片控件
 *
 * 用于显示图片内容
 */
class LOPPE_API Image : public AbstractControl {
public:
    // 禁止外部直接构造，使用 ControlFactory 创建
protected:
    Image() = default;
    explicit Image(const std::string& src);
    friend class ControlFactory;

    // 运行时 src 属性变化（DSL 更新/动态换图）时重新同步并预加载
    void OnSyncFromResolvedNode(const ResolvedNode& rn, ControlSync mask) override;

    void onRender(DisplayList& displayList, const ui::Rect& clip) const override;

public:
    std::string getTypeName() const override;

    void setSrc(const std::string& src);
    const std::string& getSrc() const;

private:
    std::string src_;
};

} // namespace loppe
