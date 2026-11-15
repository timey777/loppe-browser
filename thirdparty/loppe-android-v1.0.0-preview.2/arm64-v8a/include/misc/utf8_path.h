/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#ifndef LOPPE_UTF8_PATH_H
#define LOPPE_UTF8_PATH_H

#include <cstdio>
#include <string>

// UTF-8 路径的文件访问小工具：路径按 UTF-8 字节传入，语义与 fopen / 存在性探测一致，不抛异常。

namespace loppe {
namespace utf8path {

// 以 UTF-8 路径打开文件（mode 同 fopen）。失败返回 nullptr。
std::FILE* Open(const std::string& utf8Path, const char* mode);

// 路径是否存在（不抛异常；等价于一次「能不能打开」的探测）。
bool Exists(const std::string& utf8Path);

// 是否是一个**普通文件**（目录、不存在、无法访问 → false）。不抛异常。
bool IsRegularFile(const std::string& utf8Path);

// 整个文件读成文本（二进制读，不做换行转换）。失败或空文件返回 false。
bool ReadText(const std::string& utf8Path, std::string& out);

} // namespace utf8path
} // namespace loppe

#endif // LOPPE_UTF8_PATH_H
