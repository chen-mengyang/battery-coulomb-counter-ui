/**
 * ui_lvgl.c - 库仑计 ST7789 240x240 UI（LVGL v8）
 *
 * 布局坐标表（绝对定位，屏幕固定 240x240）：
 *   y    6~26   状态胶囊（放电中/充电中）
 *   y   28~124  电池主体：外壳 196x96 / 填充条 / 电极头
 *              SOC 百分比 40px 粗体叠在电池正中
 *   y  130~146  预计续航（小字青色）
 *   y  156       分隔线
 *   y  162~196  电压 / 电流 / 功率 三列（16px 数值 + 12px 汉字标签）
 *   y  204       分隔线
 *   y  210~228  满充满放自学习容量（汉字说明 + 数值 + 误差）
 *
 * 依赖：lv_conf.h 打开 LV_FONT_MONTSERRAT_16 / LV_FONT_MONTSERRAT_40；
 *       自制中文子集字库 font_cn_12（生成命令见 README.md）。
 */

#include "ui.h"
#include <stdio.h>

LV_FONT_DECLARE(font_cn_12);

#define CLR_BG        lv_color_hex(0x0B0E13)   /* 屏底深色 */
#define CLR_TEXT      lv_color_hex(0xFFFFFF)
#define CLR_MUTED     lv_color_hex(0x8A8F96)
#define CLR_LINE      lv_color_hex(0x22262D)
#define CLR_SHELL     lv_color_hex(0x9AA0A8)   /* 电池壳 / 电极头 */
#define CLR_RANGE     lv_color_hex(0x5DC8E8)   /* 续航青色 */
#define CLR_DISCH     lv_color_hex(0xF0997B)   /* 放电橙 */
#define CLR_CHG       lv_color_hex(0x67C98A)   /* 充电绿 */
#define CLR_LOW       lv_color_hex(0xF05555)   /* 低电量红 */
#define CLR_TAG_BG_D  lv_color_hex(0x3A2410)
#define CLR_TAG_BG_C  lv_color_hex(0x12301C)
#define CLR_CAP_TXT   lv_color_hex(0xB8BDC4)

/* 电池内部可用宽度（外壳 196 减去左右边框） */
#define BATT_INNER_W   188
#define BATT_X         16
#define BATT_Y         28
#define BATT_W         196
#define BATT_H         96

static lv_obj_t *s_tag, *s_tag_txt;
static lv_obj_t *s_shell, *s_fill, *s_cap;
static lv_obj_t *s_soc, *s_range;
static lv_obj_t *s_val[3];
static lv_obj_t *s_unit[3];
static lv_obj_t *s_line1, *s_line2;
static lv_obj_t *s_cap_row, *s_cap_pre, *s_cap_val, *s_cap_suf;

/* ---------------------------------------------------------------- 工具 */

static lv_obj_t *mk_rect(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                         lv_coord_t w, lv_coord_t h, lv_coord_t r, lv_color_t color)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_radius(o, r, LV_PART_MAIN);
    lv_obj_set_style_border_width(o, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(o, color, LV_PART_MAIN);
    lv_obj_set_style_pad_all(o, 0, LV_PART_MAIN);
    return o;
}

static lv_obj_t *mk_label(lv_obj_t *parent, const char *txt, const lv_font_t *font,
                          lv_color_t color, lv_coord_t x, lv_coord_t y,
                          lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_coord_t pad;

    lv_obj_set_pos(l, x, y);
    lv_obj_set_size(l, w, h);
    lv_obj_set_style_text_font(l, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, color, LV_PART_MAIN);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(l, 0, LV_PART_MAIN);

    /* 上下对半留白 = 垂直居中 */
    pad = (h - lv_font_get_line_height(font)) / 2;
    lv_obj_set_style_pad_top(l, pad > 0 ? pad : 0, LV_PART_MAIN);

    lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
    lv_label_set_text(l, txt);
    return l;
}

static int clamp_soc(float soc)
{
    int s = (int)(soc + 0.5f);
    if (s < 0)   s = 0;
    if (s > 100) s = 100;
    return s;
}

/* ---------------------------------------------------------------- 初始化 */

void ui_init(void)
{
    static const lv_coord_t col_x[3] = { 60, 120, 180 };
    lv_obj_t *scr = lv_scr_act();
    int i;

    lv_obj_set_style_bg_color(scr, CLR_BG, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* 状态胶囊 */
    s_tag = mk_rect(scr, 92, 6, 56, 20, 10, CLR_TAG_BG_D);
    s_tag_txt = lv_label_create(s_tag);
    lv_obj_set_style_text_font(s_tag_txt, &font_cn_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_tag_txt, CLR_DISCH, LV_PART_MAIN);
    lv_obj_center(s_tag_txt);
    lv_label_set_text(s_tag_txt, "放电中");

    /* 电池外壳（透明底 + 灰描边） */
    s_shell = lv_obj_create(scr);
    lv_obj_clear_flag(s_shell, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(s_shell, BATT_X, BATT_Y);
    lv_obj_set_size(s_shell, BATT_W, BATT_H);
    lv_obj_set_style_radius(s_shell, 18, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_shell, 3, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_shell, CLR_SHELL, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_shell, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_shell, 0, LV_PART_MAIN);

    /* 电极头（右侧小凸块） */
    s_cap = mk_rect(scr, BATT_X + BATT_W + 3, 52, 9, 48, 4, CLR_SHELL);

    /* 填充条：宽度随 SOC 变化 */
    s_fill = mk_rect(scr, BATT_X + 4, BATT_Y + 4, BATT_INNER_W, BATT_H - 8, 13, CLR_CHG);

    /* SOC 大字叠在电池正中 */
    s_soc = mk_label(scr, "76%", &lv_font_montserrat_40, CLR_TEXT,
                     BATT_X, BATT_Y, BATT_W, BATT_H);

    /* 续航 */
    s_range = mk_label(scr, "续航 --", &font_cn_12, CLR_RANGE, 16, 130, 208, 16);

    /* 分隔线 */
    s_line1 = mk_rect(scr, 24, 156, 192, 1, 0, CLR_LINE);
    s_line2 = mk_rect(scr, 24, 204, 192, 1, 0, CLR_LINE);

    /* 电压 / 电流 / 功率 三列 */
    for (i = 0; i < 3; i++) {
        s_val[i]  = mk_label(scr, "--", &lv_font_montserrat_16, CLR_TEXT,
                             col_x[i] - 36, 162, 72, 18);
        s_unit[i] = mk_label(scr, "--", &font_cn_12, CLR_MUTED,
                             col_x[i] - 36, 182, 72, 14);
    }
    lv_label_set_text(s_unit[0], "电压 V");
    lv_label_set_text(s_unit[1], "电流 A");
    lv_label_set_text(s_unit[2], "功率 kW");

    /* 底部容量行：三段文字水平居中排布 */
    s_cap_row = lv_obj_create(scr);
    lv_obj_clear_flag(s_cap_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(s_cap_row, 0, 208);
    lv_obj_set_size(s_cap_row, 240, 18);
    lv_obj_set_style_bg_opa(s_cap_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_cap_row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_cap_row, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_cap_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_cap_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    s_cap_pre = lv_label_create(s_cap_row);
    lv_obj_set_style_text_font(s_cap_pre, &font_cn_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_cap_pre, CLR_CAP_TXT, LV_PART_MAIN);
    lv_label_set_text(s_cap_pre, "自学习容量：");

    s_cap_val = lv_label_create(s_cap_row);
    lv_obj_set_style_text_font(s_cap_val, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_cap_val, CLR_TEXT, LV_PART_MAIN);
    lv_label_set_text(s_cap_val, "--");

    s_cap_suf = lv_label_create(s_cap_row);
    lv_obj_set_style_text_font(s_cap_suf, &font_cn_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_cap_suf, CLR_CAP_TXT, LV_PART_MAIN);
    lv_label_set_text(s_cap_suf, "(±5%)");
}

/* ---------------------------------------------------------------- 刷新 */

void ui_update(const ui_data_t *d)
{
    char buf[32];
    lv_color_t soc_color;
    int soc;
    if (d == NULL) return;

    soc = clamp_soc(d->soc);

    /* --- SOC 填充条：色相 = SOC × 1.2°（0 红 → 120 绿），与手机电量语义一致 ---
     * 注意 LVGL v8 的 lv_color_hsv_to_rgb(h, s, v) 直接返回颜色，
     * 且 s/v 的合法范围是 0~100（百分比），不是 0~255。 */
    soc_color = lv_color_hsv_to_rgb((uint16_t)(soc * 12 / 10), 78, 86);
    lv_obj_set_width(s_fill, (lv_coord_t)((BATT_INNER_W * soc) / 100));
    lv_obj_set_style_bg_color(s_fill, soc_color, LV_PART_MAIN);

    /* --- SOC 大字：≤10% 转红 --- */
    lv_snprintf(buf, sizeof(buf), "%d%%", soc);
    lv_label_set_text(s_soc, buf);
    lv_obj_set_style_text_color(s_soc, (soc <= 10) ? CLR_LOW : CLR_TEXT, LV_PART_MAIN);

    /* --- 状态胶囊 --- */
    if (d->charging) {
        lv_obj_set_style_bg_color(s_tag, CLR_TAG_BG_C, LV_PART_MAIN);
        lv_obj_set_style_text_color(s_tag_txt, CLR_CHG, LV_PART_MAIN);
        lv_label_set_text(s_tag_txt, "充电中");
    } else {
        lv_obj_set_style_bg_color(s_tag, CLR_TAG_BG_D, LV_PART_MAIN);
        lv_obj_set_style_text_color(s_tag_txt, CLR_DISCH, LV_PART_MAIN);
        lv_label_set_text(s_tag_txt, "放电中");
    }

    /* --- 续航：无距离来源时显示 -- --- */
    if (d->range_km >= 0) {
        lv_snprintf(buf, sizeof(buf), "续航 %d km", d->range_km);
    } else {
        lv_snprintf(buf, sizeof(buf), "续航 --");
    }
    lv_label_set_text(s_range, buf);

    /* --- 电压 --- */
    lv_snprintf(buf, sizeof(buf), "%.2f", d->voltage);
    lv_label_set_text(s_val[0], buf);

    /* --- 电流：符号按充放方向强制，幅度取绝对值 --- */
    {
        float a = d->current >= 0 ? d->current : -d->current;
        lv_snprintf(buf, sizeof(buf), "%s%.1f", d->charging ? "+" : "-", a);
        lv_label_set_text(s_val[1], buf);
        lv_obj_set_style_text_color(s_val[1], d->charging ? CLR_CHG : CLR_DISCH, LV_PART_MAIN);
    }

    /* --- 功率 --- */
    lv_snprintf(buf, sizeof(buf), "%.2f", d->power_kw);
    lv_label_set_text(s_val[2], buf);

    /* --- 自学习容量：capacity_ah < 0 表示尚未标定 --- */
    if (d->capacity_ah < 0.0f) {
        lv_label_set_text(s_cap_val, "--");
    } else {
        lv_snprintf(buf, sizeof(buf), "%.1fAh", d->capacity_ah);
        lv_label_set_text(s_cap_val, buf);
    }
}