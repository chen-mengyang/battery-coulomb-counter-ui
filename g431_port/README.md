# G431 移植层（STM32G431C8T6 + ST7789 240×280）

反向合并：本仓库**裸机 UI 为主体**，`g431_port/` 只放与 CubeIDE 工程
`stm32g431c8t6_prj` 对接的转接代码。不含 LVGL。

## 硬件（本地板）

| 信号 | 引脚 | 说明 |
|---|---|---|
| LCD_CS | PA0 | 手动 CS |
| LCD_DC | PA1 | D/CX |
| LCD_BL | PA2 | 高电平开背光 |
| SCK/MOSI | PB3/PB5 | SPI3，8bit，CPOL=0/CPHA=0，42.5MHz |
| KEY/LED | PA15/PC13 | 保留，本演示未用 |
| RESET | 未接 MCU | 模块内部上拉，用 SWRESET |

## 文件

- `g431_port.h` — 屏尺寸宏（240×280，UI 顶部对齐占 0..239）+ 转接声明
- `g431_st7789.c` — `ST7789_SetWindow/PushPixels/PushPixelsSolid` 实现（自带 CS/DC，
  拷入 CubeIDE 工程即用）；`ST7789_Init` 复用工程 `st7789_init()`
- `g431_main_demo.c` — main.c USER CODE 粘贴参考

## 拷入 CubeIDE 工程

把以下文件拷入 `stm32g431c8t6_prj/Core/Src|Inc`：

```
g431_port/g431_port.h      -> Core/Inc/
g431_port/g431_st7789.c    -> Core/Src/
ui_raw_v2.h                -> Core/Inc/
ui_raw_v2.c                -> Core/Src/
ui_raw_draw.c              -> Core/Src/
gfx_raw.h                  -> Core/Inc/
gfx_raw.c                  -> Core/Src/
font_raw.h                 -> Core/Inc/
```

`main.c` 按 `g431_main_demo.c` 改三处 USER CODE 即可。

## 分辨率

UI 按 240×240 设计，本屏可见 240×280：UI 顶部对齐（y=0..239），
底部 40 行由 demo 清成背景色留白。
