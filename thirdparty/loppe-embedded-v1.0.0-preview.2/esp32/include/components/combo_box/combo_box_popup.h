/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "../container_control.h"
#include "combo_box_item.h"
#include <memory>

namespace loppe {

class ComboBox;

/**
 * @brief 下拉框弹出菜单容器
 */
class ComboBoxPopup : public ContainerControl {
public:
    explicit ComboBoxPopup(ComboBox* owner);
    
    void onRender(DisplayList& displayList, const ui::Rect& clip) const override;
    
    std::string getTypeName() const override { return "ComboBoxPopup"; }
    
protected:
    bool handleLocalMouseMove(const ControlLocalMouseMoveEvent& e) override;
    bool handleLocalMouseButton(const ControlLocalMouseButtonEvent& e) override;

public:
    std::string getItemText(int idx) const;
    
    const std::vector<std::shared_ptr<ComboBoxItem>>& getItems() const { return items_; }
    
    void addItem(std::shared_ptr<ComboBoxItem> item);

    // 清空全部条目（setOptions 替换列表时用；只清内容，不动弹层自身）
    void clearItems();

private:
    ComboBox* owner_ = nullptr;
    std::vector<std::shared_ptr<ComboBoxItem>> items_;
};

} // namespace loppe