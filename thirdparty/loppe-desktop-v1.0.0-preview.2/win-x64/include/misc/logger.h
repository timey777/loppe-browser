/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once
#include <cstddef>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include "loppe_export.h"

// 包含本头会 #undef Windows 的 ERROR 宏（取错误码请用 GetLastError()）；日志错误级别名为 ERR。
#ifdef ERROR
#undef ERROR
#endif

namespace loppe {

    // 日志分类（位掩码，可组合过滤）。默认只显示 USER。
    enum class LogCategory : int {
        USER   = 1 << 0,  // 用户/业务日志（默认，排查问题只看这一类）
        SYSTEM = 1 << 1,  // 引擎/诊断日志
        BENCH  = 1 << 2,  // 基准测试日志（默认隐藏，需要时开启）
        PERF   = 1 << 3,  // 性能测试日志（默认隐藏，需要时开启）
        LAYOUT = 1 << 4,  // 布局引擎逐帧 trace（脏/测量/重测/缓存命中决策）。默认隐藏；
                          // 开启方式：LOPPE_LOG_CATEGORY=...,layout 或 LOPPE_LAYOUT_TRACE=1。
        ALL    = USER | SYSTEM | BENCH | PERF | LAYOUT
    };

    // 组合分类：Logger::setLogCategory(LogCategory::USER | LogCategory::PERF)
    constexpr inline LogCategory operator|(LogCategory a, LogCategory b) {
        return static_cast<LogCategory>(static_cast<int>(a) | static_cast<int>(b));
    }

    // 日志级别枚举（数值越大越严重）
    enum class LogLevel {
        TRACE = 0,
        DEBUG = 1,
        INFO  = 2,
        WARN  = 3,
        ERR   = 4,
        FATAL = 5
    };

    // 颜色枚举
    enum class LogColor {
        DEFAULT,
        RED,
        GREEN,
        YELLOW
    };

    struct LOPPE_API LogMessage {
        LogCategory category = LogCategory::USER;
        LogLevel level = LogLevel::INFO;
        LogColor color = LogColor::DEFAULT;
        std::string text;
        bool isError = false;
    };

    class LOPPE_API ILogSink {
    public:
        virtual ~ILogSink() = default;
        virtual void onLogMessage(const LogMessage& message) = 0;
    };

    class LOPPE_API LogSinkSubscription {
    public:
        LogSinkSubscription() = default;
        ~LogSinkSubscription();

        LogSinkSubscription(const LogSinkSubscription&) = delete;
        LogSinkSubscription& operator=(const LogSinkSubscription&) = delete;

        LogSinkSubscription(LogSinkSubscription&& other) noexcept;
        LogSinkSubscription& operator=(LogSinkSubscription&& other) noexcept;

        bool valid() const;
        void reset();

    private:
        explicit LogSinkSubscription(size_t sinkId);

        size_t sinkId_ = 0;

        friend class Logger;
    };

    class LOPPE_API ILogger {
    public:
        virtual ~ILogger() = default;
        virtual void write(LogCategory category, LogLevel level, LogColor color, const std::string& msg, std::ostream& os = std::cout) = 0;
        virtual LogSinkSubscription subscribe(ILogSink& sink) = 0;
    };

    LOPPE_API ILogger& getLogger();

    class LOPPE_API Logger {
    private:
        // 设置颜色
        static void setColor(LogColor color, std::ostream& os);

        // reset color
        static void resetColor(std::ostream& os);

        static void writeLine(LogCategory category, LogLevel level, LogColor color, const std::string& msg, std::ostream& os);

    public:
        // 启用/禁用日志（master switch）
        static bool& enabled();

        static void setEnabled(bool e);

        // 日志级别过滤
        static LogLevel& logLevel();

        static void setLogLevel(LogLevel level);

        static bool shouldLog(LogLevel level);

        // 日志分类过滤（默认 ALL，显示所有分类）
        static LogCategory& logCategory();

        static void setLogCategory(LogCategory category);

        static bool shouldLog(LogCategory category, LogLevel level);

        // 写入日志（带分类和级别）
        static void write(LogCategory category, LogLevel level, LogColor color, const std::string& msg, std::ostream& os = std::cout);

        static LogSinkSubscription subscribe(ILogSink& sink);

        static void unsubscribe(size_t sinkId);

        // 带颜色的输出
        template<typename T>
        static void logWithColor(LogColor color, const T& msg, std::ostream& os = std::cout) {
            if (&os == &std::cout && !enabled()) return;

            std::ostringstream oss;
            oss << msg;
            write(LogCategory::USER, LogLevel::INFO, color, oss.str(), os);
        }
    };

    // 便捷函数
    LOPPE_API const char* logLevelToString(LogLevel level);
    LOPPE_API const char* logCategoryToString(LogCategory category);

} // namespace loppe

// =========================================================================
// 日志宏（loppe_debug）—— 分类 + 级别的控制台日志
//   · 由 Logger::setEnabled / setLogLevel / setLogCategory 过滤
//   · 默认只显示 USER（用户/业务）分类，排查问题时不会被基准/性能日志打扰
//
// 调试日志编译开关（LOPPE_ENABLE_DEBUG_LOG）：
//   · TRACE/DEBUG 级日志（LOPPE_TRACE / LOPPE_DEBUG / LOPPE_*_SYS）：Debug 构建生效，
//     Release 构建编译为 no-op（零开销）。
//   · INFO/WARN/ERROR 等业务日志始终编译，运行时按级别过滤。
//
// 显示哪些日志（配置）：
//   · 代码：Logger::setLogCategory(LogCategory::USER) 只看用户日志；
//           Logger::setLogCategory(LogCategory::ALL) 显示所有（含基准/性能）。
//   · 环境变量：LOPPE_LOG_CATEGORY=user,system,bench,perf（或 all）
//               LOPPE_LOG_LEVEL=trace|debug|info|warn|error|fatal
// =========================================================================
#define LOPPE_LOG_IMPL(cat, lvl, color, os, expr) \
    do { \
        if (loppe::Logger::shouldLog(loppe::LogCategory::cat, loppe::LogLevel::lvl)) { \
            std::ostringstream _loppe_oss; \
            _loppe_oss << expr; \
            loppe::getLogger().write(loppe::LogCategory::cat, loppe::LogLevel::lvl, loppe::LogColor::color, _loppe_oss.str(), os); \
        } \
    } while(0)

// 调试日志 no-op（LOPPE_ENABLE_DEBUG_LOG=0 时）：表达式保留但不执行。
#define LOPPE_DEBUGLOG_OFF(x) \
    do { if (false) { std::ostringstream _loppe_off; _loppe_off << x; } } while(0)

// ── 调试级日志（受编译开关控制，Release 编译掉）──
#if LOPPE_ENABLE_DEBUG_LOG
#  define LOPPE_TRACE(x)   LOPPE_LOG_IMPL(USER,   TRACE, DEFAULT, std::cout, x)
#  define LOPPE_DEBUG(x)   LOPPE_LOG_IMPL(USER,   DEBUG, DEFAULT, std::cout, x)
#  define LOPPE_TRACE_SYS(x) LOPPE_LOG_IMPL(SYSTEM, TRACE, DEFAULT, std::cout, x)
#  define LOPPE_DEBUG_SYS(x) LOPPE_LOG_IMPL(SYSTEM, DEBUG, DEFAULT, std::cout, x)
#else
#  define LOPPE_TRACE(x)   LOPPE_DEBUGLOG_OFF(x)
#  define LOPPE_DEBUG(x)   LOPPE_DEBUGLOG_OFF(x)
#  define LOPPE_TRACE_SYS(x) LOPPE_DEBUGLOG_OFF(x)
#  define LOPPE_DEBUG_SYS(x) LOPPE_DEBUGLOG_OFF(x)
#endif

// ── 用户/业务日志（USER 分类，始终编译，运行时按级别过滤）──
#define LOPPE_LOG(x)     LOPPE_LOG_IMPL(USER,   INFO,  DEFAULT, std::cout, x)  // 通用业务日志
#define LOPPE_INFO(x)    LOPPE_LOG_IMPL(USER,   INFO,  DEFAULT, std::cout, x)
#define LOPPE_WARN(x)    LOPPE_LOG_IMPL(USER,   WARN,  YELLOW,  std::cout, x)
#define LOPPE_ERROR(x)   LOPPE_LOG_IMPL(USER,   ERR,   RED,     std::cerr, x)
#define LOPPE_SUCCESS(x) LOPPE_LOG_IMPL(USER,   INFO,  GREEN,   std::cout, x)
#define LOPPE_FATAL(x)   LOPPE_LOG_IMPL(USER,   FATAL, RED,     std::cerr, x)

// ── 布局引擎逐帧 trace（LAYOUT 分类，始终编译，运行时按分类门控）──
// 默认关闭；开启：LOPPE_LOG_CATEGORY=...,layout 或 LOPPE_LAYOUT_TRACE=1。
#define LOPPE_LAYOUT_TRACE(x) LOPPE_LOG_IMPL(LAYOUT, TRACE, DEFAULT, std::cout, x)

// ── 引擎诊断日志（SYSTEM 分类，INFO 级始终编译）──
#define LOPPE_INFO_SYS(x)  LOPPE_LOG_IMPL(SYSTEM, INFO,  DEFAULT, std::cout, x)

// ── 基准/性能日志（BENCH/PERF 分类，纯控制台日志；默认隐藏，需要时开启）──
#define LOPPE_BENCH(x)   LOPPE_LOG_IMPL(BENCH,  INFO, DEFAULT, std::cout, x)
#define LOPPE_PERF(x)    LOPPE_LOG_IMPL(PERF,   INFO, DEFAULT, std::cout, x)