/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include <vector>
#include <string>
#include "render/graphics_types.h"

/**
 * @brief 文本布局接口：提供排版几何查询（总边界、字符索引 ↔ 光标 / 选区矩形、命中）。
 */
class ITextLayout {
public:
    virtual ~ITextLayout() = default;

    /**
     * @brief 获取文本布局的总边界（相对于绘制原点 (0,0)）
     */
    virtual render::Rect getBounds() const = 0;

    /**
     * @brief 根据局部坐标 (x, y) 获取对应的字符索引
     * 
     * @param x 局部 x 坐标
     * @param y 局部 y 坐标
     * @return int 字符索引 (0 到 text.length())。
     *         如果点击在字符左半部分，返回该字符索引；右半部分则返回下一个索引。
     *         用于鼠标点击确定光标位置。
     */
    virtual int getCharIndexAt(float x, float y) const = 0;

    /**
     * @brief 获取指定字符索引处的光标位置信息
     * 
     * @param index 字符索引
     * @return Rect 光标的几何信息 (x, y, width, height)。
     *         通常 width 为 1-2 像素，height 为行高。
     */
    virtual render::Rect getCursorRect(int index) const = 0;

    /**
     * @brief 获取指定范围内的选区矩形列表
     * 
     * @param startIndex 起始索引
     * @param endIndex 结束索引
     * @return std::vector<Rect> 选区矩形列表。
     *         如果是多行文本，选区可能由多个矩形组成。
     */
    virtual std::vector<render::Rect> getSelectionRects(int startIndex, int endIndex) const = 0;

    /**
     * @brief 获取原始文本内容
     */
    virtual std::string getText() const = 0;
    
    /**
     * @brief 获取行数
     */
    virtual int getLineCount() const = 0;

    /**
     * @brief 增量改宽：同文本/同字体/同参数、仅 maxWidth 变化时调用。
     * @return true 表示已增量处理（调用方无需重建布局）；
     *         false 表示不支持增量（调用方应回退 createTextLayout 全量重建）。
     *         默认实现返回 false。
     */
    virtual bool updateMaxWidth(float /*maxWidth*/);

    /**
     * @brief 增量内容编辑：把 [startCp, startCp+delLenCp) 的文本替换为 insertedUtf8
     *        （startCp/delLenCp 为整篇全局 cp，含 '\n'）；调用前应先把文本编辑落定。
     * @return true 表示已增量处理（调用方无需全量重建）；false 表示不支持增量
     *        （调用方应回退 createTextLayout 全量重建）。默认实现返回 false。
     */
    virtual bool applyEdit(int /*startCp*/, int /*delLenCp*/,
                           const std::string& /*insertedUtf8*/);

    /**
     * @brief 获取某行的信息（可选，用于更高级的导航）
     */
    // virtual LineMetrics getLineMetrics(int lineIndex) const = 0;
};