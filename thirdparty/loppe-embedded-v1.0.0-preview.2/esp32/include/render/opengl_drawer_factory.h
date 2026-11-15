/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include <memory>

class IGraphicsContext;

std::unique_ptr<IGraphicsContext> createOpenGLGraphicsContext();

// ---- 渲染后端选择 ----
//
// 默认 OpenGL。要用纯 CPU 渲染，在 Application::run() 之前调一次：
//
//   SetRenderBackend(RenderBackend::Cpu);
//
// 典型写法是在 main() 里按构建开关决定（工程 build.lpj 的 `define` 加 LOPPE_RENDER_CPU）：
//
//   #if defined(LOPPE_RENDER_CPU)
//       SetRenderBackend(RenderBackend::Cpu);
//   #endif
enum class RenderBackend {
    OpenGL,   // 默认
    Cpu,      // 纯 CPU，不碰显卡驱动
};

void SetRenderBackend(RenderBackend backend);
RenderBackend GetRenderBackend();

// 当前是否使用 CPU 渲染后端。
bool UseCpuRenderBackend();

// 按当前选择的后端创建上下文
std::unique_ptr<IGraphicsContext> createGraphicsContext();