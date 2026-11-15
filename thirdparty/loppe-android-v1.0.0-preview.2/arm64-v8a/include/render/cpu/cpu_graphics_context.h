/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

// 纯 CPU 渲染上下文：渲染到内存位图并呈现到窗口（Windows 用 GDI）。
// 前置条件：窗口须以 GLFW_NO_API 创建。
#include <memory>

#include "render/graphics_context.h"

class CpuGraphicsContext : public IGraphicsContext {
public:
    CpuGraphicsContext();
    ~CpuGraphicsContext() override;

    void init(int width, int height) override;
    void resize(const loppe::FramebufferSizePx& framebufferSize,
                const loppe::ContentScale& contentScale = {}) override;
    void clear(float r, float g, float b, float a) override;
    void present() override;
    void shutdown() override;

    IDrawer* getDrawer() override;
    loppe::ITextRenderer* getTextRenderer() override;

    void setNativeWindowHandle(void* handle) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
