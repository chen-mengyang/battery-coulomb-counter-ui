/**
 * ui_raw_v2.c - 裸机 V2 界面（像素复刻 ui_lvgl.c），part1: 颜色/工具/初始化
 * 布局见 part2 顶部注释。
 */
#include "ui_raw_v2.h"
#include "gfx_raw.h"
#include "font_raw.h"

#define CLR_BG       GFX_RGB565(0x0B,0x0E,0x13)
#define CLR_TEXT     GFX_RGB565(0xFF,0xFF,0xFF)
#define CLR_MUTED    GFX_RGB565(0x8A,0x8F,0x96)
#define CLR_LINE     GFX_RGB565(0x22,0x26,0x2D)
#define CLR_SHELL    GFX_RGB565(0x9A,0xA0,0xA8)
#define CLR_RANGE    GFX_RGB565(0x5D,0xC8,0xE8)
#define CLR_DISCH    GFX_RGB565(0xF0,0x99,0x7B)
#define CLR_CHG      GFX_RGB565(0x67,0xC9,0x8A)
#define CLR_LOW      GFX_RGB565(0xF0,0x55,0x55)
#define CLR_TAG_BG_D GFX_RGB565(0x3A,0x24,0x10)
#define CLR_TAG_BG_C GFX_RGB565(0x12,0x30,0x1C)
#define CLR_CAP_TXT  GFX_RGB565(0xB8,0xBD,0xC4)

#define BATT_INNER_W 188
#define BATT_X       16
#define BATT_Y       28
#define BATT_W       196
#define BATT_H       96

uint16_t ui_raw_soc_color(int soc)
{
    uint16_t h = (uint16_t)((uint32_t)(soc * 12 / 10) * 255 / 360);
    uint16_t s = (uint16_t)(78 * 255 / 100);
    uint16_t v = (uint16_t)(86 * 255 / 100);
    uint8_t r, g, b, region, remainder, p, q, t;
    region    = (uint8_t)(h / 43);
    remainder = (uint8_t)((h - (uint16_t)(region * 43)) * 6);
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
    return GFX_RGB565(r, g, b);
}

int ui_raw_clamp_soc(float soc)
{
    int s = (int)(soc + 0.5f);
    if (s < 0) s = 0;
    if (s > 100) s = 100;
    return s;
}

void ui_raw_init(void)
{
    GFX_FillRect(0, 0, 240, 240, CLR_BG);
}
