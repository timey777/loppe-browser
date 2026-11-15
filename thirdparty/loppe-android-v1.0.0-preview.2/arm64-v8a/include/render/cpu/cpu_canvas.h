/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

// CPU 后端的共享画布：BGRA32（预乘 alpha）内存位图 + plutovg 画布；几何与文字绘制共用其中的裁剪/偏移状态。
// plutovg 类型仅以不透明指针出现。
#include <utility>
#include <vector>

#include "render/graphics_types.h"

typedef struct plutovg_surface plutovg_surface_t;
typedef struct plutovg_canvas plutovg_canvas_t;

namespace loppe {
namespace cpu {

// 与 GL 后端同名的绘制状态（语义照 opengl_drawer_impl.cpp）：
// clip 是「已含 offset 的 DIP 坐标」，offsetX/Y 是 pushTransform 累加出来的平移。
struct PaintState {
    bool hasClip = false;
    render::Rect clip{};
    float clipRadius = 0.0f;
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    std::vector<std::pair<float, float>> transformStack;
};

struct Canvas {
    plutovg_surface_t* surface = nullptr;
    plutovg_canvas_t* canvas = nullptr;
    int width = 0;         // 设备像素
    int height = 0;
    float scaleX = 1.0f;   // DIP → 设备像素
    float scaleY = 1.0f;
    PaintState paint;

    bool valid() const;
    unsigned char* pixels() const;
    int stride() const;
};

} // namespace cpu
} // namespace loppe
