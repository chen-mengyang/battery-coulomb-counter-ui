/**
 * ui_raw_draw.c - 裸机 V2 界面 part2: 元素级槽位刷新（布局与 ui_lvgl.c 一致）
 *
 * 防闪三层：① ui_raw_draw_static() 首帧画静态层；② ui_raw_update 内容
 * 快照全没变 → 整帧跳过（零像素）；③ 变了的元素按"槽位擦(旧∪新盒)→紧挨
 * 着画"，单元素空白 <1ms（flappy draw_score 同款），无整屏/整区清屏。
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
#include <string.h>

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

/* ---- 动态元素槽位：擦(旧∪新盒)→紧挨着画（单元素空白 <1ms，flappy 同款） ---- */
typedef struct { int16_t x, y, w, h; uint8_t on; } ui_slot_t;

static void slot_erase(ui_slot_t *s, int16_t x, int16_t y, int16_t w, int16_t h,
                       uint16_t bg)
{
    int16_t x0 = x, y0 = y;
    int16_t x1 = (int16_t)(x + w), y1 = (int16_t)(y + h);
    if (s->on) {
        if (s->x < x0) x0 = s->x;
        if (s->y < y0) y0 = s->y;
        if ((int16_t)(s->x + s->w) > x1) x1 = (int16_t)(s->x + s->w);
        if ((int16_t)(s->y + s->h) > y1) y1 = (int16_t)(s->y + s->h);
    }
    if (x1 > x0 && y1 > y0)
        GFX_FillRect(x0, y0, (int16_t)(x1 - x0), (int16_t)(y1 - y0), bg);
}

static void slot_set(ui_slot_t *s, int16_t x, int16_t y, int16_t w, int16_t h)
{
    s->x = x; s->y = y; s->w = w; s->h = h; s->on = 1;
}

/* 自学习行：前后缀固定槽位静态绘制，只有中间值动态 —— 值槽 x/w 恒定，
 * 行不随值宽度整体位移（否则整行 ~5k px 反复重绘）。 */
#define CAP_PRE "自学习容量："
#define CAP_SUF "(±5%)"
#define CAPV_W  62                    /* 值槽固定宽度（"19.6Ah"=60，留余量） */
static int16_t cap_vx;                /* 值槽 x（ui_raw_draw_static 计算） */

/* 帧间快照：内容全没变 → ui_raw_update 直接返回（零像素）。
 * 动态项只有 SOC/续航/充放电 —— 电压电流功率容量为静态绘制（不参与）。 */
typedef struct {
    int16_t fill_w;
    uint8_t charging;
    char socstr[8];
    char rangestr[20];
    uint8_t on;
} ui_snap_t;

static const raw_glyph_t *FCN(void) { return FONT_CN12; }
static int FCN_N(void) { return (int)(sizeof(FONT_CN12)/sizeof(FONT_CN12[0])); }
static const raw_glyph_t *FN16(void) { return FONT_NUM16; }
static int FN16_N(void) { return (int)(sizeof(FONT_NUM16)/sizeof(FONT_NUM16[0])); }
static const raw_glyph_t *FN40(void) { return FONT_NUM40; }
static int FN40_N(void) { return (int)(sizeof(FONT_NUM40)/sizeof(FONT_NUM40[0])); }

/* 三列静态数值：居中于 72px 格（y162），画一次不再刷新 */
static void draw_col_value(int cx, const char *s, uint16_t col)
{
    uint16_t tw = GFX_TextWidth(FN16(), FN16_N(), s);
    GFX_DrawText((int16_t)(cx - 36 + (72 - tw) / 2), 162, FN16(), FN16_N(),
                 FONT_RAW_BLOB, s, col);
}

/* 静态层：首帧画一次。框架（壳/分隔线/单位标签/前后缀）+ 静态数值
 * （电压/电流/功率/容量取 d 当前值，只画一次不参与刷新）；
 * 动态元素只有：SOC 大字、续航行、充放电胶囊（由 ui_raw_update 槽位刷新）。 */
void ui_raw_draw_static(const ui_raw_data_t *d)
{
    uint16_t w1, w3, x;
    GFX_FillRect(0, 0, 240, 240, CLR_BG2);
    GFX_DrawRoundBorder(BX2, BY2, BW2, BH2, 18, 3, CLR_SHELL2);
    GFX_FillRoundRect(BX2 + BW2 + 3, 52, 9, 48, 4, CLR_SHELL2);
    GFX_FillRect(24, 156, 192, 1, CLR_LINE2);
    GFX_FillRect(24, 204, 192, 1, CLR_LINE2);
    /* 三列单位标签（y184，固定不变） */
    GFX_DrawTextCenteredIn(60 - 36, 184, 72, FCN(), FCN_N(),
        FONT_RAW_BLOB, "电压 V", CLR_MUTED2);
    GFX_DrawTextCenteredIn(120 - 36, 184, 72, FCN(), FCN_N(),
        FONT_RAW_BLOB, "电流 A", CLR_MUTED2);
    GFX_DrawTextCenteredIn(180 - 36, 184, 72, FCN(), FCN_N(),
        FONT_RAW_BLOB, "功率 kW", CLR_MUTED2);
    /* 自学习行前缀/后缀：值槽宽度固定 → 前后缀 x 恒定 */
    w1 = GFX_TextWidth(FCN(), FCN_N(), CAP_PRE);
    w3 = GFX_TextWidth(FCN(), FCN_N(), CAP_SUF);
    x = (uint16_t)((240 - (w1 + 2 + CAPV_W + 2 + w3)) / 2);
    cap_vx = (int16_t)(x + w1 + 2);
    GFX_DrawText(x, 210, FCN(), FCN_N(), FONT_RAW_BLOB, CAP_PRE, CLR_CAP_TXT2);
    GFX_DrawText((int16_t)(x + w1 + 2 + CAPV_W + 2), 210, FCN(), FCN_N(),
                 FONT_RAW_BLOB, CAP_SUF, CLR_CAP_TXT2);
    /* 静态数值（d 非空时）：电压/电流/功率三列 + 容量值槽 */
    if (d) {
        char v0[12], v1[12], v2[12], cv[16];
        float a = d->current >= 0 ? d->current : -d->current;
        snprintf(v0, sizeof(v0), "%.2f", d->voltage);
        snprintf(v1, sizeof(v1), "%s%.1f", d->charging ? "+" : "-", a);
        snprintf(v2, sizeof(v2), "%.2f", d->power_kw);
        if (d->capacity_ah >= 0.0f)
            snprintf(cv, sizeof(cv), "%.1fAh", d->capacity_ah);
        else
            snprintf(cv, sizeof(cv), "--");
        draw_col_value(60, v0, CLR_TEXT2);
        draw_col_value(120, v1, d->charging ? CLR_CHG2 : CLR_DISCH2);
        draw_col_value(180, v2, CLR_TEXT2);
        {
            uint16_t tw = GFX_TextWidth(FN16(), FN16_N(), cv);
            GFX_DrawText((int16_t)(cap_vx + (CAPV_W - tw) / 2), 209,
                         FN16(), FN16_N(), FONT_RAW_BLOB, cv, CLR_TEXT2);
        }
    }
}

void ui_raw_update(const ui_raw_data_t *d)
{
    char socstr[8], rangestr[20];
    int soc, fill_w;
    uint16_t scol;
    static ui_snap_t snap;
    static ui_slot_t slot_soc, slot_rng;

    if (!d) return;
    soc = ui_raw_clamp_soc(d->soc);
    scol = ui_raw_soc_color(soc);
    fill_w = (BIW2 * soc) / 100;

    /* 本帧动态内容渲染成串（跳帧比较与绘制共用） */
    snprintf(socstr, sizeof(socstr), "%d%%", soc);
    if (d->range_km >= 0) snprintf(rangestr, sizeof(rangestr), "续航 %d km", d->range_km);
    else snprintf(rangestr, sizeof(rangestr), "续航 --");

    /* 跳帧：内容全没变 → 零像素（静止帧完全不闪） */
    if (snap.on &&
        snap.fill_w == fill_w && snap.charging == d->charging &&
        strcmp(snap.socstr, socstr) == 0 &&
        strcmp(snap.rangestr, rangestr) == 0)
        return;

    /* 1) 状态胶囊：仅充放电翻转才动（形状恒定，圆角擦+画紧挨） */
    if (!snap.on || snap.charging != d->charging) {
        GFX_FillRoundRect(92, 6, 56, 20, 10, CLR_BG2);
        if (d->charging) {
            GFX_FillRoundRect(92, 6, 56, 20, 10, CLR_TAG_BG_C2);
            GFX_DrawTextCenteredIn(92, 9, 56, FCN(), FCN_N(),
                FONT_RAW_BLOB, "充电中", CLR_CHG2);
        } else {
            GFX_FillRoundRect(92, 6, 56, 20, 10, CLR_TAG_BG_D2);
            GFX_DrawTextCenteredIn(92, 9, 56, FCN(), FCN_N(),
                FONT_RAW_BLOB, "放电中", CLR_DISCH2);
        }
    }

    /* 2) 填充条：只擦右端差量带（|Δ|+2r 宽 ≈2.3k px），再整条重画（纯色快填） */
    if (snap.on && snap.fill_w != fill_w) {
        int16_t oend = (int16_t)(BX2 + 4 + snap.fill_w);
        int16_t nend = (int16_t)(BX2 + 4 + fill_w);
        int16_t b0 = (oend < nend ? oend : nend) - 13;
        int16_t b1 = (oend > nend ? oend : nend) + 13;
        if (b0 < BX2 + 4) b0 = (int16_t)(BX2 + 4);
        if (b1 > BX2 + 4 + BIW2) b1 = (int16_t)(BX2 + 4 + BIW2);
        if (b1 > b0)
            GFX_FillRect(b0, BY2 + 4, (int16_t)(b1 - b0), BH2 - 8, CLR_BG2);
    }
    if (fill_w > 0)
        GFX_FillRoundRect(BX2 + 4, BY2 + 4, fill_w, BH2 - 8, 13, scol);

    /* 3) SOC 大字：槽并集擦，底色按填充条右缘分两段（条内=填充色/其余 bg）。
     *    数字行 y53..101 在条的直边段内，右缘即 20+fill_w，无圆角干扰。 */
    {
        uint16_t tw = GFX_TextWidth(FN40(), FN40_N(), socstr);
        int16_t sx = (int16_t)(BX2 + (BW2 - tw) / 2);
        int16_t sy = (int16_t)(BY2 + 25);
        int16_t fill_end = (int16_t)(BX2 + 4 + (fill_w > 0 ? fill_w : 0));
        int16_t x0 = sx, y0 = sy;
        int16_t x1 = (int16_t)(sx + tw), y1 = (int16_t)(sy + 48);
        if (slot_soc.on) {
            if (slot_soc.x < x0) x0 = slot_soc.x;
            if (slot_soc.y < y0) y0 = slot_soc.y;
            if ((int16_t)(slot_soc.x + slot_soc.w) > x1) x1 = (int16_t)(slot_soc.x + slot_soc.w);
            if ((int16_t)(slot_soc.y + slot_soc.h) > y1) y1 = (int16_t)(slot_soc.y + slot_soc.h);
        }
        if (fill_end > x0) {
            int16_t seg = (fill_end < x1) ? fill_end : x1;
            if (seg > x0)
                GFX_FillRect(x0, y0, (int16_t)(seg - x0), (int16_t)(y1 - y0), scol);
            if (x1 > seg)
                GFX_FillRect(seg, y0, (int16_t)(x1 - seg), (int16_t)(y1 - y0), CLR_BG2);
        } else {
            GFX_FillRect(x0, y0, (int16_t)(x1 - x0), (int16_t)(y1 - y0), CLR_BG2);
        }
        GFX_DrawText(sx, sy, FN40(), FN40_N(), FONT_RAW_BLOB, socstr,
                     (soc <= 10) ? CLR_LOW2 : CLR_TEXT2);
        slot_set(&slot_soc, sx, sy, (int16_t)tw, 48);
    }

    /* 4) 续航文本槽（居中位移 → 并集擦+画紧挨） */
    {
        uint16_t tw = GFX_TextWidth(FCN(), FCN_N(), rangestr);
        int16_t rx = (int16_t)((240 - tw) / 2);
        slot_erase(&slot_rng, rx, 130, (int16_t)tw, 18, CLR_BG2);
        GFX_DrawText(rx, 130, FCN(), FCN_N(), FONT_RAW_BLOB, rangestr, CLR_RANGE2);
        slot_set(&slot_rng, rx, 130, (int16_t)tw, 18);
    }

    /* 保存本帧快照 */
    snap.fill_w = (int16_t)fill_w;
    snap.charging = d->charging;
    strcpy(snap.socstr, socstr);
    strcpy(snap.rangestr, rangestr);
    snap.on = 1;
}

void ui_raw_draw_stats(float frame_ms, float fps)
{
    char a[16], b[16];
    static const int n16 = (int)(sizeof(FONT_NUM16) / sizeof(FONT_NUM16[0]));
    static char lastA[16], lastB[16];
    static ui_slot_t slotA, slotB;
    uint16_t ok  = CLR_CHG2;    /* 绿：达标 */
    uint16_t bad = CLR_LOW2;    /* 红：未达标 */
    /* 量化：ms 取 1ms、fps 取 0.5 —— 稳态下字符串不变，状态带不动；
     * 变了也只擦各自数字槽（不再整清 40 行） */
    frame_ms = (float)((int)(frame_ms + 0.5f));
    fps = (float)((int)(fps * 2.0f + 0.5f)) / 2.0f;
    snprintf(a, sizeof(a), "%.1f", frame_ms);
    snprintf(b, sizeof(b), "%.1f", fps);
    if (strcmp(a, lastA) != 0) {
        uint16_t tw = GFX_TextWidth(FONT_NUM16, n16, a);
        slot_erase(&slotA, 8, 249, (int16_t)tw, 19, CLR_BG2);
        GFX_DrawText(8, 249, FONT_NUM16, n16, FONT_RAW_BLOB, a,
                     (frame_ms <= 40.0f) ? ok : bad);
        slot_set(&slotA, 8, 249, (int16_t)tw, 19);
        strcpy(lastA, a);
    }
    if (strcmp(b, lastB) != 0) {
        uint16_t tw = GFX_TextWidth(FONT_NUM16, n16, b);
        int16_t bx = (int16_t)(232 - tw);
        slot_erase(&slotB, bx, 249, (int16_t)tw, 19, CLR_BG2);
        GFX_DrawText(bx, 249, FONT_NUM16, n16, FONT_RAW_BLOB, b,
                     (fps >= 25.0f) ? ok : bad);
        slot_set(&slotB, bx, 249, (int16_t)tw, 19);
        strcpy(lastB, b);
    }
}
