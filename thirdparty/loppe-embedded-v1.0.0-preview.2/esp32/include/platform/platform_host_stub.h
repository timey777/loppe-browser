/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once
// platform_host_stub.h — 嵌入式 IPlatformHost 空实现。
// 桌面版通过 Stage::setPlatformHost() 注入；嵌入式无光标/剪贴板，全部 no-op。
#include "engine/platform/platform_host.h"

namespace loppe_embedded {

class EmbeddedPlatformHost : public loppe::IPlatformHost {
public:
    void setCursor(int /*cursorType*/) override {}
    void setClipboardString(const std::string& /*str*/) override {}
    std::string getClipboardString() override { return std::string(); }
};

} // namespace loppe_embedded