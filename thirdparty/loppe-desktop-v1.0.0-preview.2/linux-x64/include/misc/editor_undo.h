/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

// editor_undo.h — 文本编辑撤销/重做封装（EditState + UndoRedo + 捕获/应用回调）
#pragma once

#include <functional>
#include <string>

#include "misc/undo_redo.h"

namespace loppe {

// 文本编辑撤销/重做封装：状态为 EditState（value + caretPos），构造时注入读取/恢复状态的回调，提供 push / undo / redo / clear。
class EditorUndo {
public:
    struct EditState {
        std::string value;
        int caretPos = 0;
    };
    using CaptureFn = std::function<EditState()>;
    using ApplyFn = std::function<void(const EditState&)>;

    EditorUndo(CaptureFn capture, ApplyFn apply);

    // 修改前调用：记录当前状态；连续同类型操作（typing/deleting）自动合并为一步。
    void push(bool typing, bool deleting);

    void undo();
    void redo();

    void clear();
    bool canUndo() const;
    bool canRedo() const;

private:
    CaptureFn capture_;
    ApplyFn apply_;
    UndoRedo<EditState> undoRedo_;
};

} // namespace loppe