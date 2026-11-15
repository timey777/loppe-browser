#include "platform/android_platform.h"

#include <android/asset_manager_jni.h>
#include <android/log.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace app {
namespace {

namespace fs = std::filesystem;

constexpr const char* kLogTag = "LoppeAppPlatform";
constexpr const char* kPlatformClassName = "com/example/android/LoppePlatform";

JavaVM* gJavaVm = nullptr;
jclass gPlatformClass = nullptr;

struct PlatformContext {
    AAssetManager* assetManager = nullptr;
    std::string filesDir;
    std::string extractedAssetsRoot;
    bool assetsExtracted = false;
};

PlatformContext& platformContext() {
    static PlatformContext context;
    return context;
}

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

void ensurePlatformClass(JNIEnv* env) {
    if (!env || gPlatformClass) {
        return;
    }

    jclass localClass = env->FindClass(kPlatformClassName);
    if (!localClass) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "FindClass failed for %s", kPlatformClassName);
        return;
    }

    gPlatformClass = static_cast<jclass>(env->NewGlobalRef(localClass));
    env->DeleteLocalRef(localClass);
}

bool copyAssetTree(AAssetManager* assetManager, const std::string& assetPath, const fs::path& outputRoot) {
    if (!assetManager) {
        return false;
    }

    const char* dirPath = assetPath.empty() ? "" : assetPath.c_str();
    AAssetDir* dir = AAssetManager_openDir(assetManager, dirPath);
    if (!dir) {
        return false;
    }

    std::vector<std::string> children;
    while (const char* name = AAssetDir_getNextFileName(dir)) {
        children.emplace_back(name);
    }
    AAssetDir_close(dir);

    bool copiedAny = false;
    for (const auto& child : children) {
        const std::string childAssetPath = assetPath.empty() ? child : assetPath + "/" + child;
        AAsset* asset = AAssetManager_open(assetManager, childAssetPath.c_str(), AASSET_MODE_STREAMING);
        if (asset) {
            const off_t assetLength = AAsset_getLength(asset);
            std::string content(static_cast<size_t>(assetLength), '\0');
            const int64_t bytesRead = AAsset_read(asset, content.data(), content.size());
            AAsset_close(asset);
            if (bytesRead < 0) {
                continue;
            }

            const fs::path outputPath = outputRoot / fs::path(childAssetPath);
            fs::create_directories(outputPath.parent_path());
            std::ofstream stream(outputPath, std::ios::binary);
            stream.write(content.data(), bytesRead);
            copiedAny = true;
            continue;
        }

        copiedAny = copyAssetTree(assetManager, childAssetPath, outputRoot) || copiedAny;
    }

    return copiedAny;
}

std::string loadAssetTextInternal(const std::string& assetPath) {
    auto& platform = platformContext();
    std::string result;

    if (platform.assetManager) {
        AAsset* asset = AAssetManager_open(platform.assetManager, assetPath.c_str(), AASSET_MODE_BUFFER);
        if (asset) {
            const off_t length = AAsset_getLength(asset);
            result.assign(static_cast<size_t>(length), '\0');
            const int64_t bytesRead = AAsset_read(asset, result.data(), result.size());
            AAsset_close(asset);
            if (bytesRead < 0) {
                result.clear();
            } else {
                result.resize(static_cast<size_t>(bytesRead));
            }
        }
    }

    if (result.empty() && !platform.extractedAssetsRoot.empty()) {
        const fs::path extracted = fs::path(platform.extractedAssetsRoot) / fs::path(assetPath);
        if (fs::exists(extracted)) {
            std::ifstream stream(extracted, std::ios::binary);
            result = std::string((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
        }
    }

    return result;
}

}  // namespace

void InitializeJavaVm(JavaVM* vm) {
    gJavaVm = vm;
}

JNIEnv* GetJniEnv(bool* didAttach) {
    if (didAttach) {
        *didAttach = false;
    }
    if (!gJavaVm) {
        return nullptr;
    }

    JNIEnv* env = nullptr;
    const jint status = gJavaVm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (status == JNI_OK) {
        return env;
    }
    if (status != JNI_EDETACHED) {
        return nullptr;
    }
    if (gJavaVm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
        return nullptr;
    }
    if (didAttach) {
        *didAttach = true;
    }
    return env;
}

void ReleaseJniEnv(bool didAttach) {
    if (didAttach && gJavaVm) {
        gJavaVm->DetachCurrentThread();
    }
}

void ConfigurePlatform(JNIEnv* env, AAssetManager* assetManager, std::string filesDir) {
    ensurePlatformClass(env);

    auto& platform = platformContext();
    platform.assetManager = assetManager;
    platform.filesDir = std::move(filesDir);
    platform.extractedAssetsRoot = (fs::path(platform.filesDir) / "loppe_assets").string();
    platform.assetsExtracted = false;
}

void ExtractAssetsIfNeeded() {
    auto& platform = platformContext();
    if (platform.assetsExtracted || !platform.assetManager || platform.extractedAssetsRoot.empty()) {
        return;
    }

    try {
        fs::create_directories(platform.extractedAssetsRoot);
        copyAssetTree(platform.assetManager, "", platform.extractedAssetsRoot);
        platform.assetsExtracted = true;
    } catch (const std::exception& ex) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "Asset extraction failed: %s", ex.what());
    }
}

const std::string& GetExtractedAssetsRoot() {
    return platformContext().extractedAssetsRoot;
}

std::string LoadAndroidAssetText(const std::string& assetPath) {
    return loadAssetTextInternal(assetPath);
}

void NotifyTextInputState(bool active, bool multiline) {
    bool didAttach = false;
    JNIEnv* env = GetJniEnv(&didAttach);
    if (!env || !gPlatformClass) {
        ReleaseJniEnv(didAttach);
        return;
    }

    const jmethodID method = env->GetStaticMethodID(gPlatformClass, "syncSoftInputState", "(ZZ)V");
    if (method) {
        env->CallStaticVoidMethod(gPlatformClass, method, active ? JNI_TRUE : JNI_FALSE, multiline ? JNI_TRUE : JNI_FALSE);
    }
    ReleaseJniEnv(didAttach);
}

void SetClipboardText(const std::string& text) {
    bool didAttach = false;
    JNIEnv* env = GetJniEnv(&didAttach);
    if (!env || !gPlatformClass) {
        ReleaseJniEnv(didAttach);
        return;
    }

    const jmethodID method = env->GetStaticMethodID(gPlatformClass, "setClipboardText", "(Ljava/lang/String;)V");
    if (!method) {
        ReleaseJniEnv(didAttach);
        return;
    }

    jstring value = env->NewStringUTF(text.c_str());
    env->CallStaticVoidMethod(gPlatformClass, method, value);
    env->DeleteLocalRef(value);
    ReleaseJniEnv(didAttach);
}

std::string GetClipboardText() {
    bool didAttach = false;
    JNIEnv* env = GetJniEnv(&didAttach);
    if (!env || !gPlatformClass) {
        ReleaseJniEnv(didAttach);
        return std::string();
    }

    const jmethodID method = env->GetStaticMethodID(gPlatformClass, "getClipboardText", "()Ljava/lang/String;");
    if (!method) {
        ReleaseJniEnv(didAttach);
        return std::string();
    }

    auto result = static_cast<jstring>(env->CallStaticObjectMethod(gPlatformClass, method));
    std::string text = jstringToUtf8(env, result);
    if (result) {
        env->DeleteLocalRef(result);
    }
    ReleaseJniEnv(didAttach);
    return text;
}

}  // namespace app