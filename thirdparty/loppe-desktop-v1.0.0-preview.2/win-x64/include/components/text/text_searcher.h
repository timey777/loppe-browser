#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace loppe {

// 文本查找状态机：查询串、匹配口径、命中集、当前命中项。
// 命中以 UTF-8 **字节**区间表示，并同时给出对应的**码点**区间；命中允许重叠
// （"ana" 在 "banana" 里算 2 处）。
// 本类不含界面：查询串与匹配口径由调用方写入（setQuery/setMatchCase/setWholeWord），
// 查找/跳转由调用方触发（search/move），结果通过下面三个钩子交回调用方。
class TextSearcher {
public:
    struct Match {
        size_t startByte = 0;
        size_t endByte = 0;
    };
    // 命中的码点区间 [start, end)
    using CodepointRange = std::pair<int, int>;

    // byte 偏移 → 码点偏移（偏移越界按文本长度处理）
    static int codepointOffset(const std::string& text, size_t byteOffset);

    // ── 开关与查询串 ──────────────────────────────────────────────────
    // 查找是否打开；关着时命中集为空
    bool isOpen() const;
    // 打开。命中不在这里算：调用方随后 search()/searchIfOpen()
    void open();
    // 关闭：清空命中（回调 onResultsChanged），查询串保留
    void close();
    const std::string& query() const;
    // 整串替换。**不**自动重算命中：调用方随后 search()/searchIfOpen()
    void setQuery(std::string query);

    // ── 匹配口径 ──────────────────────────────────────────────────────
    // 区分大小写（默认 true）。只折叠 ASCII 大小写，非 ASCII 字节按原样比。
    bool matchCase() const;
    // 设「区分大小写」。**不**自动重算命中：调用方随后 search()/searchIfOpen()
    void setMatchCase(bool on);
    // 全字匹配（默认 false）：命中前后的紧邻字节都不算「词字符」才算一处
    //（词字符 = ASCII 字母数字、下划线、非 ASCII 字节）。
    bool wholeWord() const;
    // 设「全字匹配」。**不**自动重算命中：调用方随后 search()/searchIfOpen()
    void setWholeWord(bool on);

    // ── 调用方挂钩 ────────────────────────────────────────────────────
    // 取当前文本；search() 从这里取文本（未设置时按空文本搜索）
    std::function<std::string()> textSource;
    // 命中集变化后调用（含 0 命中）
    std::function<void()> onResultsChanged;
    // 当前命中项变化后调用；参数是同一段命中的字节区间与码点区间
    std::function<void(size_t startByte, size_t endByte, int startCp, int endCp)> onActiveMatch;

    // ── 搜索 ──────────────────────────────────────────────────────────
    // 搜索 textSource() 的文本；resetActive=false 时保持当前命中项。applyActive=false 时只重算命中，不回调 onActiveMatch（适用于文本变化触发的自动重算）。
    void search(bool resetActive = true, bool applyActive = true);
    // 搜索栏未打开时不动作
    void searchIfOpen(bool resetActive = true, bool applyActive = true);
    // 搜索给定文本（未设置 textSource 时使用）
    void searchIn(const std::string& text, bool resetActive = true, bool applyActive = true);

    // ── 结果 ──────────────────────────────────────────────────────────
    // 命中的字节区间（按命中顺序）
    const std::vector<Match>& matches() const;
    // 命中的码点区间，与 matches() 一一对应
    const std::vector<CodepointRange>& ranges() const;
    size_t matchCount() const;
    size_t activeIndex() const;
    // 当前命中项；没有命中 → nullptr
    const Match* activeMatch() const;
    // 当前命中项的码点区间；没有当前项 → false
    bool activeRange(int& startCp, int& endCp) const;
    // 移到下一个（backwards=false）/ 上一个命中项；没有命中 → false
    bool move(bool backwards);
    // 重新回调一次 onActiveMatch（宿主据此落选区/滚到视口内）；没有命中时不动作。
    void applyActiveMatch();
    // 第一个**起点不早于** fromCp 的命中项的下标；命中项都在它之前 → false（当前项不动）
    bool indexAtOrAfter(int fromCp, size_t& index) const;

private:
    // applyActive=false = 只报「命中集变了」，不报「当前命中项落成你的选区」（见 search 的注释）
    void notify(bool applyActive = true);
    void notifyActive();

    std::string query_;
    std::vector<Match> matches_;
    std::vector<CodepointRange> ranges_;
    size_t active_ = 0;
    bool open_ = false;
    bool matchCase_ = true;
    bool wholeWord_ = false;
};

} // namespace loppe
