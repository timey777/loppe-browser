#pragma once

#include <android/asset_manager.h>
#include <jni.h>

#include <string>

namespace app {

void InitializeJavaVm(JavaVM* vm);
JNIEnv* GetJniEnv(bool* didAttach);
void ReleaseJniEnv(bool didAttach);

void ConfigurePlatform(JNIEnv* env, AAssetManager* assetManager, std::string filesDir);
void ExtractAssetsIfNeeded();
const std::string& GetExtractedAssetsRoot();

std::string LoadAndroidAssetText(const std::string& assetPath);

void NotifyTextInputState(bool active, bool multiline);
void SetClipboardText(const std::string& text);
std::string GetClipboardText();

}  // namespace app