/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

// 简化版 CSS Grid 布局。
//
// 支持范围（grid-template-columns/rows）：px / % / fr / auto；gap（行列间距，单值）；
// auto-placement（项按文档顺序 行优先 填入 cell）；stretch（auto 尺寸的项默认拉伸填满 cell）。
#ifndef GRID_LAYOUT_H
#define GRID_LAYOUT_H

#include "engine/pipeline/box_model_engine.h"
#include <string>
#include <vector>

namespace loppe {
    class ResolvedNode;
}

class GridLayout {
public:
    enum class TrackUnit { Px, Percent, Fr, Auto };
    struct GridTrack {
        TrackUnit unit = TrackUnit::Auto;
        float value = 0.0f;
    };

    static void measureGrid(BoxModelEngine* engine, loppe::ResolvedNode* node,
                            float availableForChildrenW, float availableForChildrenH,
                            float requestedW, float requestedH);

    // 解析轨道最终尺寸，stretch 定子项尺寸，再递归。
    static void resolveGrid(BoxModelEngine* engine, loppe::ResolvedNode* node);

    // 只定位子项并递归。
    static void positionGrid(BoxModelEngine* engine, loppe::ResolvedNode* node,
                             float contentAbsoluteX, float contentAbsoluteY);

    // 轨道工具
    static std::vector<GridTrack> parseTracks(const std::string& s);

    // 把轨道模板解析成最终尺寸。
    //   containerContent : 容器内容尺寸（definite）；<0 表示求内在尺寸（fr/% 无法解析）
    //   contentPerTrack  : 每个轨道的内容尺寸（auto 轨道用它；缺失时为 0）
    static std::vector<float> resolveTrackSizes(
        const std::vector<GridTrack>& tracks,
        float containerContent,
        const std::vector<float>& contentPerTrack,
        float gap);
};

#endif // GRID_LAYOUT_H