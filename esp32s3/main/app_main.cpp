// app_main.cpp — esp32s3：把 loppe-ui 引擎跑在 ESP32-S3 上（最小骨架）
//
// 界面在 main/main.lui：构建时由 CMake 的 EMBED_FILES 编进固件（见 main/CMakeLists.txt）——
// 改界面只改它，不用动这个文件。这个文件只管三件**板级**的事：
//   ① 板级驱动：LCD（ST7789）+ 触摸（XPT2046）—— 引脚/初始化在本目录的 .cpp 里改；
//   ② 主循环：采样触摸 → 交给引擎 → tick() → 按脏区刷屏；
//   ③ 脏区策略：外扩几像素 / 最多几块 / 面积占比多少就整屏（见下面 Config）。
// ⚠ tick() 不能省：删了就没有视觉反馈（触摸像没反应）。
// 屏幕/引脚在**本目录** lcd_st7789.cpp / touch_xpt2046.cpp（按你的板卡改）。
//
// 界面可以有多份（见 docs/loppe-ui/LOPPE_MULTI_UI.md）：
//   · 多列几份 EMBED_FILES，每份一对 _binary_... 符号，用 uiText() 取文本；
//   · 换整屏 = 再调一次 s_host.loadUi(...)；
//   · 换成「外壳 + 每页一份」＝ 外壳里留一个 panel(id="page-slot")，页面用
//     ComponentInstantiator::SpawnElementViewUnderId(s_host.scene(), "page-slot", 页面文本, &s_host.runtime())
//     挂上去，换页时先对上次返回的 ElementView 调 remove()。
#include <cstddef>
#include <cstdint>

// ---- loppe 引擎（来自预编译包：thirdparty/embedded/esp32/include）+ 位图字库 ----
#include "platform/embedded_host.h"   // 引擎接入就这一个头
#include "embedded_font.h"            // kEmbeddedFont

// ---- 板级驱动（本目录）----
#include "lcd_st7789.h"
#include "touch_xpt2046.h"

#if defined(ESP_PLATFORM)
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#define APP_TAG "esp32s3"
#endif

// 屏幕尺寸（ST7789 240x320）
#define LCD_W 240
#define LCD_H 320

// 界面：main/main.lui 由 CMake 的 EMBED_FILES 编进来（符号名 = 文件名 + _start/_end）
extern const uint8_t kMainLuiStart[] asm("_binary_main_lui_start");
extern const uint8_t kMainLuiEnd[] asm("_binary_main_lui_end");

static uint16_t* s_framebuffer = nullptr;      // 帧缓冲（PSRAM）
static loppe_embedded::St7789Lcd s_lcd;        // 板级 LCD
static loppe_embedded::Xpt2046Touch s_touch;   // 板级触摸（本目录驱动）
static loppe_embedded::EmbeddedHost s_host;    // 引擎 + 场景 + 脏区

#if defined(ESP_PLATFORM)
extern "C" void app_main(void)
{
    ESP_LOGI(APP_TAG, "starting (frame=%dx%d)", LCD_W, LCD_H);

    // 1) 帧缓冲（PSRAM 优先，失败回退内部 RAM）+ LCD + 触摸
    s_framebuffer = static_cast<uint16_t*>(
        heap_caps_malloc(sizeof(uint16_t) * LCD_W * LCD_H, MALLOC_CAP_SPIRAM));
    if (!s_framebuffer) s_framebuffer = static_cast<uint16_t*>(malloc(sizeof(uint16_t) * LCD_W * LCD_H));
    if (!s_framebuffer) {
        ESP_LOGE(APP_TAG, "no memory for framebuffer");
        abort();
    }

    s_lcd.init(LCD_W, LCD_H);
    // 触摸必须在 s_lcd.init() 之后：它复用 LCD 已经初始化好的 SPI 总线
    if (!s_touch.init(LCD_W, LCD_H))
        ESP_LOGE(APP_TAG, "touch init failed (check T_CS pin)");

    // 2) 引擎 + 界面
    loppe_embedded::EmbeddedHost::Config cfg;
    cfg.width = LCD_W;
    cfg.height = LCD_H;
    cfg.framebuffer = s_framebuffer;
    cfg.font = &loppe_embedded::kEmbeddedFont;
    cfg.clearColor = loppe_embedded::Framebuffer::rgb888To565(0x22, 0x22, 0x22);
    // 脏区策略（按你的刷屏带宽调）
    cfg.dirtyMargin = 2;        // 每块外扩像素
    cfg.maxDirtyRects = 4;      // 一帧超过 4 块 → 整屏刷
    cfg.fullFlushPercent = 60;  // 脏区面积 ≥ 整屏 60% → 整屏刷
    if (!s_host.init(cfg)) {
        ESP_LOGE(APP_TAG, "engine init failed (framebuffer / font?)");
        abort();
    }

    if (!s_host.loadUi(reinterpret_cast<const char*>(kMainLuiStart),
                       static_cast<size_t>(kMainLuiEnd - kMainLuiStart))) {
        ESP_LOGE(APP_TAG, "main/main.lui parse failed (empty AST)");
    }

    // 想加交互就在这里绑（选择器 = #id，跨控件重建依然有效）：
    //   s_host.scene()->onClick("#btn-hello", [](loppe::UIEventSystem::UIEvent&) { … });
    // 运行时改文字：s_host.scene()->getControlManager()->findControlById("lb-hello")
    //               → dynamic_cast<loppe::Text*> → setText(…)

    // 3) 主循环：触摸 → 引擎 → 画 → 只刷脏区
    while (true) {
        int x = 0, y = 0;
        const bool touched = s_touch.read(x, y);   // 有没有摸由驱动判
        s_host.touchSample(touched, x, y);         // 触摸 → 鼠标事件（按下 / 移动 / 抬手 + 清指针）
        s_host.tick();                             // 消费更新计划 + 有脏区才重画

        if (!s_host.needsFullFlush() && s_host.dirtyRects().empty()) {
            vTaskDelay(pdMS_TO_TICKS(16));         // 屏幕没变化：不刷屏
            continue;
        }

        if (s_host.needsFullFlush()) {
            s_lcd.flush(s_host.framebuffer());     // 整屏：重建/重排/首帧/脏区太碎
        } else {
            for (const loppe::ui::Rect& r : s_host.dirtyRects()) {
                s_lcd.flushRegion(s_host.framebuffer(), static_cast<int>(r.x), static_cast<int>(r.y),
                                  static_cast<int>(r.width), static_cast<int>(r.height));
            }
        }
        s_host.clearDamage();                      // 刷完清账

        vTaskDelay(pdMS_TO_TICKS(1));              // 有变化就尽快出下一帧
    }
}
#endif