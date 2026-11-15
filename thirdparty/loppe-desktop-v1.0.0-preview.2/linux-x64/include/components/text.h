/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "abstract_control.h"
#include "engine/resource/resource_manager.h"
#include "render/text_layout.h"
#include "render/text_measurer.h"
#include "render/drawer.h"
#include "scene.h"
#include "engine/core/node_flags.h"
#include "misc/float_utils.h"
#include <cmath>

namespace loppe {

/**
 * @brief 文本控件
 *
 * 用于显示文本内容
 */
class LOPPE_API Text : public AbstractControl {
public:
    // 禁止外部直接构造，使用 ControlFactory 创建
protected:
    Text() = default;
    explicit Text(const std::string& text);
    friend class ControlFactory;

    // 子类钩子：Styles 段同步文本/字体，Layout 段同步 wrap/layoutWidth。
    void OnSyncFromResolvedNode(const ResolvedNode& rn, ControlSync mask) override;

    void onRender(DisplayList& displayList, const ui::Rect& clip) const override;

public:
    std::string getTypeName() const override;

    // Padding。
    void setPadding(float left, float top, float right, float bottom) override;

    // 文本相关属性
    void setText(const std::string& text);
    const std::string& getText() const;

    // 字号；0 表示使用默认大小
    void setFontSize(float size);
    float getFontSize() const;

    void setFontFamily(const std::string& family);
    const std::string& getFontFamily() const;

    void setBold(bool bold);
    bool isBold() const;

    void setItalic(bool italic);
    bool isItalic() const;

    // 是否允许换行（由上层根据是否行内元素设置）
    void setWrap(bool wrap);
    bool getWrap() const;

    // 文本行高
    void setLineHeight(float lineHeight);
    float getLineHeight() const;

    // 文本对齐
    enum class Alignment {
        Left,
        Center,
        Right
    };

    void setAlignment(Alignment align);
    Alignment getAlignment() const;

    void setAlignment(loppe::TextAlign align);

    // 接受字符串对齐值（"left" / "center" / "right"，大小写不敏感）
    void setAlignment(const std::string& s);

    // 文本颜色
    void setTextColor(const ui::Color& color);
    const ui::Color& getTextColor() const;

    // 布局宽度（用于对齐，不影响 bounds）。
    void setLayoutWidth(float width);
    float getLayoutWidth() const;

private:
    std::string text_;
    std::string fontFamily_;
    float fontSize_ = 0.0f;  // 0表示使用默认大小
    float lineHeight_ = 0.0f;
    bool bold_ = false;
    bool italic_ = false;
    Alignment alignment_ = Alignment::Left;
    ui::Color textColor_{0.0f, 0.0f, 0.0f, 0.0f};
    bool wrap_ = true;
    float layoutWidth_ = 0.0f;

    mutable std::shared_ptr<ITextLayout> textLayout_;
    mutable bool layoutDirty_ = true;
    mutable float lastLayoutWidth_ = 0.0f;

    void updateTextLayout();
};

} // namespace loppe
