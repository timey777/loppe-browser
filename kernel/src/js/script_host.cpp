/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#include "script_host.h"

#include "quickjs.h"

#include "components/abstract_control.h"
#include "components/component_instantiator.h"
#include "components/control_factory.h"
#include "components/element_view.h"
#include "components/input.h"
#include "components/scene.h"
#include "components/text.h"
#include "components/uitypes.h"
#include "engine/core/attribute.h"
#include "engine/core/parse.h"
#include "engine/core/resolved_node.h"
#include "engine/core/resolved_node_query.h"
#include "engine/document_runtime.h"
#include "page/script_scene.h"

#if defined(ESP_PLATFORM)
#include "esp_heap_caps.h"
#endif

#include <chrono>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace loppe_script {

namespace {

// ===== 设计说明（头文件只写「怎么用」）=====
//
// 【为什么只有 ui.render 一个建界面的入口】逐个控件写 setText/setColor/setFlex 会变成几百个 C 函数，
//   而「脚本产出整份文档」把建树这件事还给 .lui：脚本写文档，引擎解析、排版、绘制。
//   运行期只想改一处时再用 ui.set（改属性/class），不必为了一个数字重画整份文档。
//
// 【为什么 ui.set 是「写回 AST + 标脏」而不是「重建整棵树」】ASTRepaint 会整屏重画；
//   写回 astNode->setAttribute + node->markDirty(flags) 走 PartialUpdate，
//   脏区只有该控件那一块（真机上就是「只刷几 KB」）。
//   Text/Input 有现成的 setText/setValue（内部同样只脏自己），优先走它们。
//
// 【四条护栏，缺一条就会跑飞】见 Config：内存上限（PSRAM 堆）、栈上限、单次占用中断、每帧任务数。
//
// 【事件回调里能改属性、不能重建文档】回调只写 AST + 标脏，真正落地在宿主下一次 tick()；
//   重建文档（ui.render）会把派发方握着的命中路径换掉，所以回调里只登记，pump 时再落地。

int64_t nowMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

struct Timer {
    int id = 0;
    int64_t dueMs = 0;
    int64_t intervalMs = 0;   // > 0 = 重复触发
    JSValue fn = JS_UNDEFINED;
    bool cancelled = false;
};

struct Ctx {
    ScriptScene* scene = nullptr;
    DocumentRuntime* runtime = nullptr;
    ScriptHost::Config cfg;

    JSRuntime* rt = nullptr;
    JSContext* ctx = nullptr;

    std::function<void(const char*)> logSink;
    std::map<std::string, JSValue> handlers;   // key = "#id|event"
    std::vector<Timer> timers;
    int nextTimerId = 1;

    // Element 句柄层：原型对象常驻；同 id 复用同一个 JS 对象（el === el2）。
    // 文档整份替换后整体失效（invalidateElements）——旧句柄仍可按 id 重新解析，
    // 缓存清掉只是让「下一次 query」拿到新对象，与「render 会重置状态」的语义一致。
    JSClassID elementClassId = 0;   // quickjs-ng 的 class id 按 runtime 注册，每个实例自己的
    JSValue elementProto = JS_UNDEFINED;
    std::map<std::string, JSValue> elementCache;

    // 页面文档：脚本交出的 .lui 由宿主解析（shutdown 之后残留回调一律失效，
    // 否则会拿野 ctx 调 JS）
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
    ScriptHost::DocumentHandler documentHandler;   // ui.render 的宿主实现（空 = 不可用）
    std::string pendingDoc;                        // 事件回调里 render 的：pump 时再应用
    bool hasPendingDoc = false;
    bool inCallback = false;                       // 正在跑事件回调（此时不能重建文档）

    ScriptHost::NetHandler netHandler;         // net.get 的宿主实现（空 = 不可用）
    ScriptHost::ResourceHandler resourceHandler;   // 模块图取资源（页面自己的代码，不查网络权限）
    ScriptHost::NavigateHandler navigateHandler;   // ui.navigate 的宿主实现（空 = 不可用）

    int64_t busyStartMs = 0;                   // 中断判定的起点
    std::string evalBuf;                       // eval 用：NUL 结尾的源码副本
    bool ready = false;
};

Ctx* C(JSContext* ctx) { return static_cast<Ctx*>(JS_GetContextOpaque(ctx)); }

std::string toUtf8(JSContext* ctx, JSValueConst v)
{
    size_t len = 0;
    const char* s = JS_ToCStringLen(ctx, &len, v);
    if (!s) {
        JSValue e = JS_GetException(ctx);   // 清掉 pending（Symbol 之类无法转字符串）
        JS_FreeValue(ctx, e);
        return std::string();
    }
    std::string out(s, len);
    JS_FreeCString(ctx, s);
    return out;
}

std::string takeException(JSContext* ctx)
{
    JSValue e = JS_GetException(ctx);
    const std::string message = toUtf8(ctx, e);      // 「SyntaxError: ...」这一行
    std::string stack;
    JSValue st = JS_GetPropertyStr(ctx, e, "stack");
    if (!JS_IsUndefined(st) && !JS_IsException(st)) stack = toUtf8(ctx, st);
    JS_FreeValue(ctx, st);
    JS_FreeValue(ctx, e);

    if (message.empty() && stack.empty()) return std::string("unknown error");
    if (stack.empty()) return message;
    if (message.empty()) return stack;
    return message + "\n" + stack;
}

// JS 值 → DSL 属性字符串（布尔/数字也按字面量写进去）
std::string attrString(JSContext* ctx, JSValueConst v)
{
    if (JS_IsBool(v)) return JS_ToBool(ctx, v) ? "true" : "false";
    return toUtf8(ctx, v);
}

struct Lookup {
    std::shared_ptr<loppe::AbstractControl> ctrl;
    loppe::ResolvedNode* node = nullptr;
};

// 按 id 在场景自己的解析树里找节点。
// 不走 ControlFactory::FindNodeByUid：那张 uid 表是进程级全局的，
// ResolvedTree::clear() 会整张清掉，而多文档宿主里别的文档一重建就会波及本页。
Lookup resolve(Ctx* c, const std::string& id)
{
    Lookup lk;
    if (!c->scene || id.empty()) return lk;
    auto* tree = c->scene->getResolvedTree();
    if (!tree) return lk;
    lk.node = loppe::ResolvedNodeQuery::FindById(tree, id);
    if (lk.node) lk.ctrl = lk.node->getControl().lock();
    return lk;
}

bool startsWith(const std::string& s, const char* p)
{
    const size_t n = std::strlen(p);
    return s.size() >= n && s.compare(0, n, p) == 0;
}

// 改这个属性会影响什么（决定标哪种脏；标少了 = 「改了不生效」）
loppe::DirtyFlag flagsForProp(const std::string& p)
{
    static const char* kLayout[] = {
        "width", "height", "min-width", "min-height", "max-width", "max-height",
        "margin", "padding", "flex", "gap", "display", "position",
        "top", "left", "right", "bottom", "align-items", "justify-content", "flex-direction"
    };
    for (const char* k : kLayout) {
        if (p == k || startsWith(p, (std::string(k) + "-").c_str())) return loppe::DirtyFlag::Layout;
    }
    return loppe::DirtyFlag::Style | loppe::DirtyFlag::Paint;
}

JSValue throwError(JSContext* ctx, JSValue (*fn)(JSContext*, const char*, ...), const char* fmt,
                   const std::string& what)
{
    return fn(ctx, fmt, what.c_str());
}

// ===== Element：元素句柄（对象化的 ui.*，同一套底层）=====
//
// 【同 id 同对象】elementCache 保证 el === ui.query('#x') 第二次的结果；
//   文档整份替换后缓存整体失效（invalidateElements），下一次 query 拿到新对象。
// 【句柄不会悬挂】句柄里只存 id，每次访问按 id 现查解析树：同 id 节点换了控件照样可用，
//   id 没了才抛 stale —— 与「按 id 寻址」的 DOM 语义一致，也不需要世代号。

struct ElementHandle {
    Ctx* c = nullptr;
    std::string id;
};

JSClassID valueClassId(JSValueConst v) { return JS_GetClassID(v); }

void elementFinalizer(JSRuntime*, JSValue val)
{
    auto* h = static_cast<ElementHandle*>(JS_GetOpaque(val, valueClassId(val)));
    delete h;
}

Lookup elemLookup(JSContext* ctx, JSValueConst this_val)
{
    Ctx* c = C(ctx);
    Lookup bad;
    auto* h = static_cast<ElementHandle*>(
        JS_GetOpaque2(ctx, this_val, valueClassId(this_val)));
    if (!h) {
        JS_ThrowTypeError(ctx, "not an Element");
        return bad;
    }
    Lookup lk = resolve(c, h->id);
    if (!lk.ctrl) JS_ThrowReferenceError(ctx, "stale element '#%s'（文档里已没有这个 id）",
                                         h->id.c_str());
    return lk;
}

// 同 id 取同一个 JS 对象；返回的引用归调用方（缓存里另持一份）
JSValue elementFor(Ctx* c, const std::string& id)
{
    JSContext* ctx = c->ctx;
    auto it = c->elementCache.find(id);
    if (it != c->elementCache.end() && !JS_IsUndefined(it->second))
        return JS_DupValue(ctx, it->second);

    auto* h = new ElementHandle{ c, id };
    JSValue obj = JS_NewObjectClass(ctx, c->elementClassId);
    if (JS_IsException(obj)) {
        delete h;
        return obj;
    }
    JS_SetOpaque(obj, h);
    if (!JS_IsUndefined(c->elementProto)) JS_SetPrototype(ctx, obj, c->elementProto);
    c->elementCache[id] = JS_DupValue(ctx, obj);
    return obj;
}

void invalidateElements(Ctx* c)
{
    for (auto& kv : c->elementCache) JS_FreeValue(c->ctx, kv.second);
    c->elementCache.clear();
}

// ── Element 方法 ──

JSValue jsElAddClass(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    Lookup lk = elemLookup(ctx, this_val);
    if (!lk.ctrl) return JS_EXCEPTION;
    if (argc < 1) return JS_ThrowTypeError(ctx, "el.addClass(cls)");
    lk.ctrl->addClass(toUtf8(ctx, argv[0]));
    return JS_UNDEFINED;
}

JSValue jsElRemoveClass(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    Lookup lk = elemLookup(ctx, this_val);
    if (!lk.ctrl) return JS_EXCEPTION;
    if (argc < 1) return JS_ThrowTypeError(ctx, "el.removeClass(cls)");
    lk.ctrl->removeClass(toUtf8(ctx, argv[0]));
    return JS_UNDEFINED;
}

JSValue jsElToggleClass(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    Lookup lk = elemLookup(ctx, this_val);
    if (!lk.ctrl) return JS_EXCEPTION;
    if (argc < 1) return JS_ThrowTypeError(ctx, "el.toggleClass(cls, on?)");
    const std::string cls = toUtf8(ctx, argv[0]);
    const bool on = (argc >= 2) ? (JS_ToBool(ctx, argv[1]) != 0) : !lk.ctrl->hasClass(cls);
    if (on) lk.ctrl->addClass(cls);
    else lk.ctrl->removeClass(cls);
    return JS_UNDEFINED;
}

JSValue jsElHasClass(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    Lookup lk = elemLookup(ctx, this_val);
    if (!lk.ctrl) return JS_EXCEPTION;
    if (argc < 1) return JS_ThrowTypeError(ctx, "el.hasClass(cls)");
    return JS_NewBool(ctx, lk.ctrl->hasClass(toUtf8(ctx, argv[0])));
}

// 当前几何（border box，dp）。布局没跑完时是 0 —— 想要「插入后再量」就下一帧再读。
JSValue jsElRect(JSContext* ctx, JSValueConst this_val, int, JSValueConst*)
{
    Lookup lk = elemLookup(ctx, this_val);
    if (!lk.ctrl) return JS_EXCEPTION;
    float x = 0, y = 0, w = 0, h = 0;
    if (lk.node) {
        const loppe::ui::Rect b = lk.node->layout.getBorderBox();
        x = b.x; y = b.y; w = b.width; h = b.height;
    }
    JSValue o = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, o, "x", JS_NewFloat64(ctx, x));
    JS_SetPropertyStr(ctx, o, "y", JS_NewFloat64(ctx, y));
    JS_SetPropertyStr(ctx, o, "w", JS_NewFloat64(ctx, w));
    JS_SetPropertyStr(ctx, o, "h", JS_NewFloat64(ctx, h));
    return o;
}

JSValue jsElGetAttr(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    Lookup lk = elemLookup(ctx, this_val);
    if (!lk.ctrl) return JS_EXCEPTION;
    if (argc < 1) return JS_ThrowTypeError(ctx, "el.getAttr(name)");
    const std::string name = toUtf8(ctx, argv[0]);
    if (lk.node && lk.node->astNode) {
        if (const auto* a = lk.node->astNode->getAttribute(name))
            return JS_NewString(ctx, a->as<std::string>().c_str());
    }
    return JS_NULL;
}

JSValue jsElSetAttr(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    Lookup lk = elemLookup(ctx, this_val);
    if (!lk.ctrl) return JS_EXCEPTION;
    if (argc < 2) return JS_ThrowTypeError(ctx, "el.setAttr(name, value)");
    const std::string name = toUtf8(ctx, argv[0]);
    const std::string val = attrString(ctx, argv[1]);
    if (lk.node && lk.node->astNode) lk.node->astNode->setAttribute(name, Attribute(val));
    lk.node->markDirty(flagsForProp(name));
    return JS_UNDEFINED;
}

// ── Element 属性（text/value/display 可写；visible/tag/id 只读）──

enum ElProp { kElText = 0, kElValue, kElDisplay, kElVisible, kElTag, kElId };

JSValue elPropGet(JSContext* ctx, JSValueConst this_val, int magic)
{
    if (magic == kElId) {
        auto* h = static_cast<ElementHandle*>(JS_GetOpaque2(ctx, this_val, valueClassId(this_val)));
        return h ? JS_NewString(ctx, h->id.c_str()) : JS_EXCEPTION;
    }
    Lookup lk = elemLookup(ctx, this_val);
    if (!lk.ctrl) return JS_EXCEPTION;
    switch (magic) {
    case kElText: {
        if (auto* t = dynamic_cast<loppe::Text*>(lk.ctrl.get()))
            return JS_NewString(ctx, t->getText().c_str());
        if (auto* i = dynamic_cast<loppe::Input*>(lk.ctrl.get()))
            return JS_NewString(ctx, i->getValue().c_str());
        return JS_NewString(ctx, "");
    }
    case kElValue: {
        if (auto* i = dynamic_cast<loppe::Input*>(lk.ctrl.get()))
            return JS_NewString(ctx, i->getValue().c_str());
        if (auto* t = dynamic_cast<loppe::Text*>(lk.ctrl.get()))
            return JS_NewString(ctx, t->getText().c_str());
        return JS_NewString(ctx, "");
    }
    case kElDisplay: {
        if (lk.node && lk.node->astNode) {
            if (const auto* a = lk.node->astNode->getAttribute("display"))
                return JS_NewString(ctx, a->as<std::string>().c_str());
        }
        return JS_NewString(ctx, "flex");
    }
    case kElVisible:
        return JS_NewBool(ctx, lk.ctrl->isVisible());
    case kElTag: {
        std::string tag;
        if (lk.node && lk.node->astNode) tag = lk.node->astNode->type;
        return JS_NewString(ctx, tag.c_str());
    }
    default:
        return JS_UNDEFINED;
    }
}

JSValue elPropSet(JSContext* ctx, JSValueConst this_val, JSValueConst val, int magic)
{
    Lookup lk = elemLookup(ctx, this_val);
    if (!lk.ctrl) return JS_EXCEPTION;
    switch (magic) {
    case kElText:
    case kElValue: {
        const std::string v = toUtf8(ctx, val);
        if (auto* t = dynamic_cast<loppe::Text*>(lk.ctrl.get())) { t->setText(v); return JS_UNDEFINED; }
        if (auto* i = dynamic_cast<loppe::Input*>(lk.ctrl.get())) { i->setValue(v, true); return JS_UNDEFINED; }
        return JS_ThrowTypeError(ctx, "el.%s: 该元素不持有文本", magic == kElText ? "text" : "value");
    }
    case kElDisplay: {
        const std::string v = toUtf8(ctx, val);
        if (lk.node && lk.node->astNode) lk.node->astNode->setAttribute("display", Attribute(v));
        lk.node->markDirty(loppe::DirtyFlag::Layout);
        return JS_UNDEFINED;
    }
    default:
        return JS_ThrowTypeError(ctx, "el: 只读属性不可写");
    }
}

// ── Element.on / off：与 ui.on 同一套登记（见下方事件区）──

// ===== ui.* =====

JSValue jsUiGet(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv)
{
    Ctx* c = C(ctx);
    if (argc < 1) return JS_ThrowTypeError(ctx, "ui.get(id)");
    const std::string id = toUtf8(ctx, argv[0]);
    Lookup lk = resolve(c, id);
    if (!lk.ctrl) return JS_ThrowReferenceError(ctx, "ui.get: no element '%s'", id.c_str());

    JSValue o = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, o, "id", JS_NewString(ctx, id.c_str()));
    std::string tag;
    if (lk.node && lk.node->astNode) tag = lk.node->astNode->type;
    JS_SetPropertyStr(ctx, o, "tag", JS_NewString(ctx, tag.c_str()));

    std::string text;
    std::string value;
    if (auto* t = dynamic_cast<loppe::Text*>(lk.ctrl.get())) text = t->getText();
    if (auto* i = dynamic_cast<loppe::Input*>(lk.ctrl.get())) value = i->getValue();
    JS_SetPropertyStr(ctx, o, "text", JS_NewString(ctx, text.c_str()));
    JS_SetPropertyStr(ctx, o, "value", JS_NewString(ctx, value.c_str()));

    JS_SetPropertyStr(ctx, o, "visible", JS_NewBool(ctx, lk.ctrl->isVisible()));

    float x = 0, y = 0, w = 0, h = 0;
    if (lk.node) {
        const loppe::ui::Rect b = lk.node->layout.getBorderBox();
        x = b.x; y = b.y; w = b.width; h = b.height;
    }
    JS_SetPropertyStr(ctx, o, "x", JS_NewFloat64(ctx, x));
    JS_SetPropertyStr(ctx, o, "y", JS_NewFloat64(ctx, y));
    JS_SetPropertyStr(ctx, o, "w", JS_NewFloat64(ctx, w));
    JS_SetPropertyStr(ctx, o, "h", JS_NewFloat64(ctx, h));
    return o;
}

JSValue jsUiText(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv)
{
    Ctx* c = C(ctx);
    if (argc < 1) return JS_ThrowTypeError(ctx, "ui.text(id)");
    const std::string id = toUtf8(ctx, argv[0]);
    Lookup lk = resolve(c, id);
    if (!lk.ctrl) return JS_ThrowReferenceError(ctx, "ui.text: no element '%s'", id.c_str());
    if (auto* t = dynamic_cast<loppe::Text*>(lk.ctrl.get()))
        return JS_NewString(ctx, t->getText().c_str());
    return JS_NewString(ctx, "");
}

JSValue jsUiSet(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv)
{
    Ctx* c = C(ctx);
    if (argc < 3) return JS_ThrowTypeError(ctx, "ui.set(id, prop, value)");
    const std::string id = toUtf8(ctx, argv[0]);
    const std::string prop = toUtf8(ctx, argv[1]);
    const std::string val = attrString(ctx, argv[2]);

    Lookup lk = resolve(c, id);
    if (!lk.ctrl) return JS_ThrowReferenceError(ctx, "ui.set: no element '%s'", id.c_str());

    // ① 控件自己就能吃下的属性：走它们的 setter（内部只脏自己，最省）
    if (prop == "text" || prop == "value") {
        if (auto* t = dynamic_cast<loppe::Text*>(lk.ctrl.get())) { t->setText(val); return JS_UNDEFINED; }
        if (auto* i = dynamic_cast<loppe::Input*>(lk.ctrl.get())) { i->setValue(val, true); return JS_UNDEFINED; }
    }

    // ② class：按空格切，整份替换（保留 .none 这类运行时状态由调用方自己管）
    if (prop == "class") {
        std::vector<std::string> cls;
        std::string cur;
        for (char ch : val) {
            if (ch == ' ') { if (!cur.empty()) { cls.push_back(cur); cur.clear(); } }
            else cur.push_back(ch);
        }
        if (!cur.empty()) cls.push_back(cur);
        lk.ctrl->setClassList(cls, true);
        return JS_UNDEFINED;
    }

    // ③ 其余：写回 AST + 标脏（PartialUpdate）
    if (!lk.node) return JS_ThrowInternalError(ctx, "ui.set: '%s' has no resolved node", id.c_str());
    if (lk.node->astNode) lk.node->astNode->setAttribute(prop, Attribute(val));
    lk.node->markDirty(flagsForProp(prop));
    return JS_UNDEFINED;
}

JSValue jsUiAddClass(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv)
{
    Ctx* c = C(ctx);
    if (argc < 2) return JS_ThrowTypeError(ctx, "ui.addClass(id, cls)");
    Lookup lk = resolve(c, toUtf8(ctx, argv[0]));
    if (!lk.ctrl) return JS_ThrowReferenceError(ctx, "ui.addClass: no such element");
    lk.ctrl->addClass(toUtf8(ctx, argv[1]));
    return JS_UNDEFINED;
}

JSValue jsUiRemoveClass(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv)
{
    Ctx* c = C(ctx);
    if (argc < 2) return JS_ThrowTypeError(ctx, "ui.removeClass(id, cls)");
    Lookup lk = resolve(c, toUtf8(ctx, argv[0]));
    if (!lk.ctrl) return JS_ThrowReferenceError(ctx, "ui.removeClass: no such element");
    lk.ctrl->removeClass(toUtf8(ctx, argv[1]));
    return JS_UNDEFINED;
}

JSValue jsUiToggleClass(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv)
{
    Ctx* c = C(ctx);
    if (argc < 2) return JS_ThrowTypeError(ctx, "ui.toggleClass(id, cls, on)");
    Lookup lk = resolve(c, toUtf8(ctx, argv[0]));
    if (!lk.ctrl) return JS_ThrowReferenceError(ctx, "ui.toggleClass: no such element");
    const std::string cls = toUtf8(ctx, argv[1]);
    const bool on = (argc >= 3) ? (JS_ToBool(ctx, argv[2]) != 0) : !lk.ctrl->hasClass(cls);
    if (on) lk.ctrl->addClass(cls);
    else lk.ctrl->removeClass(cls);
    return JS_UNDEFINED;
}

// ===== ui.render(lui)：交出整份页面文档 =====
//
// 页面文档由脚本产出，不是脚本逐个造控件：一次 render = 一份 .lui 全文，
// 宿主解析后整份替换该标签的文档（样式/布局/控件全部由引擎重建）。
//
// 为什么事件回调里不当场应用：派发方手里握着一条命中路径（裸指针），
// 回调里重建文档会把路径上的节点换掉 → 冒泡下一跳踩空。所以回调里只登记，
// pump（不在派发栈上）时再落地。
JSValue jsUiRender(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv)
{
    Ctx* c = C(ctx);
    if (argc < 1) return JS_ThrowTypeError(ctx, "ui.render(luiSource)");
    if (!c->documentHandler) return JS_ThrowPlainError(ctx, "ui.render: 不可用（宿主没有接文档）");

    const std::string lui = toUtf8(ctx, argv[0]);
    if (lui.empty()) return JS_ThrowTypeError(ctx, "ui.render: 空文档");

    if (c->inCallback) {
        c->pendingDoc = lui;
        c->hasPendingDoc = true;
        return JS_UNDEFINED;
    }

    c->busyStartMs = nowMs();
    const std::string error = c->documentHandler(lui);
    if (!error.empty()) return JS_ThrowPlainError(ctx, "ui.render: %s", error.c_str());
    invalidateElements(c);   // 文档整份替换：句柄缓存作废，下一次 query 重新建
    return JS_UNDEFINED;
}

// ===== ui.navigate(url)：页面内跳转 =====
//
// 只把 url 原样交给宿主：相对地址怎么解析、同标签还是新标签、记不记历史、
// 权限够不够，全是宿主的策略（这一层不该知道地址栏规则）。
// 宿主只有「受理 / 不受理」两种回答，不受理时把原因抛给脚本 —— 页面能 try/catch，
// 也能把错误显示在自己身上。

JSValue jsUiNavigate(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv)
{
    Ctx* c = C(ctx);
    if (argc < 1) return JS_ThrowTypeError(ctx, "ui.navigate(url)");
    if (!c->navigateHandler) return JS_ThrowPlainError(ctx, "ui.navigate: 不可用（宿主没有接导航）");

    const std::string url = toUtf8(ctx, argv[0]);
    const std::string error = c->navigateHandler(url);
    if (!error.empty()) return JS_ThrowPlainError(ctx, "ui.navigate: %s", error.c_str());
    return JS_UNDEFINED;
}

// ===== 事件 =====

std::string eventKey(const std::string& id, const std::string& ev) { return "#" + id + "|" + ev; }

JSValue buildEventObject(Ctx* c, loppe::UIEventSystem::UIEvent& e)
{
    JSContext* ctx = c->ctx;
    JSValue o = JS_NewObject(ctx);
    // id = 处理器绑定的那个元素（DOM 的 currentTarget）；targetId = 真正被点中的最深控件（DOM 的 target）。
    // 最深控件常常是没写 id 的内层文字 → 退回绑定的那个，别给脚本一个空串。
    const std::string bound = e.target ? e.target->getId() : std::string();
    std::string deepest = e.deepestTarget ? e.deepestTarget->getId() : std::string();
    if (deepest.empty()) deepest = bound;
    JS_SetPropertyStr(ctx, o, "id", JS_NewString(ctx, bound.c_str()));
    JS_SetPropertyStr(ctx, o, "targetId", JS_NewString(ctx, deepest.c_str()));
    JS_SetPropertyStr(ctx, o, "text", JS_NewString(ctx, e.text.c_str()));
    JS_SetPropertyStr(ctx, o, "key", JS_NewInt32(ctx, e.key));
    // 修饰键掩码（GLFW：1=Shift 2=Ctrl 4=Alt 8=Super）与指针按键（0=左 1=右 2=中）
    JS_SetPropertyStr(ctx, o, "mods", JS_NewInt32(ctx, e.mods));
    JS_SetPropertyStr(ctx, o, "button", JS_NewInt32(ctx, e.button));
    return o;
}

// 登记一个元素事件。ui.on 与 el.on 都走这里；同一 (id,event) 重绑 = 换回调。
// buttonFilter >= 0 时只放行该指针按键（右键菜单用：MouseUp + 只收右键）。
JSValue bindElementEvent(JSContext* ctx, const std::string& id, const std::string& ev,
                         JSValueConst fn, int buttonFilter = -1)
{
    Ctx* c = C(ctx);
    const std::string key = eventKey(id, ev);
    const std::string sel = "#" + id;

    auto it = c->handlers.find(key);
    const bool first = (it == c->handlers.end());
    if (first) c->handlers.emplace(key, JS_DupValue(ctx, fn));
    else {
        JS_FreeValue(ctx, it->second);           // 重绑 = 换回调
        it->second = JS_DupValue(ctx, fn);
    }
    if (!first) return JS_UNDEFINED;

    // shutdown() 之后场景里还留着这些回调：ctx 已销毁，必须靠 alive 标记整体失效。
    // 回调期间置 inCallback：这时不能重建文档（派发方握着命中路径的裸指针）。
    std::weak_ptr<bool> alive = c->alive;
    auto invoke = [c, key, alive, buttonFilter](loppe::UIEventSystem::UIEvent& e) {
        const auto ok = alive.lock();
        if (!ok || !*ok) return;
        if (buttonFilter >= 0 && (int)e.button != buttonFilter) return;
        auto hit = c->handlers.find(key);
        if (hit == c->handlers.end() || JS_IsUndefined(hit->second)) return;
        c->busyStartMs = nowMs();
        JSValue evObj = buildEventObject(c, e);
        const bool prevCallback = c->inCallback;
        c->inCallback = true;
        JSValue r = JS_Call(c->ctx, hit->second, JS_UNDEFINED, 1, &evObj);
        c->inCallback = prevCallback;
        JS_FreeValue(c->ctx, evObj);
        if (JS_IsException(r)) {
            const std::string msg = takeException(c->ctx);
            if (c->logSink) c->logSink(("[js] handler error: " + msg + "\n").c_str());
        }
        JS_FreeValue(c->ctx, r);
    };

    if (ev == "tap" || ev == "click") c->scene->onClick(sel, invoke);
    else if (ev == "input") c->scene->onInput(sel, invoke);
    else if (ev == "change") c->scene->onChange(sel, invoke);
    else if (ev == "scroll") c->scene->onScroll(sel, invoke);
    else if (ev == "mousedown") c->scene->onMouseDown(sel, invoke);
    else if (ev == "mouseup") c->scene->onMouseUp(sel, invoke);
    else if (ev == "keydown") c->scene->onKeyDown(sel, invoke);
    else if (ev == "keyup") c->scene->onKeyUp(sel, invoke);
    else if (ev == "focus") c->scene->onFocus(sel, invoke);
    else if (ev == "blur") c->scene->onBlur(sel, invoke);
    else return JS_ThrowTypeError(ctx, "on: unknown event '%s'", ev.c_str());
    return JS_UNDEFINED;
}

// contextmenu = 「右键 mouseup」的语法糖（引擎不发 contextmenu 事件，右键只有 MouseUp + button=1）
JSValue bindContextMenuEvent(JSContext* ctx, const std::string& id, JSValueConst fn)
{
    return bindElementEvent(ctx, id, "mouseup", fn, /*buttonFilter=*/1);
}

// 解绑：Scene 侧没有 off，只能把表项清成空壳 —— 回调还在场景里，但进来直接返回。
void unbindElementEvent(JSContext* ctx, const std::string& id, const std::string& ev)
{
    Ctx* c = C(ctx);
    auto it = c->handlers.find(eventKey(id, ev));
    if (it != c->handlers.end()) {
        JS_FreeValue(ctx, it->second);
        it->second = JS_UNDEFINED;
    }
}

JSValue jsUiOn(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv)
{
    if (argc < 3 || !JS_IsFunction(ctx, argv[2]))
        return JS_ThrowTypeError(ctx, "ui.on(id, event, fn)");
    const std::string ev = toUtf8(ctx, argv[1]);
    if (ev == "contextmenu") return bindContextMenuEvent(ctx, toUtf8(ctx, argv[0]), argv[2]);
    return bindElementEvent(ctx, toUtf8(ctx, argv[0]), ev, argv[2]);
}

// 「contextmenu」落到 mouseup 那条登记上（见 bindContextMenuEvent）
std::string offEventName(const std::string& ev) { return ev == "contextmenu" ? "mouseup" : ev; }

JSValue jsUiOff(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv)
{
    if (argc < 2) return JS_ThrowTypeError(ctx, "ui.off(id, event)");
    unbindElementEvent(ctx, toUtf8(ctx, argv[0]), offEventName(toUtf8(ctx, argv[1])));
    return JS_UNDEFINED;
}

// ===== ui.query(selector)：'#id' / '.class' → Element（找不到返回 null）=====
//
// .class 取**第一个**带 id 的命中节点（没写 id 的节点无法按 id 寻址，跳过）；
// 委托派发（event.target 的句柄化）在 MVP2 接上后不再依赖这条限制。

JSValue jsUiQuery(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv)
{
    Ctx* c = C(ctx);
    if (argc < 1) return JS_ThrowTypeError(ctx, "ui.query(selector)");
    const std::string sel = toUtf8(ctx, argv[0]);
    if (sel.empty()) return JS_NULL;

    if (sel[0] == '#') {
        const std::string id = sel.substr(1);
        Lookup lk = resolve(c, id);
        if (!lk.ctrl) return JS_NULL;
        return elementFor(c, id);
    }

    if (sel[0] == '.') {
        const std::string cls = sel.substr(1);
        auto* tree = c->scene ? c->scene->getResolvedTree() : nullptr;
        if (!tree || !tree->getRoot()) return JS_NULL;
        std::vector<loppe::ResolvedNode*> stack{ tree->getRoot() };
        while (!stack.empty()) {
            loppe::ResolvedNode* n = stack.back();
            stack.pop_back();
            if (auto ctrl = n->getControl().lock()) {
                if (!ctrl->getId().empty() && ctrl->hasClass(cls))
                    return elementFor(c, ctrl->getId());
            }
            for (loppe::ResolvedNode* ch : n->getChildren()) stack.push_back(ch);
        }
        return JS_NULL;
    }

    return JS_ThrowTypeError(ctx, "ui.query: 只支持 '#id' 或 '.class'");
}

// ── el.on / el.off：句柄版的事件登记（同 id 同 key，与 ui.on 共享一张表）──

JSValue jsElOn(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    auto* h = static_cast<ElementHandle*>(JS_GetOpaque2(ctx, this_val, valueClassId(this_val)));
    if (!h) return JS_ThrowTypeError(ctx, "not an Element");
    if (argc < 2 || !JS_IsFunction(ctx, argv[1]))
        return JS_ThrowTypeError(ctx, "el.on(event, fn)");
    const std::string ev = toUtf8(ctx, argv[0]);
    if (ev == "contextmenu") return bindContextMenuEvent(ctx, h->id, argv[1]);
    return bindElementEvent(ctx, h->id, ev, argv[1]);
}

JSValue jsElOff(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    auto* h = static_cast<ElementHandle*>(JS_GetOpaque2(ctx, this_val, valueClassId(this_val)));
    if (!h) return JS_ThrowTypeError(ctx, "not an Element");
    if (argc < 1) return JS_ThrowTypeError(ctx, "el.off(event)");
    unbindElementEvent(ctx, h->id, offEventName(toUtf8(ctx, argv[0])));
    return JS_UNDEFINED;
}

// ===== 定时器 =====

JSValue jsSetTimeout(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv)
{
    Ctx* c = C(ctx);
    if (argc < 2 || !JS_IsFunction(ctx, argv[0]))
        return JS_ThrowTypeError(ctx, "setTimeout(fn, ms)");
    int64_t ms = 0;
    JS_ToInt64(ctx, &ms, argv[1]);
    if (ms < 0) ms = 0;

    Timer t;
    t.id = c->nextTimerId++;
    t.dueMs = nowMs() + ms;
    t.fn = JS_DupValue(ctx, argv[0]);
    c->timers.push_back(t);
    return JS_NewInt32(ctx, t.id);
}

JSValue jsClearTimeout(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv)
{
    Ctx* c = C(ctx);
    if (argc < 1) return JS_UNDEFINED;
    int32_t id = 0;
    JS_ToInt32(ctx, &id, argv[0]);
    for (Timer& t : c->timers) {
        if (t.id == id) {
            JS_FreeValue(ctx, t.fn);
            t.fn = JS_UNDEFINED;
            t.cancelled = true;
        }
    }
    return JS_UNDEFINED;
}

// ===== net.get(url)：联网在宿主侧（权限门也在宿主侧），这里只搬字节 =====
//
// 失败必须抛「可捕获」的错误：quickjs-ng 把 InternalError 标成 uncatchable
// （见 quickjs.h 的注释），页面 try/catch 接不住 → 整份 app.js 直接中断。
// 权限拒绝用 DOMException(NotAllowedError)，与浏览器习惯一致；其余用普通 Error。

JSValue jsNetGet(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv)
{
    Ctx* c = C(ctx);
    if (argc < 1) return JS_ThrowTypeError(ctx, "net.get(url)");
    if (!c->netHandler) return JS_ThrowPlainError(ctx, "net.get: 不可用（宿主没有接网络）");

    const std::string url = toUtf8(ctx, argv[0]);
    std::string error;
    c->busyStartMs = nowMs();
    const std::string body = c->netHandler(url, &error);
    if (!error.empty()) {
        static const char* kDenied = "PermissionDenied";
        if (error.rfind(kDenied, 0) == 0) {
            return JS_ThrowDOMException(ctx, "NotAllowedError", "net.get: %s", error.c_str());
        }
        return JS_ThrowPlainError(ctx, "net.get: %s", error.c_str());
    }
    return JS_NewStringLen(ctx, body.data(), body.size());
}

// ===== console =====

JSValue jsConsoleLog(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv)
{
    Ctx* c = C(ctx);
    std::string line;
    for (int i = 0; i < argc; ++i) {
        if (i) line += ' ';
        line += toUtf8(ctx, argv[i]);
    }
    line += '\n';
    if (c->logSink) c->logSink(line.c_str());
    return JS_UNDEFINED;
}

// ===== 中断：单次执行不许长时间霸占 CPU =====
// 注意 quickjs-ng 的签名是 (JSContext*, void*)：与 Bellard 原版的 (JSRuntime*, void*) 不同
int interruptHandler(JSContext*, void* opaque)
{
    auto* c = static_cast<Ctx*>(opaque);
    if (!c || c->cfg.interruptMs <= 0) return 0;
    return (nowMs() - c->busyStartMs) > c->cfg.interruptMs ? 1 : 0;
}

#if defined(ESP_PLATFORM)
// 板上：JS 的堆全部走 PSRAM（内部 RAM 只有 512KB，QuickJS 全是几十字节的小分配）
void* jsCalloc(void*, size_t count, size_t size)
{
    return heap_caps_calloc(count, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}
void* jsMalloc(void*, size_t n) { return heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
void jsFree(void*, void* p) { heap_caps_free(p); }
void* jsRealloc(void*, void* p, size_t n)
{
    return heap_caps_realloc(p, n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}
size_t jsUsable(const void* p) { return heap_caps_get_allocated_size(const_cast<void*>(p)); }
const JSMallocFunctions kPsramMf{ jsCalloc, jsMalloc, jsFree, jsRealloc, jsUsable };
#endif

// quickjs-ng 的 JS_NewCFunctionMagic 固定按 generic_magic 解释回调；
// getter/setter_magic 要自己用 JSCFunctionType 联合体摆正函数指针槽位再交 JS_NewCFunction2。
JSValue newGetterMagic(JSContext* ctx, const char* name,
                       JSValue (*fn)(JSContext*, JSValueConst, int), int magic)
{
    JSCFunctionType ft{};
    ft.getter_magic = fn;
    return JS_NewCFunction2(ctx, ft.generic, name, 0, JS_CFUNC_getter_magic, magic);
}

JSValue newSetterMagic(JSContext* ctx, const char* name,
                       JSValue (*fn)(JSContext*, JSValueConst, JSValueConst, int), int magic)
{
    JSCFunctionType ft{};
    ft.setter_magic = fn;
    return JS_NewCFunction2(ctx, ft.generic, name, 1, JS_CFUNC_setter_magic, magic);
}

// ===== 模块系统（ES module + .lui 资源导入 + 'loppe' 虚拟模块）=====
//
// 入口脚本以 JS_EVAL_TYPE_MODULE 执行；import './x.js' / './x.lui' 按 URL 解析
// （相对路径以导入方为基准），取资源走 ResourceHandler（页面自己的代码图，不查网络权限）。
// .lui 后缀 → 模块导出默认字符串（文件原文）；'loppe' → 下面这个标准库源码。

// 'loppe' —— 页面脚本标准库。机制都在这里，不占引擎；页面照着用。
const char* kLoppeModuleSource = R"JS(// 'loppe' —— 页面脚本标准库（虚拟模块）。
//
// 响应式：signal 是状态的唯一来源；effect 自动跟踪它读过的 signal，
// signal 变了只重跑绑定它的 effect —— 不整份渲染、不手写 ui.set 循环。
// 组件：类 + 模板 + 生命周期，结构归模板（.lui 片段），行为归类。

let current = null;   // 正在跑的 effect（signal getter 里登记依赖）

export function signal(v) {
  const s = {
    _v: v,
    _subs: new Set(),
    get value() {
      if (current) { current._deps.add(s); s._subs.add(current); }
      return s._v;
    },
    set value(nv) {
      if (Object.is(nv, s._v)) return;
      s._v = nv;
      for (const run of [...s._subs]) run();
    },
    peek() { return s._v; },   // 读值不登记依赖（定时器、回调里用）
  };
  return s;
}

// effect(fn)：立刻跑一遍 fn；fn 里读到的 signal 任何一个变了就重跑。
// 返回 stop()（解除绑定；组件 detach 时自动调）。
export function effect(fn) {
  const run = () => {
    for (const d of run._deps) d._subs.delete(run);
    run._deps = new Set();
    const prev = current;
    current = run;
    try { fn(); } finally { current = prev; }
  };
  run._deps = new Set();
  run();
  return () => {
    for (const d of run._deps) d._subs.delete(run);
    run._deps = new Set();
  };
}

// computed(fn)：由别的 signal 推导出来的只读信号（对外只给 .value / .peek）。
export function computed(fn) {
  const s = signal(undefined);
  effect(() => { s.value = fn(); });
  return s;
}

// Component —— 组件基类。
//   template() 返回 .lui 片段；mount 默认把它插到挂载点下（ui.append，样式实例隔离）。
//   onMount 里绑事件、用 this.effect(...) 把状态绑到元素属性上。
//   detach() 拆掉：停掉全部 effect（引擎卸载时宿主也会调）。
export class Component {
  constructor(props) {
    this.props = props || {};
    this.parent = null;
    this.root = null;
    this._stops = [];
    this._mounted = false;
  }

  template() { return ""; }

  mount(parent) {
    const p = typeof parent === "string" ? ui.query(parent) : parent;
    if (!p) throw new Error("mount: 找不到挂载点");
    this.parent = p;
    const tpl = this.template();
    if (tpl) {
      const ids = ui.append(p.id, tpl);
      this.root = (ids && ids.length) ? ui.query("#" + ids[0]) : p;
    } else {
      this.root = p;
    }
    this._mounted = true;
    this.onMount();
  }

  effect(fn) {
    const stop = effect(fn);
    this._stops.push(stop);
    return stop;
  }

  detach() {
    for (const stop of this._stops) stop();
    this._stops = [];
    if (this._mounted) this.onDetach();
    this._mounted = false;
  }

  emit(name, data) {
    const fn = this.props["on" + name];
    if (typeof fn === "function") fn.call(this, data);
  }

  onMount() {}
  onDetach() {}
}
)JS";

// 把一段 .lui 片段挂到已有节点下（组件模板落位用）。返回新建顶层节点的 id 数组。
JSValue jsUiAppend(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv)
{
    Ctx* c = C(ctx);
    if (argc < 2) return JS_ThrowTypeError(ctx, "ui.append(parentId, dsl, index?)");
    const std::string parentId = toUtf8(ctx, argv[0]);
    const std::string dsl = toUtf8(ctx, argv[1]);
    int index = -1;
    if (argc >= 3) {
        int32_t v = -1;
        if (JS_ToInt32(ctx, &v, argv[2]) == 0) index = v;
    }
    c->busyStartMs = nowMs();
    loppe::LoppeParser parser;
    std::vector<loppe::ElementView> views = loppe::ComponentInstantiator::SpawnElementViewsUnderId(
        c->scene, parentId, parser.parse(dsl), c->runtime, index);
    JSValue arr = JS_NewArray(ctx);
    uint32_t i = 0;
    for (loppe::ElementView& ev : views) {
        if (!ev.valid()) continue;
        JS_SetPropertyUint32(ctx, arr, i++, JS_NewString(ctx, ev.id().c_str()));
    }
    return arr;
}

// el.remove()：从文档里摘掉这个节点（连子树）。句柄只存 id，摘掉后再用才抛 stale。
JSValue jsElRemove(JSContext* ctx, JSValueConst this_val, int, JSValueConst*)
{
    auto* h = static_cast<ElementHandle*>(JS_GetOpaque2(ctx, this_val, valueClassId(this_val)));
    if (!h) return JS_ThrowTypeError(ctx, "not an Element");
    Ctx* c = C(ctx);
    loppe::ResolvedNode* node = loppe::ResolvedNodeQuery::FindById(c->scene->getResolvedTree(), h->id);
    if (!node) return JS_ThrowReferenceError(ctx, "stale element '#%s'", h->id.c_str());
    c->busyStartMs = nowMs();
    loppe::ControlFactory::getInstance()->RetireResolvedNode(node, /*eraseFromAst=*/true);
    return JS_UNDEFINED;
}

// ── 模块加载 ──

char* jsStrndup(JSContext* ctx, const std::string& s)
{
    char* p = static_cast<char*>(js_malloc(ctx, s.size() + 1));
    if (!p) return nullptr;
    std::memcpy(p, s.c_str(), s.size() + 1);
    return p;
}

// URL 拼接：rel 是相对路径时以 base 所在目录为基准，支持 ./ ../ 与 / 开头（站点根）。
std::string urlJoin(const std::string& base, const std::string& rel)
{
    if (rel.find("://") != std::string::npos) return rel;
    if (base.empty()) return rel;

    std::string origin, path;
    const auto schemeEnd = base.find("://");
    const auto pathStart = base.find('/', schemeEnd == std::string::npos ? 0 : schemeEnd + 3);
    if (pathStart == std::string::npos) { origin = base; path = "/"; }
    else { origin = base.substr(0, pathStart); path = base.substr(pathStart); }

    std::string joined;
    if (!rel.empty() && rel[0] == '/') joined = rel;
    else {
        const auto slash = path.rfind('/');
        joined = path.substr(0, slash == std::string::npos ? 0 : slash + 1) + rel;
    }

    // 归一化 . 与 ..
    std::vector<std::string> segs;
    std::string cur;
    auto flush = [&]() {
        if (cur.empty() || cur == ".") { cur.clear(); return; }
        if (cur == "..") {
            if (!segs.empty()) segs.pop_back();
            cur.clear();
            return;
        }
        segs.push_back(cur);
        cur.clear();
    };
    for (char ch : joined) {
        if (ch == '/') flush();
        else cur += ch;
    }
    flush();

    std::string out = origin;
    for (const std::string& seg : segs) out += "/" + seg;
    if (out == origin) out += "/";
    return out;
}

// JS 字符串字面量（.lui 文本包成 export default 用）
std::string jsonQuote(const std::string& s)
{
    std::string out = "\"";
    for (unsigned char ch : s) {
        switch (ch) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (ch < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof buf, "\\u%04x", ch);
                out += buf;
            } else out += static_cast<char>(ch);
        }
    }
    out += "\"";
    return out;
}

bool endsWith(const std::string& s, const char* suffix)
{
    const size_t n = std::strlen(suffix);
    return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
}

JSModuleDef* compileModule(JSContext* ctx, const std::string& src, const char* name)
{
    // 同 eval：调用方缓冲必须 NUL 结尾，这里 std::string 天然满足
    JSValue val = JS_Eval(ctx, src.data(), src.size(), name,
                          JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    if (JS_IsException(val)) return nullptr;
    auto* m = static_cast<JSModuleDef*>(JS_VALUE_GET_PTR(val));
    JS_FreeValue(ctx, val);   // 模块定义由 runtime 持有，这里只放编译期的那份引用
    return m;
}

char* moduleNormalize(JSContext* ctx, const char* base, const char* name, void*)
{
    const std::string rel = name ? name : "";
    const std::string resolved = (rel == "loppe") ? rel : urlJoin(base ? base : "", rel);
    return jsStrndup(ctx, resolved);
}

JSModuleDef* moduleLoader(JSContext* ctx, const char* name, void* opaque)
{
    Ctx* c = static_cast<Ctx*>(opaque);
    const std::string moduleName = name ? name : "";

    if (moduleName == "loppe") {
        return compileModule(ctx, kLoppeModuleSource, "loppe");
    }

    std::string body, err;
    if (c->resourceHandler) body = c->resourceHandler(moduleName, &err);
    else if (c->netHandler) body = c->netHandler(moduleName, &err);
    else err = "宿主没有接资源取用";
    if (!err.empty()) {
        JS_ThrowReferenceError(ctx, "import '%s': %s", moduleName.c_str(), err.c_str());
        return nullptr;
    }

    if (endsWith(moduleName, ".lui")) {
        return compileModule(ctx, "export default " + jsonQuote(body) + ";", moduleName.c_str());
    }
    return compileModule(ctx, body, moduleName.c_str());
}

void installApi(Ctx* c)
{
    JSContext* ctx = c->ctx;

    JSValue global = JS_GetGlobalObject(ctx);

    // ── Element 类：注册到本 runtime + 建原型（方法 + 魔法属性）──
    JSClassDef elementDef{};
    elementDef.class_name = "Element";
    elementDef.finalizer = &elementFinalizer;
    const bool classOk = JS_NewClassID(c->rt, &c->elementClassId) >= 0 &&
                         JS_NewClass(c->rt, c->elementClassId, &elementDef) == 0;
    if (!classOk) {
        if (c->logSink) c->logSink("[js] Element class registration failed\n");
    } else {
        JSValue proto = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, proto, "addClass",
                          JS_NewCFunction(ctx, jsElAddClass, "addClass", 1));
        JS_SetPropertyStr(ctx, proto, "removeClass",
                          JS_NewCFunction(ctx, jsElRemoveClass, "removeClass", 1));
        JS_SetPropertyStr(ctx, proto, "toggleClass",
                          JS_NewCFunction(ctx, jsElToggleClass, "toggleClass", 2));
        JS_SetPropertyStr(ctx, proto, "hasClass",
                          JS_NewCFunction(ctx, jsElHasClass, "hasClass", 1));
        JS_SetPropertyStr(ctx, proto, "rect", JS_NewCFunction(ctx, jsElRect, "rect", 0));
        JS_SetPropertyStr(ctx, proto, "getAttr",
                          JS_NewCFunction(ctx, jsElGetAttr, "getAttr", 1));
        JS_SetPropertyStr(ctx, proto, "setAttr",
                          JS_NewCFunction(ctx, jsElSetAttr, "setAttr", 2));
        JS_SetPropertyStr(ctx, proto, "on", JS_NewCFunction(ctx, jsElOn, "on", 2));
        JS_SetPropertyStr(ctx, proto, "off", JS_NewCFunction(ctx, jsElOff, "off", 1));
        JS_SetPropertyStr(ctx, proto, "remove", JS_NewCFunction(ctx, jsElRemove, "remove", 0));

        // 可写属性：text / value / display（getter+setter 都走魔法函数）
        const struct { const char* name; int magic; } writable[] = {
            { "text", kElText }, { "value", kElValue }, { "display", kElDisplay },
        };
        for (const auto& p : writable) {
            JSValue g = newGetterMagic(ctx, p.name, elPropGet, p.magic);
            JSValue s = newSetterMagic(ctx, p.name, elPropSet, p.magic);
            JSAtom atom = JS_NewAtom(ctx, p.name);
            // JS_DefinePropertyGetSet 自己吞掉 g/s 的引用（内部末尾 FreeValue），别再放
            JS_DefinePropertyGetSet(ctx, proto, atom, g, s, JS_PROP_C_W_E);
            JS_FreeAtom(ctx, atom);
        }
        // 只读属性：visible / tag / id
        const struct { const char* name; int magic; } readonly[] = {
            { "visible", kElVisible }, { "tag", kElTag }, { "id", kElId },
        };
        for (const auto& p : readonly) {
            JSValue g = newGetterMagic(ctx, p.name, elPropGet, p.magic);
            JSAtom atom = JS_NewAtom(ctx, p.name);
            // 裸 JS_DefineProperty 不吞引用：必须带 HAS_GET 才会真装上 getter，用完自己放
            JS_DefineProperty(ctx, proto, atom, JS_UNDEFINED, g, JS_UNDEFINED,
                              JS_PROP_HAS_GET | JS_PROP_ENUMERABLE | JS_PROP_CONFIGURABLE);
            JS_FreeAtom(ctx, atom);
            JS_FreeValue(ctx, g);
        }

        c->elementProto = proto;   // 常驻引用，shutdown 时释放
    }

    JSValue ui = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, ui, "render", JS_NewCFunction(ctx, jsUiRender, "render", 1));
    JS_SetPropertyStr(ctx, ui, "query", JS_NewCFunction(ctx, jsUiQuery, "query", 1));
    JS_SetPropertyStr(ctx, ui, "get", JS_NewCFunction(ctx, jsUiGet, "get", 1));
    JS_SetPropertyStr(ctx, ui, "set", JS_NewCFunction(ctx, jsUiSet, "set", 3));
    JS_SetPropertyStr(ctx, ui, "text", JS_NewCFunction(ctx, jsUiText, "text", 1));
    JS_SetPropertyStr(ctx, ui, "addClass", JS_NewCFunction(ctx, jsUiAddClass, "addClass", 2));
    JS_SetPropertyStr(ctx, ui, "removeClass", JS_NewCFunction(ctx, jsUiRemoveClass, "removeClass", 2));
    JS_SetPropertyStr(ctx, ui, "toggleClass", JS_NewCFunction(ctx, jsUiToggleClass, "toggleClass", 3));
    JS_SetPropertyStr(ctx, ui, "on", JS_NewCFunction(ctx, jsUiOn, "on", 3));
    JS_SetPropertyStr(ctx, ui, "off", JS_NewCFunction(ctx, jsUiOff, "off", 2));
    JS_SetPropertyStr(ctx, ui, "append", JS_NewCFunction(ctx, jsUiAppend, "append", 3));
    JS_SetPropertyStr(ctx, ui, "navigate", JS_NewCFunction(ctx, jsUiNavigate, "navigate", 1));
    JS_SetPropertyStr(ctx, global, "ui", ui);

    JS_SetPropertyStr(ctx, global, "setTimeout", JS_NewCFunction(ctx, jsSetTimeout, "setTimeout", 2));
    JS_SetPropertyStr(ctx, global, "clearTimeout",
                      JS_NewCFunction(ctx, jsClearTimeout, "clearTimeout", 1));

    JSValue console = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, console, "log", JS_NewCFunction(ctx, jsConsoleLog, "log", 1));
    JS_SetPropertyStr(ctx, global, "console", console);

    JS_FreeValue(ctx, global);
}

void installNetApi(Ctx* c)
{
    JSContext* ctx = c->ctx;
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue net = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, net, "get", JS_NewCFunction(ctx, jsNetGet, "get", 1));
    JS_SetPropertyStr(ctx, global, "net", net);
    JS_FreeValue(ctx, global);
}

} // namespace

struct ScriptHost::Impl : Ctx {};

ScriptHost::ScriptHost() : impl_(new Impl()) {}
ScriptHost::~ScriptHost() { shutdown(); }

bool ScriptHost::init(ScriptScene* scene, DocumentRuntime* runtime, const Config& cfg)
{
    if (impl_->ready) return false;
    if (!scene || !runtime) return false;

    Ctx* c = impl_.get();
    c->scene = scene;
    c->runtime = runtime;
    c->cfg = cfg;
    // 管道 stdout 是全缓冲：不 flush 的话日志会一直堆在 libc 缓冲里
    c->logSink = [](const char* s) { std::fputs(s, stdout); std::fflush(stdout); };
    c->alive = std::make_shared<bool>(true);   // 重新 init 也要把旧回调彻底断掉

#if defined(ESP_PLATFORM)
    c->rt = JS_NewRuntime2(&kPsramMf, nullptr);
#else
    c->rt = JS_NewRuntime();
#endif
    if (!c->rt) return false;

    JS_SetMemoryLimit(c->rt, cfg.memoryLimitBytes);
    JS_SetMaxStackSize(c->rt, cfg.maxStackBytes);
    JS_SetInterruptHandler(c->rt, &interruptHandler, c);
    JS_SetModuleLoaderFunc(c->rt, moduleNormalize, moduleLoader, c);

    c->ctx = JS_NewContext(c->rt);
    if (!c->ctx) {
        JS_FreeRuntime(c->rt);
        c->rt = nullptr;
        return false;
    }
    JS_SetContextOpaque(c->ctx, c);

    installApi(c);
    installNetApi(c);
    c->ready = true;
    return true;
}

bool ScriptHost::ready() const { return impl_ && impl_->ready; }

bool ScriptHost::eval(const std::string& source, const char* filename, std::string* error)
{
    return eval(source.data(), source.size(), filename, error);
}

bool ScriptHost::eval(const char* data, size_t len, const char* filename, std::string* error)
{
    Ctx* c = impl_.get();
    if (!c->ready) {
        if (error) *error = "ScriptHost not initialized";
        return false;
    }
    if (!data) {
        if (error) *error = "eval: null source";
        return false;
    }
    // quickjs-ng 直接在**调用方缓冲**上做词法分析，而它的 EOF 判定是
    // `*p == '\0' && p >= buf_end` —— 也就是说必须由调用方保证 src[len] == '\0'。
    // 不拷一份、直接传裸指针（比如从文件读进 PSRAM 的脚本）时，源码尾字节后面是
    // 上一次残留的数据，词法分析会越过结尾继续扫，报出「invalid UTF-8 sequence /
    // unexpected end of string」这类与源码无关的错。这里统一拷到 NUL 结尾的缓冲再交出去。
    c->evalBuf.assign(data, len);
    c->busyStartMs = nowMs();
    const char* file = filename ? filename : "<eval>";
    JSValue r = JS_Eval(c->ctx, c->evalBuf.data(), c->evalBuf.size(), file, JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(r)) {
        if (error) *error = takeException(c->ctx);
        JS_FreeValue(c->ctx, r);
        return false;
    }
    JS_FreeValue(c->ctx, r);
    return true;
}

bool ScriptHost::evalToString(const std::string& source, std::string* out, std::string* error)
{
    Ctx* c = impl_.get();
    if (!c->ready) {
        if (error) *error = "ScriptHost not initialized";
        return false;
    }
    c->busyStartMs = nowMs();
    JSValue r = JS_Eval(c->ctx, source.c_str(), source.size(), "<eval>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(r)) {
        if (error) *error = takeException(c->ctx);
        JS_FreeValue(c->ctx, r);
        return false;
    }
    if (out) *out = toUtf8(c->ctx, r);
    JS_FreeValue(c->ctx, r);
    return true;
}

bool ScriptHost::evalDocument(const std::string& source, const char* filename,
                              std::string* document, std::string* error)
{
    Ctx* c = impl_.get();
    if (document) document->clear();
    if (!c->ready) {
        if (error) *error = "ScriptHost not initialized";
        return false;
    }
    c->evalBuf.assign(source);
    c->busyStartMs = nowMs();
    // 入口按 ES 模块执行（import 可用）；filename 传脚本 URL，模块解析以它为基准
    const char* file = filename ? filename : "<eval>";
    JSValue r = JS_Eval(c->ctx, c->evalBuf.data(), c->evalBuf.size(), file, JS_EVAL_TYPE_MODULE);
    if (JS_IsException(r)) {
        if (error) *error = takeException(c->ctx);
        JS_FreeValue(c->ctx, r);
        return false;
    }
    JS_FreeValue(c->ctx, r);
    return true;
}

bool ScriptHost::pump()
{
    Ctx* c = impl_.get();
    if (!c->ready) return false;

    bool didWork = false;

    // ⓪ 事件回调里登记过的文档（回调栈上不能重建，见 jsUiRender）
    if (c->hasPendingDoc) {
        std::string doc;
        doc.swap(c->pendingDoc);
        c->hasPendingDoc = false;
        c->busyStartMs = nowMs();
        const std::string err = c->documentHandler ? c->documentHandler(doc)
                                                  : std::string("宿主没有接文档");
        if (!err.empty() && c->logSink) c->logSink(("[js] render error: " + err + "\n").c_str());
        else loppe_script::invalidateElements(c);
        didWork = true;
    }

    // ① 微任务（Promise / async / await）
    for (int i = 0; i < c->cfg.maxJobsPerPump; ++i) {
        JSContext* jobCtx = nullptr;
        c->busyStartMs = nowMs();
        const int r = JS_ExecutePendingJob(c->rt, &jobCtx);
        if (r < 0) {
            const std::string msg = takeException(jobCtx ? jobCtx : c->ctx);
            if (c->logSink) c->logSink(("[js] unhandled rejection: " + msg + "\n").c_str());
            break;
        }
        if (r == 0) break;                       // 队列空了
        didWork = true;
    }

    // ② 到期定时器
    const int64_t now = nowMs();
    int fired = 0;
    for (size_t i = 0; i < c->timers.size() && fired < c->cfg.maxTimersPerPump;) {
        Timer& t = c->timers[i];
        if (t.cancelled || t.dueMs > now) { ++i; continue; }

        if (!JS_IsUndefined(t.fn)) {
            c->busyStartMs = nowMs();
            JSValue r = JS_Call(c->ctx, t.fn, JS_UNDEFINED, 0, nullptr);
            if (JS_IsException(r)) {
                const std::string msg = takeException(c->ctx);
                if (c->logSink) c->logSink(("[js] timer error: " + msg + "\n").c_str());
            }
            JS_FreeValue(c->ctx, r);
        }
        ++fired;
        didWork = true;

        if (t.intervalMs > 0) {                  // setTimeout 语义：一次性
            t.dueMs = now + t.intervalMs;
            ++i;
        } else {
            JS_FreeValue(c->ctx, t.fn);
            c->timers.erase(c->timers.begin() + static_cast<long>(i));
        }
    }
    return didWork;
}

size_t ScriptHost::bytesUsed() const
{
    Ctx* c = impl_.get();
    if (!c->ready) return 0;
    JSMemoryUsage u{};
    JS_ComputeMemoryUsage(c->rt, &u);
    return static_cast<size_t>(u.malloc_size);
}

void ScriptHost::collectGarbage()
{
    Ctx* c = impl_.get();
    if (c->ready) JS_RunGC(c->rt);
}

void ScriptHost::setLogSink(std::function<void(const char*)> sink)
{
    if (impl_) impl_->logSink = std::move(sink);
}

void ScriptHost::setDocumentHandler(DocumentHandler handler)
{
    if (impl_) impl_->documentHandler = std::move(handler);
}

void ScriptHost::setNetHandler(NetHandler handler)
{
    if (impl_) impl_->netHandler = std::move(handler);
}

void ScriptHost::setResourceHandler(ResourceHandler handler)
{
    if (impl_) impl_->resourceHandler = std::move(handler);
}

void ScriptHost::setNavigateHandler(NavigateHandler handler)
{
    if (impl_) impl_->navigateHandler = std::move(handler);
}

void ScriptHost::invalidateElements()
{
    Ctx* c = impl_.get();
    if (c && c->ready) loppe_script::invalidateElements(c);
}

void ScriptHost::shutdown()
{
    Ctx* c = impl_.get();
    if (!c || !c->ready) return;

    if (c->alive) *c->alive = false;   // 场景里残留的回调从这一刻起全是空操作
    for (auto& kv : c->handlers) JS_FreeValue(c->ctx, kv.second);
    c->handlers.clear();
    for (Timer& t : c->timers) JS_FreeValue(c->ctx, t.fn);
    c->timers.clear();
    loppe_script::invalidateElements(c);
    if (!JS_IsUndefined(c->elementProto)) JS_FreeValue(c->ctx, c->elementProto);
    c->elementProto = JS_UNDEFINED;
    c->netHandler = nullptr;

    JS_FreeContext(c->ctx);
    JS_FreeRuntime(c->rt);
    c->ctx = nullptr;
    c->rt = nullptr;
    c->ready = false;
}

} // namespace loppe_script
