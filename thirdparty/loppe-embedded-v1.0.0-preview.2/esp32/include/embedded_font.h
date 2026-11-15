/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "platform/idrawer_soft.h"

namespace loppe_embedded {

// Built-in subset used by the ESP32 example. Unsupported characters fall back
// to the ASCII question-mark glyph.
extern const BitmapFont kEmbeddedFont;

} // namespace loppe_embedded