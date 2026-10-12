/**
 * ui_st7789_raw.c - 裸机 UI 对照实现（不使用 LVGL）
 *
 * 坐标与 ui_lvgl.c 完全一致，改布局时两边同步改即可。
 */

#include "ui_st7789_raw.h"
#include <stdio.h>
#include <string.h>

extern void ST7789_FillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
extern void ST7789_DrawChar(int16_t x, int16_t y, char ch, uint8_t scale, uint16_t color);
extern uint16_t ST7789_RGB565(uint8_t r, uint8_t g, uint8_t b);

#define RGB(r, g, b)      ST7789_RGB565(r, g, b)
#define CLR_BG            RGB(0x0B, 0x0E, 0x13)
#define CLR_TEXT          RGB(0xFF, 0xFF, 0xFF)
#define CLR_MUTED         RGB(0x8A, 0x8F, 0x96)
#define CLR_LINE          RGB(0x22, 0x26, 0x2D)
#define CLR_SHELL         RGB(0x9A, 0xA0, 0xA8)
#define CLR_RANGE         RGB(0x5D, 0xC8, 0xE8)
#define CLR_DISCH         RGB(0xF0, 0x99, 0x7B)
#define CLR_CHG           RGB(0x67, 0xC9, 0x8A)
#define CLR_LOW           RGB(0xF0, 0x55, 0x55)

#define BATT_X            16
#define BATT_Y            28
#define BATT_W            196
#define BATT_H            96
#define BATT_INNER_W      188

/* HSV->RGB，与 LVGL v8 的 lv_color_hsv_to_rgb 同一算法（s/v 取 0~100）
   h = SOC × 1.2°，0 红 → 120 绿 */
static uint16_t soc_color(int soc)
{
    uint16_t h = (uint16_t)((uint32_t)(soc * 12 / 10) * 255 / 360);
    uint16_t s = (uint16_t)(78 * 255 / 100);
    uint16_t v = (uint16_t)(86 * 255 / 100);
    uint8_t r, g, b, region, remainder, p, q, t;

    region    = (uint8_t)(h / 43);
    remainder = (uint8_t)((h - (region * 43)) * 6);

    p = (uint8_t)((v * (255 - s)) >> 8);
    q = (uint8_t)((v * (255 - ((s * remainder) >> 8))) >> 8);
    t = (uint8_t)((v * (255 - ((s * (255 - remainder)) >> 8))) >> 8);

    switch (region) {
        case 0:  r = v; g = t; b = p; break;
        case 1:  r = q; g = v; b = p; break;
        case 2:  r = p; g = v; b = t; break;
        case 3:  r = p; g = q; b = v; break;
        case 4:  r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
    return RGB(r, g, b);
}

static void fill(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c)
{
    ST7789_FillRect(x, y, w, h, c);
}

static void text(int16_t x, int16_t y, const char *s, uint8_t scale, uint16_t c)
{
    while (*s) ST7789_DrawChar(x, y, *s++, scale, c);
}

/* 居中画一行（需字符串像素宽度 = len * (6*scale-1)） */
static void text_c(int16_t y, const char *s, uint8_t scale, uint16_t c)
{
    int16_t w = (int16_t)strlen(s) * (int16_t)(6 * scale - 1);
    text((240 - w) / 2, y, s, scale, c);
}

void gui_init(void)
{
    fill(0, 0, 240, 240, CLR_BG);
}

void gui_task(gauge_ui_t *ui)
{
    char buf[32];
    int soc, i;

    if (!ui->refresh) return;
    ui->refresh = 0;

    soc = (int)(ui->soc + 0.5f);
    if (soc < 0)   soc = 0;
    if (soc > 100) soc = 100;

    /* 状态胶囊 */
    if (ui->charging) {
        fill(92, 6, 56, 20, RGB(0x12, 0x30, 0x1C));
        text_c(10, "CHG", 1, CLR_CHG);
    } else {
        fill(92, 6, 56, 20, RGB(0x3A, 0x24, 0x10));
        text_c(10, "DIS", 1, CLR_DISCH);
    }

    /* 电池外壳：画 4 条边模拟描边（圆角可省） */
    {
        uint16_t shell = CLR_SHELL;
        fill(BATT_X, BATT_Y, BATT_W, 3, shell);
        fill(BATT_X, BATT_Y + BATT_H - 3, BATT_W, 3, shell);
        fill(BATT_X, BATT_Y, 3, BATT_H, shell);
        fill(BATT_X + BATT_W - 3, BATT_Y, 3, BATT_H, shell);
    }
    fill(BATT_X + BATT_W + 3, 52, 9, 48, CLR_SHELL);   /* 电极头 */

    /* 填充条 + SOC 数字 */
    if (soc > 0) fill(BATT_X + 4, BATT_Y + 4, (BATT_INNER_W * soc) / 100, BATT_H - 8, soc_color(soc));
    snprintf(buf, sizeof(buf), "%d%%", soc);
    text_c(BATT_Y + 26, buf, 4, (soc <= 10) ? CLR_LOW : CLR_TEXT);

    /* 续航 */
    if (ui->range_km >= 0) snprintf(buf, sizeof(buf), "%d km", ui->range_km);
    else                   snprintf(buf, sizeof(buf), "--");
    text_c(132, buf, 1, CLR_RANGE);

    /* 分隔线 */
    fill(24, 156, 192, 1, CLR_LINE);
    fill(24, 204, 192, 1, CLR_LINE);

    /* 电压 / 电流 / 功率 */
    snprintf(buf, sizeof(buf), "%.2f", ui->voltage);       text(50, 164, buf, 2, CLR_TEXT);
    snprintf(buf, sizeof(buf), "%.1f", ui->current >= 0 ? ui->current : -ui->current);
    text(105, 164, buf, 2, ui->charging ? CLR_CHG : CLR_DISCH);
    snprintf(buf, sizeof(buf), "%.2f", ui->power_kw);      text(165, 164, buf, 2, CLR_TEXT);

    text(50, 184, "V", 1, CLR_MUTED);
    text(108, 184, "A", 1, CLR_MUTED);
    text(168, 184, "kW", 1, CLR_MUTED);

    /* 自学习容量 */
    if (ui->capacity_ah < 0) text_c(210, "CAP --", 1, CLR_MUTED);
    else {
        snprintf(buf, sizeof(buf), "%.1fAh", ui->capacity_ah);
        text_c(210, buf, 1, CLR_TEXT);
    }
}