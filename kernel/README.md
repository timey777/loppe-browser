# kernel — loppe-browser 内核

浏览器里**与平台无关**的那一半。桌面壳与安卓壳把它当**静态库**链接（`addsub` + `link browser_kernel`），
ESP32-S3 走 IDF 的 CMake、把源码编进固件 —— 内核这一份源码只有这里。

```
src/browser_scene.{h,cpp}   DOM 绑定 + 渲染协调（引擎 Scene 之上）
src/browser_host.h          壳要提供的接口：渲染资源 + Stage 挂载（壳实现它）
src/window_host.h/.cpp      壳要提供的接口：关窗/最小化/最大化/拖动区（不装 = 空操作）
src/js/script_host.{h,cpp}  QuickJS 绑定：页面脚本、模块系统、ui.* API
src/net/http_url.{h,cpp}    地址解析（loppe:/http:/https:、hosts 匹配、file: 本地文件）
src/net/http_client.{h,cpp} HTTP 抓取（Winsock / BSD socket 两套，按 _WIN32 分支）
src/net/resource.{h,cpp}    取字节的唯一入口：http 走网络、file 读磁盘（页面 / import / net.get 共用）
src/net/favicon.*           标签图标（PNG / ICO → TGA 落盘）
src/page/page_session.*     一个标签页：加载、脚本泵、生命周期
src/page/script_scene.h     页面侧 Scene 接口
src/perm/permissions.*      权限（跨域、脚本来源、文件访问）
```

## 内核与壳的分界

内核只用两个接口跟外界打交道，**不认识窗口、不认识 Application**：

| 接口 | 壳提供什么 | 桌面 | 安卓 / ESP32 |
|---|---|---|---|
| `BrowserHost`（`browser_host.h`） | drawer / textRenderer / 窗口级 Renderer / 帧脏标志 / 视图槽表 `views()` | `BrowserApp` 多继承 `Application`，五个方法转发 | 各自的 runtime（SurfaceView / EmbeddedHost） |
| `WindowHost`（`window_host.h`） | 关窗 / 最小化 / 最大化 / 拖动区 | `window_frame.cpp` 的适配器（Win32 / X11） | 不装 → 空操作 |

## 独立编译（只用于校验内核本身）

```bash
cd kernel
loppe-cli build . Debug      # 产物 build/Debug/libbrowser_kernel.a
loppe-cli config . Debug     # 只解析，不编译
```

引擎头和 QuickJS 都取自仓库根：`../thirdparty/loppe-desktop-v1.0.0-preview.2/win-x64/include`、
`../thirdparty/quickjs-ng`。所以内核工程的产物只对桌面口径成立 —— 安卓 / ESP32
由各自的壳带着编（它们的 `LOPPE_PLATFORM_*` 宏才会生效）。

## 三个壳怎么消费

| 壳 | 目录 | 接法 |
|---|---|---|
| 桌面 | `../desktop/` | `addsub "../kernel"` + `link = [ browser_kernel, … ]`（内核头由链接带过来） |
| 安卓 | `../android/` | 同上；平台取壳文件里的 `platform = android`，本文件的 `pkg` 自动指向安卓包 |
| ESP32-S3 | `../esp32s3/` | `main/CMakeLists.txt` 把 `../kernel/src` 的源码交给 IDF（`-I` 指过去） |

新增内核源码：直接放进 `src/`（含子目录）即可 —— 库用 `src/**.c` / `src/**.cpp` glob 自动发现，
桌面与安卓都跟着变；ESP32 侧是 IDF 的同一套 glob。QuickJS 的 `.c` 也只在 `build.lpj` 里列一次。
新增**子目录**不用改构建文件（`#include "net/xxx.h"` 这种写法照旧）。

## 已知

- `js/script_host` 里按 GLFW 的修饰键掩码（1=Shift 2=Ctrl 4=Alt）解释按键，
  各壳只要把自家输入翻译成这个掩码就行，不用改内核。
- `net/http_client.cpp` 已经是 `#if defined(_WIN32)` / BSD socket 两套，
  安卓能编；ESP32 走 lwIP，接口是 POSIX 那一套，但还要确认 `select` 超时与非阻塞可用的写法。
