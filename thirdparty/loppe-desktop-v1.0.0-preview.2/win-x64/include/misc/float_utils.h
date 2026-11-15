/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include <cstdint>
#include "engine/core/node_flags.h"  // LayoutUnit / AUTO_SENTINEL（parseSize 用）

namespace loppe {

// 带容差的浮点比较工具。
bool floatGreater(float a, float b, float eps = 0.01f);

bool floatGreaterEqual(float a, float b, float eps = 0.01f);

bool floatNotEqual(float a, float b, float eps = 1e-6f);

bool floatEqual(float a, float b, float eps = 1e-6f);

// ------------------------------------------------------------
// 布局引擎共用工具
// ------------------------------------------------------------

// NaN 感知的 clamp：lo/hi 为 NaN（auto / 无界约束）时不做该侧限制。
float clampf(float v, float lo, float hi);

// 解析尺寸单位（"auto"/"100px"/"50%"/纯数值）为像素值；auto 返回 AUTO_SENTINEL。
float parseSize(const LayoutUnit& u, float parentSize, bool parentIsDefinite = true);

// 约束比较：NaN（如 UNBOUNDED）视为相等。
bool sameConstraint(float a, float b);

// 把 widthDefinite/heightDefinite 打包为位掩码（bit0=width，bit1=height）。
uint8_t packDefinite(bool widthDefinite, bool heightDefinite);

} // namespace loppe