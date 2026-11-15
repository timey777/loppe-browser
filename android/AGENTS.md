# android — 给 AI 助手的说明

Android 工程（工具链随 loppe 走，不用 gradle / CMake / Android Studio）。
界面写在 `app/src/main/assets/ui/main.lui`（Loppe DSL），C++ 只负责平台胶水。

## 写界面前先读这两份

- `docs/loppe-ui/ai/LOPPE_DSL_AI_REFERENCE.md` —— DSL 语法速查：`@style` / `@layout` / 控件树三段、
  可用属性、与标准 CSS 的差异。
- `docs/loppe-ui/ai/LOPPE_CUSTOM_CONTROL_AI_PROMPT.md` —— 要引擎没有的控件时，照它生成自定义控件。

`docs/loppe-ui/` 下其余文档是给人读的（入门、用户指南、控件清单、布局设计、自定义控件），只在需要
更细的默认值 / 事件 / 运行期接口时才查；控件属性以 `docs/loppe-ui/LOPPE_BUILTIN_CONTROLS.md` 为准。
英文版是同名 `*_EN.md`。

## 构建只用 loppe-cli

本工程只有 `build.lpj` 一份构建描述，**没有** `gradle` 工程、也没有 `CMakeLists.txt`：

```
loppe-cli build . Debug        # 产物 build/Debug/LoppeBrowser.apk
loppe-cli build . Release      # 产物 build/Release/LoppeBrowser.apk（要先配 release_* 四行）
loppe-cli config . Debug       # 只解析并打印构建清单，不编译（改完 build.lpj 先跑这个）
```

不要手敲 clang / aapt2 / javac 命令行，也不要调 gradle / ndk-build / cmake —— 工具链、sysroot、
aapt2、android.jar、JDK 全由 loppe-cli 自己找（`<loppe 安装目录>/toolchain` + `JAVA_HOME`）。

## 改这个工程

- 改界面 = 改 `app/src/main/assets/ui/main.lui`，C++ 不用动。
- **界面可以有多份 `.lui`，不限于 `main.lui`**。`assets/ui/` 下可以放任意多份（整个 `assets` 目录
  都打进 APK）：要加一个界面不要往 `main.lui` 里塞——
  - 整屏替换：`NativeBridge.loadUi("ui/xxx.lui")` 换路径调，或在 C++ 里
    `AndroidRuntime::Instance().loadUiFromAsset("ui/xxx.lui")`。
  - 外壳 + 每页一份（推荐）：`main.lui` 里留 `panel(id="page-slot") {}` 当挂载点，读 assets 文本后
    用 `ComponentInstantiator::SpawnElementViewUnderId(scene, "page-slot", 文本, documentRuntime)`
    挂上去；换页时对上次返回的 `ElementView` 调 `remove()`。
  - 细节（挂载点、卸载、共享样式、常见错误）：`docs/loppe-ui/LOPPE_MULTI_UI.md`。
- 加交互 = DSL 里给控件 `id="…"`，在 `app/src/main/cpp/scene/main_scene.cpp` 的 `reload()` 之后绑。
- 新增 `.c` / `.cpp` / `.java` 直接放进 `app/src/main/cpp`、`app/src/main/java` 就会自动编译，
  不用改 `build.lpj`（`sources` / `java` 是 glob 自动发现）。
- `../thirdparty/loppe-android-v1.0.0-preview.2/arm64-v8a/` 是引擎预编译包（`lib/` + `include/`），随工程走，不要手改。
