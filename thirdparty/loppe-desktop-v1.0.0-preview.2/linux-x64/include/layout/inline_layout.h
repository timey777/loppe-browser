/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include <memory>
namespace loppe { class ResolvedNode; }
class BoxModelEngine;

class InlineLayout {
public:
    // measure for inline/inline-block content
    static void measureInline(BoxModelEngine* engine, loppe::ResolvedNode* node,
                              float availableForChildrenW, float availableForChildrenH,
                              float requestedW, float requestedH);

    // layout for inline/inline-block content
    static void layoutInline(BoxModelEngine* engine, loppe::ResolvedNode* node,
                             float contentAbsoluteX, float contentAbsoluteY);
};