/**
 * ui_raw_draw.c - 裸机 V2 界面 part2: 全屏重画（布局与 ui_lvgl.c 一致）
 *
 *   y    6~26   状态胶囊 92,6,56x20 r10
 *   y   28~124  电池外壳16,28,196x96 r18/描边3 / 填充20,32,h88 r13 / 电极头215,52,9x48 r4
 *              SOC num40 居中，<=10%转红
 *   y  130~146  续航 cn12 青色居中
 *   y  156/204  分隔线 24~216
 *   y  162~196  三列 x=60/120/180：num16数值 + cn12标签
 *   y  210~228  自学习容量：cn12说明 + num16数值 + cn12误差，整行居中
 */
#include "ui_raw_v2.h"
#include "gfx_raw.h"
#include "font_raw.h"
#include <stdio.h>

#define CLR_BG2       GFX_RGB565(0x0B,0x0E,0x13)
#define CLR_TEXT2     GFX_RGB565(0xFF,0xFF,0xFF)
#define CLR_MUTED2    GFX_RGB565(0x8A,0x8F,0x96)
#define CLR_LINE2     GFX_RGB565(0x22,0x26,0x2D)
#define CLR_SHELL2    GFX_RGB565(0x9A,0xA0,0xA8)
#define CLR_RANGE2    GFX_RGB565(0x5D,0xC8,0xE8)
#define CLR_DISCH2    GFX_RGB565(0xF0,0x99,0x7B)
#define CLR_CHG2      GFX_RGB565(0x67,0xC9,0x8A)
#define CLR_LOW2      GFX_RGB565(0xF0,0x55,0x55)
#define CLR_TAG_BG_D2 GFX_RGB565(0x3A,0x24,0x10)
#define CLR_TAG_BG_C2 GFX_RGB565(0x12,0x30,0x1C)
#define CLR_CAP_TXT2  GFX_RGB565(0xB8,0xBD,0xC4)

#define BX2 16
#define BY2 28
#define BW2 196
#define BH2 96
#define BIW2 188

extern uint16_t ui_raw_soc_color(int soc);
extern int ui_raw_clamp_soc(float soc);

static const raw_glyph_t *FCN(void) { return FONT_CN12; }
static int FCN_N(void) { return (int)(sizeof(FONT_CN12)/sizeof(FONT_CN12[0])); }
static const raw_glyph_t *FN16(void) { return FONT_NUM16; }
static int FN16_N(void) { return (int)(sizeof(FONT_NUM16)/sizeof(FONT_NUM16[0])); }
static const raw_glyph_t *FN40(void) { return FONT_NUM40; }
static int FN40_N(void) { return (int)(sizeof(FONT_NUM40)/sizeof(FONT_NUM40[0])); }

void ui_raw_update(const ui_raw_data_t *d)
{
    char buf[32];
    char buf2[64];
    char capbuf[24];
    int soc, fill_w, i;
    uint16_t scol;
    static const int col_x[3] = { 60, 120, 180 };

    if (!d) return;
    soc = ui_raw_clamp_soc(d->soc);
    scol = ui_raw_soc_color(soc);
    fill_w = (BIW2 * soc) / 100;

    GFX_FillRect(0, 0, 240, 240, CLR_BG2);

    if (d->charging) {
        GFX_FillRoundRect(92, 6, 56, 20, 10, CLR_TAG_BG_C2);
        GFX_DrawTextCenteredIn(92, 9, 56, FCN(), FCN_N(),
            FONT_RAW_BLOB, "充电中", CLR_CHG2);
    } else {
        GFX_FillRoundRect(92, 6, 56, 20, 10, CLR_TAG_BG_D2);
        GFX_DrawTextCenteredIn(92, 9, 56, FCN(), FCN_N(),
            FONT_RAW_BLOB, "放电中", CLR_DISCH2);
    }

    GFX_DrawRoundBorder(BX2, BY2, BW2, BH2, 18, 3, CLR_SHELL2);
    GFX_FillRoundRect(BX2 + BW2 + 3, 52, 9, 48, 4, CLR_SHELL2);
    if (fill_w > 0)
        GFX_FillRoundRect(BX2 + 4, BY2 + 4, fill_w, BH2 - 8, 13, scol);

    snprintf(buf, sizeof(buf), "%d%%", soc);
    {
        uint16_t tw = GFX_TextWidth(FN40(), FN40_N(), buf);
        GFX_DrawText((int16_t)(BX2 + (BW2 - tw) / 2), (int16_t)(BY2 + 25),
                     FN40(), FN40_N(), FONT_RAW_BLOB, buf,
                     (soc <= 10) ? CLR_LOW2 : CLR_TEXT2);
    }

    if (d->range_km >= 0) snprintf(buf2, sizeof(buf2), "续航 %d km", d->range_km);
    else snprintf(buf2, sizeof(buf2), "续航 --");
    GFX_DrawTextCentered(130, 208, FCN(), FCN_N(), FONT_RAW_BLOB, buf2, CLR_RANGE2);

    GFX_FillRect(24, 156, 192, 1, CLR_LINE2);
    GFX_FillRect(24, 204, 192, 1, CLR_LINE2);

    for (i = 0; i < 3; i++) {
        const char *unit = "";
        uint16_t col = CLR_TEXT2;
        if (i == 0) {
            snprintf(buf, sizeof(buf), "%.2f", d->voltage);
            unit = "电压 V";
        } else if (i == 1) {
            float a = d->current >= 0 ? d->current : -d->current;
            snprintf(buf, sizeof(buf), "%s%.1f", d->charging ? "+" : "-", a);
            unit = "电流 A";
            col = d->charging ? CLR_CHG2 : CLR_DISCH2;
        } else {
            snprintf(buf, sizeof(buf), "%.2f", d->power_kw);
            unit = "功率 kW";
        }
        GFX_DrawTextCenteredIn(col_x[i] - 36, 162, 72,
            FN16(), FN16_N(), FONT_RAW_BLOB, buf, col);
        GFX_DrawTextCenteredIn(col_x[i] - 36, 184, 72,
            FCN(), FCN_N(), FONT_RAW_BLOB, unit, CLR_MUTED2);
    }

    {
        const char *pre = "自学习容量：";
        const char *suf = "(±5%)";
        const char *capv = "--";
        uint16_t w1, w2, w3, total, x;
        if (d->capacity_ah >= 0.0f) {
            snprintf(capbuf, sizeof(capbuf), "%.1fAh", d->capacity_ah);
            capv = capbuf;
        }
        w1 = GFX_TextWidth(FCN(), FCN_N(), pre);
        w2 = GFX_TextWidth(FN16(), FN16_N(), capv);
        w3 = GFX_TextWidth(FCN(), FCN_N(), suf);
        total = w1 + 2 + w2 + 2 + w3;
        x = (240 - total) / 2;
        GFX_DrawText(x, 210, FCN(), FCN_N(), FONT_RAW_BLOB, pre, CLR_CAP_TXT2);
        GFX_DrawText(x + w1 + 2, 209, FN16(), FN16_N(), FONT_RAW_BLOB, capv, CLR_TEXT2);
        GFX_DrawText(x + w1 + 2 + w2 + 2, 210, FCN(), FCN_N(), FONT_RAW_BLOB, suf, CLR_CAP_TXT2);
    }
}
