# android — Android 工程

由 IDE 的「New Android Project」生成。**不用 gradle、不用 CMake、不用 Android Studio**，
本机不装 NDK / SDK 也能构建（工具链随 loppe 安装包走）。

## 构建

```
loppe-cli build . Debug      # 或 Release
```

产物：**`build/Debug/LoppeBrowser.apk`**（Release = `build/Release/LoppeBrowser.apk`），同目录还有
`libLoppeBrowser.so` 与中间件。Debug / Release 各用各的目录，互不覆盖；
Debug 不优化（便于调试），Release 优化并去掉断言。

构建过程全部由 loppe-cli 自己跑：交叉编译 native → `aapt2` → `javac` → `d8` → `zipalign` →
`apksigner`。工具链（clang / sysroot / aapt2 / android.jar）取自
`<loppe 安装目录>/toolchain/android`，JDK 取自 `JAVA_HOME`（或环境变量 `LOPPE_JDK`）。

## Release 签名

IDE 里第一次「Build > Build Release」会弹「发布签名」，填 keystore / keystore 密码 / 密钥别名 /
密钥密码；也可以直接改 `build.lpj` 里的四行：

```
release_keystore   = none     # 你的 .keystore / .jks 路径
release_store_pass = none
release_key_alias  = none
release_key_pass   = none
```

`none` = 还没配（这时 Release 构建会明确报「Release 签名还没配」）。Debug 用 loppe 自带的
debug keystore，不用配。

## 改 Android 版本 / 包名

| 项 | 在哪 | 说明 |
|---|---|---|
| 包名 | `build.lpj` 的 `package` | 同时决定 Java 包目录与 JNI 符号；改它要一起改 `AndroidManifest.xml` 和 Java 目录 |
| 最低版本 | `build.lpj` 的 `min_api` | 21~34（超出范围要重裁工具链里的 sysroot） |
| 目标平台 | `build.lpj` 的 `target_sdk` | 决定用哪个 `platforms/android-XX/android.jar`；工具链里没那份会报错并指出路径 |
| ABI | 固定 `arm64-v8a` | 换 ABI 要同时换引擎预编译包（`../thirdparty/loppe-android-v<版本>/<abi>`） |

## 引擎：预编译包**随工程走**

`../thirdparty/loppe-android-v1.0.0-preview.2/arm64-v8a/` 是引擎的 Android 预编译包（`lib/arm64-v8a/{libloppe.a,libfreetype.a}`
+ `include/`），新建工程时从 IDE 拷进来；布局与 IDE 新建的工程一致（解决方案根
`thirdparty/loppe-<平台>-v<引擎版本>/<目标>`）。

- 构建只编本工程自己的 cpp，引擎以静态库参与链接。
- **C++ 侧引擎引用只有一个入口**：`#include "loppe.h"`（包里的总入口）；只要某一层就用目录汇总头
  （`components/loppe_components.h` / `engine/loppe_engine.h` / `render/loppe_render.h` …）——
  见 `app/src/main/cpp/runtime/android_runtime.cpp`。
- 工程里不出现引擎源码，也不引用引擎源码路径。

## 目录

| 文件 | 作用 |
|---|---|
| `build.lpj` | 工程描述：包名 / 清单 / Java 与 native 源 / API 级别 / Release 签名 |
| `app/src/main/AndroidManifest.xml` | package=`com.example.android`、label=`android`、入口 `.MainActivity` |
| `app/src/main/java/…` | MainActivity + SurfaceView + 平台类 + JNI 声明 |
| `app/src/main/cpp/{jni,platform,runtime,scene}` | JNI 桥 + Android 平台层 + 运行时 + 你的场景 |
| `app/src/main/assets/ui/main.lui` | **界面**（唯一真相：改它就能改界面）|
| `build/{Debug,Release}/` | 构建产物（自动创建）|
| `AGENTS.md` | 给 AI 助手的说明（先读哪份文档、改哪里）|
| `docs/loppe-ui/` | 引擎文档（人读在根，AI 速查在 `ai/`）|

**界面 = 一个 .lui 文件**（`app/src/main/assets/ui/main.lui`）：`MainActivity` 调
`NativeBridge.loadUi("ui/main.lui")` → 平台层读 assets 文本 → 场景解析 → 引擎每帧渲染。
C++ 侧只保留「平台胶水」（SurfaceView/EGL/输入/IME/JNI）。想加交互：在 `cpp/scene/main_scene.cpp`
的 `reload()` 之后绑事件（选择器 = `#id`，跨控件重建依然有效）：

```cpp
onClick("#btn-hello", [](UIEventSystem::UIEvent&) { /* ... */ });
```

DSL 语法（`@style` / `@layout` / 控件树三段）与可用属性：查工程里的 `docs/loppe-ui/`
（`ai/` 下是 AI 速查，其余是用户指南 / 控件清单 / 布局 / 自定义控件）；拿不准就先照
`assets/ui/main.lui` 改样式和文案试。

### 界面不止一份 `.lui`

`app/src/main/assets/ui/` 下可以放任意多份 `.lui`（整个 `assets` 目录都打进 APK），
`main.lui` 只是模板给的那一份：

- **整屏换**：`NativeBridge.loadUi("ui/xxx.lui")` 换路径调，或 C++ 里
  `AndroidRuntime::Instance().loadUiFromAsset("ui/xxx.lui")`。
- **外壳 + 每页一份**（推荐）：`main.lui` 末尾留空 `panel(id="page-slot") {}` 当挂载点，
  读 assets 文本后调 `ComponentInstantiator::SpawnElementViewUnderId(scene, "page-slot", 文本,
  documentRuntime)`；换页前对上次返回的 `ElementView` 调 `remove()`。

细节见 `docs/loppe-ui/LOPPE_MULTI_UI.md`。

## 安装 / 看日志（不是 build 动作）

```
adb install -r build/Debug/LoppeBrowser.apk
# Release 包：adb install -r build/Release/LoppeBrowser.apk
# 应用日志：native 侧 tag 固定为下面三个（**不是工程名**；`adb logcat -s android` 会什么都看不到）
adb logcat -s LoppeAppPlatform LoppeAppRuntime LoppeAppScene
# 引擎日志（LOPPE_LOG/WARN/ERROR）：引擎在 Android 上直写 logcat（tag Loppe）
adb logcat -s Loppe
# 崩溃记录（Java 异常 / native tombstone）：单独看 crash buffer
adb logcat -b crash -v threadtime -d
```

## 注意

- **源文件自动发现**：`build.lpj` 的 `sources` / `java` 用 glob（`app/src/main/cpp/**.c`、
  `**.cpp`、`app/src/main/java/**.java`）—— 加减自己的源文件、新建子目录都不用改清单；
  `app/src/main/cpp/**.h` 写在 `view` 里（看得见、不编译）。引擎那侧不用管（链预编译包）。
- **`assets/` 整个目录都会打进 APK**（`assets` 那行指向它），换界面不用改构建。
- **每次构建都会重跑打包步骤**（aapt2 / javac / d8 / 签名），不做增量。
- 工具链 / JDK 找不到时，报错会点名缺什么、该放哪：工具链看 `<loppe 安装目录>/toolchain` 或
  `LOPPE_TOOLCHAIN`，JDK 看 `JAVA_HOME` 或 `LOPPE_JDK`。

## 共享内核（`../kernel` 的静态库）

本工程 `addsub "../kernel"` 再 `link = [ browser_kernel, … ]`：内核编成 `libbrowser_kernel.a`
（用**安卓工具链**编，平台继承本文件的 `platform = android`），源码与 QuickJS 只在
`kernel/build.lpj` 里列一次。ESP32-S3 壳例外 —— 它走 IDF 的 CMake，把源码直接编进固件。

- 目录关系：`android/` 和 `kernel/` 是兄弟目录，`thirdparty/quickjs-ng` 也在仓库根；
- 现在**能编过、能装**，但界面仍然是模板 —— 还没有把 `BrowserScene` 挂上；
- 下一步：让 `app/src/main/cpp/runtime/android_runtime.cpp` 实现 `kernel/src/browser_host.h`
  的 `BrowserHost`（drawer / textRenderer / renderer / 帧脏标志 / 视图槽表 `views()`），
  再 `scene->setHost(this)` 把 `BrowserScene` 装进 `Stage`。
