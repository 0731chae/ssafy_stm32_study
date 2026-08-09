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

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* 파란 LED — main 이 제어한다 */
#define LED_PORT      GPIOA
#define LED_PIN       5U     /* PA5 = Zio D13 */

/* 빨간 LED — 인터럽트(ISR)가 제어한다.
 * ★ 반드시 파란 LED와 "같은 포트"여야 한다.
 *   경쟁 조건은 GPIOA->ODR 이라는 하나의 레지스터를 공유할 때만 발생하기 때문. */
#define RED_PORT      GPIOA
#define RED_PIN       6U     /* PA6 = Zio D12 */

/* 빨간 소자의 트리거 극성.
 *   0 = High 에서 켜짐 (LED 직결, NPN 트랜지스터 구동)
 *   1 = Low  에서 켜짐 (PNP 방식 부저 모듈 등) */
#define RED_ACTIVE_LOW  0

#if (RED_ACTIVE_LOW)
  #define RED_ON_BSRR    (1U << (RED_PIN + 16))   /* Low  로 만들면 ON  */
  #define RED_OFF_BSRR   (1U << RED_PIN)          /* High 로 만들면 OFF */
  #define RED_IS_ON()    ((RED_PORT->ODR & (1U << RED_PIN)) == 0U)
#else
  #define RED_ON_BSRR    (1U << RED_PIN)          /* High 로 만들면 ON  */
  #define RED_OFF_BSRR   (1U << (RED_PIN + 16))   /* Low  로 만들면 OFF */
  #define RED_IS_ON()    ((RED_PORT->ODR & (1U << RED_PIN)) != 0U)
#endif

/* 실행할 데모 선택 — 값을 바꾸고 다시 빌드해서 비교한다.
 *   0 : 과제 제출용. 파란 LED 1초 blink (빨간 LED 불필요)
 *   3 : 배선 점검.   두 LED 동시 점멸
 *   4 : ★발표용 시나리오 — ODR  방식 → 4번째에 빨간 LED가 사라진다
 *   5 : ★발표용 시나리오 — BSRR 방식 → 4번째에 두 LED가 함께 켜진다
 *   1 / 2 : 위 4 / 5 의 최소 버전 (시퀀스 없이 매 사이클 반복)        */
#define DEMO_MODE     0

/* 4 / 5 번 시나리오에서 정상 구간을 몇 번 반복할지 */
#define SEQ_NORMAL_BLINKS   3U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* 디버거 Live Expressions 로 관찰할 통계값.
 * volatile : ISR이 바꾸는 값이므로 컴파일러가 캐싱하지 못하게 한다. */
volatile uint32_t g_isr_red_on_count = 0;  /* ISR이 "빨간 LED 켜라" 명령을 낸 횟수 */
volatile uint32_t g_red_lost_count   = 0;  /* main이 그 명령을 지워버린 횟수   */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* ---- 핀 하나를 푸시풀 출력으로 설정 (HAL_GPIO_Init 대체) ---- */
static void Pin_ConfigOutput(GPIO_TypeDef *port, uint32_t pin)
{
    /* [1] MODER : 2bit/pin — 01 = General purpose output */
    port->MODER   &= ~(3U << (pin * 2));   /* 해당 2비트 클리어 */
    port->MODER   |=  (1U << (pin * 2));   /* 01 세팅 */

    /* [2] OTYPER : 1bit/pin — 0 = Push-Pull */
    port->OTYPER  &= ~(1U << pin);

    /* [3] OSPEEDR : 2bit/pin — 00 = Low speed */
    port->OSPEEDR &= ~(3U << (pin * 2));

    /* [4] PUPDR : 2bit/pin — 00 = No pull-up/pull-down */
    port->PUPDR   &= ~(3U << (pin * 2));

    /* [5] 초기 상태 OFF (BSRR 상위 16비트 = Reset) */
    port->BSRR = (1U << (pin + 16));
}

/* ---- GPIO 초기화 ---- */
static void GPIO_Init_BareMetal(void)
{
    /* RCC_AHB1ENR bit0 = GPIOAEN : GPIOA에 클럭 공급.
     * 클럭이 없으면 아래 레지스터 write가 전부 무시된다. */
    RCC->AHB1ENR |= (1U << 0);
    (void)RCC->AHB1ENR;                    /* 클럭 안정화 대기 (더미 리드) */

    Pin_ConfigOutput(LED_PORT,    LED_PIN);      /* PA5 : LED   */
    Pin_ConfigOutput(RED_PORT, RED_PIN);   /* PA6 : 빨간 LED  */

    /* Active Low 구성이면 리셋 직후 Low(=ON) 상태이므로 즉시 꺼준다 */
    RED_PORT->BSRR = RED_OFF_BSRR;
}

/* ---- SysTick 기반 1ms 지연 (HAL_Delay 대체) ---- */
static void SysTick_Init_BareMetal(void)
{
    /* SysTick은 Cortex-M4 코어 내부 주변장치 (0xE000E010) */
    SysTick->LOAD = (SystemCoreClock / 1000U) - 1U;  /* 1ms마다 언더플로 */
    SysTick->VAL  = 0U;                               /* 카운터 클리어 */
    SysTick->CTRL = (1U << 2)    /* CLKSOURCE = 1 : 프로세서 클럭(AHB) 사용 */
                  | (1U << 0);   /* ENABLE    = 1 : 카운터 시작
                                  * TICKINT = 0 → 인터럽트 대신 폴링 방식 */
}

static void delay_ms_BareMetal(uint32_t ms)
{
    while (ms--)
    {
        /* CTRL bit16 = COUNTFLAG : 0까지 셌으면 1이 됨.
         * 이 레지스터를 읽는 순간 자동으로 0으로 클리어된다. */
        while ((SysTick->CTRL & (1U << 16)) == 0U)
        {
            /* 대기 */
        }
    }
}

/* ==========================================================================
 *  경쟁 조건(race condition) 실험
 *
 *  등장인물
 *    - main  : PA5(LED)를 켠다.
 *    - ISR   : PA6(빨간 LED)를 켠다.
 *  둘 다 GPIOA->ODR 이라는 "하나의 레지스터"를 건드린다는 것이 핵심이다.
 * ========================================================================== */

/* [인터럽트 측] 빨간 LED를 켜라는 명령.
 * PendSV 예외 핸들러(stm32f4xx_it.c)에서 호출된다. */
void Red_ISR_TurnOn(void)
{
    /* 어느 쪽이든 read-modify-write 라는 점은 동일하다 */
#if (RED_ACTIVE_LOW)
    RED_PORT->ODR &= ~(1U << RED_PIN);   /* Low  로 → 켜짐 */
#else
    RED_PORT->ODR |=  (1U << RED_PIN);   /* High 로 → 켜짐 */
#endif
    g_isr_red_on_count++;
}

/* 실제로는 수만 번에 한 번 우연히 터지는 타이밍이라 재현이 어렵다.
 * 그래서 "하필 그 순간"을 소프트웨어로 강제 발생시킨다.
 * PendSV 는 NVIC 없이 SCB->ICSR 비트 하나로 즉시 띄울 수 있는 예외다. */
static void Trigger_Interrupt_Now(void)
{
    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;   /* PendSV 를 pending 상태로 */
    __DSB();                               /* 쓰기 완료 보장 */
    __ISB();                               /* 다음 명령 전에 예외 진입 보장 */
}

/* ---------- 방식 A : ODR 직접 조작 (버그) ---------- */
static void LED_On_ODR_Racy(void)
{
    uint32_t tmp;

    /* ① 읽기 (LDR) — 이 순간 ODR = 0x0000, 빨간 LED 비트도 0 */
    tmp = LED_PORT->ODR;

    /* ★ 여기서 인터럽트 발생.
     *   ISR 이 ODR 의 6번 비트를 1로 만들고 돌아온다 (ODR = 0x0040).
     *   하지만 tmp 는 여전히 0x0000 — 낡은 사진을 손에 쥔 상태.        */
    Trigger_Interrupt_Now();

    /* ② 수정 (ORR) — 낡은 값 기준으로 계산 */
    tmp |= (1U << LED_PIN);                /* tmp = 0x0020 */

    /* ③ 쓰기 (STR) — 32비트 전체를 덮어쓴다.
     *   ISR 이 켜둔 6번 비트가 여기서 증발한다. ODR = 0x0020            */
    LED_PORT->ODR = tmp;

    if (!RED_IS_ON())
    {
        g_red_lost_count++;             /* 명령이 유실됨 */
    }
}

/* ---------- 방식 B : BSRR 사용 (정상) ---------- */
static void LED_On_BSRR_Safe(void)
{
    /* 같은 타이밍에 인터럽트를 발생시킨다 — 조건은 완전히 동일 */
    Trigger_Interrupt_Now();

    /* 단일 STR 명령. ODR 을 읽지 않으므로 낡은 값이 존재할 수 없고,
     * 1을 쓴 비트만 하드웨어가 반응하므로 6번 비트는 건드려지지 않는다. */
    LED_PORT->BSRR = (1U << LED_PIN);

    if (!RED_IS_ON())
    {
        g_red_lost_count++;             /* 여기는 절대 증가하지 않는다 */
    }
}

/* ==========================================================================
 *  발표용 시나리오
 *
 *   1~3회차 : 파랑 1초 → 빨강 1초 를 번갈아 반복 (평범하게 잘 돌아가는 구간)
 *   4회차   : 파랑을 켜기 "직전"에 빨강 인터럽트가 발생
 *             - use_bsrr == 0 → 파랑만 켜진다  (빨강 명령이 증발)
 *             - use_bsrr == 1 → 둘 다 켜진다  (정상)
 * ========================================================================== */
static void Run_Demo_Sequence(uint32_t use_bsrr)
{
    uint32_t i;

    /* ---------- 정상 구간 ---------- */
    for (i = 0U; i < SEQ_NORMAL_BLINKS; i++)
    {
        LED_PORT->BSRR = (1U << LED_PIN);           /* 파랑 ON  */
        delay_ms_BareMetal(1000);
        LED_PORT->BSRR = (1U << (LED_PIN + 16));    /* 파랑 OFF */

        RED_PORT->BSRR = RED_ON_BSRR;               /* 빨강 ON  */
        delay_ms_BareMetal(1000);
        RED_PORT->BSRR = RED_OFF_BSRR;              /* 빨강 OFF */
    }

    /* ---------- 4회차 : 여기서 사고가 난다 ---------- */
    if (use_bsrr)
    {
        LED_On_BSRR_Safe();     /* 인터럽트 발생 → BSRR 로 파랑 ON */
    }
    else
    {
        LED_On_ODR_Racy();      /* 인터럽트 발생 → ODR 로 파랑 ON  */
    }

    delay_ms_BareMetal(3000);   /* 결과를 눈으로 확인할 시간 */

    /* ---------- 정리 후 다음 사이클 ---------- */
    LED_PORT->BSRR = (1U << (LED_PIN + 16));
    RED_PORT->BSRR = RED_OFF_BSRR;
    delay_ms_BareMetal(2000);   /* 사이클 구분용 암전 */
}
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
  /* USER CODE BEGIN 2 */
  GPIO_Init_BareMetal();
  SysTick_Init_BareMetal();
  /* USER CODE END 2 */

  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
#if (DEMO_MODE == 0)
    /* ---------- 과제 제출용 : LED 1초 blink ---------- */
    LED_PORT->BSRR = (1U << LED_PIN);          /* 하위 16bit → SET   : LED ON  */
    delay_ms_BareMetal(1000);

    LED_PORT->BSRR = (1U << (LED_PIN + 16));   /* 상위 16bit → RESET : LED OFF */
    delay_ms_BareMetal(1000);

#elif (DEMO_MODE == 4)
    Run_Demo_Sequence(0U);   /* ODR  방식 : 4회차에 빨강이 사라진다 */

#elif (DEMO_MODE == 5)
    Run_Demo_Sequence(1U);   /* BSRR 방식 : 4회차에 둘 다 켜진다   */

#elif (DEMO_MODE == 3)
    /* ---------- 빨간 LED 배선 점검 ----------
     * PA6 를 1초 High / 1초 Low 로 번갈아 만든다. LED(PA5)는 High 구간 표시용.
     *
     *   파랑 켜져 있을 때 빨강도 켜짐  → Active High  (RED_ACTIVE_LOW 를 0 으로)
     *   파랑 꺼져 있을 때 빨강이 켜짐  → Active Low   (RED_ACTIVE_LOW 를 1 로)
     *   계속 켜짐 / 전혀 안 켜짐 → 배선 또는 빨간 LED 종류 문제                */

    LED_PORT->BSRR    = (1U << LED_PIN);            /* LED   ON   */
    RED_PORT->BSRR = (1U << RED_PIN);         /* PA6 = High */
    delay_ms_BareMetal(1000);

    LED_PORT->BSRR    = (1U << (LED_PIN + 16));     /* LED   OFF  */
    RED_PORT->BSRR = (1U << (RED_PIN + 16));  /* PA6 = Low  */
    delay_ms_BareMetal(1000);

#else
    /* ---------- 경쟁 조건 실험 ----------
     * 기대 동작 : LED가 켜지는 순간 ISR이 빨간 LED도 켜므로, 1초간 둘 다 ON.
     * 실제 결과 : MODE 1 이면 빨간 LED가 울리지 않는다.                        */

    /* 매 사이클 동일한 조건에서 시작하도록 빨간 LED를 확실히 꺼둔다 */
    RED_PORT->BSRR = RED_OFF_BSRR;

  #if (DEMO_MODE == 1)
    LED_On_ODR_Racy();     /* 빨간 LED ON 명령이 증발한다 → LED만 켜짐 */
  #else
    LED_On_BSRR_Safe();    /* 빨간 LED ON 명령이 살아남는다 → 둘 다 켜짐 */
  #endif

    delay_ms_BareMetal(1000);

    /* 둘 다 끄고 1초 대기 */
    LED_PORT->BSRR    = (1U << (LED_PIN + 16));
    RED_PORT->BSRR = RED_OFF_BSRR;
    delay_ms_BareMetal(1000);
#endif
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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 50;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
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
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();

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
