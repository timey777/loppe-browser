/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#ifndef LOPPE_UPDATE_STEP_GUARD_H
#define LOPPE_UPDATE_STEP_GUARD_H

#include "loppe_export.h"

#include <exception>
#include <functional>

class DocumentRuntime;

namespace loppe {

// 一步更新的围栏：run() 内抛出的异常会被兜住并记录，调用方据此放弃本步后续操作。
class LOPPE_API UpdateStepGuard {
public:
    // runtime：失败上报对象（可空）；fallback：没有 runtime 时至少要做的动作（可空）
    using Fallback = std::function<void()>;

    explicit UpdateStepGuard(DocumentRuntime* runtime = nullptr, Fallback fallback = Fallback());

    void reset(DocumentRuntime* runtime, Fallback fallback = Fallback());

    // 跑一步更新。返回 false = 这一步抛了异常（已记日志 + 已安排恢复），调用方不要再继续后面的事。
    template <class Fn>
    bool run(const char* phase, Fn&& fn) {
        try {
            fn();
            return true;
        } catch (const std::exception& e) {
            fail(phase, e.what());
            return false;
        } catch (...) {
            fail(phase, "(unknown exception)");
            return false;
        }
    }

    // 返回 true = 引擎内部兜住过失败（那一趟的树可能是半成品）→ 调用方放弃本趟（取走即清）
    bool pipelineFailed(const char* phase);

private:
    void fail(const char* phase, const char* detail);

    DocumentRuntime* runtime_ = nullptr;
    Fallback fallback_;
};

} // namespace loppe

#endif // LOPPE_UPDATE_STEP_GUARD_H
