/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "engine/platform/platform_host.h"
#include "engine/platform/Window.h"

// 基于 GLFW Window 的平台宿主实现（光标、剪贴板），用于桌面端。
struct WindowPlatformHost : public loppe::IPlatformHost {
	Window* window;

	explicit WindowPlatformHost(Window* w);

	void setCursor(int cursorType) override;

	void setClipboardString(const std::string& s) override;

	std::string getClipboardString() override;
};