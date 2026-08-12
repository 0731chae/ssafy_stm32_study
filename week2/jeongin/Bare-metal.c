#include <stdint.h>

/*
 * 32비트 메모리 주소를 하드웨어 레지스터처럼 접근하기 위한 매크로
 *
 * volatile:
 * 해당 주소의 값은 하드웨어에 의해 변할 수 있으므로
 * 컴파일러가 읽기/쓰기를 생략하지 못하게 한다.
 */
#define REG32(address) (*(volatile uint32_t *)(address))

/* RCC 레지스터 */
#define RCC_CR          REG32(0x40023800U)
#define RCC_CFGR        REG32(0x40023808U)
#define RCC_AHB1ENR     REG32(0x40023830U)

/* GPIOA 레지스터 */
#define GPIOA_MODER     REG32(0x40020000U)
#define GPIOA_OTYPER    REG32(0x40020004U)
#define GPIOA_OSPEEDR   REG32(0x40020008U)
#define GPIOA_PUPDR     REG32(0x4002000CU)
#define GPIOA_BSRR      REG32(0x40020018U)

/* Cortex-M4 SysTick 레지스터 */
#define SYST_CSR        REG32(0xE000E010U)
#define SYST_RVR        REG32(0xE000E014U)
#define SYST_CVR        REG32(0xE000E018U)

/* GPIO 핀 번호 */
#define LED_PIN         5U

/*
 * 시스템 클럭을 내부 HSI 16MHz로 설정한다.
 *
 * 리셋 직후에도 HSI가 기본 클럭이지만,
 * 실행 환경과 관계없이 16MHz를 사용하도록 명시적으로 설정한다.
 */
static void Clock_Init_16MHz(void)
{
    /* RCC_CR의 HSION(bit 0)을 1로 설정 */
    RCC_CR |= (1U << 0);

    /* HSIRDY(bit 1)가 1이 될 때까지 대기 */
    while ((RCC_CR & (1U << 1)) == 0U)
    {
    }

    /*
     * RCC_CFGR 설정
     * SW[1:0]   = 00: 시스템 클럭으로 HSI 선택
     * HPRE[3:0] = 0000: AHB 분주하지 않음
     */
    RCC_CFGR &= ~((3U << 0) | (0xFU << 4));

    /* SWS[1:0]가 00이 될 때까지 대기 */
    while ((RCC_CFGR & (3U << 2)) != 0U)
    {
    }
}

/*
 * PA5를 Push-Pull 출력 핀으로 설정한다.
 * NUCLEO-F446RE의 내장 LD2와 D13은 PA5에 연결되어 있다.
 */
static void GPIOA_PA5_Init(void)
{
    /*
     * RCC_AHB1ENR의 GPIOAEN(bit 0)을 1로 설정
     * GPIOA에 클럭 공급
     */
    RCC_AHB1ENR |= (1U << 0);

    /* 클럭 활성화 적용을 위한 더미 읽기 */
    (void)RCC_AHB1ENR;

    /*
     * GPIOA_MODER의 PA5 영역은 bit 11:10
     * 00: 입력
     * 01: 일반 출력
     * 10: Alternate Function
     * 11: Analog
     */

    /* PA5의 기존 MODER 값 제거 */
    GPIOA_MODER &= ~(3U << (LED_PIN * 2U));

    /* PA5를 일반 출력 모드 01로 설정 */
    GPIOA_MODER |=  (1U << (LED_PIN * 2U));

    /* PA5를 Push-Pull 출력으로 설정: OT5 = 0 */
    GPIOA_OTYPER &= ~(1U << LED_PIN);

    /* PA5 출력 속도를 Low로 설정: OSPEED5 = 00 */
    GPIOA_OSPEEDR &= ~(3U << (LED_PIN * 2U));

    /* PA5 Pull-up/Pull-down 사용 안 함: PUPD5 = 00 */
    GPIOA_PUPDR &= ~(3U << (LED_PIN * 2U));

    /*
     * BSRR의 bit 21에 1을 쓰면 PA5가 LOW가 된다.
     * LED 초기 상태를 OFF로 설정한다.
     */
    GPIOA_BSRR = (1U << (LED_PIN + 16U));
}

/*
 * SysTick을 사용한 약 1초 지연
 *
 * 시스템 클럭 16MHz:
 * 16,000,000번 카운트하면 약 1초
 */
static void Delay_1Second(void)
{
    /* SysTick 정지 */
    SYST_CSR = 0U;

    /*
     * SysTick은 0까지 포함하여 세므로
     * 16,000,000 - 1을 Reload 값으로 설정한다.
     */
    SYST_RVR = 16000000U - 1U;

    /* 현재 카운터 및 COUNTFLAG 초기화 */
    SYST_CVR = 0U;

    /*
     * CSR bit 2 CLKSOURCE = 1: 프로세서 클럭 사용
     * CSR bit 1 TICKINT   = 0: 인터럽트 사용 안 함
     * CSR bit 0 ENABLE    = 1: SysTick 시작
     */
    SYST_CSR = (1U << 2) | (1U << 0);

    /* COUNTFLAG(bit 16)가 1이 될 때까지 기다린다. */
    while ((SYST_CSR & (1U << 16)) == 0U)
    {
    }

    /* SysTick 정지 */
    SYST_CSR = 0U;
}

int main(void)
{
    /* 시스템 클럭을 HSI 16MHz로 설정 */
    Clock_Init_16MHz();

    /* PA5를 GPIO 출력으로 설정 */
    GPIOA_PA5_Init();

    while (1)
    {
        /*
         * BSRR bit 5에 1 쓰기
         * PA5 HIGH → LED 켜기
         */
        GPIOA_BSRR = (1U << LED_PIN);
        Delay_1Second();

        /*
         * BSRR bit 21에 1 쓰기
         * PA5 LOW → LED 끄기
         */
        GPIOA_BSRR = (1U << (LED_PIN + 16U));
        Delay_1Second();
    }
}
