/**
 * g431_st7789.c - 转接：gfx_raw 的 ST7789_* 回调 -> 本地硬件
 *
 * 用法：把本文件 + g431_port.h 拷入 CubeIDE 工程 Core/Src|Inc，
 * 与工程自带的 st7789.c/h、spi.h、main.h 一起编译即可。
 * （本仓库存一份，与 g431_main_demo.c 配套，方便对照。）
 *
 * 说明：
 *  - 自带 CS/DC 操作（main.h 引脚宏是公开的），不依赖 st7789.c 内部静态函数。
 *  - y 经 G431_YOFF 平移（面板 240x280 垂直居中，GRAM 起始行 40）。
 *  - RGB565 字节序：ST7789 串行要求 MSB 先行，与 GFX_RGB565 一致。
 *  - ST7789_Init 不在这里实现：复用工程 st7789_init()（SWRESET、无 RST 脚）。
 *  - 发送走直写 SPI 寄存器 sp_write + 批量块推（25Hz 全屏刷新的关键）。
 */
#include "g431_port.h"

#if defined(STM32G431xx)
#include "main.h"
#include "spi.h"

#define P_CS_L()   HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET)
#define P_CS_H()   HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET)
#define P_DC_CMD() HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_RESET)
#define P_DC_DAT() HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET)

/* 直写 SPI 寄存器发送：绕开 HAL_SPI_Transmit 每次调用的状态机开销。
 * 8bit 帧模式（MX_SPI3_Init），逐字节写 DR 由 TXE 按线速（42.5MHz）自然
 * 背压；结束后等 TX FIFO 排空 + BSY 清零，才允许切换 DC/CS。 */
static void sp_write(const uint8_t *b, uint32_t n)
{
    while (n--) {
        while (!(hspi3.Instance->SR & SPI_SR_TXE)) {}
        *(__IO uint8_t *)&hspi3.Instance->DR = *b++;
    }
    while (hspi3.Instance->SR & SPI_SR_FTLVL) {}
    while (hspi3.Instance->SR & SPI_SR_BSY) {}
}

static void p_wr_cmd(uint8_t c)
{
    P_CS_L(); P_DC_CMD();
    sp_write(&c, 1);
    P_CS_H();
}

static void p_wr_data(const uint8_t *b, uint32_t n)
{
    P_CS_L(); P_DC_DAT();
    sp_write(b, n);
    P_CS_H();
}

void ST7789_SetWindow(int16_t x, int16_t y, int16_t w, int16_t h)
{
    uint8_t b[4];
    uint16_t x1 = (uint16_t)(x + w - 1);
    uint16_t y1 = (uint16_t)(y + h - 1 + G431_YOFF);   /* 屏幕坐标 → GRAM */
    y = (int16_t)(y + G431_YOFF);
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
    static uint8_t chunk[512];
    int32_t left = n;
    if (left <= 0) return;
    P_CS_L(); P_DC_DAT();
    while (left > 0) {
        uint32_t cnt = (left > 256) ? 256u : (uint32_t)left;
        uint32_t i;
        for (i = 0; i < cnt; i++) {
            chunk[2 * i]     = (uint8_t)(px[i] >> 8);   /* MSB first */
            chunk[2 * i + 1] = (uint8_t)(px[i]);
        }
        sp_write(chunk, cnt * 2u);
        px += cnt;
        left -= (int32_t)cnt;
    }
    P_CS_H();
}

void ST7789_PushPixelsSolid(uint16_t color, int32_t n)
{
    static uint8_t pat[512];          /* 256 px 纯色样例，整块反复重发 */
    static uint16_t pat_col;
    static uint8_t pat_valid;
    int32_t left = n;
    if (left <= 0) return;
    if (!pat_valid || pat_col != color) {
        uint32_t i;
        for (i = 0; i < sizeof(pat); i += 2) {
            pat[i]     = (uint8_t)(color >> 8);         /* MSB first */
            pat[i + 1] = (uint8_t)(color & 0xFF);
        }
        pat_col = color;
        pat_valid = 1;
    }
    P_CS_L(); P_DC_DAT();
    while (left > 0) {
        uint32_t px = (left > 256) ? 256u : (uint32_t)left;
        sp_write(pat, px * 2u);
        left -= (int32_t)px;
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
