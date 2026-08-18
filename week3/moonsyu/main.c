#include "stm32f446xx.h"
#include <stdint.h>

int main(void)
{
    volatile uint32_t echo_time = 0U;
    volatile uint32_t distance = 0U;
    uint8_t status = 0U;

    /*
     * 시스템 클럭을 HSI 16MHz로 설정
     */
    RCC->CR |= RCC_CR_HSION;

    while ((RCC->CR & RCC_CR_HSIRDY) == 0U)
    {
    }

    /* 시스템 클럭을 HSI로 전환 */
    RCC->CFGR &= ~RCC_CFGR_SW;

    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI)
    {
    }

    /* AHB, APB1, APB2 Prescaler = 1 */
    RCC->CFGR &= ~(RCC_CFGR_HPRE |
        RCC_CFGR_PPRE1 |
        RCC_CFGR_PPRE2);

    /*
     * GPIOA, GPIOB 클럭 활성화
     */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;

    /*
     * TIM2 클럭 활성화
     * TIM2는 APB1 버스
     */
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

    /* 클럭 활성화 완료 대기 */
    (void)RCC->AHB1ENR;
    (void)RCC->APB1ENR;

    /*
     * PB10: 초음파 TRIG 출력
     * MODER10 = 01
     */
    GPIOB->MODER &= ~(3U << (10U * 2U));
    GPIOB->MODER |= (1U << (10U * 2U));

    /*
     * PA8: 초음파 ECHO 입력
     * MODER8 = 00
     */
    GPIOA->MODER &= ~(3U << (8U * 2U));

    /* PA8 Pull-down */
    GPIOA->PUPDR &= ~(3U << (8U * 2U));
    GPIOA->PUPDR |= (2U << (8U * 2U));

    /*
     * PB1: LED 출력
     * MODER1 = 01
     */
    GPIOB->MODER &= ~(3U << (1U * 2U));
    GPIOB->MODER |= (1U << (1U * 2U));

    /* PB10, PB1 Push-Pull */
    GPIOB->OTYPER &= ~(1U << 10U);
    GPIOB->OTYPER &= ~(1U << 1U);

    /* 출력 속도 Low Speed */
    GPIOB->OSPEEDR &= ~(3U << (10U * 2U));
    GPIOB->OSPEEDR &= ~(3U << (1U * 2U));

    /* TRIG LOW */
    GPIOB->BSRR = (1U << (10U + 16U));

    /* LED OFF: Active-High LED 기준 */
    GPIOB->BSRR = (1U << (1U + 16U));

    /*
     * TIM2 설정
     *
     * 타이머 클럭: 16MHz
     * 16MHz / (15 + 1) = 1MHz
     * 1카운트 = 1us
     */
    TIM2->CR1 = 0U;
    TIM2->PSC = 15U;
    TIM2->ARR = 0xFFFFFFFFU;
    TIM2->CNT = 0U;

    /* Prescaler 적용 */
    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR = 0U;

    /* TIM2 시작 */
    TIM2->CR1 |= TIM_CR1_CEN;

    while (1)
    {
        status = 0U;
        echo_time = 0U;
        distance = 0U;

        /*
         * TRIG LOW 2us
         */
        GPIOB->BSRR = (1U << (10U + 16U));

        TIM2->CNT = 0U;

        while (TIM2->CNT < 2U)
        {
        }

        /*
         * TRIG HIGH 10us
         */
        GPIOB->BSRR = (1U << 10U);

        TIM2->CNT = 0U;

        while (TIM2->CNT < 10U)
        {
        }

        /*
         * TRIG LOW
         */
        GPIOB->BSRR = (1U << (10U + 16U));

        /*
         * PA8 ECHO가 HIGH가 될 때까지 대기
         * 최대 30ms
         */
        TIM2->CNT = 0U;

        while (((GPIOA->IDR & (1U << 8U)) == 0U) &&
            (TIM2->CNT < 30000U))
        {
        }

        /*
         * ECHO가 정상적으로 HIGH가 됐는지 확인
         */
        if (((GPIOA->IDR & (1U << 8U)) != 0U) &&
            (TIM2->CNT < 30000U))
        {
            /*
             * ECHO HIGH 시간 측정
             */
            TIM2->CNT = 0U;

            while (((GPIOA->IDR & (1U << 8U)) != 0U) &&
                (TIM2->CNT < 30000U))
            {
            }

            echo_time = TIM2->CNT;

            /*
             * ECHO가 정상적으로 LOW로 돌아온 경우
             */
            if (((GPIOA->IDR & (1U << 8U)) == 0U) &&
                (echo_time < 30000U))
            {
                /*
                 * 거리 계산
                 * 단위: cm
                 */
                distance = echo_time / 58U;

                /*
                 * 10cm 이하: LED OFF
                 * 11cm 이상: LED ON
                 */
                if (distance >= 11U)
                {
                    status = 1U;
                }
                else
                {
                    status = 0U;
                }
            }
        }

        /*
         * PB1 LED 제어
         */
        if (status != 0U)
        {
            /* LED ON */
            GPIOB->BSRR = (1U << 1U);
        }
        else
        {
            /* LED OFF */
            GPIOB->BSRR = (1U << (1U + 16U));
        }

        /*
         * 다음 측정까지 60ms 대기
         */
        TIM2->CNT = 0U;

        while (TIM2->CNT < 60000U)
        {
        }
    }
}
