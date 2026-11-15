/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "drawer.h"
#include "graphics_context.h"
#include "text_measurer.h"

namespace loppe {
class ITextRenderer;
}
class OpenGLTextRendererAndroid;

class LOPPE_API OpenGLDrawerAndroid : public IDrawer, public IGraphicsContext {
public:
	OpenGLDrawerAndroid();
	~OpenGLDrawerAndroid() override;

	void init(int width, int height) override;
	void resize(const loppe::FramebufferSizePx& framebufferSize,
		const loppe::ContentScale& contentScale = {}) override;
	void clear(float r, float g, float b, float a) override;
	void present() override;
	void shutdown() override;
	IDrawer* getDrawer() override;
	loppe::ITextRenderer* getTextRenderer() override;

	void drawRect(float x, float y, float w, float h, const DrawStyle& style = DrawStyle()) override;
	void drawRoundedRect(float x, float y, float w, float h, float rx, float ry, const DrawStyle& style = DrawStyle()) override;
	void drawGaussianShadow(float x, float y, float w, float h, float cornerRadius,
	                        float sigma, const DrawStyle& style = DrawStyle()) override;
	void drawImage(ImageHandle img, float x, float y, float w, float h, const DrawStyle& style = DrawStyle()) override;

	ImageHandle createImageFromMemory(const unsigned char* data, int width, int height, int channels) override;
	void unloadImage(ImageHandle img) override;

	void setClipRect(const render::Rect& r) override;
	bool intersectClipRect(const render::Rect& r, render::Rect* outNewClip) override;
	bool getClipRect(render::Rect* outCurrent) const override;
	void resetClip() override;

	void setClipCornerRadius(float radius) override;
	float getClipCornerRadius() const override;

	void pushTransform(float dx, float dy) override;
	void popTransform() override;

public:
	class ImplData;

private:
	ImplData* d;
	OpenGLTextRendererAndroid* textRenderer_ = nullptr;
};