/**
 * g431_main_demo.c - CubeIDE main.c USER CODE 参考（裸机库仑计 UI 演示）
 *
 * 把下面两段分别贴到 stm32g431c8t6_prj/Core/Src/main.c 的
 *   USER CODE BEGIN Includes / USER CODE BEGIN 2 / USER CODE BEGIN 3
 * 并把 g431_port.h/g431_st7789.c + ui_raw_v2.* + gfx_raw.* + font_raw.h
 * 拷入工程 Core/Src|Inc 即可。flappy.c 不再需要。
 *
 * ---- Includes ----
 * #include "st7789.h"
 * #include "g431_port.h"
 * #include "ui_raw_v2.h"
 *
 * ---- USER CODE BEGIN 2 ----
 *   G431_BacklightOn();
 *   st7789_init();                      // SWRESET 初始化 + 黑屏
 *   HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);  // LED 常亮
 *   ui_raw_init();                      // 清 240x240 背景
 *   // 底部 40 行留白清成背景色
 *   GFX_FillRect(0, 240, 240, 40, GFX_RGB565(0x0B,0x0E,0x13));
 *
 * ---- USER CODE BEGIN 3 (while 1) ----
 *   {
 *     static int t = 0;
 *     ui_raw_data_t d = { 0 };
 *     d.soc = 76.0f; d.voltage = 72.5f + (t % 10) * 0.01f;
 *     d.current = -12.3f; d.power_kw = 0.89f;
 *     d.capacity_ah = -1.0f; d.range_km = 50; d.charging = (t / 20) % 2;
 *     ui_raw_update(&d);
 *     // 底部留白保持背景（ui 只画 0..239）
 *     t++;
 *     HAL_Delay(500);
 *   }
 */
