# esp32s3 — 给 AI 助手的说明

ESP32-S3 工程（ESP-IDF + loppe-ui 引擎预编译包）。界面写在 `main/main.lui`（Loppe DSL），
构建时编进固件；板级驱动在 `main/`。

## 写界面前先读这两份

- `docs/loppe-ui/ai/LOPPE_DSL_AI_REFERENCE.md` —— DSL 语法速查：`@style` / `@layout` / 控件树三段、
  可用属性、与标准 CSS 的差异。
- `docs/loppe-ui/ai/LOPPE_CUSTOM_CONTROL_AI_PROMPT.md` —— 要引擎没有的控件时，照它生成自定义控件。

`docs/loppe-ui/` 下其余文档是给人读的（入门、用户指南、控件清单、布局设计、自定义控件），只在需要
更细的默认值 / 事件 / 运行期接口时才查；控件属性以 `docs/loppe-ui/LOPPE_BUILTIN_CONTROLS.md` 为准。
英文版是同名 `*_EN.md`。

## 构建只用 loppe-cli

```
loppe-cli build . Debug        # 内部跑 idf.py build（本目标的 build 命令），产物在工程 build/
loppe-cli config . Debug       # 只解析 build.lpj、打印清单，不编译
```

编译由 `build.lpj` 里目标级 `build` 的两条命令完成（先按需生成中文字库，再 `idf.py build`）。
要改构建参数改这两条命令（或 `CMakeLists.txt` / `sdkconfig.defaults`），不要换构建目录，
也不要用 IDE 之外的方式另起一套构建。参数说明看 `docs/loppe-ui/LOPPE_CLI_GUIDE.md`。

## 改这个工程

- 改界面 = 改 `main/main.lui`（构建真相是 ESP-IDF，`loppe-cli build . Debug` 内部跑 `idf.py build`）。
- **界面可以有多份 `.lui`，不限于 `main.lui`**。要加一个界面不要往 `main.lui` 里塞：
  - 整屏替换：新写一份 `.lui`，加进 `main/CMakeLists.txt` 的 `EMBED_FILES`（符号名 = 路径里 `/` 换成
    `_` 再加 `_start`/`_end`），在 `main/app_main.cpp` 加一对 `extern ... asm("_binary_...")`，
    运行期 `s_host.loadUi(...)` 换过去。
  - 外壳 + 每页一份（推荐）：`main.lui` 里留 `panel(id="page-slot") {}` 当挂载点，
    页面用 `ComponentInstantiator::SpawnElementViewUnderId(s_host.scene(), "page-slot", 页面文本,
    &s_host.runtime())` 挂上去；换页时对上次返回的 `ElementView` 调 `remove()`。
  - 细节（多文件写法、卸载、共享样式、常见错误）：`docs/loppe-ui/LOPPE_MULTI_UI.md`。
- 界面里用到中文 → 把字写进 `main/font_chars.txt`，再跑 `python tools/generate_font.py`。
- 引脚 / 初始化在 `main/lcd_st7789.*` 与 `main/touch_xpt2046.*`，按你的板卡改。
- `../thirdparty/loppe-embedded-v1.0.0-preview.2/esp32/` 是引擎预编译包（`lib/` + `include/`），随工程走，不要手改。
