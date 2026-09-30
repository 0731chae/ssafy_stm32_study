#include "main.h"

void SystemClock_Config(void);
static void Buzzer_Init(void);
void Error_Handler(void);

int main(void)
{
    uint32_t notes[] = {262, 294, 330, 349, 392, 440, 494, 523};

    HAL_Init();
    SystemClock_Config();
    Buzzer_Init();

    while (1)
    {
        for (int i = 0; i < 8; i++)
        {
            uint32_t period = 1000000 / notes[i];

            TIM3->ARR = period - 1;
            TIM3->CCR3 = period / 2;
            TIM3->EGR = TIM_EGR_UG;
            HAL_Delay(400);

            TIM3->CCR3 = 0;
            TIM3->EGR = TIM_EGR_UG;
            HAL_Delay(100);
        }

        HAL_Delay(1000);
    }
}

static void Buzzer_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_TIM3_FORCE_RESET();
    __HAL_RCC_TIM3_RELEASE_RESET();

    gpio.Pin = GPIO_PIN_8;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = GPIO_AF2_TIM3;
    HAL_GPIO_Init(GPIOC, &gpio);

    TIM3->PSC = 83;
    TIM3->ARR = 999;
    TIM3->CCR3 = 0;
    TIM3->CCMR2 = TIM_CCMR2_OC3M_1
                | TIM_CCMR2_OC3M_2
                | TIM_CCMR2_OC3PE;
    TIM3->CCER = TIM_CCER_CC3E;
    TIM3->EGR = TIM_EGR_UG;
    TIM3->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    osc.PLL.PLLM = 16;
    osc.PLL.PLLN = 336;
    osc.PLL.PLLP = RCC_PLLP_DIV4;
    osc.PLL.PLLQ = 2;
    osc.PLL.PLLR = 2;

    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
    {
        Error_Handler();
    }

    clk.ClockType = RCC_CLOCKTYPE_SYSCLK
                  | RCC_CLOCKTYPE_HCLK
                  | RCC_CLOCKTYPE_PCLK1
                  | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK)
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

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
}
#endif
