/**
 * gfx_raw.c - 裸机 2D 原语实现（无帧缓冲，直刷 ST7789）
 *
 * 圆角算法：角部按圆方程扫描，中间矩形整体推色，Fill 尽量合并成整行 Push。
 * 文字：UTF-8 解码 -> 查表 -> 逐行 Blit，1-bit 点阵，前景点亮、背景透明。
 */
#include "gfx_raw.h"
#include "font_raw.h"

void GFX_FillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    if (w <= 0 || h <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > 240) w = 240 - x;
    if (y + h > 240) h = 240 - y;
    if (w <= 0 || h <= 0) return;
    ST7789_SetWindow(x, y, w, h);
    ST7789_PushPixelsSolid(color, (int32_t)w * h);
}

static int in_corner(int dx, int dy, int r)
/* 角部第dy行(0=top)应保留的宽度：圆心在内角(r,r)。dx兼容旧调用，忽略。 */
{
    int x;
    (void)dx;
    for (x = 0; x < r; x++) {
        int xx = x - r;
        int yy = dy - r;
        if (xx * xx + yy * yy <= r * r) return r - x;
    }
    return 1;
}

void GFX_FillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color)
{
    int16_t yy;
    if (w <= 0 || h <= 0) return;
    if (r <= 0) { GFX_FillRect(x, y, w, h, color); return; }
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    /* 中间主体 */
    GFX_FillRect(x, y + r, w, h - 2 * r, color);
    /* 上下圆角带：逐行算左右留白 */
    for (yy = 0; yy < r; yy++) {
        int keep = in_corner(r - 1, yy, r); /* 该行每侧保留宽度 */
        int xl = x + (r - keep);
        int ww = w - 2 * (r - keep);
        GFX_FillRect(xl, y + yy, ww, 1, color);
        GFX_FillRect(xl, y + h - 1 - yy, ww, 1, color);
    }
    /* 左右中带 */
    GFX_FillRect(x, y + r, r, h - 2 * r, color);
    GFX_FillRect(x + w - r, y + r, r, h - 2 * r, color);
}

void GFX_DrawRoundBorder(int16_t x, int16_t y, int16_t w, int16_t h,
                         int16_t r, int16_t lw, uint16_t color)
{
    /* 外框实心圆角 - 内框背景色挖空 = 描边（保证四角连续无缺口） */
    /* 本UI背景纯色且边框画在清屏后，此法成立；填充条在边框内，后画覆盖 */
    uint16_t bg = GFX_RGB565(0x0B, 0x0E, 0x13);
    GFX_FillRoundRect(x, y, w, h, r, color);
    GFX_FillRoundRect(x + lw, y + lw, w - 2 * lw, h - 2 * lw,
                      r > lw ? r - lw : 0, bg);
}

const raw_glyph_t *GFX_FindGlyph(const raw_glyph_t *tab, int n, uint32_t cp)
{
    int i;
    for (i = 0; i < n; i++)
        if (tab[i].cp == cp) return &tab[i];
    return 0;
}

static uint32_t utf8_next(const char **p)
{
    const unsigned char *s = (const unsigned char *)*p;
    uint32_t cp;
    if (s[0] < 0x80) { *p += 1; return s[0]; }
    if ((s[0] & 0xE0) == 0xC0) { cp = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F); *p += 2; return cp; }
    if ((s[0] & 0xF0) == 0xE0) {
        cp = ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        *p += 3; return cp;
    }
    if ((s[0] & 0xF8) == 0xF0) {
        cp = ((s[0] & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        *p += 4; return cp;
    }
    *p += 1; return 0xFFFD;
}

uint16_t GFX_TextWidth(const raw_glyph_t *tab, int n, const char *utf8)
{
    const char *p = utf8;
    uint16_t w = 0;
    while (*p) {
        uint32_t cp = utf8_next(&p);
        const raw_glyph_t *g = GFX_FindGlyph(tab, n, cp);
        if (g) w += g->w + 1;
    }
    if (w) w -= 1;
    return w;
}

void GFX_DrawText(int16_t x, int16_t y, const raw_glyph_t *tab, int n,
                  const uint8_t *blob, const char *utf8, uint16_t color)
{
    const char *p = utf8;
    int16_t cx = x;
    while (*p) {
        uint32_t cp = utf8_next(&p);
        const raw_glyph_t *g = GFX_FindGlyph(tab, n, cp);
        int row, col;
        int stride;
        if (!g || g->len == 0) { cx += (g ? g->w : 6) + 1; continue; }
        stride = (g->w + 7) / 8;
        for (row = 0; row < g->h; row++) {
            for (col = 0; col < g->w; col++) {
                uint8_t byte = blob[g->off + row * stride + col / 8];
                if (byte & (0x80 >> (col % 8))) {
                    ST7789_SetWindow(cx + col, y + row, 1, 1);
                    ST7789_PushPixelsSolid(color, 1);
                }
            }
        }
        cx += g->w + 1;
    }
}

void GFX_DrawTextCentered(int16_t y, int16_t w, const raw_glyph_t *tab, int n,
                          const uint8_t *blob, const char *utf8, uint16_t color)
{
    uint16_t tw = GFX_TextWidth(tab, n, utf8);
    GFX_DrawText((240 - tw) / 2, y, tab, n, blob, utf8, color);
    (void)w;
}

void GFX_DrawTextCenteredIn(int16_t x, int16_t y, int16_t w, const raw_glyph_t *tab, int n,
                            const uint8_t *blob, const char *utf8, uint16_t color)
{
    uint16_t tw = GFX_TextWidth(tab, n, utf8);
    int16_t sx = x + (w - (int16_t)tw) / 2;
    if (sx < x) sx = x;
    GFX_DrawText(sx, y, tab, n, blob, utf8, color);
}
