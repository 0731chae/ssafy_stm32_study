#include "stm32f4xx.h"

// 약 1초 동안 CPU 지연을 발생시키는 소프트웨어 딜레이 함수
void delay_ms(volatile uint32_t ms) {
    for (volatile uint32_t i = 0; i < ms * 3000; i++) {
        __NOP(); // CPU 공회전 (지연용)
    }
}

int main(void) {
    // 1. GPIOA 클록 공급 (RCC AHB1ENR 레지스터 0번 비트 GPIOAEN = 1)
    RCC->AHB1ENR |= (1U << 0);

    // 2. PA5 핀을 Output 모드로 설정 (MODER 레지스터 10,11번 비트 -> 01)
    GPIOA->MODER &= ~(3U << 10); // 기존 비트 클리어 (00)
    GPIOA->MODER |=  (1U << 10); // General purpose output mode (01)

    while (1) {
        // 3. PA5 핀 HIGH 출력 (LED ON)
        GPIOA->BSRR = (1U << 5);   // BSRR 5번 비트에 1 세팅
        delay_ms(1000);            // 1초 대기

        // 4. PA5 핀 LOW 출력 (LED OFF)
        GPIOA->BSRR = (1U << 21);  // BSRR 21번 비트(Reset 5)에 1 세팅
        delay_ms(1000);            // 1초 대기
    }
}