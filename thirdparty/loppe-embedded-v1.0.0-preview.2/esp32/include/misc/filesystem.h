/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <string>

// 资源路径解析：结果 = 根 + "/" + path。
//
// 根（优先级）：
//   1) setRoot() 的运行时覆盖（启动阶段调一次；之后只读）
//   2) 默认 = 当前 exe 所在目录（资源放 exe 旁边 <exe目录>/resources/…；
//      无「当前 exe」概念的平台（嵌入式）→ 根为空）
class FileSystem
{
public:
  static std::string getPath(const std::string& path);

  // 运行时覆盖根："" = 不拼前缀（path 按 cwd 解析）。启动阶段调一次，之后只读（多线程读安全）。
  static void setRoot(const std::string& root);
  // 撤销覆盖，回到默认（当前 exe 所在目录）。
  static void resetRoot();

private:
  static std::string const& getRoot();
};

// FILESYSTEM_H
#endif