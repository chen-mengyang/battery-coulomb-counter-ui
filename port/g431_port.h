/**
 * g431_port.h - STM32G431C8T6 + ST7789 240x280 转接层（裸机 UI 用）
 *
 * 本地 CubeMX 引脚（见 stm32g431c8t6_prj/Core/Inc/main.h）：
 *   LCD_CS   PA0    LCD_DC  PA1    LCD_BL  PA2
 *   LCD_SCK  PB3    LCD_MOSI PB5   （SPI3，8bit，CPOL=0/CPHA=0，42.5MHz）
 *   KEY_BTN  PA15（保留，未用）     LED PC13（保留，未用）
 *   RESET 未接 MCU（模块内部上拉）→ 用 SWRESET 命令复位。
 *
 * 分辨率适配：裸机 UI 按 240x240 设计，本屏可见 240x280。
 * UI 内容顶部对齐，底部 40 行留白（清背景色）。
 */
#ifndef G431_PORT_H
#define G431_PORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 本屏可见高度；UI 高固定 240，偏移宏供 main 演示清屏用 */
#define G431_SCR_W      240
#define G431_SCR_H      280
#define G431_UI_H       240
#define G431_UI_YOFF    0   /* 顶部对齐：UI 占屏 y=0..239，底部 40 行留白 */

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
