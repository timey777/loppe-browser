#include "runtime/android_runtime.h"

#include <android/log.h>

#include <atomic>
#include <filesystem>
#include <memory>
#include <string>
#include <typeindex>
#include <vector>

#include "loppe.h"

#include "platform/android_platform.h"
#include "scene/main_scene.h"

namespace app {
namespace {

constexpr const char* kLogTag = "LoppeAppRuntime";
constexpr int kAndroidPointerActionDown = 0;
constexpr int kAndroidPointerActionUp = 1;
constexpr int kAndroidPointerActionMove = 2;
constexpr int kAndroidPointerActionCancel = 3;
constexpr int kAndroidPointerActionPointerDown = 5;
constexpr int kAndroidPointerActionPointerUp = 6;

bool isFocusedTextInput(loppe::Scene* scene) {
    if (!scene || !scene->getUIEventSystem()) {
        return false;
    }
    auto* focusNode = scene->getUIEventSystem()->getFocusNode();
    if (!focusNode) {
        return false;
    }
    auto control = focusNode->getControl().lock();
    if (!control) {
        return false;
    }
    auto* input = dynamic_cast<loppe::Input*>(control.get());
    return input != nullptr;
}

bool isFocusedTextInputMultiline(loppe::Scene* scene) {
    if (!scene || !scene->getUIEventSystem()) {
        return false;
    }
    auto* focusNode = scene->getUIEventSystem()->getFocusNode();
    if (!focusNode) {
        return false;
    }
    auto control = focusNode->getControl().lock();
    if (!control) {
        return false;
    }
    auto* input = dynamic_cast<loppe::Input*>(control.get());
    return input != nullptr && input->getType() == "textarea";
}

int mapAndroidActionToMouseButtonAction(int action) {
    switch (action) {
    case kAndroidPointerActionDown:
    case kAndroidPointerActionPointerDown:
        return GLFW_PRESS;
    case kAndroidPointerActionUp:
    case kAndroidPointerActionPointerUp:
    case kAndroidPointerActionCancel:
        return GLFW_RELEASE;
    default:
        return GLFW_RELEASE;
    }
}

int mapAndroidKeyToGlfw(int keyCode) {
    switch (keyCode) {
    case 4: return GLFW_KEY_ESCAPE;
    case 61: return GLFW_KEY_TAB;
    case 62: return GLFW_KEY_SPACE;
    case 66: return GLFW_KEY_ENTER;
    case 67: return GLFW_KEY_BACKSPACE;
    case 92: return GLFW_KEY_PAGE_UP;
    case 93: return GLFW_KEY_PAGE_DOWN;
    case 112: return GLFW_KEY_DELETE;
    case 122: return GLFW_KEY_HOME;
    case 123: return GLFW_KEY_END;
    case 19: return GLFW_KEY_UP;
    case 20: return GLFW_KEY_DOWN;
    case 21: return GLFW_KEY_LEFT;
    case 22: return GLFW_KEY_RIGHT;
    case 57: return GLFW_KEY_LEFT_ALT;
    case 58: return GLFW_KEY_RIGHT_ALT;
    case 59: return GLFW_KEY_LEFT_SHIFT;
    case 60: return GLFW_KEY_RIGHT_SHIFT;
    case 113: return GLFW_KEY_LEFT_CONTROL;
    case 114: return GLFW_KEY_RIGHT_CONTROL;
    default:
        break;
    }

    if (keyCode >= 29 && keyCode <= 54) {
        return GLFW_KEY_A + (keyCode - 29);
    }
    if (keyCode >= 7 && keyCode <= 16) {
        return GLFW_KEY_0 + (keyCode - 7);
    }
    return GLFW_KEY_UNKNOWN;
}

int mapAndroidKeyActionToGlfw(int action) {
    switch (action) {
    case 0: return GLFW_PRESS;
    case 1: return GLFW_RELEASE;
    case 2: return GLFW_REPEAT;
    default: return GLFW_PRESS;
    }
}

int mapAndroidMetaStateToGlfwMods(int metaState) {
    constexpr int kMetaShiftOn = 0x1;
    constexpr int kMetaAltOn = 0x02;
    constexpr int kMetaCtrlOn = 0x1000;

    int mods = 0;
    if ((metaState & kMetaShiftOn) != 0) mods |= 0x0001;
    if ((metaState & kMetaAltOn) != 0) mods |= 0x0004;
    if ((metaState & kMetaCtrlOn) != 0) mods |= GLFW_MOD_CONTROL;
    return mods;
}

class AndroidRuntimeImpl {
public:
    void configurePlatform(JNIEnv* env, AAssetManager* assetManager, std::string filesDir) {
        ConfigurePlatform(env, assetManager, std::move(filesDir));
    }

    void loadUiFromAsset(const std::string& assetPath) {
        setSource(LoadAndroidAssetText(assetPath));
    }

    void init() {
        if (initialized_) {
            return;
        }

        registerAllParsers();
        initializeResources();

        graphicsContext_ = createOpenGLGraphicsContext();
        graphicsContext_->init(1, 1);
        // 文字走独立的 text renderer：drawer 不是文字引擎，必须显式拿 getTextRenderer()
        renderer_ = std::make_unique<loppe::Renderer>(graphicsContext_->getDrawer(), graphicsContext_->getTextRenderer());
        compositor_ = std::make_unique<loppe::Compositor>();
        compositor_->setRenderer(renderer_.get());

        documentRuntime_ = std::make_unique<DocumentRuntime>();
        styleEngine_ = std::make_unique<StyleEngine>();
        layoutEngine_ = std::make_unique<LayoutEngine>();
        astBuildEngine_ = std::make_unique<ASTBuildEngine>();
        boxModelEngine_ = std::make_unique<BoxModelEngine>();

        documentRuntime_->registerService(styleEngine_.get());
        documentRuntime_->registerService(layoutEngine_.get());
        documentRuntime_->registerService(astBuildEngine_.get());
        documentRuntime_->registerService(boxModelEngine_.get());
        // 文字测量器用 getTextRenderer()（别拿 drawer 强转，GL 后端是 nullptr）
        documentRuntime_->setTextMeasurer(graphicsContext_->getTextRenderer());
        documentRuntime_->initializeAll({
            typeid(StyleEngine),
            typeid(LayoutEngine),
            typeid(ASTBuildEngine),
            typeid(BoxModelEngine),
        });

        // ⚠ 必须给 scene resolver（跟随 Stage 当前 scene）：漏了会白屏
        // 协调器只管文档更新，不再持有合成器与刷新标志（那两样归本 runtime 自己）。
        host_ = std::make_unique<loppe::ViewCoordinator>(
            documentRuntime_.get(),
            loppe::ViewportSizeDp{},
            []() -> loppe::Scene* {
                auto scene = loppe::Stage::getInstance().currentScene();
                return scene ? scene.get() : nullptr;
            });

        scene_ = std::make_shared<MainScene>(*documentRuntime_);
        scene_->setSource(dslSource_);
        loppe::Stage::getInstance().replaceScene(scene_);
        scene_->reload();

        // Platform host for clipboard
        struct AndroidPlatformHost : public loppe::IPlatformHost {
            void setClipboardString(const std::string& str) override { SetClipboardText(str); }
            std::string getClipboardString() override { return GetClipboardText(); }
        };
        static AndroidPlatformHost s_platformHost;
        loppe::Stage::getInstance().setPlatformHost(&s_platformHost);

        initialized_ = true;
        pendingPresent_.store(true);
        syncTextInputStateWithPlatform();
        __android_log_print(ANDROID_LOG_INFO, kLogTag, "template runtime initialized");
    }

    void onSurfaceCreated() {
        const bool hadInitialized = initialized_;
        init();

        if (!hadInitialized || !graphicsContext_) {
            return;
        }

        const int initWidth = framebufferSize_.width > 0 ? framebufferSize_.width : 1;
        const int initHeight = framebufferSize_.height > 0 ? framebufferSize_.height : 1;

        graphicsContext_->init(initWidth, initHeight);

        if (viewportSize_.width.value > 0.0f && viewportSize_.height.value > 0.0f) {
            host_->setViewport(viewportSize_);
            graphicsContext_->resize({initWidth, initHeight}, contentScale_);
        }

        if (compositor_) {
            compositor_->clear();
        }
        if (scene_) {
            scene_->reload();
        }
        composedOnce_ = false;   // 合成器被清空，下一帧要重新生成 display list

        pendingPresent_.store(true);
        syncTextInputStateWithPlatform();
    }

    void onPause() {
        if (!initialized_) {
            return;
        }
        if (scene_) {
            scene_->clearPointerState();
        }
        lastTextInputActive_ = false;
        lastTextInputMultiline_ = false;
        NotifyTextInputState(false, false);
    }

    void onResume() {
        if (!initialized_) {
            return;
        }
        pendingPresent_.store(true);
        syncTextInputStateWithPlatform();
    }

    void clearPointerState() {
        if (!initialized_ || !scene_) {
            return;
        }
        scene_->clearPointerState();
        pendingPresent_.store(true);
    }

    void resize(int width, int height, float scaleX, float scaleY) {
        init();
        if (width <= 0 || height <= 0) {
            return;
        }

        framebufferSize_ = { width, height };
        contentScale_ = loppe::sanitizeScale({ { scaleX }, { scaleY } });
        viewportSize_ = loppe::toViewportSizeDp(framebufferSize_, contentScale_);
        host_->setViewport(viewportSize_);
        graphicsContext_->resize(framebufferSize_, contentScale_);
        host_->handleResize(viewportSize_);
        syncTextInputStateWithPlatform();
        pendingPresent_.store(true);
    }

    void step() {
        init();
        if (viewportSize_.width.value <= 0.0f || viewportSize_.height.value <= 0.0f) {
            return;
        }

        auto currentScene = loppe::Stage::getInstance().currentScene();
        UpdateScheduler* sched = currentScene ? currentScene->scheduler() : nullptr;
        if (sched && currentScene->getControlManager()) {
            const bool hasAnimation = currentScene->getControlManager()->processAnimatedControls();
            if (hasAnimation) {
                sched->requestPaint();
            }
        }

        // makePendingPlan() 只产出计划、不清状态；host_->apply() 在入口确认掉这份计划
        // （ASTRepaint 全清，其余按快照精确清），下一帧不会重复落地同一趟。
        auto updatePlan = sched ? sched->makePendingPlan() : DocumentUpdatePlan();
        if (host_) {
            host_->apply(updatePlan);
        }

        if (currentScene && currentScene->getResolvedTree()) {
            loppe::ControlFactory::getInstance()->executePendingRemovals();
        }

        syncTextInputStateWithPlatform();

        // 协调器不再替宿主合成：本帧有更新（或还没出过第一帧）时先重建 display list，
        // 否则 paintAll 回放的是上一份，看起来就是画面不动。
        if (currentScene && (!composedOnce_ || updatePlan.kind != DocumentUpdateKind::None)) {
            compositor_->renderScene(currentScene.get());
            composedOnce_ = true;
        }

        graphicsContext_->clear(1.0f, 1.0f, 1.0f, 1.0f);
        compositor_->paintAll();
        graphicsContext_->present();
        pendingPresent_.store(false);
    }

    void onTouch(int, int action, float x, float y) {
        init();

        auto currentScene = loppe::Stage::getInstance().currentScene();
        if (!currentScene) {
            return;
        }

        const loppe::LogicalPointDp logicalPoint = loppe::toLogicalPointDp({ x, y }, contentScale_);
        // 来源标成 Touch：引擎按输入方式分支的交互（如「按住内容拖动就滚动」）靠它区分
        //（设备上还接鼠标/手写笔时，把 MotionEvent.getToolType() 传下来逐事件填即可）
        if (action == kAndroidPointerActionMove) {
            currentScene->onMouseMove(loppe::MouseMoveEvent{ logicalPoint, loppe::InputSource::Touch });
        } else {
            currentScene->onMouseMove(loppe::MouseMoveEvent{ logicalPoint, loppe::InputSource::Touch });
            currentScene->onMouseButton(loppe::MouseButtonEvent{
                logicalPoint, 0, mapAndroidActionToMouseButtonAction(action), 0,
                loppe::InputSource::Touch });
        }

        syncTextInputStateWithPlatform();
        pendingPresent_.store(true);
    }

    void onScroll(float dx, float dy) {
        init();

        auto currentScene = loppe::Stage::getInstance().currentScene();
        if (!currentScene) {
            return;
        }

        const loppe::LogicalScrollDeltaDp logicalScroll = loppe::toLogicalScrollDeltaDp({ dx, dy }, contentScale_);
        currentScene->onScroll(loppe::ScrollEvent{ logicalScroll });
        pendingPresent_.store(true);
    }

    void onKey(int keyCode, int action, int metaState, int unicodeCodePoint) {
        init();

        auto currentScene = loppe::Stage::getInstance().currentScene();
        if (!currentScene) {
            return;
        }

        currentScene->onKey(loppe::KeyEvent{
            mapAndroidKeyToGlfw(keyCode),
            0,
            mapAndroidKeyActionToGlfw(action),
            mapAndroidMetaStateToGlfwMods(metaState) });

        if (unicodeCodePoint > 0 && action != 1) {
            currentScene->onChar(loppe::CharEvent{ static_cast<unsigned int>(unicodeCodePoint) });
        }

        syncTextInputStateWithPlatform();
        pendingPresent_.store(true);
    }

    bool isTextInputActive() const {
        return isFocusedTextInput(loppe::Stage::getInstance().currentScene().get());
    }

    bool isTextInputMultiline() const {
        return isFocusedTextInputMultiline(loppe::Stage::getInstance().currentScene().get());
    }

    bool handleBackPressed() {
        init();
        auto currentScene = loppe::Stage::getInstance().currentScene();
        if (!isFocusedTextInput(currentScene.get())) {
            return false;
        }

        if (currentScene->getUIEventSystem()) {
            currentScene->getUIEventSystem()->updateFocus(nullptr);
        }
        syncTextInputStateWithPlatform();
        pendingPresent_.store(true);
        return true;
    }

private:
    // 界面来源：assets 文本 → 场景（场景还没建就先存着，init() 里补上）
    void setSource(std::string dslSource) {
        dslSource_ = std::move(dslSource);
        if (scene_) {
            scene_->setSource(dslSource_);
            scene_->reload();
        }
    }

    void syncTextInputStateWithPlatform() {
        const bool active = isTextInputActive();
        const bool multiline = active && isTextInputMultiline();
        if (active == lastTextInputActive_ && multiline == lastTextInputMultiline_) {
            return;
        }

        lastTextInputActive_ = active;
        lastTextInputMultiline_ = multiline;
        NotifyTextInputState(active, multiline);
    }

    static void registerAllParsers() {
        loppe::ParserRegistry::instance().registerParser("import", std::make_shared<loppe::ImportParser>());
        loppe::ParserRegistry::instance().registerParser("style", std::make_shared<loppe::StyleParser>());
        loppe::ParserRegistry::instance().registerParser("layout", std::make_shared<loppe::LayoutParser>());
        loppe::ParserRegistry::instance().registerParser("content", std::make_shared<loppe::ContentParser>());
    }

    static void initializeResources() {
        auto& resourceManager = ResourceManager::getInstance();
        ExtractAssetsIfNeeded();

        const std::string& extractedAssetsRoot = GetExtractedAssetsRoot();
        if (!extractedAssetsRoot.empty()) {
            resourceManager.setResourceRoot(extractedAssetsRoot);
        }
        resourceManager.initialize();

        auto fontNames = resourceManager.getAllFontNames();
        if (fontNames.empty()) {
            const char* fallbackPaths[] = {
                "/system/fonts/Roboto-Regular.ttf",
                "/system/fonts/NotoSansCJK-Regular.ttc",
                "/system/fonts/DroidSans.ttf",
            };
            for (const char* path : fallbackPaths) {
                if (resourceManager.loadFont(path)) {
                    break;
                }
            }
            fontNames = resourceManager.getAllFontNames();
        }

        if (!fontNames.empty()) {
            resourceManager.setDefaultFont(fontNames.front(), 14.0f);
        }
    }

    bool initialized_ = false;
    loppe::ViewportSizeDp viewportSize_;
    loppe::FramebufferSizePx framebufferSize_;
    loppe::ContentScale contentScale_;
    bool lastTextInputActive_ = false;
    bool lastTextInputMultiline_ = false;
    std::string dslSource_;
    std::atomic<bool> pendingPresent_{false};
    // 合成器里是否已经有一份本场景的 display list（surface 重建清掉后要重新生成）
    bool composedOnce_ = false;

    std::unique_ptr<IGraphicsContext> graphicsContext_;
    std::unique_ptr<loppe::Renderer> renderer_;
    std::unique_ptr<loppe::Compositor> compositor_;
    std::unique_ptr<DocumentRuntime> documentRuntime_;
    std::unique_ptr<StyleEngine> styleEngine_;
    std::unique_ptr<LayoutEngine> layoutEngine_;
    std::unique_ptr<ASTBuildEngine> astBuildEngine_;
    std::unique_ptr<BoxModelEngine> boxModelEngine_;
    std::unique_ptr<loppe::ViewCoordinator> host_;
    std::shared_ptr<MainScene> scene_;
};

AndroidRuntimeImpl& runtimeImpl() {
    static AndroidRuntimeImpl instance;
    return instance;
}

}  // namespace

AndroidRuntime& AndroidRuntime::Instance() {
    static AndroidRuntime instance;
    return instance;
}

void AndroidRuntime::configurePlatform(JNIEnv* env, AAssetManager* assetManager, std::string filesDir) {
    runtimeImpl().configurePlatform(env, assetManager, std::move(filesDir));
}

void AndroidRuntime::loadUiFromAsset(const std::string& assetPath) {
    runtimeImpl().loadUiFromAsset(assetPath);
}

void AndroidRuntime::init() {
    runtimeImpl().init();
}

void AndroidRuntime::onSurfaceCreated() {
    runtimeImpl().onSurfaceCreated();
}

void AndroidRuntime::onPause() {
    runtimeImpl().onPause();
}

void AndroidRuntime::onResume() {
    runtimeImpl().onResume();
}

void AndroidRuntime::resize(int width, int height, float scaleX, float scaleY) {
    runtimeImpl().resize(width, height, scaleX, scaleY);
}

void AndroidRuntime::step() {
    runtimeImpl().step();
}

void AndroidRuntime::clearPointerState() {
    runtimeImpl().clearPointerState();
}

void AndroidRuntime::onTouch(int pointerId, int action, float x, float y) {
    runtimeImpl().onTouch(pointerId, action, x, y);
}

void AndroidRuntime::onScroll(float dx, float dy) {
    runtimeImpl().onScroll(dx, dy);
}

void AndroidRuntime::onKey(int keyCode, int action, int metaState, int unicodeCodePoint) {
    runtimeImpl().onKey(keyCode, action, metaState, unicodeCodePoint);
}

bool AndroidRuntime::isTextInputActive() const {
    return runtimeImpl().isTextInputActive();
}

bool AndroidRuntime::isTextInputMultiline() const {
    return runtimeImpl().isTextInputMultiline();
}

bool AndroidRuntime::handleBackPressed() {
    return runtimeImpl().handleBackPressed();
}

}  // namespace app