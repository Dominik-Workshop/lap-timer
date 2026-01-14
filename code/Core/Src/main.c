/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "display.h"
#include "sprites.h"
#include <stdio.h> // For sprintf
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// STM32F411RE Sector 7 address (128KB sector)
#define FLASH_STORAGE_ADDR 0x08060000
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c3;

TIM_HandleTypeDef htim2;

/* USER CODE BEGIN PV */
volatile uint8_t timer_state = 0; // 0: IDLE, 1: RUNNING, 2: FINISHED
volatile uint32_t captured_time_ticks = 0;
volatile uint32_t last_trigger_sys_time = 0;

uint32_t best_time_ticks = 0xFFFFFFFF; // Default to "Empty"

#define TIME_DISPLAY_X 1
#define TIME_DISPLAY_Y 22

// DISPLAY RESOLUTION MODES
typedef enum {
    RES_MODE_MS,   // Minutes : Seconds : Milliseconds  (MM:SS:MMM)
    RES_MODE_US    // Seconds : Milliseconds : Microsec (SSS:MMM:UUU) (microsec resolution down to 10us ticks)
} display_res_mode_t;

volatile display_res_mode_t display_mode = RES_MODE_MS;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C3_Init(void);
static void MX_TIM2_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// --- INTERRUPT CALLBACK ---
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if(GPIO_Pin == GATE_TRIGGER_1_Pin || GPIO_Pin == GATE_TRIGGER_2_Pin || GPIO_Pin == BTN1_Pin)
    {
        uint32_t now = HAL_GetTick();
        if ((now - last_trigger_sys_time) > 1000)
        {
            last_trigger_sys_time = now;

            if(timer_state == 0 || timer_state == 2) // Start
            {
                __HAL_TIM_SET_COUNTER(&htim2, 0);
                timer_state = 1;
            }
            else if(timer_state == 1) // Stop
            {
                captured_time_ticks = __HAL_TIM_GET_COUNTER(&htim2);
                timer_state = 2;
            }
        }
    }
}

// --- Helper: Erase Sector 7 and Write a single 32-bit value ---
void Save_Best_Time(uint32_t ticks)
{
    HAL_FLASH_Unlock();

    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t SectorError;

    EraseInitStruct.TypeErase    = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.Sector       = FLASH_SECTOR_7; // Sector 7 (0x08060000)
    EraseInitStruct.NbSectors    = 1;
    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3; // 2.7V to 3.6V

    // Note: This might freeze the CPU for 1-2 seconds!
    if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) == HAL_OK)
    {
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, FLASH_STORAGE_ADDR, ticks);
    }

    HAL_FLASH_Lock();
}

/* Change the 4th parameter to display_font_t */
void display_formatted_time(int x, int y, uint32_t time_ms, display_font_t font) {
    char buf[20];

    uint32_t total_seconds = time_ms / 1000;
    uint32_t minutes = (total_seconds / 60) % 100;
    uint32_t seconds = total_seconds % 60;
    uint32_t milliseconds = time_ms % 1000;

    // Format string into a temporary buffer
    // MM:SS:MMM
    sprintf(buf, "%02lu:%02lu:%03lu", (unsigned long)minutes, (unsigned long)seconds, (unsigned long)milliseconds);

    // Pass the font directly to display_printf
    display_printf(x, y, DISPLAY_COLOR_WHITE, font, "%s", buf);
}

// Main Display function for the large timer (MM:SS:MMM)
void display_time_large(uint32_t time_ms){
    uint32_t ms = time_ms % 1000;
    uint32_t sec = (time_ms / 1000) % 60;
    uint32_t min = (time_ms / 60000) % 99;

    // Minutes
    display_printf(TIME_DISPLAY_X, TIME_DISPLAY_Y, DISPLAY_COLOR_WHITE, display_font_16x26, "%02lu", (unsigned long)min);
    display_bitmap(TIME_DISPLAY_X+(2*16), TIME_DISPLAY_Y+3, DISPLAY_COLOR_BLACK, bitmap_deliminator_8_16, 8, 16);

    // Seconds
    display_printf(TIME_DISPLAY_X+(2*16)+7, TIME_DISPLAY_Y, DISPLAY_COLOR_WHITE, display_font_16x26, "%02lu", (unsigned long)sec);
    display_bitmap(TIME_DISPLAY_X+(2*16)+7+(2*16), TIME_DISPLAY_Y+3, DISPLAY_COLOR_BLACK, bitmap_deliminator_8_16, 8, 16);

    // Milliseconds
    display_printf(TIME_DISPLAY_X+(2*16)+7+(2*16)+7, TIME_DISPLAY_Y, DISPLAY_COLOR_WHITE, display_font_16x26, "%03lu", (unsigned long)ms);

    display_line(0, TIME_DISPLAY_Y+37, 127, TIME_DISPLAY_Y+37, DISPLAY_COLOR_WHITE);
    display_printf(5, TIME_DISPLAY_Y+27, DISPLAY_COLOR_WHITE, display_font_7x10, "min | sec |  ms");}

// Show high-resolution time using the large font layout.
// Input: ticks = TIM2 counter (each tick = 10 us)
void display_time_highres_large_from_ticks(uint32_t ticks) {
    // Convert ticks -> total microseconds
    uint64_t total_us = (uint64_t)ticks * 10ULL;

    // Derive components
    uint32_t sec = (uint32_t)(total_us / 1000000ULL);         // seconds (may be >99)
    uint32_t ms  = (uint32_t)((total_us / 1000ULL) % 1000ULL); // milliseconds 0..999
    uint32_t micro10 = (uint32_t)((total_us % 1000ULL) / 10ULL); // 10us units 0..99

    // Cap seconds to two digits for display (if >99 you can show 99)
    if (sec > 99) sec = 99;

    // Draw using the same positions as display_time_large so it fits
    // Left: seconds (2 digits)
    display_printf(TIME_DISPLAY_X, TIME_DISPLAY_Y, DISPLAY_COLOR_WHITE, display_font_16x26, "%02lu", (unsigned long)sec);

    // colon/separator
    display_bitmap(TIME_DISPLAY_X+(2*16), TIME_DISPLAY_Y+3, DISPLAY_COLOR_BLACK, bitmap_deliminator_8_16, 8, 16);

    // Middle: milliseconds (3 digits)
    display_printf(TIME_DISPLAY_X+(2*16)+7, TIME_DISPLAY_Y, DISPLAY_COLOR_WHITE, display_font_16x26, "%03lu", (unsigned long)ms);

    // colon/separator
    display_bitmap(TIME_DISPLAY_X+(2*16)+7+(3*16), TIME_DISPLAY_Y+3, DISPLAY_COLOR_BLACK, bitmap_deliminator_8_16, 8, 16);

    // Right: microseconds in 10us units (2 digits)
    // We print %02lu so it uses two digits and fits into the rightmost digit space.
    display_printf(TIME_DISPLAY_X+(2*16)+7+(3*16)+7, TIME_DISPLAY_Y, DISPLAY_COLOR_WHITE, display_font_16x26, "%02lu", (unsigned long)micro10);

    display_line(0, TIME_DISPLAY_Y+37, 127, TIME_DISPLAY_Y+37, DISPLAY_COLOR_WHITE);
	display_printf(5, TIME_DISPLAY_Y+27, DISPLAY_COLOR_WHITE, display_font_7x10, "sec |  ms  |  us");
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  uint32_t btn3_press_start = 0;
  uint8_t btn3_action_done = 0;

  // BTN2 edge detection / debounce
  uint16_t prev_btn2_state = GPIO_PIN_SET; // assume not pressed (pullup)
  uint32_t btn2_press_time = 0;
  const uint32_t BTN2_DEBOUNCE_MS = 50; // debounce threshold
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
  MX_I2C3_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
  HAL_Delay(100);
  display_init();

  // --- Load Best Time from Flash ---
  best_time_ticks = *(__IO uint32_t *)FLASH_STORAGE_ADDR;

  // Sanity check: if flash was empty (0xFFFFFFFF), treat as "No Time"
  if(best_time_ticks == 0xFFFFFFFF) best_time_ticks = 0;

  display_fill(DISPLAY_COLOR_BLACK);
  display_bitmap(0, 0, DISPLAY_COLOR_BLACK, bitmap_lap_timer_logo_128_64, 128, 64);
  display_render();

  HAL_Delay(2000);
  display_fill(DISPLAY_COLOR_BLACK);
  display_render();

  HAL_TIM_Base_Start(&htim2);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
      // --- BTN3 Logic (Short Press = Reset Current, Long Press = Reset Best) ---
      if(HAL_GPIO_ReadPin(BTN3_GPIO_Port, BTN3_Pin) == GPIO_PIN_RESET) // Button is Pressed (active low)
      {
          if(btn3_press_start == 0) {
              btn3_press_start = HAL_GetTick(); // Start timer
          }

          // Check for Long Press while holding
          if((HAL_GetTick() - btn3_press_start > 2000) && !btn3_action_done)
          {
              best_time_ticks = 0; // Reset variable
              Save_Best_Time(0);   // Erase Flash

              // Visual Feedback
              display_fill(DISPLAY_COLOR_BLACK);
              display_printf(10, 30, DISPLAY_COLOR_WHITE, display_font_7x10, "RESET BEST!");
              display_render();
              HAL_Delay(1000);

              btn3_action_done = 1; // Prevent repeated triggers
          }
      }
      else // Button is Released
      {
          // If we were pressing the button...
          if(btn3_press_start != 0)
          {
              // Check if it was a Short Press (and not already handled as a long press)
              if(!btn3_action_done && (HAL_GetTick() - btn3_press_start < 2000))
              {
                  // --- SHORT PRESS ACTION: Reset Current Timer ---
                  timer_state = 0;        // Go back to IDLE
                  captured_time_ticks = 0; // Clear captured result
                  __HAL_TIM_SET_COUNTER(&htim2, 0); // Reset hardware timer
              }

              // Reset flags
              btn3_press_start = 0;
              btn3_action_done = 0;
          }
      }
      // ---------------------------------------

      // --- BTN2: toggle resolution on a short press (debounced) ---
      uint16_t curr_btn2 = HAL_GPIO_ReadPin(BTN2_GPIO_Port, BTN2_Pin);
      if(curr_btn2 == GPIO_PIN_RESET && prev_btn2_state == GPIO_PIN_SET) {
          // button pressed down
          btn2_press_time = HAL_GetTick();
      }
      else if(curr_btn2 == GPIO_PIN_SET && prev_btn2_state == GPIO_PIN_RESET) {
          // button released -> consider it a valid press if held > debounce
          if(btn2_press_time != 0 && (HAL_GetTick() - btn2_press_time) > BTN2_DEBOUNCE_MS) {
              // toggle display mode
              if(display_mode == RES_MODE_MS) display_mode = RES_MODE_US;
              else display_mode = RES_MODE_MS;

              display_fill(DISPLAY_COLOR_BLACK);
              // provide small feedback
//              if(display_mode == RES_MODE_MS) {
//                  display_printf(10, 30, DISPLAY_COLOR_WHITE, display_font_7x10, "MODE: MM:SS:MS");
//              } else {
//                  display_printf(10, 30, DISPLAY_COLOR_WHITE, display_font_7x10, "MODE: SS:MS:US");
//              }
//              display_render();
//              HAL_Delay(600);
          }
          btn2_press_time = 0;
      }
      prev_btn2_state = curr_btn2;
      // ---------------------------------------

      // --- Timer Display Logic ---
      uint32_t current_ticks = 0;

      if(timer_state == 1)
      {
          // Running
          current_ticks = __HAL_TIM_GET_COUNTER(&htim2);

          if(display_mode == RES_MODE_MS) {
              // convert ticks (10us) -> ms
              display_time_large(current_ticks / 100);
          } else {
              // show high-res mode directly from ticks
        	  display_time_highres_large_from_ticks(current_ticks);
          }
      }
      else if(timer_state == 2)
      {
          // Finished
          if(display_mode == RES_MODE_MS) {
              display_time_large(captured_time_ticks / 100);
          } else {
        	  display_time_highres_large_from_ticks(captured_time_ticks);
          }

          // Check for New Record (Logic runs once when state hits 2)
          static uint8_t saved_flag = 0;
          static uint32_t last_captured = 0;

          if(captured_time_ticks != last_captured) {
              saved_flag = 0; // New lap detected
              last_captured = captured_time_ticks;
          }

          if(saved_flag == 0)
          {
              // If valid time (>1 sec) AND (First run OR Better time)
              if(captured_time_ticks > 100000 && (best_time_ticks == 0 || captured_time_ticks < best_time_ticks))
              {
                  best_time_ticks = captured_time_ticks;

                  display_printf(0, 0, DISPLAY_COLOR_WHITE, display_font_6x8, "SAVING...     ");
                  display_render();

                  Save_Best_Time(best_time_ticks);
              }
              saved_flag = 1; // Mark as processed
          }
      }
      else
      {
          // Idle
          if(display_mode == RES_MODE_MS) {
              display_time_large(0);
          } else {
        	  display_time_highres_large_from_ticks(0);
          }
      }

      // --- Render Best Time (top-left) ---
      display_printf(0, 0, DISPLAY_COLOR_WHITE, display_font_6x8, "BEST:");
      if(best_time_ticks > 0 && best_time_ticks != 0xFFFFFFFF) {
          if(display_mode == RES_MODE_MS) {
              // best_time_ticks -> ms
              display_formatted_time(30, 0, best_time_ticks/100, display_font_6x8);
          } else {
              // show best time in highres: compute total_us and print SS:MMM:UUU with small font
              uint64_t total_us = (uint64_t)best_time_ticks * 10ULL;
              uint32_t us = total_us % 1000;
              uint32_t ms = (total_us / 1000) % 1000;
              uint32_t sec = (uint32_t)(total_us / 1000000ULL);
              display_printf(30, 0, DISPLAY_COLOR_WHITE, display_font_6x8, "%03lu:%03lu:%03lu",
                             (unsigned long)sec, (unsigned long)ms, (unsigned long)us);
          }
      } else {
          display_printf(30, 0, DISPLAY_COLOR_WHITE, display_font_6x8, "--:--:---");
      }

      display_render();

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 6;
  RCC_OscInitStruct.PLL.PLLN = 100;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C3_Init(void)
{

  /* USER CODE BEGIN I2C3_Init 0 */

  /* USER CODE END I2C3_Init 0 */

  /* USER CODE BEGIN I2C3_Init 1 */

  /* USER CODE END I2C3_Init 1 */
  hi2c3.Instance = I2C3;
  hi2c3.Init.ClockSpeed = 400000;
  hi2c3.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c3.Init.OwnAddress1 = 0;
  hi2c3.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c3.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c3.Init.OwnAddress2 = 0;
  hi2c3.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c3.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C3_Init 2 */

  /* USER CODE END I2C3_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 999;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 4294967295;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(USER_LED_GPIO_Port, USER_LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : GATE_TRIGGER_2_Pin GATE_TRIGGER_1_Pin */
  GPIO_InitStruct.Pin = GATE_TRIGGER_2_Pin|GATE_TRIGGER_1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : BTN3_Pin BTN2_Pin */
  GPIO_InitStruct.Pin = BTN3_Pin|BTN2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : BTN1_Pin */
  GPIO_InitStruct.Pin = BTN1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(BTN1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : USER_LED_Pin */
  GPIO_InitStruct.Pin = USER_LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(USER_LED_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI2_IRQn);

  HAL_NVIC_SetPriority(EXTI3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI3_IRQn);

  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

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

#ifdef  USE_FULL_ASSERT
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
