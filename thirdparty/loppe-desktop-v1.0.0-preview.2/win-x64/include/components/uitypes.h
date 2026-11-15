/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include <string>
#include "loppe_export.h"

namespace loppe
{
    namespace ui
    {
        // 颜色结构
        struct LOPPE_API Color
        {
            float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;

            Color() = default;
            Color(float r_, float g_, float b_, float a_ = 1.0f);

            // 从十六进制字符串解析 (#RRGGBB 或 #RRGGBBAA)
            static Color fromHex(const std::string &hex);
        };

        // 矩形区域
        struct LOPPE_API Rect
        {
            float x = 0.0f, y = 0.0f, width = 0.0f, height = 0.0f;

            Rect() = default;
            Rect(float x_, float y_, float w_, float h_);

            // 右边界（x + width）与下边界（y + height）
            float right() const;
            float bottom() const;

            // 面积为 0 或负（未设置/无效）
            bool isEmpty() const;

            // 是否与另一矩形重叠
            bool intersects(const Rect& other) const;

            // 两矩形的交集（无重叠返回空矩形）
            Rect intersection(const Rect& other) const;
        };

        struct LOPPE_API Edges
        {
            float top = 0.0f, right = 0.0f, bottom = 0.0f, left = 0.0f;

            Edges() = default;
            Edges(float all);
            Edges(float vertical, float horizontal);
            Edges(float t, float r, float b, float l);

            float &operator[](size_t i);
            const float &operator[](size_t i) const;

            // 辅助工具：快速计算水平和垂直的总和
            float horizontal() const;
            float vertical() const;

            bool any() const;
        };
    }
}