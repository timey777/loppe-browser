/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

// lcd_st7789.cpp — ST7789 SPI 驱动的 ESP-IDF 实现骨架。
// 实际引脚/SPI 总线请按你的板卡调整（示例用 spi2 主机 + 常见引脚）。
// 这是"能编译的最小驱动"，真正上屏需替换为你的驱动或接入现有项目驱动。
#include "lcd_st7789.h"
#include <cstring>
#include <vector>

using namespace loppe_embedded;

#if defined(ESP_PLATFORM)
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_attr.h"   // WORD_ALIGNED_ATTR（DMA 缓冲区必须 4 字节对齐）
#include "esp_log.h"

#define LCD_HOST SPI2_HOST
#define PIN_SCLK 12   // 屏 SCK
#define PIN_MOSI 11   // 屏 SDI（MCU→屏 数据）
#define PIN_MISO 13   // 屏 SDO（屏→MCU，读取用，可省略）
#define PIN_CS   10   // 屏 CS
#define PIN_DC   9    // 屏 DC
#define PIN_RST  8    // 屏 RESET
#define PIN_BL   7    // 屏 LED（背光）

static const char* TAG = "st7789";
static spi_device_handle_t s_spi = nullptr;
static int s_width = 240;
static int s_height = 320;

// DC: 0=命令 1=数据（ST7789/ILI9341 区分指令与数据的关键引脚）
static void lcd_set_dc(int level) {
    gpio_set_level((gpio_num_t)PIN_DC, level);
}

static void lcd_write_cmd(uint8_t cmd) {
    lcd_set_dc(0); // 命令模式
    // tx_buffer 可能走 DMA：用 4 字节对齐的静态单元，别拿栈上 uint8_t 的地址（对齐只保证 1 字节）
    static WORD_ALIGNED_ATTR uint8_t s_cmdBuf;
    s_cmdBuf = cmd;
    spi_transaction_t t{};
    t.length = 8;
    t.tx_buffer = &s_cmdBuf;
    t.user = (void*)0;
    esp_err_t e = spi_device_transmit(s_spi, &t);
    if (e != ESP_OK) ESP_LOGE(TAG, "cmd 0x%02X failed: %d", cmd, (int)e);
}

static void lcd_write_data(const uint8_t* data, size_t len) {
    lcd_set_dc(1); // 数据模式
    spi_transaction_t t{};
    t.length = len * 8;
    t.tx_buffer = data;
    t.user = (void*)0;
    esp_err_t e = spi_device_transmit(s_spi, &t);
    if (e != ESP_OK) ESP_LOGE(TAG, "data %u bytes failed: %d", (unsigned)len, (int)e);
}

static void lcd_write_data8(uint8_t data) {
    lcd_write_data(&data, 1);
}

static void lcd_set_window(int x0, int y0, int x1, int y1) {
    // 静态对齐缓冲：这个地址会交给 spi_device_transmit（可能走 DMA），不能是栈上数组
    static WORD_ALIGNED_ATTR uint8_t buf[4];
    lcd_write_cmd(0x2A); // CASET
    buf[0] = x0 >> 8; buf[1] = x0 & 0xFF; buf[2] = x1 >> 8; buf[3] = x1 & 0xFF;
    lcd_write_data(buf, 4);
    lcd_write_cmd(0x2B); // RASET
    buf[0] = y0 >> 8; buf[1] = y0 & 0xFF; buf[2] = y1 >> 8; buf[3] = y1 & 0xFF;
    lcd_write_data(buf, 4);
    lcd_write_cmd(0x2C); // RAMWR
}

void St7789Lcd::init(int width, int height) {
    s_width = width;
    s_height = height;

    spi_bus_config_t buscfg{};
    buscfg.sclk_io_num = PIN_SCLK;
    buscfg.mosi_io_num = PIN_MOSI;
    buscfg.miso_io_num = PIN_MISO;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    // 重要：不要设为整帧大小（240*320*2=150KB），那会为 SPI 预留整帧 DMA 缓冲，
    // 占满内部堆导致引擎初始化崩溃。按 1 行（240*2）+ 余量即可。
    buscfg.max_transfer_sz = 4096;
    esp_err_t eBus = spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (eBus != ESP_OK) ESP_LOGE(TAG, "spi_bus_initialize failed: %d", (int)eBus);

    spi_device_interface_config_t devcfg{};
    devcfg.clock_speed_hz = 20 * 1000 * 1000; // 20MHz SPI（40MHz 在真机长排线易时序错乱）
    devcfg.mode = 3; // 很多 ST7789/ILI9341 模块需要 mode 3（CPOL=1,CPHA=1）；mode 0 会彩色雪花
    devcfg.spics_io_num = PIN_CS;
    devcfg.queue_size = 7;
    esp_err_t eDev = spi_bus_add_device(LCD_HOST, &devcfg, &s_spi);
    if (eDev != ESP_OK) ESP_LOGE(TAG, "spi_bus_add_device failed: %d", (int)eDev);

    gpio_set_direction((gpio_num_t)PIN_DC, GPIO_MODE_OUTPUT);
    gpio_set_direction((gpio_num_t)PIN_RST, GPIO_MODE_OUTPUT);
    gpio_set_direction((gpio_num_t)PIN_BL, GPIO_MODE_OUTPUT);
    // 必须先把三根线拉到已知电平：只设 direction 的话，上电瞬间它们是随机态，
    // 屏可能停在复位态或命令/数据相位错乱 → SPI 全部发成功、屏却一片白（无任何报错）。
    gpio_set_level((gpio_num_t)PIN_DC, 1);    // 默认数据模式（写命令前 lcd_set_dc(0) 会拉低）
    gpio_set_level((gpio_num_t)PIN_RST, 1);   // 先释放复位，下面再走一次完整复位时序
    gpio_set_level((gpio_num_t)PIN_BL, 0);    // 背光先关，初始化完成后再点亮

    // 复位时序（ST7789 要求低电平 ≥ 10ms）
    gpio_set_level((gpio_num_t)PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level((gpio_num_t)PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(120));

    // 通用最小初始化（ST7789 与 ILI9341 都兼容）。
    // 240x320 屏常见 ILI9341；之前用 ST7789 私有命令（0xB2/0xB7/0xBB/0xE0/0xE1...）
    // 与 ILI9341 不兼容，导致雪花。这里只发两种控制器都支持的标准命令。
    lcd_write_cmd(0x01); // SWRESET
    vTaskDelay(pdMS_TO_TICKS(150));
    lcd_write_cmd(0x11); // SLPOUT (sleep out)
    vTaskDelay(pdMS_TO_TICKS(120));
    lcd_write_cmd(0x36); // MADCTL
    lcd_write_data8(0x00); // 竖屏 240x320，旋转 0°（RGB 序；颜色若反则改 0xC0）
    lcd_write_cmd(0x3A); // COLMOD
    lcd_write_data8(0x05); // 16bit/pixel (RGB565)
    lcd_write_cmd(0x29); // DISPON
    vTaskDelay(pdMS_TO_TICKS(50));

    gpio_set_level((gpio_num_t)PIN_BL, 1);
    ESP_LOGI(TAG, "ST7789/ILI9341 initialized %dx%d", width, height);
}

void St7789Lcd::flush(const Framebuffer& fb) {
    flushRegion(fb, 0, 0, fb.width, fb.height);
}

void St7789Lcd::flushRegion(const Framebuffer& fb, int x, int y, int w, int h) {
    if (!s_spi) return;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + w > fb.width) w = fb.width - x;
    if (y + h > fb.height) h = fb.height - y;
    if (w <= 0 || h <= 0) return;

    lcd_set_window(x, y, x + w - 1, y + h - 1);

    // DC=1 (数据) 开始连续传输
    gpio_set_level((gpio_num_t)PIN_DC, 1);

    // 逐行发送（或一次性大块发送，需显存连续）
    for (int yy = y; yy < y + h; ++yy) {
        const uint16_t* row = fb.row(yy) + x;
        // 转换：帧缓冲 RGB565 小端 → LCD 需要的字节序（ST7789 期望高位在前）
        static thread_local std::vector<uint8_t> buf;
        buf.resize(static_cast<size_t>(w) * 2);
        for (int xx = 0; xx < w; ++xx) {
            uint16_t px = row[xx];
            buf[xx * 2] = static_cast<uint8_t>(px >> 8);
            buf[xx * 2 + 1] = static_cast<uint8_t>(px & 0xFF);
        }
        lcd_write_data(buf.data(), static_cast<size_t>(w) * 2);
    }
}

#else
// 非 ESP 平台（host 模拟）：LCD 为空实现，便于本文件在 PC 上编译
void St7789Lcd::init(int width, int height) { (void)width; (void)height; }
void St7789Lcd::flush(const Framebuffer& fb) { (void)fb; }
void St7789Lcd::flushRegion(const Framebuffer& fb, int x, int y, int w, int h) {
    (void)fb; (void)x; (void)y; (void)w; (void)h;
}
#endif