/**
 * ui_raw_v2.h - 裸机 V2 界面（复刻 ui_lvgl.c 布局，带中文点阵）
 *
 * 对外 API 与 ui.h 同构但零 LVGL 依赖，可直接在 STM32G431 上替换使用。
 * 布局/颜色/文案与 ui_lvgl.c 完全一致，见 ui_raw_v2.c 顶部坐标表。
 */
#ifndef UI_RAW_V2_H
#define UI_RAW_V2_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float    soc;          /* 0~100 */
    float    voltage;      /* V */
    float    current;      /* A，放电为正、充电为负 */
    float    power_kw;     /* kW */
    float    capacity_ah;  /* <0 未标定 */
    int      range_km;     /* <0 显示 -- */
    uint8_t  charging;     /* 1 充电 / 0 放电 */
} ui_raw_data_t;

void ui_raw_init(void);
void ui_raw_update(const ui_raw_data_t *d);

#ifdef __cplusplus
}
#endif
#endif
