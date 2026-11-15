/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once
// render/ 目录汇总头（显示列表、绘制接口、文本渲染、渲染循环）。

#include "render/display_list.h"
#include "render/drawer.h"
#include "render/freetype_text_measurer.h"
#include "render/glyph_atlas.h"
#include "render/graphics_context.h"
#include "render/graphics_types.h"
#include "render/missing_glyph.h"
#include "render/opengl_drawer_factory.h"
#include "render/renderer.h"
#include "render/render_loop.h"
#include "render/text_layout.h"
#include "render/text_measurer.h"
#include "render/text_renderer.h"
#include "render/text_row_layout.h"

#include "render/svg/svg_registry.h"