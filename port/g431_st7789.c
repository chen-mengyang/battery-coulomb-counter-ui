/**
 * g431_st7789.c - 转接：gfx_raw 的 ST7789_* 回调 -> 本地硬件
 *
 * 用法：把本文件 + g431_port.h 拷入 CubeIDE 工程 Core/Src|Inc，
 * 与工程自带的 st7789.c/h、spi.h、main.h 一起编译即可。
 * （本仓库存一份，与 g431_main_demo.c 配套，方便对照。）
 *
 * 说明：
 *  - 自带 CS/DC 操作（main.h 引脚宏是公开的），不依赖 st7789.c 内部静态函数。
 *  - y 直接透传“屏坐标”（本地屏可见 240x280；UI 只用 0..239）。
 *  - RGB565 字节序：ST7789 串行要求 MSB 先行，与 GFX_RGB565 一致。
 *  - ST7789_Init 不在这里实现：复用工程 st7789_init()（SWRESET、无 RST 脚）。
 */
#include "g431_port.h"

#if defined(STM32G431xx)
#include "main.h"
#include "spi.h"

#define P_CS_L()   HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET)
#define P_CS_H()   HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET)
#define P_DC_CMD() HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_RESET)
#define P_DC_DAT() HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET)

static void p_wr_cmd(uint8_t c)
{
    P_CS_L(); P_DC_CMD();
    HAL_SPI_Transmit(&hspi3, &c, 1, HAL_MAX_DELAY);
    P_CS_H();
}

static void p_wr_data(const uint8_t *b, uint32_t n)
{
    P_CS_L(); P_DC_DAT();
    HAL_SPI_Transmit(&hspi3, (uint8_t *)b, n, HAL_MAX_DELAY);
    P_CS_H();
}

void ST7789_SetWindow(int16_t x, int16_t y, int16_t w, int16_t h)
{
    uint8_t b[4];
    uint16_t x1 = (uint16_t)(x + w - 1);
    uint16_t y1 = (uint16_t)(y + h - 1);
    p_wr_cmd(0x2A);
    b[0] = (uint8_t)((uint16_t)x >> 8); b[1] = (uint8_t)x;
    b[2] = (uint8_t)(x1 >> 8);          b[3] = (uint8_t)x1;
    p_wr_data(b, 4);
    p_wr_cmd(0x2B);
    b[0] = (uint8_t)((uint16_t)y >> 8); b[1] = (uint8_t)y;
    b[2] = (uint8_t)(y1 >> 8);          b[3] = (uint8_t)y1;
    p_wr_data(b, 4);
    p_wr_cmd(0x2C);
}

void ST7789_PushPixels(const uint16_t *px, int32_t n)
{
    int32_t i;
    P_CS_L(); P_DC_DAT();
    for (i = 0; i < n; i++) {
        uint8_t b[2];
        b[0] = (uint8_t)(px[i] >> 8);
        b[1] = (uint8_t)(px[i] & 0xFF);
        HAL_SPI_Transmit(&hspi3, b, 2, HAL_MAX_DELAY);
    }
    P_CS_H();
}

void ST7789_PushPixelsSolid(uint16_t color, int32_t n)
{
    uint8_t b[2];
    int32_t i;
    b[0] = (uint8_t)(color >> 8);
    b[1] = (uint8_t)(color & 0xFF);
    P_CS_L(); P_DC_DAT();
    for (i = 0; i < n; i++) {
        HAL_SPI_Transmit(&hspi3, b, 2, HAL_MAX_DELAY);
    }
    P_CS_H();
}

void G431_BacklightOn(void)
{
    HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, GPIO_PIN_SET);
}

#else
void ST7789_SetWindow(int16_t x, int16_t y, int16_t w, int16_t h)
{ (void)x; (void)y; (void)w; (void)h; }
void ST7789_PushPixels(const uint16_t *px, int32_t n) { (void)px; (void)n; }
void ST7789_PushPixelsSolid(uint16_t color, int32_t n) { (void)color; (void)n; }
void G431_BacklightOn(void) {}
#endif
