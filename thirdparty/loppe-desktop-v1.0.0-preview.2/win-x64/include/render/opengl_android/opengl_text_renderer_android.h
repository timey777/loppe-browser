/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "loppe_export.h"
#include "text_renderer.h"
#include "opengl_drawer_android.h"

class OpenGLTextRendererAndroid : public loppe::ITextRenderer {
public:
	using ImplData = OpenGLDrawerAndroid::ImplData;

	explicit OpenGLTextRendererAndroid(OpenGLDrawerAndroid::ImplData* backend);

	TextMetrics measureText(const std::u32string& text, const FontDesc& font, float wrapWidth = -1.0f) override;
	TextMetrics measureText(const std::string& utf8text, const FontDesc& font, float wrapWidth = -1.0f) override;
	float getLineHeight(const FontDesc& font) override;
	std::shared_ptr<ITextLayout> createTextLayout(const std::string& text, const Font& font,
		const TextLayoutParams& params, float maxWidth = -1.0f) override;
	void drawText(const std::string& text, float x, float y, const Font& font, const DrawStyle& style,
		float maxWidth, const TextLayoutParams& layout) override;
	void drawTextLayout(const std::shared_ptr<ITextLayout>& layout, float x, float y, const DrawStyle& style) override;
	void drawTextLayoutRange(const std::shared_ptr<ITextLayout>& layout, float x, float y, const DrawStyle& style,
		int firstLine, int lineCount) override;

private:
	OpenGLDrawerAndroid::ImplData* d;
};