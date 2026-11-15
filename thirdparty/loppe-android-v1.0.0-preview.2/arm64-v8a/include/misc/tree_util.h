/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "loppe_export.h"

namespace loppe {

class ResolvedNode;

// 树结构通用查询工具。
class LOPPE_API TreeUtil {
public:
    // node 是否位于 ancestor 子树内（含自身；任一为空 → false）
    static bool isWithin(const ResolvedNode* node, const ResolvedNode* ancestor);
};

} // namespace loppe
