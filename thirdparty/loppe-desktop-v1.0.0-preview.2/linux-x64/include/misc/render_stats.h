/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

namespace loppe {

/**
 * @brief 每帧渲染统计（性能基准用）。
 */
struct RenderStats {
    // ── Node traversal ──
    int totalControls = 0;   // total known controls (set externally)
    int visitedNodes = 0;    // children whose bounds were checked
    int visibleNodes = 0;    // children that passed culling
    int culledNodes = 0;     // visited - visible

    // ── DisplayList commands (recursive, including SubLists) ──
    int rectCommands = 0;
    int textCommands = 0;
    int textLayoutCommands = 0;
    int imageCommands = 0;
    int pushClipCommands = 0;
    int popClipCommands = 0;
    int subLists = 0;
    int totalCommands = 0;   // sum of all draw commands (excluding push/pop pairs)

    // ── Timing ──
    double cpuTimeUs = 0.0;  // microseconds spent in Scene::render()

    void reset();
};

/**
 * @brief 指向当前帧 RenderStats 的线程本地指针；非空时渲染过程累计计数。
 */
extern thread_local RenderStats* g_currentRenderStats;

} // namespace loppe