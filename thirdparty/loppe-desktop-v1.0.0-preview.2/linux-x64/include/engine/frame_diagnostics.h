/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "loppe_export.h"
#include "engine/document_update_plan.h"   // DocumentUpdateKind / FrameReason

namespace loppe {

class Scene;

// 帧诊断：只服务**桌面 Debug**，不参与绘制（Release 与嵌入式下 checkTree 为空操作）。
// 提供两件事：帧入口名（日志/trace 用）、帧末「控件态 vs 解析节点」比对。
//
// 用法：
//     const char* entry = FrameDiagnostics::passEntryName(plan.kind, reason);
//     ...                                  // 特殊路径可覆盖，如试排成功 → "local-relayout"
//     FrameDiagnostics::checkTree(scene, entry);
//
// 开关（只影响桌面 Debug）：
//     LOPPE_UI_SELFCHECK=0       关闭
//     LOPPE_UI_SELFCHECK=bounds  同时比对边界盒（默认只比对可见性，噪音最小）
//
// 报告方式：发现「持续存在」的不一致（同一处连续 3 次自检都对不上；瞬态不报）→
// LOPPE_ERROR + 追加写资源根下的 `ui-selfcheck.log`。每个条目最多报 3 次、汇总行最多 20 次。
class LOPPE_API FrameDiagnostics {
public:
    // (kind, reason) → 入口名："full" / "layout-only" / "partial" / "paint-only" / "resize" 等。
    // 只用于日志定位；调用方可再覆盖成更具体的路径名。
    static const char* passEntryName(DocumentUpdateKind kind, FrameReason reason);

    // 遍历控件树（可见性、边界盒）+ 解析树（帧末残留 Style 脏）。
    // where：帧入口名（passEntryName 或调用方的覆盖值），只用于日志定位。
    static void checkTree(Scene* scene, const char* where);
};

} // namespace loppe
