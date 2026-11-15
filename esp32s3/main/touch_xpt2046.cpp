/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

// touch_xpt2046.cpp — XPT2046 电阻触摸驱动（ESP-IDF 实现）。
//
// 接线（与 LCD 共用 SPI 总线）：
//   T_CLK -> 和 LCD 的 SCLK 并在一起（lcd_st7789.cpp 里的 PIN_SCLK = 12）
//   T_DIN -> 和 LCD 的 MOSI 并在一起（PIN_MOSI = 11）
//   T_DO  -> 和 LCD 的 MISO 并在一起（PIN_MISO = 13）
//   T_CS  -> 单独一根 GPIO（下面 PIN_TOUCH_CS，不能和 LCD 的 CS 重复）
//   T_IRQ -> 可选（下面 PIN_TOUCH_IRQ）；不接就靠采样值判断按下
#include "touch_xpt2046.h"

#if defined(ESP_PLATFORM)
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#endif

namespace loppe_embedded {

#if defined(ESP_PLATFORM)

// ---- 引脚：按你的板卡改 ----
#define TOUCH_HOST SPI2_HOST  // 与 LCD 同一条总线（见 lcd_st7789.cpp 的 LCD_HOST）
#define PIN_TOUCH_CS 14       // T_CS
#define PIN_TOUCH_IRQ -1      // T_IRQ：接了 T_IRQ 就填对应 GPIO（如 15，能省掉空闲时的 SPI 轮询），没接保持 -1

// ---- 标定默认值（已按实测方向设好；换别的屏/装法不对时按下面的说明调）----
// X_MIN/X_MAX、Y_MIN/Y_MAX = 该轴原始值的两个端点（按住屏幕两个边缘各读一次，
//   小的填 MIN、大的填 MAX），MIN 和 MAX 本身只表示原始值大小，和屏幕左右上下无关；
#define TOUCH_CAL_X_MIN 200
#define TOUCH_CAL_X_MAX 3700
#define TOUCH_CAL_Y_MIN 250
#define TOUCH_CAL_Y_MAX 3750
// 方向：SWAP_XY = 两个轴交叉了（手指左右动、准星上下动）；
//       INVERT_X/Y = 某一根轴反了（手指往左、准星往右 → 翻转 INVERT_X）。
#define TOUCH_CAL_SWAP_XY false
#define TOUCH_CAL_INVERT_X true
#define TOUCH_CAL_INVERT_Y false
// 落点整体偏移（像素）：准星/按下的位置相对手指固定偏一点点（常见于便宜面板、
// 或手指接触点与 LCD 像素本来就不重合）→ 用这两个微调，别去动 MIN/MAX。
#define TOUCH_CAL_OFFSET_X 0
#define TOUCH_CAL_OFFSET_Y 0

// ---- 采样参数 ----
#define TOUCH_SPI_HZ (1 * 1000 * 1000)  // XPT2046 上限约 2MHz，1MHz 稳
#define TOUCH_SAMPLES 4                 // 每次读几个样本取均值
#define TOUCH_MIN_VALID 100             // 有效原始值下限（没按时通常落在 0 或 4095 附近）
#define TOUCH_MAX_VALID 4000            // 有效原始值上限
#define TOUCH_SPREAD_MAX 80             // 同一帧里 X（或 Y）样本的最大差：超了当噪声，不算触摸

// ---- 「到底有没有摸」：压力（Z1/Z2）判定 + 滞回 ----
// 电阻屏**没被摸时 X/Y 是悬空的**，ADC 读出来的值会乱飘，而且常常贴着一端（→ 映射到屏幕角落）。
// 只靠「X/Y 落在量程内」判断不了有没有按：真机现象就是「不碰屏幕准星也在角落里飘、按下对不上」。
// 所以要额外读接触电阻 Z1/Z2，用标准判据：
//     z = z1 + 4095 - z2        （没摸：z1 小、z2 接近满量程 → z 小；摸上：z 明显变大）
//     摸上了 ⟺ z ≥ TOUCH_Z_MIN
// 阈值 400 是各家 XPT2046 驱动通用的经验值（Paul Stoffregen 的库里就是 Z_THRESHOLD 400）；
// 拿不准就看串口每秒那行 raw ... z=... —— 没摸时的 z 与按上去时的 z 差得越多越好调。
//
// **滞回（TOUCH_Z_RELEASE）**：手指/牙签连点时接触不会完全断开，z 会在阈值附近抖 ——
// 只用单阈值会出现两种坏结果：还按着却判成松手（一次点击变两次）、已经抬了却没掉到阈值以下
// （松手检测不到 → 后续点击全丢）。所以「按上」用高阈值、**「松开」用低阈值**。
#define TOUCH_USE_PRESSURE 1  // 1 = 用 Z1/Z2 判定有没有摸（推荐）；0 = 只看 X/Y（会被悬空读数骗）
#define TOUCH_Z_MIN 400       // z ≥ 它 = 按上了（调大更严）
#define TOUCH_Z_RELEASE 200   // z < 它 = 松手了（必须 ≤ TOUCH_Z_MIN；这是滞回的另一半）

// XPT2046 控制字节：S=1、12bit、差分、PD1PD0=01（ADC 保持开启，读数更稳）
//   A2A1A0 = 101 -> X 位置；001 -> Y 位置；011 -> Z1；100 -> Z2（Z1/Z2 用差分档）
#define XPT_CMD_X 0xD1
#define XPT_CMD_Y 0x91
#define XPT_CMD_Z1 0xB1
#define XPT_CMD_Z2 0xC1

static const char* TAG = "xpt2046";
static spi_device_handle_t s_touchSpi = nullptr;

// 「按着」的滞回状态 + 最后一次可信坐标：
// 按着的时候，个别帧样本不可信（噪声/被拒）**不该被当成松手** —— 沿用上一帧的坐标继续报「按着」，
// 不然一次抖动会变成「松手 + 再按下」＝一次点击变两次（连点时尤其明显）。
static bool     s_active = false;
static uint16_t s_lastRawX = 0;
static uint16_t s_lastRawY = 0;

// 发 1 字节命令 + 读 2 字节：XPT2046 在命令后先输出 1 个 BUSY 位，再是 12 位数据，
// 所以把收到的 16 位整体右移 3 位再取低 12 位。
static uint16_t xpt_transfer(uint8_t cmd)
{
    uint8_t tx[3] = { cmd, 0x00, 0x00 };
    uint8_t rx[3] = { 0x00, 0x00, 0x00 };
    spi_transaction_t t{};
    t.length = 8 * 3;
    t.tx_buffer = tx;
    t.rx_buffer = rx;
    spi_device_transmit(s_touchSpi, &t);
    return static_cast<uint16_t>((((static_cast<uint16_t>(rx[1]) << 8) | rx[2]) >> 3) & 0x0FFF);
}

static bool rawValid(uint16_t v)
{
    return v > TOUCH_MIN_VALID && v < TOUCH_MAX_VALID;
}

// 原始值 -> 0..1（超出标定范围就夹住）
static float axisToUnit(uint16_t raw, uint16_t lo, uint16_t hi, bool invert)
{
    if (hi <= lo) return 0.0f;
    float t = (static_cast<float>(raw) - static_cast<float>(lo)) / static_cast<float>(hi - lo);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return invert ? 1.0f - t : t;
}

bool Xpt2046Touch::init(int screenWidth, int screenHeight)
{
    screenW_ = screenWidth;
    screenH_ = screenHeight;

    cal_.xMin = TOUCH_CAL_X_MIN;
    cal_.xMax = TOUCH_CAL_X_MAX;
    cal_.yMin = TOUCH_CAL_Y_MIN;
    cal_.yMax = TOUCH_CAL_Y_MAX;
    cal_.swapXY = TOUCH_CAL_SWAP_XY;
    cal_.invertX = TOUCH_CAL_INVERT_X;
    cal_.invertY = TOUCH_CAL_INVERT_Y;
    cal_.offsetX = TOUCH_CAL_OFFSET_X;
    cal_.offsetY = TOUCH_CAL_OFFSET_Y;

    // 挂在 LCD 已经初始化好的那条总线上：mode/时钟是「每设备」的，
    // XPT2046 用 mode 0 + 1MHz，LCD 保持 mode 3 + 20MHz，互不影响。
    spi_device_interface_config_t devcfg{};
    devcfg.clock_speed_hz = TOUCH_SPI_HZ;
    devcfg.mode = 0;
    devcfg.spics_io_num = PIN_TOUCH_CS;
    devcfg.queue_size = 3;
    if (spi_bus_add_device(TOUCH_HOST, &devcfg, &s_touchSpi) != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_add_device failed (T_CS=%d)，确认它和 LCD 的 CS 不是同一个引脚",
                 PIN_TOUCH_CS);
        s_touchSpi = nullptr;
        return false;
    }

#if PIN_TOUCH_IRQ >= 0
    gpio_config_t io{};
    io.pin_bit_mask = 1ULL << PIN_TOUCH_IRQ;
    io.mode = GPIO_MODE_INPUT;
    io.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io);
#endif

    ESP_LOGI(TAG, "ready: T_CS=%d T_IRQ=%d (SPI%d shared with LCD, %dHz)",
             PIN_TOUCH_CS, PIN_TOUCH_IRQ, (int)TOUCH_HOST, TOUCH_SPI_HZ);
    return true;
}

bool Xpt2046Touch::readRaw(uint16_t& rawX, uint16_t& rawY)
{
    if (!s_touchSpi) return false;

#if PIN_TOUCH_IRQ >= 0
    if (gpio_get_level((gpio_num_t)PIN_TOUCH_IRQ) != 0) return false; // T_IRQ 高 = 没按下
#endif

    // ① 【按着没按着】只看接触电阻 z（带滞回）——这是唯一可信的判据：X/Y 悬空时也会读到量程内的乱值。
    const uint16_t z1 = xpt_transfer(XPT_CMD_Z1);
    const uint16_t z2 = xpt_transfer(XPT_CMD_Z2);
    const int32_t z = static_cast<int32_t>(z1) + 4095 - static_cast<int32_t>(z2);

    trace_.z1 = z1;
    trace_.z2 = z2;
    trace_.z = z;

#if TOUCH_USE_PRESSURE
    if (s_active) {
        if (z < TOUCH_Z_RELEASE) s_active = false;   // 松手：z 掉到释放阈值以下（滞回的下半）
    } else if (z >= TOUCH_Z_MIN) {
        s_active = true;                             // 按上：z 到按下阈值（滞回的上半）
    }
    if (!s_active) {
        trace_.touched = false;
        trace_.rawX = trace_.rawY = 0;
        s_lastRawX = s_lastRawY = 0;
        return false;
    }
#endif

    // ② 读 X/Y 样本：按着的时候才采
    uint32_t sumX = 0, sumY = 0;
    int valid = 0;
    uint16_t xLo = 0xFFFF, xHi = 0, yLo = 0xFFFF, yHi = 0;
    for (int i = 0; i < TOUCH_SAMPLES; ++i) {
        const uint16_t x = xpt_transfer(XPT_CMD_X);
        const uint16_t y = xpt_transfer(XPT_CMD_Y);
        if (!rawValid(x) || !rawValid(y)) continue; // 这次采样不在有效范围（多半是没按）
        sumX += x;
        sumY += y;
        if (x < xLo) xLo = x;
        if (x > xHi) xHi = x;
        if (y < yLo) yLo = y;
        if (y > yHi) yHi = y;
        ++valid;
    }

    // 同一帧里样本自己都不一致（差得离谱）→ 是噪声，不是手指（手指不会几微秒内跳几十像素）
    const bool noisy = valid > 0 &&
                       ((xHi - xLo) > TOUCH_SPREAD_MAX || (yHi - yLo) > TOUCH_SPREAD_MAX);
    const bool usable = (valid >= 2) && !noisy;

    trace_.xLo = xLo; trace_.xHi = xHi;
    trace_.yLo = yLo; trace_.yHi = yHi;

    if (usable) {
        s_lastRawX = static_cast<uint16_t>(sumX / valid);
        s_lastRawY = static_cast<uint16_t>(sumY / valid);
    } else if (!s_active) {
        trace_.touched = false;
        return false;
    }
    // 按着但这一帧样本不可信：**沿用上一帧坐标继续报「按着」** ——
    // 一帧噪声不该被上层当成一次松手（否则一次点击会变成两次、连点还会乱）。

    rawX = s_lastRawX;
    rawY = s_lastRawY;
    trace_.rawX = rawX;
    trace_.rawY = rawY;
    trace_.touched = true;
    return true;
}

bool Xpt2046Touch::read(int& x, int& y)
{
    uint16_t rawX = 0, rawY = 0;
    if (!readRaw(rawX, rawY)) return false;

    float nx = axisToUnit(rawX, cal_.xMin, cal_.xMax, cal_.invertX);
    float ny = axisToUnit(rawY, cal_.yMin, cal_.yMax, cal_.invertY);
    if (cal_.swapXY) {
        const float t = nx;
        nx = ny;
        ny = t;
    }

    int px = static_cast<int>(nx * (screenW_ - 1) + 0.5f) + cal_.offsetX;
    int py = static_cast<int>(ny * (screenH_ - 1) + 0.5f) + cal_.offsetY;
    if (px < 0) px = 0;
    if (py < 0) py = 0;
    if (px > screenW_ - 1) px = screenW_ - 1;
    if (py > screenH_ - 1) py = screenH_ - 1;

    x = px;
    y = py;
    return true;
}

#else
// 非 ESP 平台（host 模拟）：空实现，便于本文件在 PC 上编译
bool Xpt2046Touch::init(int screenWidth, int screenHeight)
{
    screenW_ = screenWidth;
    screenH_ = screenHeight;
    return false;
}
bool Xpt2046Touch::readRaw(uint16_t& rawX, uint16_t& rawY)
{
    (void)rawX;
    (void)rawY;
    return false;
}
bool Xpt2046Touch::read(int& x, int& y)
{
    (void)x;
    (void)y;
    return false;
}
#endif

} // namespace loppe_embedded
