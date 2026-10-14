/**
  ******************************************************************************
  * @file    st7789.h
  * @brief   ST7789V2 LCD driver for 1.69" 240x280 module (4-line SPI).
  *          - No hardware RESET pin on this board (module RESET tied high);
  *            driver uses the SWRESET command.
  *          - 32 KB RAM on the MCU ⇒ one-scanline buffer only (no framebuffer).
  ******************************************************************************
  */
#ifndef __ST7789_H
#define __ST7789_H

#include "spi.h"
#include "main.h"
#include <stdint.h>

#define ST7789_WIDTH    240
#define ST7789_HEIGHT   280   /* visible panel area of this module */
#define ST7789_RAM_ROWS 320   /* ST7789 GRAM is fixed 240x320; the 240x280
                               * panel only shows a 280-row window of it, so
                               * solid fills write the whole RAM to guarantee
                               * the visible area is fully covered */

/* ST7789 command set (verified ST7789VW datasheet §9.1) */
#define ST7789_SWRESET 0x01
#define ST7789_SLPOUT  0x11
#define ST7789_NORON   0x13
#define ST7789_INVON   0x21
#define ST7789_DISPON  0x29
#define ST7789_CASET   0x2A
#define ST7789_RASET   0x2B
#define ST7789_RAMWR   0x2C
#define ST7789_MADCTL  0x36
#define ST7789_COLMOD  0x3A

/* ST7789 GRAM is 240x320; this panel shows a 280-row window starting at
 * RAM row 40 (rows 0..39 are not bonded). All rect/blit functions take
 * SCREEN coordinates (0..279) and internally add this offset. */
#define ST7789_VRAM_OFFSET 40

/* MADCTL (36h): 0x00 = portrait, RGB order, top-to-bottom.
 * Use 0x08 (BGR) if Red/Blue look swapped; 0x80/0x40 to flip rows/cols. */
#define ST7789_MADCTL_CFG 0x00

/* COLMOD (3Ah) for 4-line serial: bit7=0 (serial), MDT=01 → 16-bit RGB565 */
#define ST7789_COLMOD_16BIT 0x05

/* RGB565 colour primaries */
#define ST7789_RED     0xF800
#define ST7789_GREEN   0x07E0
#define ST7789_BLUE    0x001F

void st7789_init(void);
void st7789_fill_color(uint16_t color);
void st7789_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
/* Screen-coordinate local drawing (see ST7789_VRAM_OFFSET). */
void st7789_fill_rect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
/* data = packed RGB565 row-major, MSB-first byte pairs for the rect. */
void st7789_blit_rect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, const uint8_t *data);

#endif /* __ST7789_H */
