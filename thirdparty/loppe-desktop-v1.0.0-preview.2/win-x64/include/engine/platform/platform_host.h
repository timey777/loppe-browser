/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include <string>
#include "loppe_export.h"

namespace loppe
{
	// 平台宿主接口：承载光标、剪贴板等平台级 UI 能力
	struct LOPPE_API IPlatformHost {
		virtual ~IPlatformHost() = default;
		virtual void setCursor(int cursorType);
		virtual void setClipboardString(const std::string& str);
		virtual std::string getClipboardString();
	};
}