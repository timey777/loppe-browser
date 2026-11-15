/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

// CPU 后端的 IDrawer 实现（plutovg）：矩形/圆角/边框/图片/阴影绘制。
#include <cstdint>
#include <vector>

#include "render/drawer.h"

struct plutovg_surface;
typedef struct plutovg_surface plutovg_surface_t;

namespace loppe {
namespace cpu {
struct Canvas;
}
}

class PlutovgDrawer : public IDrawer {
public:
    explicit PlutovgDrawer(loppe::cpu::Canvas* canvas);
    ~PlutovgDrawer() override;

    void drawRect(float x, float y, float w, float h, const DrawStyle& style = DrawStyle()) override;
    void drawRoundedRect(float x, float y, float w, float h, float rx, float ry,
                         const DrawStyle& style = DrawStyle()) override;
    void drawGaussianShadow(float x, float y, float w, float h, float cornerRadius,
                            float sigma, const DrawStyle& style = DrawStyle());
    void drawImage(ImageHandle img, float x, float y, float w, float h,
                   const DrawStyle& style = DrawStyle()) override;

    ImageHandle createImageFromMemory(const unsigned char* data, int width, int height, int channels) override;
    void unloadImage(ImageHandle img) override;

    void setClipRect(const render::Rect& r) override;
    bool intersectClipRect(const render::Rect& r, render::Rect* outNewClip) override;
    bool getClipRect(render::Rect* outCurrent) const override;
    void resetClip() override;

    void setClipCornerRadius(float radius) override;
    float getClipCornerRadius() const override;

    void pushTransform(float dx, float dy) override;
    void popTransform() override;

private:
    // 开始一次绘制；返回 false 表示画布不可用。
    bool beginPaint();
    void endPaint();
    void fillRoundRect(float x, float y, float w, float h, float radius,
                       float r, float g, float b, float a);
    void strokeRoundRect(float x, float y, float w, float h, float radius, float lineWidth,
                         float r, float g, float b, float a);

    loppe::cpu::Canvas* c_;
    std::vector<plutovg_surface_t*> images_;   // 下标 + 1 == ImageHandle
};
