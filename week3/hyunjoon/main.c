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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* HC-SR04 */
#define HCSR04_TRIG_PORT    GPIOC
#define HCSR04_TRIG_PIN     GPIO_PIN_2

#define HCSR04_ECHO_PORT    GPIOC
#define HCSR04_ECHO_PIN     GPIO_PIN_3

/* NUCLEO-F446RE User LED */
#define LED_PORT             GPIOA
#define LED_PIN              GPIO_PIN_5
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
static void MX_GPIO_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* =========================================================
 * DWT 초기화
 * ========================================================= */
static void DWT_Init(void)
{
    /* DWT와 Trace 기능 활성화 */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    /* Cycle Counter 초기화 */
    DWT->CYCCNT = 0;

    /* Cycle Counter 활성화 */
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}


/* =========================================================
 * DWT 기반 us Delay
 * ========================================================= */
static void DWT_Delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;

    uint32_t cycles = us * (SystemCoreClock / 1000000U);

    while ((DWT->CYCCNT - start) < cycles)
    {
        ;
    }
}


/* =========================================================
 * HC-SR04 거리 측정
 *
 * return:
 *   >= 0 : 거리(cm)
 *   -1   : Echo Timeout
 * ========================================================= */
static float HCSR04_ReadDistance(void)
{
    uint32_t start_cycle;
    uint32_t end_cycle;
    uint32_t timeout_start;

    uint32_t echo_cycles;
    float echo_us;
    float distance_cm;


    /* -----------------------------------------
     * 1. TRIG LOW
     * ----------------------------------------- */
    HAL_GPIO_WritePin(
        HCSR04_TRIG_PORT,
        HCSR04_TRIG_PIN,
        GPIO_PIN_RESET
    );

    DWT_Delay_us(2);


    /* -----------------------------------------
     * 2. TRIG HIGH 10us
     * ----------------------------------------- */
    HAL_GPIO_WritePin(
        HCSR04_TRIG_PORT,
        HCSR04_TRIG_PIN,
        GPIO_PIN_SET
    );

    DWT_Delay_us(10);

    HAL_GPIO_WritePin(
        HCSR04_TRIG_PORT,
        HCSR04_TRIG_PIN,
        GPIO_PIN_RESET
    );


    /* -----------------------------------------
     * 3. ECHO HIGH 기다림
     *
     * 약 30ms timeout
     * ----------------------------------------- */
    timeout_start = DWT->CYCCNT;

    while (HAL_GPIO_ReadPin(
               HCSR04_ECHO_PORT,
               HCSR04_ECHO_PIN
           ) == GPIO_PIN_RESET)
    {
        if ((DWT->CYCCNT - timeout_start)
                > (30U * (SystemCoreClock / 1000U)))
        {
            return -1.0f;
        }
    }


    /* -----------------------------------------
     * 4. ECHO HIGH 시작 시간
     * ----------------------------------------- */
    start_cycle = DWT->CYCCNT;


    /* -----------------------------------------
     * 5. ECHO LOW 기다림
     *
     * 최대 약 30ms
     * ----------------------------------------- */
    while (HAL_GPIO_ReadPin(
               HCSR04_ECHO_PORT,
               HCSR04_ECHO_PIN
           ) == GPIO_PIN_SET)
    {
        if ((DWT->CYCCNT - start_cycle)
                > (30U * (SystemCoreClock / 1000U)))
        {
            return -1.0f;
        }
    }


    /* -----------------------------------------
     * 6. ECHO HIGH 시간 계산
     * ----------------------------------------- */
    end_cycle = DWT->CYCCNT;

    echo_cycles = end_cycle - start_cycle;


    /* cycle -> us */
    echo_us = (float)echo_cycles
              / ((float)SystemCoreClock / 1000000.0f);


    /* -----------------------------------------
     * 7. 거리 계산
     *
     * 거리 = 시간 * 음속 / 2
     *
     * 음속 ≈ 343m/s
     *
     * cm 기준:
     * distance = echo_us / 58
     * ----------------------------------------- */
    distance_cm = echo_us / 58.0f;


    return distance_cm;
}/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
    HAL_Init();

    SystemClock_Config();

    MX_GPIO_Init();


    /* DWT 초기화 */
    DWT_Init();


    /* LED OFF */
    HAL_GPIO_WritePin(
        LED_PORT,
        LED_PIN,
        GPIO_PIN_RESET
    );


    while (1)
    {
        float distance;

        distance = HCSR04_ReadDistance();


        if (distance > 0.0f)
        {
            /* --------------------------------
             * 정상 측정
             *
             * 20cm 이하이면 LED ON
             * -------------------------------- */
            if (distance <= 20.0f)
            {
                HAL_GPIO_WritePin(
                    LED_PORT,
                    LED_PIN,
                    GPIO_PIN_SET
                );
            }
            else
            {
                HAL_GPIO_WritePin(
                    LED_PORT,
                    LED_PIN,
                    GPIO_PIN_RESET
                );
            }
        }
        else
        {
            /* 측정 실패 */
            HAL_GPIO_WritePin(
                LED_PORT,
                LED_PIN,
                GPIO_PIN_RESET
            );
        }


        /* HC-SR04 측정 간격 */
        HAL_Delay(60);
    }
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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
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

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
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
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC2 */
  GPIO_InitStruct.Pin = GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PC3 */
  GPIO_InitStruct.Pin = GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : USART_TX_Pin USART_RX_Pin */
  GPIO_InitStruct.Pin = USART_TX_Pin|USART_RX_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* Configure GPIO pin : PA5 */
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* LED OFF */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
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
