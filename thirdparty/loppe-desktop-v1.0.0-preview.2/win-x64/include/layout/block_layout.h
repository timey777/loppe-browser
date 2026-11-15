/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include <memory>
namespace loppe { class ResolvedNode; }
class BoxModelEngine;

class BlockLayout {
public:
    // measure for block content (vertical stacking, text wrapping for leaf text)
        static void measureBlock(BoxModelEngine* engine, loppe::ResolvedNode* node,
                             float availableForChildrenW, float availableForChildrenH,
                             float requestedW, float requestedH);

    // layout for block content (vertical stacking)
        static void layoutBlock(BoxModelEngine* engine, loppe::ResolvedNode* node,
                            float contentAbsoluteX, float contentAbsoluteY);
};