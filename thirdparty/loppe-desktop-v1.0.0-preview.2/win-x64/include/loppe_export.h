/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

// loppe_export.h
#pragma once

// =========================================================================
// 编译期调试日志开关 LOPPE_ENABLE_DEBUG_LOG
//
//         1 —— 调试日志（LOPPE_DEBUG / LOPPE_TRACE / LOPPE_*_SYS 调试级，
//                以及 PERF_* 性能日志默认开启）编译进代码；
//         0 —— 调试日志编译为 no-op（零运行时开销、零噪音），
//                适用于 Release/生产构建。
// 默认值：Debug（未定义 NDEBUG）= 1；Release（定义 NDEBUG）= 0。
// 覆盖：构建时加 -DLOPPE_ENABLE_DEBUG_LOG=0/1，或 CMake 选项 LOPPE_ENABLE_DEBUG_LOG。
// =========================================================================
#ifndef LOPPE_ENABLE_DEBUG_LOG
#  ifdef NDEBUG
#    define LOPPE_ENABLE_DEBUG_LOG 0
#  else
#    define LOPPE_ENABLE_DEBUG_LOG 1
#  endif
#endif

#if defined(_WIN32) || defined(_WIN64)
  #if defined(LOPPE_EXPORTS)
    #define LOPPE_API __declspec(dllexport)
  #elif defined(LOPPE_STATIC)
    #define LOPPE_API
  #else
    #define LOPPE_API __declspec(dllimport)
  #endif
#else
  #if defined(__GNUC__) && __GNUC__ >= 4
    #define LOPPE_API __attribute__((visibility("default")))
  #else
    #define LOPPE_API
  #endif
#endif