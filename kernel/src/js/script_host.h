/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once
// script_host.h — 用 JS 生成页面文档（.lui）的胶水层（QuickJS）。
//
// 一个 ScriptHost 绑一个 Scene；页面文档由脚本产出：脚本调用 ui.render(lui) 交出一份
// .lui 全文，宿主解析成该 Scene 的文档（整份替换）。脚本不逐个造控件。
//
// 最小用法：
//     loppe_script::ScriptHost js;
//     loppe_script::ScriptHost::Config cfg;         // 内存上限 / 栈上限 / 单次占用上限
//     if (!js.init(scene, &runtime, cfg)) { /* 装配失败 */ }
//     js.setDocumentHandler([](const std::string& lui) -> std::string {
//         return host.applyDocument(lui);           // 返回空 = 成功，非空 = 抛给脚本的错误
//     });
//     std::string doc, err;
//     if (!js.evalDocument(appJsSource, "app.js", &doc, &err)) { /* 语法错 / 异常 */ }
//     if (!doc.empty()) host.applyDocument(doc);    // 脚本没调 ui.render 时的返回式写法
//     for (;;) js.pump();                           // 每帧一次：微任务 + 定时器 + 待应用文档
//
// 入口脚本是 ES 模块：`import doc from './page.lui'` 显式引入界面资源，
// `import { signal, effect, Component } from 'loppe'` 引入标准库（虚拟模块）。
// 模块图按 URL 解析（相对路径以导入方为基准），.lui 后缀的 import 得到文件文本。
//
// JS 侧可用的全局对象与模块：
//
//   ui.render(luiSource)             → 交出整份页面文档（.lui 全文）；事件回调里调用 = 本帧末替换
//   ui.query(selector)               → '#id' / '.class'（第一个带 id 的命中）→ Element；找不到 = null
//   ui.get(id)                       → { id, tag, text, value, visible, x, y, w, h }
//   ui.set(id, prop, value)          → 改已有元素的属性（text/value/class/样式/布局都走它）
//   ui.text(id)                      → 读文字（等价 ui.get(id).text）
//   ui.addClass(id, c) / ui.removeClass(id, c) / ui.toggleClass(id, c, on)
//   ui.on(id, event, fn) / ui.off(id, event)
//        event: "tap"|"click" | "input" | "change" | "scroll" | "mousedown" | "mouseup"
//               | "keydown" | "keyup" | "focus" | "blur" | "contextmenu"
//        事件对象：{ id, targetId, text, key, mods, button }
//        mods 是 GLFW 掩码（1=Shift 2=Ctrl 4=Alt 8=Super）；button 0=左 1=右 2=中。
//        "contextmenu" = 右键 mouseup 的语法糖；键盘事件只发给焦点控件及其祖先。
//   ui.append(parentId, dsl, index?) → 在已有节点下插入一段 .lui 片段（组件挂载用），
//                                      返回新建节点的 id 数组；index 省略 = 追加
//   ui.navigate(url)                 → 页面内跳转（相对地址按当前页面解析；宿主决定同标签还是新标签）
//   net.get(url)                     → 同步取一个 http 资源，返回响应体字符串
//   setTimeout(fn, ms) / clearTimeout(handle)
//   console.log(...)
//
// 'loppe' 虚拟模块（import { signal, effect, computed, Component } from 'loppe'）：
//   signal(v) / effect(fn) / computed(fn)  ← 细粒度响应式（见模块内注释）
//   Component                              ← 组件基类：template/mount/detach/emit
//
// Element（ui.query 返回的对象句柄，同 id 同对象）：
//   el.text / el.value / el.display   ← 可写属性（display: "flex"|"none"|...，走 AST+标脏）
//   el.visible / el.tag / el.id       ← 只读
//   el.addClass(c) / el.removeClass(c) / el.toggleClass(c, on?) / el.hasClass(c)
//   el.rect()                         → { x, y, w, h }（border box，dp）
//   el.getAttr(name) / el.setAttr(name, value)
//   el.on(event, fn) / el.off(event)  ← 与 ui.on 共享同一张登记表
//   el.remove()                        → 从文档里摘掉这个节点（连子树）
//   句柄只存 id：文档替换后同 id 照常可用，id 没了才抛 stale 错误。
//
// 线程：全部在调用 init/eval/pump 的那个任务里跑（引擎的 Scene 不是线程安全的）。

#include <cstddef>
#include <functional>
#include <memory>
#include <string>

class ScriptScene;       // browser 侧的 Scene 基类（page/script_scene.h）
class DocumentRuntime;   // 引擎的运行时（全局命名空间）

namespace loppe_script {

class ScriptHost {
public:
    struct Config {
        size_t memoryLimitBytes = 2u * 1024 * 1024;   // JS 堆上限（超了抛 InternalError，不崩）
        size_t maxStackBytes = 256u * 1024;           // 脚本递归上限
        int interruptMs = 20;                         // 单次执行连续占用超过它就中断（防死循环）
        int maxJobsPerPump = 32;                      // 每帧最多跑多少个微任务
        int maxTimersPerPump = 16;                    // 每帧最多触发多少个定时器
    };

    ScriptHost();
    ~ScriptHost();
    ScriptHost(const ScriptHost&) = delete;
    ScriptHost& operator=(const ScriptHost&) = delete;

    // 装配：Scene（改属性/绑事件）与 DocumentRuntime（换整份界面）。
    // scene 必须是 ScriptScene 派生（事件便捷绑定长在那上面，不动引擎）。
    bool init(ScriptScene* scene, DocumentRuntime* runtime, const Config& cfg);
    bool init(ScriptScene* scene, DocumentRuntime* runtime) { return init(scene, runtime, Config()); }
    bool ready() const;

    // 执行一段脚本；失败时 error 里是「文件名:行:列: 错误」原文
    bool eval(const std::string& source, const char* filename, std::string* error);
    // 同上，但脚本在调用方的缓冲里（板上从 PSRAM 直接喂，少一次内部 RAM 拷贝）
    bool eval(const char* data, size_t len, const char* filename, std::string* error);
    // 执行并把最后一个表达式的值当字符串取出（调试/自检用）
    bool evalToString(const std::string& source, std::string* out, std::string* error);

    // 执行一段「产出文档」的脚本：脚本里调 ui.render(...) 交给 handler，
    // 或者让最后一个表达式就是 .lui 文本（此时取到 document 里，交给调用方应用）。
    // document 可以为空串（脚本已经自己 render 过，或者根本没产出文档）。
    bool evalDocument(const std::string& source, const char* filename, std::string* document,
                      std::string* error);

    // 每帧一次：跑微任务（Promise/async）与到期定时器。
    // 返回 true = 这一帧真的跑过任务（宿主据此判断是否还有活要干）。
    bool pump();

    // ── 页面文档 ──
    // ui.render(lui) 的宿主实现：解析 .lui 并把它作为本 Scene 的文档（整份替换）。
    // 返回空串 = 成功，非空 = 抛回脚本的错误信息（脚本能 try/catch）。
    // 不设 = ui.render 抛「不可用」。
    using DocumentHandler = std::function<std::string(const std::string& lui)>;
    void setDocumentHandler(DocumentHandler handler);

    // net.get(url) 的宿主实现：联网与权限都在宿主侧做，这里只搬字节。
    // 不设 = net.get 抛「不可用」；实现里把失败原因写进 error，脚本拿到 InternalError。
    using NetHandler = std::function<std::string(const std::string& url, std::string* error)>;
    void setNetHandler(NetHandler handler);

    // 模块图取资源（import './x.js' / './x.lui'）：页面自己的代码/资源，
    // 走页面加载通道而不是 net 能力面（不查网络权限；不设则回落 netHandler）。
    using ResourceHandler = std::function<std::string(const std::string& url, std::string* error)>;
    void setResourceHandler(ResourceHandler handler);

    // ui.navigate(url) 的宿主实现：返回空串 = 已受理，非空 = 抛给脚本的错误信息。
    // 「在哪个标签打开 / 算不算历史记录」只有宿主知道，所以只把 url 原样交出去。
    // 不设 = ui.navigate 抛「不可用」。
    using NavigateHandler = std::function<std::string(const std::string& url)>;
    void setNavigateHandler(NavigateHandler handler);

    // Element 句柄缓存整体作废（同 id 下一次 query 重新建对象）。
    // 宿主在 ui.render 通道之外换文档（错误页/开始页/直连 applyDocument）后必须调一次；
    // 走 ui.render / 回调登记的文档替换会自动失效，不用调。
    void invalidateElements();

    // 诊断
    size_t bytesUsed() const;
    void collectGarbage();
    void setLogSink(std::function<void(const char*)> sink);   // 默认 printf

    void shutdown();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace loppe_script
