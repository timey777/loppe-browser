/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

// touch_xpt2046.h — XPT2046 电阻触摸驱动（SPI）。
// 与 ST7789/ILI9341 共用一条 SPI 总线，只额外占一根片选 T_CS。
#pragma once

#include <cstdint>

namespace loppe_embedded {

// 标定参数：把触摸的 12bit 原始值线性映射成屏幕像素。
// 默认值是一般 240x320 SPI 模块的常见范围，实际以真机串口打印的原始值为准。
struct TouchCalibration {
    uint16_t xMin = 200;   // 手指在屏幕最左边时的原始 X
    uint16_t xMax = 3700;  // 手指在屏幕最右边时的原始 X
    uint16_t yMin = 250;   // 手指在屏幕最上边时的原始 Y
    uint16_t yMax = 3750;  // 手指在屏幕最下边时的原始 Y
    bool swapXY = true;    // 触摸 X 轴是否对应屏幕 Y 轴（竖屏一般要交换）
    bool invertX = false;  // X 轴是否反向
    bool invertY = false;  // Y 轴是否反向
    // 落点整体偏移（像素）：准星/按下的位置相对手指有个固定偏差时用它微调，
    // 不用去改 MIN/MAX（那种偏差本来就不是缩放问题）。
    int offsetX = 0;
    int offsetY = 0;
};

// 最近一次采样的原始数据（诊断用：判断「没摸时读到了什么」，调压力阈值/标定都靠它）
struct TouchRawTrace {
    uint16_t xLo = 0, xHi = 0;   // 本次 X 的 N 个样本的取值范围
    uint16_t yLo = 0, yHi = 0;   // 本次 Y 的 N 个样本的取值范围
    uint16_t z1 = 0, z2 = 0;     // 接触电阻分压（没摸时 z1 小、z2 接近满量程）
    int32_t  z = 0;              // z = z1 + 4095 - z2（≥ 阈值才算「摸上了」）
    uint16_t rawX = 0, rawY = 0; // 取均值后的原始 X/Y
    bool touched = false;        // 本次「有没有摸」的判定结果
};

class Xpt2046Touch {
public:
    // 初始化：把触摸挂到已经初始化过的 SPI 总线上（必须在 St7789Lcd::init 之后调用）
    bool init(int screenWidth, int screenHeight);

    // 读原始 12bit 值（标定用）；返回 false 表示当前没有有效触摸
    bool readRaw(uint16_t& rawX, uint16_t& rawY);

    // 读屏幕坐标（像素）；返回 false 表示未触摸
    bool read(int& x, int& y);

    // 运行时改标定（例如从串口命令下发）
    TouchCalibration& calibration() { return cal_; }

    // 诊断：最近一次采样的原始数据（每帧采样时更新）
    const TouchRawTrace& trace() const { return trace_; }

private:
    TouchCalibration cal_;
    TouchRawTrace trace_;
    int screenW_ = 240;
    int screenH_ = 320;
};

} // namespace loppe_embedded
