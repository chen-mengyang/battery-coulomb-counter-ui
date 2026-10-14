/**
  ******************************************************************************
  * @file    st7789.c
  * @brief   ST7789V2 driver: 4-line serial SPI, 240x280, 16-bit RGB565.
  *
  * Hardware notes:
  *  - LCD_CS   PA0  (manual CS, SPI_NSS = SOFT)
  *  - LCD_DC   PA1  (D/CX: 0 = command, 1 = data)
  *  - LCD_BL   PA2  (backlight enable — driven HIGH in main.c)
  *  - RESET    module-tied-high, NOT wired to MCU ⇒ use SWRESET command.
  *
  * Memory: 32 KB RAM on STM32G431 ⇒ no full framebuffer (frame = 153.6 KB).
  *         Screen fills stream a 16-row homogenous block (7680 B) via SPI3_TX
  *         DMA, re-armed from the DMA TX-complete interrupt.
  ******************************************************************************
  */
#include "st7789.h"

/* ---- GPIO helpers for manual CS + D/CX ---- */
#define LCD_CS_L()   HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET)
#define LCD_CS_H()   HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET)
#define LCD_CMD()    HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_RESET)
#define LCD_DATA()   HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET)

/* ---- DMA full-screen fill state ----
 * The frame (240x320x2 = 153.6 KB) does not fit in RAM, so it is sent as
 * repeated copies of a single homogenous block buffer via SPI3_TX DMA
 * (DMA1_Channel1). The TX-complete interrupt re-arms the next block until
 * all rows are done, then clears st7789_fill_active (the loop in
 * st7789_fill_color() raises CS and returns). */
#define ST7789_BLOCK_ROWS 16                       /* 16 rows x 480 B = 7680 B */
static uint8_t          st7789_block[ST7789_BLOCK_ROWS * ST7789_WIDTH * 2];
static volatile uint8_t st7789_fill_active;        /* 1 while a burst is running */
static uint16_t         st7789_blocks_left;        /* remaining blocks */

/* Send one command byte (D/CX = 0).
 * HAL_SPI_Transmit() waits internally for TX FIFO empty + BSY reset, so the
 * last bit is fully shifted out before we raise CS — no extra polling here. */
static void st7789_send_cmd(uint8_t cmd)
{
    LCD_CS_L();
    LCD_CMD();
    HAL_SPI_Transmit(&hspi3, &cmd, 1, HAL_MAX_DELAY);
    LCD_CS_H();
}

/* Send data bytes (D/CX = 1) */
static void st7789_send_data(const uint8_t *buf, uint32_t len)
{
    LCD_CS_L();
    LCD_DATA();
    HAL_SPI_Transmit(&hspi3, (uint8_t *)buf, len, HAL_MAX_DELAY);
    LCD_CS_H();
}

/* Set the row/column write window (CASET + RASET), 16-bit MSB-first coords */
void st7789_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t buf[4];

    st7789_send_cmd(ST7789_CASET);
    buf[0] = (uint8_t)(x0 >> 8); buf[1] = (uint8_t)(x0);
    buf[2] = (uint8_t)(x1 >> 8); buf[3] = (uint8_t)(x1);
    st7789_send_data(buf, 4);

    st7789_send_cmd(ST7789_RASET);
    buf[0] = (uint8_t)(y0 >> 8); buf[1] = (uint8_t)(y0);
    buf[2] = (uint8_t)(y1 >> 8); buf[3] = (uint8_t)(y1);
    st7789_send_data(buf, 4);

    st7789_send_cmd(ST7789_RAMWR); /* memory write; following bytes = pixels */
}

/* ---- local (screen-coordinate) drawing ----
 * The panel's visible window is RAM rows [40..319], so y here (0..279)
 * is mapped to RAM y+ST7789_VRAM_OFFSET. Both helpers keep CS low and
 * D/CX high across the whole region for a continuous RAMWR burst. */

static uint8_t st7789_rect_row[ST7789_WIDTH * 2]; /* solid row buffer (480 B) */

/* Fill a rectangle with one RGB565 colour (screen coords). */
void st7789_fill_rect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
    uint16_t w = (uint16_t)(x1 - x0 + 1);
    uint16_t h = (uint16_t)(y1 - y0 + 1);
    uint16_t r, i;
    uint8_t hi = (uint8_t)(color >> 8);
    uint8_t lo = (uint8_t)(color & 0xFF);

    if (x1 < x0 || y1 < y0)
    {
        return;
    }

    for (i = 0; i < w; i++)
    {
        st7789_rect_row[i * 2]     = hi;
        st7789_rect_row[i * 2 + 1] = lo;
    }

    st7789_set_window(x0, (uint16_t)(y0 + ST7789_VRAM_OFFSET),
                      x1, (uint16_t)(y1 + ST7789_VRAM_OFFSET));

    LCD_CS_L();
    LCD_DATA();
    for (r = 0; r < h; r++)
    {
        HAL_SPI_Transmit(&hspi3, st7789_rect_row, w * 2, HAL_MAX_DELAY);
    }
    LCD_CS_H();
}

/* Blit a packed RGB565 region (row-major, MSB-first byte pairs, screen coords).
 * Data is streamed in ≤1 KB chunks while holding CS/DC for the whole burst. */
void st7789_blit_rect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, const uint8_t *data)
{
    uint32_t total = (uint32_t)(x1 - x0 + 1) * (uint32_t)(y1 - y0 + 1) * 2u;
    uint32_t sent  = 0;

    if (x1 < x0 || y1 < y0)
    {
        return;
    }

    st7789_set_window(x0, (uint16_t)(y0 + ST7789_VRAM_OFFSET),
                      x1, (uint16_t)(y1 + ST7789_VRAM_OFFSET));

    LCD_CS_L();
    LCD_DATA();
    while (sent < total)
    {
        uint32_t chunk = total - sent;
        if (chunk > 1000u)
        {
            chunk = 1000u;
        }
        HAL_SPI_Transmit(&hspi3, (uint8_t *)(data + sent), (uint16_t)chunk, HAL_MAX_DELAY);
        sent += chunk;
    }
    LCD_CS_H();
}

/* Initialise the ST7789V2 controller.
 * RESET is not wired to the MCU (module RESET pulled high), so a software
 * reset (SWRESET) is used. After SWRESET the datasheet requires ≥120 ms
 * (TRT, NVM-register load time) before further commands, and the Sleep Out
 * command cannot be issued before 120 ms — we use 150 ms to be safe. */
void st7789_init(void)
{
    /* Software reset (replaces hardware RESET, which isn't available here) */
    st7789_send_cmd(ST7789_SWRESET);
    HAL_Delay(150);  /* ≥ TRT = 120 ms */

    st7789_send_cmd(ST7789_SLPOUT);  /* sleep out (11h) */
    HAL_Delay(120);  /* "Sleep Out cannot be sent for 120 ms" after reset */

    st7789_send_cmd(ST7789_MADCTL);  /* memory access control */
    {
        uint8_t mad = ST7789_MADCTL_CFG;
        st7789_send_data(&mad, 1);
    }

    st7789_send_cmd(ST7789_COLMOD);  /* interface pixel format */
    {
        uint8_t colmod = ST7789_COLMOD_16BIT; /* 16-bit RGB565 for serial */
        st7789_send_data(&colmod, 1);
    }

    st7789_send_cmd(ST7789_INVON);   /* display inversion on (21h) */
    st7789_send_cmd(ST7789_NORON);   /* normal display mode on (13h) */
    HAL_Delay(10);

    st7789_send_cmd(ST7789_DISPON);  /* display on (29h) */
    HAL_Delay(120);  /* let the display start up */

    /* Fill with black so we start from a known state */
    st7789_fill_color(0x0000);
}

/* Fill the whole screen with one RGB565 colour via SPI3_TX DMA.
 * The ST7789 GRAM is 240x320 but this panel is 240x280, so the visible
 * window sits at some vertical offset inside the 320-row RAM.  Writing the
 * ENTIRE 240x320 RAM guarantees every visible row gets the colour no matter
 * where the panel window starts.
 * A full frame would be 240x320x2 = 150 KB ≫ 32 KB RAM, so one homogenous
 * 16-row block (7680 B) is DMA'd out repeatedly; the TX-complete interrupt
 * re-arms the next block until all 320 rows are done. */
void st7789_fill_color(uint16_t color)
{
    uint8_t hi = (uint8_t)(color >> 8);
    uint8_t lo = (uint8_t)(color & 0xFF);
    uint32_t i;
    uint32_t t0;

    if (st7789_fill_active)      /* a burst is already in flight - ignore */
    {
        return;
    }

    /* Build one block = 16 rows of 240 RGB565 pixels, MSB-first:
     * byte 0 = upper 8 bits (R[4:0] G[5:3]), byte 1 = lower 8 bits.
     * (Datasheet §8.8.3: serial data MSB first.) */
    for (i = 0; i < sizeof(st7789_block); i += 2)
    {
        st7789_block[i]     = hi;
        st7789_block[i + 1] = lo;
    }

    st7789_set_window(0, 0, ST7789_WIDTH - 1, ST7789_RAM_ROWS - 1);

    /* Continuous burst: CS low + D/CX high for the entire 320-row fill. */
    LCD_CS_L();
    LCD_DATA();

    st7789_fill_active = 1;
    st7789_blocks_left = ST7789_RAM_ROWS / ST7789_BLOCK_ROWS;   /* 20 */

    /* Kick off the first DMA block; HAL_SPI_TxCpltCallback() sends the
     * remaining blocks then clears st7789_fill_active. */
    HAL_SPI_Transmit_DMA(&hspi3, st7789_block, (uint16_t)sizeof(st7789_block));

    /* Park the CPU until the DMA burst completes (interrupt ends the wait).
     * Safety timeout prevents a hang if DMA ever misfires. */
    t0 = HAL_GetTick();
    while (st7789_fill_active && ((HAL_GetTick() - t0) < 500U))
    {
    }

    /* Last byte is guaranteed fully shifted out (HAL waits for BSY=0 inside
     * the DMA-complete ISR before invoking the user callback). */
    LCD_CS_H();
}

/* SPI3 TX-complete callback (called from DMA1_Channel1 IRQ, SPI_DMATransmitCplt):
 * - re-arms the next block of the fill burst (CS is still low and DC high),
 * - on the final block clears st7789_fill_active so st7789_fill_color() ends.
 * Safe to call HAL_SPI_Transmit_DMA() here because HAL restores
 * SPI State=READY and BSY=0 before invoking this callback. */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance != SPI3)
    {
        return;
    }

    if (st7789_blocks_left > 1)
    {
        st7789_blocks_left--;
        HAL_SPI_Transmit_DMA(&hspi3, st7789_block, (uint16_t)sizeof(st7789_block));
    }
    else
    {
        st7789_blocks_left = 0;
        st7789_fill_active = 0;
    }
}
