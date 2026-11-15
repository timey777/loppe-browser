/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include <memory>
#include <string>
#include "loppe_export.h"
#include "render/text_measurer.h"

namespace loppe {

// 基于 FreeType 的离线文本测量器（无 GL / 无窗口依赖，可用于无绘制的环境）。
//
// 用法：
//   loppe::FreetypeTextMeasurer m;
//   if (m.loadFontFile("C:/Windows/Fonts/arial.ttf")) runtime.setTextMeasurer(&m);
class LOPPE_API FreetypeTextMeasurer : public loppe::ITextMeasurer {
public:
    FreetypeTextMeasurer();
    ~FreetypeTextMeasurer() override;

    FreetypeTextMeasurer(const FreetypeTextMeasurer&) = delete;
    FreetypeTextMeasurer& operator=(const FreetypeTextMeasurer&) = delete;

    // 加载字体文件；失败（freetype 初始化或 face 载入失败）返回 false
    bool loadFontFile(const std::string& fontPath);
    bool valid() const;

    // ---- ITextMeasurer ----
    TextMetrics measureText(const std::u32string& text, const FontDesc& font, float wrapWidth = -1.0f) override;
    TextMetrics measureText(const std::string& utf8text, const FontDesc& font, float wrapWidth = -1.0f) override;
    float getLineHeight(const FontDesc& font) override;
    std::shared_ptr<ITextLayout> createTextLayout(
        const std::string& text, const Font& font,
        const TextLayoutParams& params, float maxWidth = -1.0f) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace loppe