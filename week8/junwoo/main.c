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

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_C5  523

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

TIM_HandleTypeDef htim2;

/* USER CODE BEGIN PV */

uint32_t melody[] =
{
    NOTE_C4,
    NOTE_D4,
    NOTE_E4,
    NOTE_F4,
    NOTE_G4,
    NOTE_A4,
    NOTE_B4,
    NOTE_C5
};

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);

/* USER CODE BEGIN PFP */

void Buzzer_Play(uint32_t frequency);
void Buzzer_Stop(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void Buzzer_Play(uint32_t frequency)
{
    /*
     * TIM2CLK = 84 MHz
     * PSC = 83
     *
     * Counter Clock
     * = 84 MHz / (83 + 1)
     * = 1 MHz
     */

    uint32_t counter_clock = 1000000;

    /*
     * PWM frequency
     * = Counter Clock / (ARR + 1)
     *
     * 따라서
     * ARR = Counter Clock / Frequency - 1
     */
    uint32_t arr =
        (counter_clock / frequency) - 1;

    /* ARR 변경 */
    __HAL_TIM_SET_AUTORELOAD(
        &htim2,
        arr
    );

    /*
     * Duty Cycle = 50%
     *
     * CCR1 = (ARR + 1) / 2
     */
    __HAL_TIM_SET_COMPARE(
        &htim2,
        TIM_CHANNEL_1,
        (arr + 1) / 2
    );

    /* Counter 초기화 */
    __HAL_TIM_SET_COUNTER(
        &htim2,
        0
    );

    /* PWM 시작 */
    HAL_TIM_PWM_Start(
        &htim2,
        TIM_CHANNEL_1
    );
}

void Buzzer_Stop(void)
{
    HAL_TIM_PWM_Stop(
        &htim2,
        TIM_CHANNEL_1
    );
}

/* USER CODE END 0 */


/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
    /* MCU Configuration--------------------------------------------------------*/

    HAL_Init();

    /* Configure the system clock */
    SystemClock_Config();

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_TIM2_Init();

    /* Initialize leds */
    BSP_LED_Init(LED2);

    /* Initialize USER push-button */
    BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);

    /* Infinite loop */
    while (1)
    {
        for (int i = 0; i < 8; i++)
        {
            Buzzer_Play(melody[i]);

            /* 한 음을 500 ms 동안 출력 */
            HAL_Delay(500);

            Buzzer_Stop();

            /* 음 사이 간격 */
            HAL_Delay(100);
        }

        /* 도레미파솔라시도 끝난 후 1초 대기 */
        HAL_Delay(1000);
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

    /* Configure the main internal regulator output voltage */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

    /* Initializes the RCC Oscillators */
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

    /* Initializes CPU, AHB and APB bus clocks */
    RCC_ClkInitStruct.ClockType =
        RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2;

    RCC_ClkInitStruct.SYSCLKSource =
        RCC_SYSCLKSOURCE_PLLCLK;

    RCC_ClkInitStruct.AHBCLKDivider =
        RCC_SYSCLK_DIV1;

    RCC_ClkInitStruct.APB1CLKDivider =
        RCC_HCLK_DIV2;

    RCC_ClkInitStruct.APB2CLKDivider =
        RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(
            &RCC_ClkInitStruct,
            FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }
}


/**
  * @brief TIM2 Initialization Function
  * @retval None
  */
static void MX_TIM2_Init(void)
{
    TIM_ClockConfigTypeDef sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    TIM_OC_InitTypeDef sConfigOC = {0};

    htim2.Instance = TIM2;

    /*
     * TIM2CLK = 84 MHz
     *
     * 84 MHz / (83 + 1)
     * = 1 MHz
     */
    htim2.Init.Prescaler = 83;

    htim2.Init.CounterMode =
        TIM_COUNTERMODE_UP;

    /*
     * 초기 PWM 주기
     * 실제 음 출력 시 Buzzer_Play()에서 변경됨
     */
    htim2.Init.Period = 999;

    htim2.Init.ClockDivision =
        TIM_CLOCKDIVISION_DIV1;

    htim2.Init.AutoReloadPreload =
        TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
    {
        Error_Handler();
    }

    sClockSourceConfig.ClockSource =
        TIM_CLOCKSOURCE_INTERNAL;

    if (HAL_TIM_ConfigClockSource(
            &htim2,
            &sClockSourceConfig) != HAL_OK)
    {
        Error_Handler();
    }

    if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
    {
        Error_Handler();
    }

    sMasterConfig.MasterOutputTrigger =
        TIM_TRGO_RESET;

    sMasterConfig.MasterSlaveMode =
        TIM_MASTERSLAVEMODE_DISABLE;

    if (HAL_TIMEx_MasterConfigSynchronization(
            &htim2,
            &sMasterConfig) != HAL_OK)
    {
        Error_Handler();
    }

    /*
     * PWM Channel 1
     */
    sConfigOC.OCMode =
        TIM_OCMODE_PWM1;

    /*
     * 초기 Duty 50%
     * 실제 값은 Buzzer_Play()에서 다시 설정됨
     */
    sConfigOC.Pulse = 500;

    sConfigOC.OCPolarity =
        TIM_OCPOLARITY_HIGH;

    sConfigOC.OCFastMode =
        TIM_OCFAST_DISABLE;

    if (HAL_TIM_PWM_ConfigChannel(
            &htim2,
            &sConfigOC,
            TIM_CHANNEL_1) != HAL_OK)
    {
        Error_Handler();
    }

    /*
     * TIM2_CH1 GPIO 설정 호출
     */
    HAL_TIM_MspPostInit(&htim2);
}


/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /*
     * USART2 GPIO
     */
    GPIO_InitStruct.Pin =
        USART_TX_Pin | USART_RX_Pin;

    GPIO_InitStruct.Mode =
        GPIO_MODE_AF_PP;

    GPIO_InitStruct.Pull =
        GPIO_NOPULL;

    GPIO_InitStruct.Speed =
        GPIO_SPEED_FREQ_VERY_HIGH;

    GPIO_InitStruct.Alternate =
        GPIO_AF7_USART2;

    HAL_GPIO_Init(
        GPIOA,
        &GPIO_InitStruct
    );
}


/**
  * @brief TIM MSP Initialization
  */
void HAL_TIM_Base_MspInit(
    TIM_HandleTypeDef* htim_base)
{
    if (htim_base->Instance == TIM2)
    {
        /*
         * TIM2 Peripheral Clock Enable
         */
        __HAL_RCC_TIM2_CLK_ENABLE();
    }
}


/**
  * @brief TIM Post Initialization
  *
  * PA0 -> TIM2_CH1
  */
void HAL_TIM_MspPostInit(
    TIM_HandleTypeDef* htim)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (htim->Instance == TIM2)
    {
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /*
         * PA0
         * TIM2_CH1
         * Alternate Function 1
         */
        GPIO_InitStruct.Pin =
            GPIO_PIN_0;

        GPIO_InitStruct.Mode =
            GPIO_MODE_AF_PP;

        GPIO_InitStruct.Pull =
            GPIO_NOPULL;

        GPIO_InitStruct.Speed =
            GPIO_SPEED_FREQ_LOW;

        GPIO_InitStruct.Alternate =
            GPIO_AF1_TIM2;

        HAL_GPIO_Init(
            GPIOA,
            &GPIO_InitStruct
        );
    }
}


/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
    }
}


#ifdef USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  */
void assert_failed(uint8_t *file, uint32_t line)
{
}

#endif
