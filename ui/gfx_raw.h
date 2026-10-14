/**
 * gfx_raw.h - 裸机 2D 原语（ST7789 直刷，无帧缓冲）
 * UI 240x240 顶部对齐 + 屏底 40 行状态带（y=240..279），总高 280。
 *
 * 只提供本 UI 需要的最小集合：
 *   FillRect / FillRoundRect / DrawRoundBorder / BlitGlyph1bit / Text*
 * 像素输出经 ST7789_* 回调直接推屏，RAM 占用 ~0。
 */
#ifndef GFX_RAW_H
#define GFX_RAW_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 由移植层实现（st7789_port.c）：开窗 + 连续推 RGB565 */
void ST7789_SetWindow(int16_t x, int16_t y, int16_t w, int16_t h);
void ST7789_PushPixels(const uint16_t *px, int32_t n);
void ST7789_PushPixelsSolid(uint16_t color, int32_t n);

/* RGB888 -> RGB565 */
static inline uint16_t GFX_RGB565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

void GFX_FillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void GFX_FillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color);
/* 圆角描边：线宽 lw，内收绘制 */
void GFX_DrawRoundBorder(int16_t x, int16_t y, int16_t w, int16_t h,
                         int16_t r, int16_t lw, uint16_t color);

/* ---- 1-bit 点阵文字（结构定义见 font_raw.h） ---- */
struct raw_glyph_s;
typedef struct raw_glyph_s raw_glyph_t;

const raw_glyph_t *GFX_FindGlyph(const raw_glyph_t *tab, int n, uint32_t cp);
uint16_t GFX_TextWidth(const raw_glyph_t *tab, int n, const char *utf8);
void GFX_DrawText(int16_t x, int16_t y, const raw_glyph_t *tab, int n,
                  const uint8_t *blob, const char *utf8, uint16_t color);
void GFX_DrawTextCentered(int16_t y, int16_t w, const raw_glyph_t *tab, int n,
                          const uint8_t *blob, const char *utf8, uint16_t color);
/* 以区域 (x,w) 居中，y 为基线顶 */
void GFX_DrawTextCenteredIn(int16_t x, int16_t y, int16_t w, const raw_glyph_t *tab, int n,
                            const uint8_t *blob, const char *utf8, uint16_t color);

#ifdef __cplusplus
}
#endif
#endif
