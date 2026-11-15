/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "text_measurer.h"
#include "graphics_types.h"
#include "loppe_export.h"
#include <memory>

namespace loppe {

/**
 * @brief 文字引擎接口：文本测量/排版（继承自 ITextMeasurer）+ 字形绘制。
 */
class LOPPE_API ITextRenderer : public ITextMeasurer {
public:
    ~ITextRenderer() override = default;

    // drawText: text drawn at (x,y). maxWidth <=0 means no wrapping.
    // Alignment/wrap/lineHeight are supplied via TextLayoutParams.
    virtual void drawText(const std::string& text, float x, float y, const Font& font,
                          const DrawStyle& style = DrawStyle(), float maxWidth = -1.0f,
                          const TextLayoutParams& layout = TextLayoutParams()) = 0;

    /**
     * @brief 绘制预计算的文本布局（整棵）
     * @param layout 由 createTextLayout 创建的布局对象
     * @param x,y    绘制起始坐标
     * @param style  绘制样式
     */
    virtual void drawTextLayout(const std::shared_ptr<ITextLayout>& layout,
                                float x, float y, const DrawStyle& style) = 0;

    /**
     * @brief 只绘制 layout 的第 firstLine 行起的 lineCount 行（可见行裁剪）
     *
     * 超大文本滚动时只为视口内可见行提交字形。lineCount <= 0 视为整棵全量。
     */
    virtual void drawTextLayoutRange(const std::shared_ptr<ITextLayout>& layout,
                                     float x, float y, const DrawStyle& style,
                                     int firstLine, int lineCount) = 0;
};

} // namespace loppe