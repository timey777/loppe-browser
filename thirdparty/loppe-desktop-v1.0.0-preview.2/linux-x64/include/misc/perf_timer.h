/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>

// 性能日志经 Logger 打到 PERF 分类（控制台），受 setLogCategory / setLogLevel 过滤。
// 默认分类 = USER，性能日志默认不显示；需要时：
//   Logger::setLogCategory(LogCategory::ALL) 或环境变量 LOPPE_LOG_CATEGORY=perf
//
// Quick reference:
//
//   PERF_SCOPE("pipeline");                  // RAII: prints "[perf]   pipeline: 17.8 ms"
//   PERF_SCOPE_MSG("resize", "%.0fx%.0f", w, h);  // RAII with extra info
//
//   PerfWatch w("sync");
//   w.split("buildMap");                     // auto-formatted split
//   w.split("updateCtrls", "%d ctls", n);    // split with extra context
//   w.splitCounted("updateCtrls", 5000);     // split + auto avg
//
//   PerfCounter pc;
//   for (...) { PerfCounter::Tick t(pc); doWork(); }
//   pc.report("updateControl");              // calls / total / avg
//
//   loppe::PerfLog::setEnabled(false);         // global off
//   loppe::PerfLog::setMinMs(1.0);             // only report >1ms

namespace loppe {

// 全局性能日志开关。默认：Debug 构建开启，Release 构建关闭。
class PerfLog {
public:
    static bool& enabled();
    static double& minMs();

    static void setEnabled(bool e);
    static void setMinMs(double ms);

    // PERF_SCOPE 嵌套时的缩进深度（线程本地）。
    static int& depth();
};

// 格式化并经 Logger 输出到 PERF 分类（受 setLogCategory 过滤，纯控制台）。
void perfLogFmt(const char* fmt, ...);

// RAII 作用域计时器（经 PERF_SCOPE 宏使用）。输出按嵌套深度缩进：
//   [perf]   pipeline: 17.8 ms
class PerfScope {
public:
    explicit PerfScope(const char* name);
    ~PerfScope();

private:
    using Clock = std::chrono::steady_clock;
    double elapsed() const;

    const char* name_;
    Clock::time_point start_{};
};

// 手动多段计时器，输出可组合：sync=10.30ms | updateCtrls=1.20ms | total=11.52 ms
class PerfWatch {
public:
    explicit PerfWatch(const char* name);
    ~PerfWatch();

    // 记录一个分段点，输出形如 label=12.34ms。
    void split(const char* label);

    // 记录分段点并附 printf 风格上下文：
    //   w.split("syncCtrls", "%d updated, %d skipped", n, s);
    //   输出：syncCtrls=10.30ms (5002 updated, 0 skipped)
    void split(const char* label, const char* extraFmt, ...);

    // 记录分段点并自动附加数量与单件平均耗时（count<=0 时不记录）：
    //   w.splitCounted("updateCtrls", 5000);
    //   输出：updateCtrls=10.30ms (5000 items, 2.06us avg)
    void splitCounted(const char* label, int count, const char* itemName = "items");

    // 覆盖最终输出行使用的名字。
    void setName(const char* n);

private:
    using Clock = std::chrono::steady_clock;
    double elapsedFrom(Clock::time_point t) const;
    void append(const char* label, double ms, const char* extra = nullptr);

    const char* name_;
    Clock::time_point start_{};
    Clock::time_point last_{};
    std::string buf_;
};

} // namespace loppe

// 独立的调用次数 + 总耗时统计器。
namespace loppe {

class PerfCounter {
public:
    using Clock = std::chrono::steady_clock;

    // RAII tick：构造时开始计时，析构时自动累计。
    class Tick {
    public:
        explicit Tick(PerfCounter& c);
        ~Tick();
        Tick(const Tick&) = delete;
        Tick& operator=(const Tick&) = delete;
    private:
        PerfCounter* c_;
        Clock::time_point t0_{};
    };

    // 输出累计统计并清零：
    //   [perf] name: 5000 calls, 10.32ms total, 2.06us avg
    void report(const char* name, const char* itemName = "calls");

    void reset();
    int count() const;

private:
    void add(double us);
    int count_ = 0;
    double totalUs_ = 0.0;
};

} // namespace loppe

// 线程安全的聚合计数器：只累计不打印，经 snapshot() 读取。
namespace loppe {

class PerfProbe {
public:
    using Clock = std::chrono::steady_clock;

    struct Snapshot {
        uint64_t calls  = 0;   // 采样次数（如 reMeasureChildren 调用次数）
        uint64_t visits = 0;   // 块内访问量（如被重测覆盖的子树节点数）
        double   ms     = 0.0; // 累计耗时(ms)
    };

    void setEnabled(bool e);
    bool track() const;

    void reset();

    // 读取当前聚合值（不打印、不清零）。
    Snapshot snapshot() const;

    // RAII 采样：构造时记起点并 +1 调用数，析构时累加访问量与耗时。
    class Sample {
    public:
        explicit Sample(PerfProbe& p);
        ~Sample();
        // 补充本采样块的访问量（子树规模），析构时一并累加。
        void setVisits(uint64_t v);
        Sample(const Sample&) = delete;
        Sample& operator=(const Sample&) = delete;
    private:
        PerfProbe* p_;
        uint64_t visits_ = 0;
        bool active_ = false;
        Clock::time_point t0_{};
    };

private:
    std::atomic<bool>     track_{false};
    std::atomic<uint64_t> calls_{0};
    std::atomic<uint64_t> visits_{0};
    std::atomic<uint64_t> us_{0};
};

} // namespace loppe

// RAII 作用域计时器，输出按嵌套深度缩进。
// Usage:  { PERF_SCOPE("pipeline"); ... }
#define PERF_SCOPE(name) \
    loppe::PerfScope _perf_scope_##__LINE__(name)

// 条件 printf。经 Logger 输出到 PERF 分类（控制台），受分类/级别过滤。
// PerfLog::enabled() == false 时为 no-op。
#define PERF_LOG(fmt, ...) \
    do { if (loppe::PerfLog::enabled()) loppe::perfLogFmt(fmt, ##__VA_ARGS__); } while(0)
