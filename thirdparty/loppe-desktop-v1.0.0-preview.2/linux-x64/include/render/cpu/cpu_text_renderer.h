/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

// CPU 后端的文字引擎（ITextRenderer 实现）：度量/排版/字形绘制到 CPU 画布。
#include <memory>

#include "render/text_renderer.h"

namespace loppe {
namespace cpu {
struct Canvas;
}
}

class CpuTextRenderer : public loppe::ITextRenderer {
public:
    explicit CpuTextRenderer(loppe::cpu::Canvas* canvas);
    ~CpuTextRenderer() override;

    // ---- ITextMeasurer ----
    TextMetrics measureText(const std::u32string& text, const FontDesc& font, float wrapWidth = -1.0f) override;
    TextMetrics measureText(const std::string& utf8text, const FontDesc& font, float wrapWidth = -1.0f) override;
    float getLineHeight(const FontDesc& font) override;
    std::shared_ptr<ITextLayout> createTextLayout(const std::string& text, const Font& font,
                                                  const TextLayoutParams& params,
                                                  float maxWidth = -1.0f) override;

    // ---- ITextRenderer ----
    void drawText(const std::string& text, float x, float y, const Font& font,
                  const DrawStyle& style = DrawStyle(), float maxWidth = -1.0f,
                  const TextLayoutParams& layout = TextLayoutParams()) override;
    void drawTextLayout(const std::shared_ptr<ITextLayout>& layout, float x, float y,
                        const DrawStyle& style) override;
    void drawTextLayoutRange(const std::shared_ptr<ITextLayout>& layout, float x, float y,
                             const DrawStyle& style, int firstLine, int lineCount) override;

    // 字形表（引擎内部使用）。
    struct Impl;

private:
    void blitGlyph(int glyphIndex, float gx, float gy, const DrawStyle& style);
    float ScaleX() const;
    float ScaleY() const;

    std::unique_ptr<Impl> impl_;
};
