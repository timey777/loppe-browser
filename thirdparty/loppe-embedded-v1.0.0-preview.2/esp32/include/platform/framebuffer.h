/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once
// framebuffer.h — 平台无关的 RGB565 帧缓冲抽象。
// 所有嵌入式 target（esp32/stm32）共享同一份软件渲染代码，只替换底层像素搬运。
#include <cstdint>
#include <cstddef>

namespace loppe_embedded {

// RGB565 帧缓冲（大端可切换由具体 LCD 驱动处理）
struct Framebuffer {
    uint16_t* pixels = nullptr;  // RGB565
    int width = 0;
    int height = 0;
    size_t stride = 0;           // 每行像素数（可能大于 width，用于行对齐）

    void init(uint16_t* buf, int w, int h, size_t strideInPixels = 0) {
        pixels = buf;
        width = w;
        height = h;
        stride = strideInPixels > 0 ? strideInPixels : static_cast<size_t>(w);
    }

    inline uint16_t* row(int y) { return pixels + static_cast<size_t>(y) * stride; }
    inline const uint16_t* row(int y) const { return pixels + static_cast<size_t>(y) * stride; }

    // 将 0..255 RGB 转换为 RGB565
    static inline uint16_t rgb888To565(int r, int g, int b) {
        return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    }
};

} // namespace loppe_embedded