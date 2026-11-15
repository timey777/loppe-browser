/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

// Flex 布局的测量与排布逻辑。
#ifndef FLEX_LAYOUT_H
#define FLEX_LAYOUT_H

#include "engine/pipeline/box_model_engine.h"
#include <memory>
#include <vector>
#include <string>
#include <engine/core/resolved_tree.h>
namespace loppe {
    class ResolvedNode;

    // 一条 flex line 的数据（flex-wrap 换行分段后的子项索引 + 主/交叉尺寸）。
    struct FlexLine {
        std::vector<size_t> childIndices;
        float mainSize = 0.0f;
        float crossSize = 0.0f;
    };
}
class FlexLayout {
public:
    static void measureFlex(BoxModelEngine* engine, loppe::ResolvedNode* node,
                             float availableForChildrenW, float availableForChildrenH,
                             float requestedW, float requestedH);

    // 解析子项最终尺寸（grow/shrink/stretch + 交叉轴压缩），不定位。
    static void resolveFlex(BoxModelEngine* engine, loppe::ResolvedNode* node);

    // 定位子项并递归。
    static void positionFlex(BoxModelEngine* engine, loppe::ResolvedNode* node,
                             float contentAbsoluteX, float contentAbsoluteY);

    static bool isRowDirection(loppe::FlexDirection flexDirection);
    static bool isReverseDirection(loppe::FlexDirection flexDirection);
    static float getMainSize(const loppe::ResolvedNode* node, bool isRow);
    static float getCrossSize(const loppe::ResolvedNode* node, bool isRow);
    static float getMarginMain(const loppe::ResolvedNode* node, bool isRow);
    static float getMarginCross(const loppe::ResolvedNode* node, bool isRow);
    static void collectFlexLines(loppe::ResolvedNode* node,
                                 float availableMain,
                                 bool isRow,
                                 bool isWrap,
                                 float gap,
                                 std::vector<loppe::FlexLine>& outLines);
};

#endif // FLEX_LAYOUT_H