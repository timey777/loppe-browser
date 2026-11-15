/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "../container_control.h"
#include "combo_box_item.h"
#include "combo_box_popup.h"
#include "combo_trigger.h"

#include <functional>
#include <string>
#include <vector>

namespace loppe {

/**
 * @brief 下拉框控件
 * 
 * 提供选项下拉功能的控件
 */
class LOPPE_API ComboBox : public ContainerControl {
public:
    ComboBox();

    std::string getTypeName() const override;

    // 设置选项列表：**替换**（先清空再灌），不是追加 —— 可以反复调用来刷新
    // （如串口列表热插拔）。若无选中项或旧选中项已不存在，则选第一条（无选项则清空选择）。
    // 旧选中项仍在新列表里时保持选择。
    void setOptions(const std::vector<std::string>& options);

    void setSelectedIndex(int idx);

    int getSelectedIndex() const;

    // 选项条数（宿主/测试查看当前列表）
    int getOptionCount() const;

    // 弹层是否展开（宿主可据此避免在交互中刷新选项）
    bool isOpen() const;

    // 无选中项时触发按钮显示的占位文本（缺省 "Select..."）
    void setPlaceholder(const std::string& text);

    // 弹层即将打开的回调（在算尺寸/显示之前调用）：宿主可在此**刷新选项**。
    void setOnWillOpen(std::function<void()> cb);

    void setTextColor(const ui::Color& color);
    void setFontSize(float size);

    // 展开箭头用的图标名（内置图标按名称引用；空 = 不画箭头）。DSL 对应 `arrow` 属性。
    void setArrowIcon(const std::string& name);
    std::string getArrowIcon() const;

    void onRender(DisplayList& displayList, const ui::Rect& clip) const override;

    void setBounds(const ui::Rect& b) override;

    void togglePopup();

    void openPopup();

    void closePopup();

    ui::Rect getScreenBounds() const;

protected:
    bool handleLocalMouseMove(const ControlLocalMouseMoveEvent& e) override;
    bool handleLocalMouseButton(const ControlLocalMouseButtonEvent& e) override;
    void OnSyncFromResolvedNode(const ResolvedNode& rn, ControlSync mask) override;

private:
    friend class ComboBoxPopup;

    void updatePopupBounds();
    void applySelectedIndex(int idx);

    int selectedIndex_ = -1;
    bool isOpen_ = false;
    std::string placeholder_ = "Select...";
    std::function<void()> onWillOpen_;
    std::shared_ptr<ComboBoxPopup> popup_;
    std::shared_ptr<ComboTrigger> trigger_;
};

} // namespace loppe