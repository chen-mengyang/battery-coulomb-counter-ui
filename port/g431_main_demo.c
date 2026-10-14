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
 *   ui_raw_draw_static(&d);             // 框架 + 电压/电流/功率/容量静态值
 *   // 之后 ui_raw_update：内容没变整帧跳过（零像素）；变了只做元素级
 *   // 槽位擦+画（单元素空白 <1ms）→ 25Hz 无闪烁
 *   // 注意：newlib-nano 缺省禁用 %f（输出空串），UI 数值一律走
 *   // ui_raw_draw.c 的 fmt_fx 定点格式化，勿改回 snprintf("%.2f")。
 *   // 240x280 面板：port 层 G431_YOFF=60 让 UI 垂直居中，无需额外处理。
 *
 * ---- USER CODE BEGIN 3 (while 1) ----
 *   // 25Hz 节拍：40ms 一个 tick（跳过丢帧/重对齐），循环体调
 *   // ui_raw_update(&d) 即可；完整实现直接抄本仓
 *   // Core/Src/main.c 的 USER CODE 3。最简版本：
 *   {
 *     static int t = 0;
 *     ui_raw_data_t d = { 0 };
 *     d.soc = 76.0f; d.voltage = 72.5f + (t % 10) * 0.01f;
 *     d.current = -12.3f; d.power_kw = 0.89f;
 *     d.capacity_ah = -1.0f; d.range_km = 50; d.charging = (t / 20) % 2;
 *     ui_raw_update(&d);
 *     t++;
 *     HAL_Delay(40);   // 25Hz；正式应用请按 HAL_GetTick 对齐节拍
 *   }
 */
