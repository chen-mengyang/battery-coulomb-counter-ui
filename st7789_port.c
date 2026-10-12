/**
 * st7789_port.c - STM32G431 SPI+DMA 移植模板（ST7789 240x240竖屏）
 *
 * 接线建议：SPI1 SCK/MOSI 推 ST7789 SCL/SDA，另配 D/C + RST + CS。
 *   SPI: 8bit, CPOL=0 CPHA=0, Baud<=40MHz(80MHz PCLK/2), DMA TX。
 *   D/C: 0=命令 1=数据。
 *
 * 实现下面3个函数即可跑 ui_raw_v2：
 *   ST7789_SetWindow / ST7789_PushPixels / ST7789_PushPixelsSolid
 * 中断/DMA完成标志按需替换 ST7789_WaitIdle()。
 */
#include <stdint.h>

/* ---- 按你的 CubeMX 引脚改这几行 ---- */
#if defined(STM32G431xx)
#include "main.h"
#include "spi.h"
/* 例：D/C=PB0 RST=PB1 CS=PB2，请按实际改 */
#define LCD_DC_PORT   GPIOB
#define LCD_DC_PIN    GPIO_PIN_0
#define LCD_RST_PORT  GPIOB
#define LCD_RST_PIN   GPIO_PIN_1
#define LCD_CS_PORT   GPIOB
#define LCD_CS_PIN    GPIO_PIN_2
#define LCD_SPI       hspi1

static void LCD_DC(int v) { HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, v ? GPIO_PIN_SET : GPIO_PIN_RESET); }
static void LCD_CS(int v) { HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, v ? GPIO_PIN_SET : GPIO_PIN_RESET); }
static void LCD_WR_CMD(uint8_t c) { LCD_DC(0); LCD_CS(0); HAL_SPI_Transmit(&LCD_SPI, &c, 1, 10); LCD_CS(1); }
static void LCD_WR_DATA8(uint8_t c) { LCD_DC(1); LCD_CS(0); HAL_SPI_Transmit(&LCD_SPI, &c, 1, 10); LCD_CS(1); }

static void ST7789_WaitIdle(void)
{
    /* DMA方式：等待 HAL_SPI_TxCpltCallback 置位；阻塞方式可留空 */
}

void ST7789_SetWindow(int16_t x, int16_t y, int16_t w, int16_t h)
{
    int16_t x1 = x + w - 1, y1 = y + h - 1;
    LCD_WR_CMD(0x2A);
    LCD_WR_DATA8((x >> 8) & 0xFF); LCD_WR_DATA8(x & 0xFF);
    LCD_WR_DATA8((x1 >> 8) & 0xFF); LCD_WR_DATA8(x1 & 0xFF);
    LCD_WR_CMD(0x2B);
    LCD_WR_DATA8((y >> 8) & 0xFF); LCD_WR_DATA8(y & 0xFF);
    LCD_WR_DATA8((y1 >> 8) & 0xFF); LCD_WR_DATA8(y1 & 0xFF);
    LCD_WR_CMD(0x2C);
}

void ST7789_PushPixels(const uint16_t *px, int32_t n)
{
    LCD_DC(1); LCD_CS(0);
    /* 建议换成 HAL_SPI_Transmit_DMA + WaitIdle；RGB565大端推 */
    HAL_SPI_Transmit(&LCD_SPI, (uint8_t *)px, (uint16_t)n * 2, 1000);
    LCD_CS(1);
}

void ST7789_PushPixelsSolid(uint16_t color, int32_t n)
{
    /* 小n直接循环推；大块建议静态DMA缓冲分片推 */
    uint8_t hi = (color >> 8) & 0xFF, lo = color & 0xFF;
    int32_t i;
    LCD_DC(1); LCD_CS(0);
    for (i = 0; i < n; i++) {
        uint8_t b[2] = { hi, lo };
        HAL_SPI_Transmit(&LCD_SPI, b, 2, 100);
    }
    LCD_CS(1);
    (void)ST7789_WaitIdle;
}

/* 初始化序列（上电调一次）：复位 + MADCTL竖屏 + RGB565 + 开显示 */
void ST7789_Init(void)
{
    HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_RESET);
    HAL_Delay(20);
    HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(120);
    LCD_WR_CMD(0x36); LCD_WR_DATA8(0x00);   /* 竖屏RGB */
    LCD_WR_CMD(0x3A); LCD_WR_DATA8(0x05);   /* 16bit */
    LCD_WR_CMD(0x11); HAL_Delay(120);       /* Sleep out */
    LCD_WR_CMD(0x29);                       /* Display on */
}

#else
/* 非STM32编译（PC验证）时的空实现，链接用 */
void ST7789_SetWindow(int16_t x, int16_t y, int16_t w, int16_t h)
{ (void)x; (void)y; (void)w; (void)h; }
void ST7789_PushPixels(const uint16_t *px, int32_t n) { (void)px; (void)n; }
void ST7789_PushPixelsSolid(uint16_t color, int32_t n) { (void)color; (void)n; }
#endif
