/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "engine/core/coordinate_types.h"

class IDrawer;

namespace loppe {
class ITextRenderer;
}

class IGraphicsContext {
public:
    virtual ~IGraphicsContext() = default;

    virtual void init(int width, int height) = 0;
    virtual void resize(const loppe::FramebufferSizePx& framebufferSize,
                        const loppe::ContentScale& contentScale = {}) = 0;
    virtual void clear(float r, float g, float b, float a) = 0;
    virtual void present() = 0;
    virtual void shutdown() = 0;

    virtual IDrawer* getDrawer() = 0;

    // 文字引擎：测量/排版/字形绘制
    virtual loppe::ITextRenderer* getTextRenderer() = 0;

    // CPU 后端需在此传入原生窗口句柄（Windows 传 HWND）；其它后端无需调用（默认空实现）。
    virtual void setNativeWindowHandle(void* /*handle*/);
};