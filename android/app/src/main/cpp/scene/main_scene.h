#pragma once

#include <string>

#include "loppe.h"

namespace app {

// 主场景：只做「DSL 文本 → 引擎」这一件事。
// 界面写在 app/src/main/assets/ui/main.lui（构建时打进 APK 的 assets）。
// 要加交互就在 reload() 之后绑事件：`onClick("#btn", [](UIEventSystem::UIEvent&) { … })`。
class MainScene : public loppe::Scene {
public:
    using Scene::Scene;

    void setSource(std::string source) { source_ = std::move(source); }
    const std::string& source() const { return source_; }

    // 解析 source_ 并交给引擎
    bool reload();

private:
    std::string source_;
};

}  // namespace app
