/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include <string>
#include <cstdint>
#include <memory>
#include "graphics_types.h"
#include "text_layout.h"
#include "loppe_export.h"

class LOPPE_API IDrawer {
public:
    virtual ~IDrawer() = default;

    virtual void drawRect(float x, float y, float w, float h, const DrawStyle& style = DrawStyle()) = 0;
    virtual void drawRoundedRect(float x, float y, float w, float h, float rx, float ry, const DrawStyle& style = DrawStyle()) = 0;
    virtual void drawImage(ImageHandle img, float x, float y, float w, float h, const DrawStyle& style = DrawStyle()) = 0;

    // 外阴影（高斯）：(x,y,w,h) = 阴影形状盒，sigma = blur / 2，阴影色取 style.strokeColor。
    // 默认实现按该形状填一个硬边圆角矩形，不做模糊。
    virtual void drawGaussianShadow(float x, float y, float w, float h, float cornerRadius,
                                    float sigma, const DrawStyle& style = DrawStyle());

    virtual ImageHandle createImageFromMemory(const unsigned char* data, int width, int height, int channels) = 0;
    virtual void unloadImage(ImageHandle img) = 0;

    virtual void setClipRect(const render::Rect& r) = 0;
    virtual bool intersectClipRect(const render::Rect& r, render::Rect* outNewClip) = 0;
    virtual bool getClipRect(render::Rect* outCurrent) const = 0;
    virtual void resetClip() = 0;

    // Set the corner radius for the current clip region.
    // radius > 0 enables rounded-corner clipping; 0 disables it (scissor-only).
    virtual void setClipCornerRadius(float radius) = 0;
    virtual float getClipCornerRadius() const = 0;

    virtual void pushTransform(float dx, float dy) = 0;
    virtual void popTransform() = 0;
};