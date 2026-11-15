/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once
#include "property_provider.h"
#include "resolved_node.h"

namespace loppe {

// 节点属性解析器：把 ResolvedNode 的样式与布局属性映射为 IPropertyProvider 接口。
class PropertyResolver : public IPropertyProvider {
public:
    explicit PropertyResolver(const ResolvedNode* n);
    explicit PropertyResolver(const std::shared_ptr<ResolvedNode>& n);

    // --- IPropertyProvider 实现 ---

    // 几何属性
    LayoutUnit getWidth() const override;
    LayoutUnit getHeight() const override;
    LayoutUnit getMinWidth() const override;
    LayoutUnit getMinHeight() const override;
    LayoutUnit getMaxWidth() const override;
    LayoutUnit getMaxHeight() const override;
    ui::Edges getPadding() const override;
    ui::Edges getMargin() const override;
    std::array<bool, 4> getMarginAuto() const override;
    ui::Edges getBorderWidth() const override;

    // 布局上下文
    FormattingContext getDisplay() const override;
    Position getPosition() const override;

    // 文字与内容
    float getFontSize() const override;
    float getLineHeight() const override;
    std::string getFontFamily() const override;
    FontWeight getFontWeight() const override;
    FontStyle getFontStyle() const override;
    TextAlign getTextAlign() const override;
    Overflow getOverflow() const override;
    Overflow getOverflowX() const override;
    Overflow getOverflowY() const override;
    float getScrollbarWidth() const override;
    bool getScrollbarGutterStable() const override;

    // 悬浮/绝对定位
    bool isOverlay() const override;
    Anchor getAnchor() const override;
    float getOffsetX() const override;
    float getOffsetY() const override;

    // Flex 布局
    FlexDirection getFlexDirection() const override;
    AlignItems getAlignItems() const override;
    AlignSelf getAlignSelf() const override;
    JustifyContent getJustifyContent() const override;
    FlexWrap getFlexWrap() const override;
    float getFlexGrow() const override;
    float getFlexShrink() const override;
    float getGap() const override;

    // Grid 布局
    std::string getGridTemplateColumns() const override;
    std::string getGridTemplateRows() const override;

private:
    const ResolvedNode* node;
};

} // namespace loppe