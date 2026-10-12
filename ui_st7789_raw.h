/**
 * ui_st7789_raw.h - 不依赖 LVGL 的裸机 UI（对照实现）
 *
 * 与 ui_lvgl.c 显示同样的布局，但不使用任何 GUI 库。
 * 屏幕 240x240、刷新 2Hz、元素 7 个，裸写反而更短更快。
 *
 * 需要你已有的 ST7789 驱动提供下面四个原语（名字可自行替换）：
 *   void ST7789_FillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
 *   void ST7789_DrawChar(int16_t x, int16_t y, char ch, uint8_t scale, uint16_t color);
 *   uint16_t ST7789_RGB565(uint8_t r, uint8_t g, uint8_t b);
 *   void ST7789_Show(void);   // 可选：需要整屏缓冲时统一刷出
 */
#ifndef UI_ST7789_RAW_H
#define UI_ST7789_RAW_H

#include <stdint.h>

typedef struct {
    float    soc;          /* 0~100                */
    float    voltage;      /* V                     */
    float    current;      /* A，放电为正           */
    float    power_kw;     /* kW                    */
    float    capacity_ah;  /* <0 表示未标定          */
    int      range_km;     /* <0 显示 --            */
    uint8_t  charging;     /* 1 充电 / 0 放电       */
    uint8_t  refresh;      /* 置 1 请求重绘         */
} gauge_ui_t;

void gui_init(void);
void gui_task(gauge_ui_t *ui);   /* 每 100ms 调一次，内部按 refresh 决定是否重绘 */

#endif