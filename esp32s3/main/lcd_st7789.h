/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

// lcd_st7789.h — ST7789 SPI LCD 驱动（示例，可替换为你的屏）。
// 提供最小接口：init / flush（把 RGB565 帧缓冲送到屏幕）。
#pragma once

#include "platform/framebuffer.h"

namespace loppe_embedded {

class St7789Lcd {
public:
    // 初始化屏幕（分辨率、SPI 引脚等由具体板卡决定）
    void init(int width, int height);
    // 整屏刷写：把帧缓冲内容送到 LCD
    void flush(const Framebuffer& fb);
    // 可选：区域刷写（局部刷新优化用）
    void flushRegion(const Framebuffer& fb, int x, int y, int w, int h);
};

} // namespace loppe_embedded