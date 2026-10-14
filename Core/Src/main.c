/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "st7789.h"
#include "g431_port.h"
#include "ui_raw_v2.h"
#include "gfx_raw.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_SPI3_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  /* SPI3 is configured in CubeMX (.ioc): DataSize=8BIT + /4 = 42.5 MHz,
   * both correct for the ST7789 4-line serial interface. Nothing to override
   * here — the LCD driver only needs the backlight + button/LED setup below. */

  /* Backlight ON (active-high load-switch on this board) — otherwise the
   * screen stays dark even with a perfect init sequence. */
  HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, GPIO_PIN_SET);

  /* LED off initially (PC13 is active-low: RESET turns it ON). */
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);

  /* Initialise the ST7789V2 (no HW reset available → SWRESET). */
  st7789_init();

  /* LED on (active-low) ⇒ system is up and screen initialised. */
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);

  /* Coulomb-counter UI demo runs in USER CODE 3 below (25Hz paced loop). */
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  /* Coulomb-counter UI demo @ 25Hz:
   * - static values: voltage / current / power / capacity are drawn ONCE by
   *   ui_raw_draw_static(&d) from the current variable values (per request);
   * - dynamic loop: soc 0->100% sawtooth (8s/round) with linked range =
   *   soc*0.7 km; charge/discharge pill fixed to "discharge" (current static);
   * - frame skip + element-level slots keep the refresh flicker-free;
   * - DWT frame timing on the bottom 40-row status band (y=240..279):
   *   left = frame time in ms, right = measured FPS (green = on target). */
  {
    ui_raw_data_t d = { 0 };
    static uint32_t nextFrame, fpsWin;
    static uint32_t fpsCnt = 0;
    float msShow = 0.0f, fpsShow = 0.0f;
    uint32_t cyc0;
    float soc_f = 0.0f;

    /* DWT cycle counter: ms = cycles / 170000 (SYSCLK = 170MHz). */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    /* Static values for the one-shot layer draw. */
    d.voltage = 72.5f;
    d.current = -12.3f;
    d.power_kw = 0.89f;
    d.capacity_ah = 19.6f;
    d.charging = 0;

    /* Static layer once: bg clear (incl. bottom band) + shell + labels
     * + static values above. Afterwards ui_raw_update only refreshes
     * SOC/range - no per-frame full-screen clear->redraw (that flickered). */
    ui_raw_init();                 /* clears the 240x240 UI area */
    GFX_FillRect(0, 240, 240, 40, GFX_RGB565(0x0B, 0x0E, 0x13));
    ui_raw_draw_static(&d);        /* frame + one-shot static values */
    nextFrame = fpsWin = HAL_GetTick();

    while (1)
    {
      uint32_t now = HAL_GetTick();

      /* 40ms cadence = 25Hz; on overrun drop missed slots and re-align. */
      if ((int32_t)(now - nextFrame) < 0) continue;
      nextFrame += 40;
      if ((int32_t)(now - nextFrame) >= 0) nextFrame = now + 40;

      /* ---- Dynamic simulation: soc + linked range only ---- */
      soc_f += 0.5f;                              /* 0->100% in 8s */
      if (soc_f >= 100.0f) soc_f = 0.0f;
      d.soc = soc_f;
      d.range_km = (int)(soc_f * 0.7f + 0.5f);    /* 0..70 km */

      cyc0 = DWT->CYCCNT;
      ui_raw_update(&d);

      /* Measured FPS: frames actually finished within a 1s window. */
      fpsCnt++;
      if ((int32_t)(now - fpsWin) >= 1000) {
        fpsShow = (float)fpsCnt * 1000.0f / (float)(now - fpsWin);
        fpsCnt = 0;
        fpsWin = now;
      }

      /* Status band shows the previous frame's numbers (green = on target). */
      ui_raw_draw_stats(msShow, fpsShow);
      msShow = (float)(DWT->CYCCNT - cyc0) / 170000.0f;
    }
  }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM17 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM17)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
