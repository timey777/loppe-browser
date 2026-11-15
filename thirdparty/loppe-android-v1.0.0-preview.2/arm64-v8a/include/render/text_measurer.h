/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include <string>
#include <memory>
#include "render/text_layout.h"

// 简单的文本度量结构
struct TextMetrics {
    float width = 0.0f;
    float ascent = 0.0f;
    float descent = 0.0f;
    float lineHeight = 0.0f;
};

struct FontDesc {
    std::string family;
    float size = 12.0f;
    // bold/italic 参与测量，与绘制使用同一套字形口径（斜体不改变宽度）。
    bool bold = false;
    bool italic = false;
};

// 文本测量接口
namespace loppe {

class ITextMeasurer {
public:
    virtual ~ITextMeasurer() = default;
    // measureText: measure text metrics. If wrapWidth > 0, measurement should
    // consider wrapping to that width and return total measured height in
    // TextMetrics::lineHeight. Default wrapWidth <= 0 means no wrapping.
    virtual TextMetrics measureText(const std::u32string& text, const FontDesc& font, float wrapWidth = -1.0f) = 0;
    virtual TextMetrics measureText(const std::string& utf8text, const FontDesc& font, float wrapWidth = -1.0f) = 0;
    virtual float getLineHeight(const FontDesc& font) = 0;

    // createTextLayout: build a reusable text layout (wrapping, alignment,
    // line height).
    virtual std::shared_ptr<ITextLayout> createTextLayout(
        const std::string& text, const Font& font,
        const TextLayoutParams& params, float maxWidth = -1.0f) = 0;
};

} // namespace loppe