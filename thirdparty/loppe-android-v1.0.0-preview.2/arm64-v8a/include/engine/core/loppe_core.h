/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once
// engine/core/ 目录汇总头（AST、解析、解析后树与几何）。

#include "engine/core/ast_node.h"
#include "engine/core/ast_snippet.h"
#include "engine/core/attribute.h"
#include "engine/core/computed_layout.h"
#include "engine/core/computed_style.h"
#include "engine/core/coordinate_types.h"
#include "engine/core/input_source.h"
#include "engine/core/node_flags.h"
#include "engine/core/parse.h"
#include "engine/core/property_provider.h"
#include "engine/core/property_resolver.h"
#include "engine/core/resolved_builder.h"
#include "engine/core/resolved_node.h"
#include "engine/core/resolved_node_query.h"
#include "engine/core/resolved_rule_scope.h"
#include "engine/core/resolved_tree.h"
#include "engine/core/resolved_tree_view.h"
#include "engine/core/tree_synchronizer.h"

#include "engine/core/diff/resolved_tree_differ.h"