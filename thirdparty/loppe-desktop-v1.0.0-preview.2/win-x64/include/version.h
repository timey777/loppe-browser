/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

// version.h — loppe-ui 引擎版本（= 界面 DSL 版本）
//
// 界面 DSL（.lui）里的 `@version(1.1.0)` 就是拿这里的版本做兼容检查：**只比主版本**，
// 相同 → 通过（同主版本下更高的小版本按兼容处理）；不同 → 解析报错。不写 `@version` = 不检查。
#pragma once

#ifndef LOPPE_UI_VERSION
#define LOPPE_UI_VERSION "1.0.0"
#endif

#include <string>

#include "loppe_export.h"

namespace loppe {

// 引擎版本字符串（"x.y.z"；可带 `-后缀`，如 "1.0.0-preview.1"）
LOPPE_API const char* engineVersion();

// 版本号是否形如 x[.y[.z]]（纯数字段，非空），可带后缀（`-` / `+` 起，只含数字/字母/'-'/'.'，如 "1.0.0-preview.1"）
LOPPE_API bool isValidVersion(const std::string& v);

// 版本比较：a < b → -1，a == b → 0，a > b → 1；缺段按 0（"1.0" == "1.0.0"），后缀不参与比较
LOPPE_API int compareVersions(const std::string& a, const std::string& b);

// `@version` 兼容检查：主版本相同 → true；不同 → false，并把原因写进 *err
// （err 形如 "@version(2.0) requires UI DSL major 2, but this engine is 1.0.0"）
LOPPE_API bool checkDslVersion(const std::string& requested, std::string* err);

} // namespace loppe
