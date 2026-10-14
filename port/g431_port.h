/**
 * g431_port.h - STM32G431C8T6 + ST7789 240x280 转接层（裸机 UI 用）
 *
 * 本地 CubeMX 引脚（见 stm32g431c8t6_prj/Core/Inc/main.h）：
 *   LCD_CS   PA0    LCD_DC  PA1    LCD_BL  PA2
 *   LCD_SCK  PB3    LCD_MOSI PB5   （SPI3，8bit，CPOL=0/CPHA=0，42.5MHz）
 *   KEY_BTN  PA15（保留，未用）     LED PC13（保留，未用）
 *   RESET 未接 MCU（模块内部上拉）→ 用 SWRESET 命令复位。
 *
 * 分辨率适配：裸机 UI 按 240x240 设计，本屏可见 240x280；
 * UI 在 280 行内垂直居中（经 G431_YOFF，见下）。
 */
#ifndef G431_PORT_H
#define G431_PORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 本屏可见高度；UI 高固定 240 */
#define G431_SCR_W      240
#define G431_SCR_H      280
#define G431_UI_H       240
/* 240x280 面板 = 240x320 GRAM 的中段：可见起始行 = GRAM 40（同工程
 * st7789.h 的 ST7789_VRAM_OFFSET）。ST7789_SetWindow 把 UI y 写到
 * y+G431_YOFF：40 = 面板可见起始行，+20 = (280-240)/2 垂直居中余量。
 * UI y0..239 → GRAM 60..299 → 屏幕 y20..259（上下各留 20，胶囊完整）。
 * 换屏后若整体偏移不对，调此宏（常见值 40 或 60）。 */
#define G431_YOFF       (40 + (280 - 240) / 2)   /* = 60 */

/* 由 CubeIDE 工程侧 st7789.c 实现，本文件只声明转接入口 */
void ST7789_SetWindow(int16_t x, int16_t y, int16_t w, int16_t h);
void ST7789_PushPixels(const uint16_t *px, int32_t n);
void ST7789_PushPixelsSolid(uint16_t color, int32_t n);
void ST7789_Init(void);

/* 背光开（移植 main 上电调一次） */
void G431_BacklightOn(void);

#ifdef __cplusplus
}
#endif
#endif
