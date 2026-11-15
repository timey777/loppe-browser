#include "scene/main_scene.h"

#include <android/log.h>

#include <exception>

#include "loppe.h"

namespace app {
namespace {

constexpr const char* kLogTag = "LoppeAppScene";

}  // namespace

bool MainScene::reload()
{
    if (!getDocumentRuntime()) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "DocumentRuntime is not ready");
        return false;
    }
    if (source_.empty()) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "DSL source is empty");
        return false;
    }

    try {
        loppe::LoppeParser parser;
        auto ast = parser.parse(source_).root();
        if (!ast) {
            __android_log_print(ANDROID_LOG_ERROR, kLogTag, "Parser returned empty AST");
            return false;
        }
        // 只排计划（resolved tree 由平台层每帧重建）
        getDocumentRuntime()->notifyASTUpdated(ast);
        return true;
    } catch (const std::exception& ex) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "DSL parse failed: %s", ex.what());
        return false;
    }
}

}  // namespace app
