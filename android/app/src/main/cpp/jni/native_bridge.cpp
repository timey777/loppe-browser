#include <jni.h>
#include <android/asset_manager_jni.h>

#include <string>

#include "platform/android_platform.h"
#include "runtime/android_runtime.h"

namespace {

std::string jstringToUtf8(JNIEnv* env, jstring value) {
    if (!value) {
        return std::string();
    }
    const char* chars = env->GetStringUTFChars(value, nullptr);
    if (!chars) {
        return std::string();
    }
    std::string result(chars);
    env->ReleaseStringUTFChars(value, chars);
    return result;
}

}  // namespace

extern "C" JNIEXPORT void JNICALL
Java_com_example_android_NativeBridge_configurePlatform(JNIEnv* env, jclass, jobject assetManager, jstring filesDir) {
    app::AndroidRuntime::Instance().configurePlatform(
        env,
        AAssetManager_fromJava(env, assetManager),
        jstringToUtf8(env, filesDir));
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_android_NativeBridge_loadUi(JNIEnv* env, jclass, jstring assetPath) {
    app::AndroidRuntime::Instance().loadUiFromAsset(jstringToUtf8(env, assetPath));
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_android_NativeBridge_init(JNIEnv*, jclass) {
    app::AndroidRuntime::Instance().init();
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_android_NativeBridge_onSurfaceCreated(JNIEnv*, jclass) {
    app::AndroidRuntime::Instance().onSurfaceCreated();
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_android_NativeBridge_onPause(JNIEnv*, jclass) {
    app::AndroidRuntime::Instance().onPause();
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_android_NativeBridge_onResume(JNIEnv*, jclass) {
    app::AndroidRuntime::Instance().onResume();
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_android_NativeBridge_resize(JNIEnv*, jclass, jint width, jint height, jfloat scaleX, jfloat scaleY) {
    app::AndroidRuntime::Instance().resize(width, height, scaleX, scaleY);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_android_NativeBridge_step(JNIEnv*, jclass) {
    app::AndroidRuntime::Instance().step();
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_android_NativeBridge_clearPointerState(JNIEnv*, jclass) {
    app::AndroidRuntime::Instance().clearPointerState();
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_android_NativeBridge_onTouch(JNIEnv*, jclass, jint pointerId, jint action, jfloat x, jfloat y) {
    app::AndroidRuntime::Instance().onTouch(pointerId, action, x, y);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_android_NativeBridge_onScroll(JNIEnv*, jclass, jfloat dx, jfloat dy) {
    app::AndroidRuntime::Instance().onScroll(dx, dy);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_android_NativeBridge_onKey(JNIEnv*, jclass, jint keyCode, jint action, jint metaState, jint unicodeCodePoint) {
    app::AndroidRuntime::Instance().onKey(keyCode, action, metaState, unicodeCodePoint);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_android_NativeBridge_isTextInputActive(JNIEnv*, jclass) {
    return app::AndroidRuntime::Instance().isTextInputActive() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_android_NativeBridge_isTextInputMultiline(JNIEnv*, jclass) {
    return app::AndroidRuntime::Instance().isTextInputMultiline() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_android_NativeBridge_handleBackPressed(JNIEnv*, jclass) {
    return app::AndroidRuntime::Instance().handleBackPressed() ? JNI_TRUE : JNI_FALSE;
}

jint JNI_OnLoad(JavaVM* vm, void*) {
    app::InitializeJavaVm(vm);
    return JNI_VERSION_1_6;
}