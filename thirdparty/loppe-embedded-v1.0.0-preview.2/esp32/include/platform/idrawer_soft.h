/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

// 纯软件 IDrawer + ITextRenderer（RGB565 帧缓冲）。
#include "render/drawer.h"
#include "render/text_measurer.h"
#include "render/text_renderer.h"
#include "framebuffer.h"
#include <vector>
#include <memory>
#include <cstdint>

namespace loppe_embedded {

struct BitmapGlyph {
    const uint8_t* bitmap = nullptr;
    int width = 0;
    int height = 0;
    int rowBytes = 0;
    int bearingX = 0;
    int bearingY = 0;
    int advance = 0;
    int bitsPerPixel = 1;
};

// 位图字体描述
struct BitmapFont {
    const char* name = "embedded";
    int size = 8;             // 默认字号（像素）
    int glyphW = 8;           // 字形宽（像素）
    int glyphH = 8;           // 字形高（像素）
    int lineHeight = 10;      // 行高（像素）
    int baseline = 8;         // baseline 距 cell 顶部（像素）
    int bytesPerRow = 1;      // 每行字节数
    int bitsPerPixel = 1;     // 1 = 单色，4 = 打包灰度
    // 字形查找：返回 glyphH 行、每行 glyphW 位（bit=1 为前景）；nullptr = 未命中
    const uint8_t* (*lookup)(char32_t ch) = nullptr;
    // 可选：字形宽度回调（变宽字体），默认 glyphW
    int (*advance)(char32_t ch) = nullptr;
    // 可选：按像素字号返回紧凑字形与度量
    bool (*lookupSized)(char32_t ch, int pixelSize, BitmapGlyph* out) = nullptr;
    int (*lineHeightSized)(int pixelSize) = nullptr;
};

class SoftDrawer : public IDrawer, public loppe::ITextRenderer {
public:
    explicit SoftDrawer(const BitmapFont* font = nullptr);
    ~SoftDrawer() override;

    void setFramebuffer(Framebuffer* fb) { fb_ = fb; }
    Framebuffer* framebuffer() const { return fb_; }
    void clear(uint16_t rgb565);
    // 清一块：设备像素，忽略裁剪与变换
    void clearRect(const render::Rect& r, uint16_t rgb565);

    // 默认字体（测量 / 绘制用），可运行时切换
    void setFont(const BitmapFont* font) { font_ = font; }

    // ---- ITextMeasurer ----
    TextMetrics measureText(const std::u32string& text, const FontDesc& font, float wrapWidth = -1.0f) override;
    TextMetrics measureText(const std::string& utf8text, const FontDesc& font, float wrapWidth = -1.0f) override;
    float getLineHeight(const FontDesc& font) override;

    // ---- IDrawer ----
    void drawRect(float x, float y, float w, float h, const DrawStyle& style = DrawStyle()) override;
    void drawRoundedRect(float x, float y, float w, float h, float rx, float ry, const DrawStyle& style = DrawStyle()) override;
    void drawGaussianShadow(float x, float y, float w, float h, float cornerRadius,
                            float sigma, const DrawStyle& style = DrawStyle()) override;
    void drawImage(ImageHandle img, float x, float y, float w, float h, const DrawStyle& style = DrawStyle()) override;
    void drawText(const std::string& text, float x, float y, const Font& font, const DrawStyle& style = DrawStyle(),
                  float maxWidth = -1.0f, const TextLayoutParams& layout = TextLayoutParams()) override;
    std::shared_ptr<ITextLayout> createTextLayout(const std::string& text, const Font& font,
                                                  const TextLayoutParams& params, float maxWidth = -1.0f) override;
    void drawTextLayout(const std::shared_ptr<ITextLayout>& layout, float x, float y, const DrawStyle& style) override;
    void drawTextLayoutRange(const std::shared_ptr<ITextLayout>& layout, float x, float y, const DrawStyle& style, int firstLine, int lineCount) override;
    ImageHandle createImageFromMemory(const unsigned char* data, int width, int height, int channels) override;
    void unloadImage(ImageHandle img) override;

    // 裁剪
    void setClipRect(const render::Rect& r) override;
    bool intersectClipRect(const render::Rect& r, render::Rect* outNewClip) override;
    bool getClipRect(render::Rect* outCurrent) const override;
    void resetClip() override;
    void setClipCornerRadius(float radius) override;
    float getClipCornerRadius() const override;

    // 变换（平移）
    void pushTransform(float dx, float dy) override;
    void popTransform() override;

private:
    struct ImgData { int w, h, channels; std::vector<unsigned char> data; };

    Framebuffer* fb_ = nullptr;
    const BitmapFont* font_ = nullptr;

    bool         hasClip_ = false;
    render::Rect clipRect_{};
    float        clipRadius_ = 0.0f;
    std::vector<std::pair<float, float>> transformStack_;

    int nextImageHandle_ = 1;
    std::vector<ImgData> images_;

    // 内部像素写入（带裁剪与变换）
    bool pixelInClip(int x, int y) const;
    bool clipCoversRow(int y, int x0, int x1) const;
    void fillRect(int x0, int y0, int x1, int y1, uint16_t color, float opacity);
    // 圆角角部：rInner < 0 为实心角；>= 0 只填 rInner < d <= r 的弧带
    void fillCircleCorner(int cx, int cy, int r, int quadrant, uint16_t color, float opacity,
                          int rInner = -1);
    void blitGlyph(int x, int baselineY, char32_t ch, uint16_t color, float opacity, float pixelSize);
    float glyphAdvance(char32_t ch, float pixelSize) const;
};

class SoftTextLayout : public ITextLayout {
public:
    // linePrefix[i][k] = 第 i 行前 k 个码点的绘制宽度，末项 = 整行宽
    SoftTextLayout(const std::string& text, const Font& font, float lineHeight,
                   const std::vector<std::vector<float>>& linePrefix, float maxW,
                   bool wrap, int lineCount, const std::vector<std::u32string>& lines, render::TextAlign align);
    render::Rect getBounds() const override;
    int getCharIndexAt(float x, float y) const override;
    render::Rect getCursorRect(int index) const override;
    std::vector<render::Rect> getSelectionRects(int startIndex, int endIndex) const override;
    std::string getText() const override;
    int getLineCount() const override;
    const Font& font() const { return font_; }
    const std::vector<std::u32string>& lines() const { return lines_; }
    render::TextAlign align() const { return align_; }

private:
    std::string text_;
    Font font_;
    std::vector<std::u32string> lines_;
    std::vector<std::vector<float>> linePrefix_;
    float lineHeight_;
    float width_;
    int lineCount_;
    render::TextAlign align_;
};

} // namespace loppe_embedded
