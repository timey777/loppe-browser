# 用 loppe-cli 构建与运行 LOPPE 工程

> 📖 中文 | [English](LOPPE_CLI_GUIDE_EN.md)

---

## 1. 本文讲什么

本文讲在一个 LOPPE 工程里怎么编译、怎么只校验构建脚本、产物在哪、参数从哪里来。命令是 `loppe-cli`。

**适用读者**：手上已有一个 LOPPE 工程（IDE 新建的，或手写的 `build.lpj`），要在这个工程里编译运行的人或 AI 助手。

**前置条件**

| 项 | 要求 |
|---|---|
| `loppe-cli.exe` | 与 `loppe.exe` 放在同一个目录，随 IDE 安装包提供 |
| 工程文件 | 工程根有 `build.lpj`（界面文件写在哪个目录由工程自己决定） |
| 工具链 | 桌面工程用 IDE 目录里的 `toolchain/`；Android / ESP32 工程的工具链路径写在 `build.lpj` 顶部 |

**内容导览**

| 你想做的事 | 去第几节 |
|---|---|
| 直接编译跑起来 | §2 |
| 知道每个参数干什么 | §3 |
| 用工具校验 `build.lpj` | §4 |
| 找到产物、重新编译 | §5 |
| 判断哪种做法不成立 | §6 |
| 报错了怎么读 | §7 |

---

## 2. 编译与运行

在工程根目录打开终端，执行：

```bash
loppe-cli build .
```

- 工程路径省略 = 当前目录；配置省略 = `Debug`。
- 产物在 `<工程根>/build/Debug/`；桌面工程的可执行文件名取自 `build.lpj` 里的 `exe <名>`。
- 桌面工程直接运行产物里的 `.exe`。Android 工程产物是 `app/build/apk/app-debug.apk`，安装与看日志用 `adb`。ESP32 工程由 `build.lpj` 里的目标级 `build` 调 `idf.py build`，产物在工程自己的 `build/`。

发布用的 Release：

```bash
loppe-cli build . Release
```

Release 下桌面工程用 `subsystem = windows`（不弹控制台），Debug 保留控制台以便看日志；这个差别写在工程的 `build.lpj` 里。

---

## 3. 参数

```bash
loppe-cli build [<工程目录|build.lpj 路径>] [Debug|Release] [--targets=a,b,c] [--out=<目录>] [--toolchain=<名>]
loppe-cli config [<工程目录|build.lpj 路径>] [Debug|Release] [--targets=a,b,c] [--out=<目录>] [--toolchain=<名>]
loppe-cli help
```

| 参数 | 含义 |
|---|---|
| 工程路径 | 第一个位置参数。可以是工程目录，也可以直接给 `build.lpj` 的路径；省略 = 当前目录 |
| 配置名 | `Debug` 或 `Release`，位置任意；省略 = `Debug` |
| `--targets=a,b,c` | 只构建这几个目标（名取自 `build.lpj` 里 `exe <名>` / `lib <名>`），不写 `--targets` = 构建全部目标 |
| `--out=<目录>` | 构建输出根目录，实际产物在 `<out>/<配置>/`；省略 = `<工程根>/build` |
| `--toolchain=<名>` | 用构建器内置的工具链名（当前只有 `windows-x64`）；别的平台在 `build.lpj` 里写 `toolchain <根>` |
| `--help` / `-h` | 打印帮助 |

退出码 `0` = 成功，非 `0` = 失败（脚本没解析过、编译错、链接错都在这一档）。

```bash
loppe-cli build . Debug --targets=myapp
loppe-cli build D:/work/myproj Release --out=D:/work/out
loppe-cli build template/desktop Release        # 直接编一个工程模板目录
```

---

## 4. 只校验，不编译

`config` 与 `build` 走同一条链路，到「生成构建清单」就停：它解析 `build.lpj`、展开路径与 glob、检查目标名，然后把清单打印出来，不调用编译器。改了 `build.lpj` 想先确认语法和路径对不对时用它：

```bash
loppe-cli config .
loppe-cli config . Debug --targets=myapp
```

工程根找不到 `build.lpj`、变量没定义、数组写错这类问题在这一步就会报出来，带行号。

---

## 5. 产物与重复编译

| 配置 | 产物位置（默认 `--out`） |
|---|---|
| `Debug` | `<工程根>/build/Debug/` |
| `Release` | `<工程根>/build/Release/` |

- 编译是增量的：清单没变的部分不重编，只重编改过的源文件。
- `build/` 可以整个删掉再编一次，代价是第一次全量编译变慢。
- 桌面工程把 `ui/main.lui` 这类界面文件放在 exe 旁边（或向上两级）就能运行期读到，改界面文案不必重编。界面文件的查找顺序由工程自己的加载代码决定。

---

## 6. 这些做法不成立

| 做法 | 为什么不行 |
|---|---|
| 用 `cmake`、`make`、`msbuild`、`cl.exe` 直接编工程 | 工程没有 `CMakeLists.txt`、没有 `.sln`；构建信息只有 `build.lpj` 一份 |
| 手动敲 `clang++ 源文件列表` | 工具链路径、include、库、`-D` 都在 `build.lpj` 与 IDE 的平台配置里；手敲必然漏 |
| 往工程里加 `CMakeLists.txt` 作为「另一条构建线」 | 构建器不读它；两份清单会各说一套，改了一边另一边不生效 |
| 给 `loppe.exe` 传 `build` 之类参数 | `loppe.exe` 是图形界面，不解析命令行，给参数也照常开窗。命令行入口只有 `loppe-cli.exe` |
| 在仓库里对子工程目录单独执行 `loppe-cli build` | 仓库内的子工程共享根 `build.lpj` 的变量，直接在子目录编会报「未定义变量 'pkg'」；仓库场景按目标点名编（见工程自带的说明） |

---

## 7. 编译输出怎么读

编译输出被整理过，每条错误是一行，格式与 VS 一致：

```
src/main.cpp(31,5): error: use of undeclared identifier 'foo'
```

- 工程内的文件路径相对**工程根**显示。
- 编译失败的边只报 `[loppe] FAILED: src/main.cpp（目标 myapp）`；具体原因看紧跟的 `error:` 行。
- 源码片段、`^` 指向、`N errors generated.` 这类上下文行原样保留。

---

## 相关文档

| 文档 | 内容 |
|---|---|
| `LOPPE_GETTING_STARTED.md` | 从 C++ 接入引擎、第一个界面 |
| `LOPPE_DSL_USER_GUIDE.md` | `.lui` 语法、选择器、样式与布局属性 |
| `LOPPE_BUILTIN_CONTROLS.md` | 内置控件的属性、默认样式、事件与运行期接口 |
| `ai/LOPPE_DSL_AI_REFERENCE.md` | 属性全表 + 与标准 CSS 的关键差异（AI 用） |
