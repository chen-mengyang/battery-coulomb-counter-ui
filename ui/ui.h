/**
 * ui.h - 库仑计 ST7789 240x240 屏幕 UI（LVGL v8）
 *
 * 布局见 ui_lvgl.c 顶部坐标表，全部为绝对定位，屏幕分辨率固定 240x240。
 */
#ifndef UI_H
#define UI_H

#include <stdint.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float    soc;          /* 0~100，电量百分比                                */
    float    voltage;      /* V                                              */
    float    current;      /* A，放电为正、充电为负（与模拟链路符号约定一致）   */
    float    power_kw;     /* kW，已换算好的功率                              */
    float    capacity_ah;  /* 满充满放自学习容量，首次上电前传 -1 表示未标定   */
    int      range_km;     /* 预计续航里程，<0 表示无数据（显示 --）           */
    uint8_t  charging;     /* 1=充电中 0=放电中                               */
} ui_data_t;

void ui_init(void);
void ui_update(const ui_data_t *d);

#ifdef __cplusplus
}
#endif
#endif
