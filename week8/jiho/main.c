#include "stm32f4xx_hal.h"

TIM_HandleTypeDef htim3;

#define TIMER_COUNTER_FREQ 1000000U

#define NOTE_C4 262U
#define NOTE_D4 294U
#define NOTE_E4 330U
#define NOTE_F4 349U
#define NOTE_G4 392U
#define NOTE_A4 440U
#define NOTE_B4 494U
#define NOTE_C5 523U

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM3_Init(void);

void Buzzer_SetFrequency(uint32_t freq);
void Buzzer_Stop(void);
void Buzzer_PlayScale(void);
void Error_Handler(void);

int main(void)
{
    HAL_Init();

    SystemClock_Config();

    MX_GPIO_Init();
    MX_TIM3_Init();

    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);

    Buzzer_Stop();

    while (1)
    {
        Buzzer_PlayScale();

        HAL_Delay(2000);
    }
}

void Buzzer_SetFrequency(uint32_t freq)
{
    uint32_t arr;

    if (freq == 0U)
    {
        Buzzer_Stop();
        return;
    }

    /*
     * PWM Frequency
     *
     * fPWM = TIMER_COUNTER_FREQ / (ARR + 1)
     *
     * 따라서
     *
     * ARR = TIMER_COUNTER_FREQ / fPWM - 1
     */

    arr = ((TIMER_COUNTER_FREQ + (freq / 2U)) / freq) - 1U;

    /*
     * ARR 변경
     *
     * ARR이 PWM 주기를 결정하므로
     * 여기서 음 높이가 결정된다.
     */
    __HAL_TIM_SET_AUTORELOAD(&htim3, arr);

    /*
     * CCR = ARR의 약 절반
     *
     * Duty Cycle ≈ 50%
     */
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, (arr + 1U) / 2U);

    /*
     * 새로운 주파수가 바로 적용되도록
     * Counter를 0부터 다시 시작
     */
    __HAL_TIM_SET_COUNTER(&htim3, 0U);
}

void Buzzer_Stop(void)
{
    /*
     * MH-FMD가 Low Active라고 가정
     *
     * PWM Polarity를 LOW로 설정했으므로
     * CCR = 0이면 Active 구간이 없어져 부저가 꺼진다.
     */
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0U);
}

void Buzzer_PlayScale(void)
{
    const uint32_t melody[] =
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

    const uint32_t melodySize = sizeof(melody) / sizeof(melody[0]);

    for (uint32_t i = 0; i < melodySize; i++)
    {
        Buzzer_SetFrequency(melody[i]);

        HAL_Delay(500);

        Buzzer_Stop();

        HAL_Delay(100);
    }
}

static void MX_TIM3_Init(void)
{
    TIM_OC_InitTypeDef sConfigOC = {0};

    uint32_t pclk1;
    uint32_t tim3Clock;
    uint32_t prescaler;

    /*
     * APB1 Clock 확인
     */
    pclk1 = HAL_RCC_GetPCLK1Freq();

    /*
     * STM32F4에서는 APB Prescaler가 1이 아니면
     * Timer Clock = PCLK × 2
     */
    if ((RCC->CFGR & RCC_CFGR_PPRE1) == 0U)
    {
        tim3Clock = pclk1;
    }
    else
    {
        tim3Clock = pclk1 * 2U;
    }

    /*
     * TIM3 Counter Clock을 1 MHz로 설정
     *
     * fCNT = fTIM / (PSC + 1)
     *
     * 따라서
     *
     * PSC = fTIM / 1MHz - 1
     */
    prescaler = (tim3Clock / TIMER_COUNTER_FREQ) - 1U;

    htim3.Instance = TIM3;
    htim3.Init.Prescaler = prescaler;
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = 999U;
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
    {
        Error_Handler();
    }

    /*
     * PWM Mode 1
     *
     * MH-FMD 모듈이 Low Level Trigger이므로
     * Active Low로 설정
     */
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 0U;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_LOW;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

    if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
    {
        Error_Handler();
    }
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();

    /*
     * PB4
     *
     * NUCLEO-F446RE Arduino D5
     * TIM3_CH1
     */
    GPIO_InitStruct.Pin = GPIO_PIN_4;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;

    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /*
     * 이해하기 쉽게 HSI 16 MHz를 그대로 사용
     *
     * SYSCLK = 16 MHz
     * HCLK   = 16 MHz
     * PCLK1  = 16 MHz
     * TIM3   = 16 MHz
     *
     * 따라서
     *
     * PSC = 15
     *
     * 16 MHz / (15 + 1)
     * = 1 MHz
     */

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK |
                                  RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 |
                                  RCC_CLOCKTYPE_PCLK2;

    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;

    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
    {
        Error_Handler();
    }
}

void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
    }
}