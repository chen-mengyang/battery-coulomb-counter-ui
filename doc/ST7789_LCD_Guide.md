# STM32G431C8T6 + ST7789 240×280 LCD 项目文档

> 开发环境：STM32CubeIDE 2.2.0 · STM32Cube FW_G4 V1.6.3 · 芯片 STM32G431C8T6（LQFP48）
> 屏幕：1.69 寸 IPS，240×280，驱动 IC **ST7789V2**，4-Line SPI 接口
> 功能：屏幕点亮 + 按键三原色切换 + 系统状态 LED

---

## 1. 功能概述

- 上电后屏幕初始化完成，显示黑色，**系统状态 LED（PC13）常亮**表示系统正常运行。
- 短按按键 **KEY_BTN（PA15，低有效）**，屏幕在 **红 → 绿 → 蓝** 三种颜色间循环切换，同时 LED 闪烁一次。
- 屏幕刷新使用 **SPI3 + DMA（DMA1_Channel1）** 传输，时钟 42.5 MHz，全屏填充约 **29 ms**。

---

## 2. 硬件连接

| 功能 | 引脚 | 端口 | 说明 |
|---|---|---|---|
| LCD_SCK | PB3 | SPI3_SCK (AF6) | SPI 时钟，**Very High 速度** |
| LCD_MOSI | PB5 | SPI3_MOSI (AF6) | SPI 数据 |
| LCD_CS | PA0 | GPIO 输出 | 手动片选（SPI_NSS = SOFT） |
| LCD_DC | PA1 | GPIO 输出 | D/CX：0=命令，1=数据 |
| LCD_BL | PA2 | GPIO 输出 | 背光使能（**高电平点亮**） |
| KEY_BTN | PA15 | GPIO 输入（上拉） | 按键，低电平有效 |
| LED | PC13 | GPIO 输出 | 系统状态灯（**低电平点亮**） |

> 注意：模块 RESET 引脚在模组上已上拉，**未连接到 MCU**，因此驱动使用 **SWRESET 命令** 完成复位。

---

## 3. CubeMX 配置要点（`.ioc`）

| 配置项 | 值 | 说明 |
|---|---|---|
| SPI3 Mode | Master，TX Only，Simplex | 只写不读 |
| SPI3 Data Size | **8-bit** | ST7789 4 线串行按字节对齐（CubeMX 默认 4-bit 会花屏！） |
| BaudRate Prescaler | **/4** | PCLK1=170 MHz ÷ 4 = **42.5 MHz**（手册上限约 62.5 MHz） |
| First Bit | MSB first | 必须 |
| NSS | Software | 用 PA0 手动控制 CS |
| CLK Polarity/Phase | Low / 1st Edge | CPOL=0, CPHA=0 |
| PB3/PB5 GPIO Speed | **Very High** | 42.5 MHz 下必须 |
| DMA | SPI3_TX → DMA1_Channel1 | NORMAL 模式，字节对齐，内存递增 |
| NVIC | DMA1_Channel1 IRQ 使能 | 用于 TX 完成中断续传 |

---

## 4. 软件架构

### 4.1 文件清单

```
Core/
├── Inc/
│   ├── st7789.h          ← ST7789 驱动头文件（尺寸/命令/颜色定义）
│   └── ...
└── Src/
    ├── main.c            ← 主逻辑（背光、初始化、按键循环）
    ├── st7789.c          ← ST7789 驱动（初始化/DMA 填充/窗口）
    └── ...
```

### 4.2 关键设计点（st7789.c）

**（1）无硬件 RESET → 软件复位**
```c
st7789_send_cmd(ST7789_SWRESET);   // 01h
HAL_Delay(150);                    // ≥ 手册 TRT = 120 ms
```

**（2）初始化序列**
```
SWRESET → 150ms → SLPOUT(11h) → 120ms → MADCTL(36h)=0x00 → COLMOD(3Ah)=0x05(16bit RGB565)
→ INVON(21h) → NORON(13h) → 10ms → DISPON(29h) → 120ms → 填充黑色
```

**（3）内存受限方案（无整屏 framebuffer）**

整帧 = 240×320×2B = **153.6 KB**，而 MCU 只有 **32 KB RAM**。因此：
- 采用 **16 行同色块缓冲（7680 B）**，预填充一次；
- 通过 **SPI3_TX DMA** 连续发送 20 块覆盖全部 320 行；
- **TX 完成中断**（`HAL_SPI_TxCpltCallback`）里自动续发下一块，CS 全程拉低、D/CX 全程拉高。

**（4）分辨率 240×280 vs GRAM 240×320**
- ST7789 的 GRAM 固定 **240×320**，本面板只显示其中 280 行；
- 因此**实心色填充写入全部 320 行**（`ST7789_RAM_ROWS 320`），确保无论面板映射哪 280 行，屏幕都能被完全覆盖。

---

## 5. 主程序逻辑（main.c）

### USER CODE BEGIN 2（上电初始化）
```c
/* SPI3 已由 CubeMX 配好：8BIT + /4 = 42.5 MHz，无需覆盖 */
HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, GPIO_PIN_SET);   // 背光开
HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);        // LED 灭
st7789_init();                                                    // LCD 初始化
HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);          // LED 亮（系统就绪）
```

### USER CODE BEGIN 3（主循环：按键切换颜色）
```c
btn = HAL_GPIO_ReadPin(KEY_BTN_GPIO_Port, KEY_BTN_Pin);
if ((btn == GPIO_PIN_RESET) && (last == GPIO_PIN_SET))   // 下降沿检测
{
  idx = (idx + 1) % 3;
  st7789_fill_color(colors[idx]);                        // 红→绿→蓝
  HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);            // LED 闪烁
}
last = btn;
HAL_Delay(10);   // 软件消抖
```

---

## 6. 性能优化记录

| 阶段 | 变更 | 结果 |
|---|---|---|
| 初版 | SPI 10.625 MHz（/16）阻塞逐行 | ~116 ms/屏，感觉慢 |
| 优化 1 | 预分频 /8 → 21.25 MHz | ~58 ms/屏 |
| 优化 2 | 删除冗余 `SPI_WAIT_IDLE()`（HAL 内部已等 BSY） | 更干净 |
| 优化 3 | **DMA + 16 行块缓冲**，中断续传 | 调用 320→20 次，CPU 空闲 |
| 优化 4 | **预分频 /4 → 42.5 MHz** | **~29 ms/屏** |
| 优化 5 | PB3/PB5 GPIO Speed → **Very High** | 42.5 MHz 边沿更陡 |

> **关于"为什么要等 BSY"的结论**：`HAL_SPI_Transmit()` 返回前内部已在
> `SPI_EndRxTxTransaction()`（`stm32g4xx_hal_spi.c`）里等过 TX FIFO 空 + **BSY 复位**，
> 所以无需外部再轮询。真正要紧的是**抬 CS / 切 D/CX 前必须等传输完成**——这点 HAL 已保证。

---

## 7. 遇到的坑与解决方案

| 现象 | 根因 | 解决 |
|---|---|---|
| 屏幕全黑 | CubeMX 生成 `SPI_DATASIZE_4BIT`，4-bit 帧破坏字节边界 | 改 `SPI_DATASIZE_8BIT` |
| 屏幕全黑 | `LCD_BL` 初始化被拉低（背光灭） | `main.c` 里拉高 `LCD_BL` |
| 无法初始化 | 板上无 RESET 引脚（模组 RESET 上拉） | 用 `SWRESET` 命令 + 150ms 等待 |
| 填充不满 | 只写了 280 行，面板实际映射 GRAM 中某 280 行 | 整屏写入 320 行 |
| 按键无反应 | 需上升/下降沿检测 + 消抖 | `last` 沿检测 + 10ms `HAL_Delay` |

---

## 8. 使用说明

1. 用 STM32CubeIDE 打开 `stm32g431c8t6_prj`。
2. 项目文件在 `Core/Src/`、`Core/Inc/` 下，已在构建源中自动包含。
3. **Build → Run** 烧录调试。
4. 上电：屏幕黑，LED 常亮。
5. 按键：红 → 绿 → 蓝 循环，LED 每次闪烁。

---

## 9. 调试建议 / 常见问题

- **花屏 / 噪点**：先降速验证——把 `spi.c` 的 `BaudRatePrescaler` 从 `/4` 改回 `/8`（21.25 MHz）。若好了说明是 42.5 MHz 信号完整性问题（缩短连线 / 检查 FPC 焊接）。
- **颜色反色（负片）**：去掉初始化里的 `ST7789_INVON`，或改用 `INVOFF (0x20)`（IPS 面板极性个体差异）。
- **红蓝互换**：把 `st7789.h` 的 `ST7789_MADCTL_CFG` 改为 `0x08`（BGR）。
- **上下 / 左右颠倒**：`MADCTL_CFG` 加 `0x80`（行翻转）/ `0x40`（列翻转）。
- **按键偶尔误触发**：增大消抖延时（>10ms）。
- **LED 极性**：PC13 低电平点亮，如需反逻辑请自行调整。

---

## 10. 附录：ST7789 常用命令速查

| 命令 | 编码 | 说明 |
|---|---|---|
| SWRESET | 0x01 | 软件复位 |
| SLPOUT | 0x11 | 睡眠退出 |
| NORON | 0x13 | 正常显示模式 |
| INVON / INVOFF | 0x21 / 0x20 | 显示反转 开 / 关 |
| DISPON / DISPOFF | 0x29 / 0x28 | 显示 开 / 关 |
| CASET | 0x2A | 列地址窗口 |
| RASET | 0x2B | 行地址窗口 |
| RAMWR | 0x2C | 写显存 |
| MADCTL | 0x36 | 内存访问控制（方向/BGR） |
| COLMOD | 0x3A | 像素格式（串行 16bit=0x05） |

---

*文档维护：随代码同步更新。修改 SPI 时钟 / MADCTL / 初始化序列后请更新本文档。*
