#pragma once

#include <android/asset_manager.h>
#include <jni.h>

#include <string>

namespace app {

// Android 侧运行时：开/停 GL 上下文、每帧 step、把触摸/按键喂给引擎。
// 界面来源只有一个 —— assets 里的 .lui 文件（loadUiFromAsset）。
class AndroidRuntime {
public:
    static AndroidRuntime& Instance();

    void configurePlatform(JNIEnv* env, AAssetManager* assetManager, std::string filesDir);
    void loadUiFromAsset(const std::string& assetPath);
    void init();
    void onSurfaceCreated();
    void onPause();
    void onResume();
    void resize(int width, int height, float scaleX, float scaleY);
    void step();
    void clearPointerState();
    void onTouch(int pointerId, int action, float x, float y);
    void onScroll(float dx, float dy);
    void onKey(int keyCode, int action, int metaState, int unicodeCodePoint);
    bool isTextInputActive() const;
    bool isTextInputMultiline() const;
    bool handleBackPressed();

private:
    AndroidRuntime() = default;
};

}  // namespace app