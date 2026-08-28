# STM32 Interrupt Study

## 1. 인터럽트의 개념

인터럽트(Interrupt)는 CPU가 현재 수행 중인 작업을 잠시 중단하고, 우선 처리해야 하는 사건을 처리한 뒤 다시 원래 작업으로 복귀하는 메커니즘이다.

임베디드 시스템에서는 다음과 같이 언제 발생할지 정확히 예측하기 어려운 사건이 자주 발생한다.

- GPIO 버튼 입력
- UART 데이터 수신
- Timer 만료
- ADC 변환 완료
- DMA 전송 완료
- 센서 신호 발생

인터럽트를 사용하면 CPU가 모든 장치의 상태를 계속 확인할 필요 없이, 하드웨어 이벤트가 발생했을 때만 해당 작업을 처리할 수 있다.

```text
[외부/내부 사건 발생]

버튼 눌림 / UART 수신 / Timer 만료
        ↓
[주변장치]
EXTI / USART / TIM 등이 사건 감지
        ↓
Interrupt Request(IRQ) 발생
        ↓
[NVIC]
Interrupt Enable / Priority 확인
        ↓
[Cortex-M4 CPU]
현재 실행 상태를 Stack에 저장
        ↓
Vector Table에서 ISR 주소 확인
        ↓
ISR 실행
        ↓
ISR 종료
        ↓
Stack에 저장한 실행 상태 복원
        ↓
원래 프로그램 실행 재개
```

핵심적으로 인터럽트는 단순한 함수 호출이 아니라, CPU의 실행 문맥(Context)을 잠시 저장하고 다른 실행 흐름으로 전환한 뒤 다시 복귀하는 과정이다.

---

## 2. 인터럽트가 임베디드 시스템에서 필요한 이유

임베디드 시스템의 MCU는 센서, 모터, UART, Timer, ADC, GPIO 등 다양한 하드웨어를 동시에 관리해야 한다.

모든 하드웨어 상태를 CPU가 직접 반복해서 확인하면 CPU 자원을 불필요하게 사용하게 된다.

인터럽트를 사용하면 CPU는 평소 자신의 작업을 수행하다가 사건이 발생했을 때만 반응할 수 있다.

```text
CPU : 원래 작업 수행
        ↓
        ↓
        ↓
UART 데이터 도착!
        ↓
Interrupt 발생
        ↓
UART ISR 실행
        ↓
원래 작업 복귀
```

인터럽트의 주요 장점은 다음과 같다.

- 비동기 이벤트 처리에 적합하다.
- CPU가 주변장치를 계속 감시할 필요가 없다.
- 중요한 사건에 빠르게 반응할 수 있다.
- NVIC를 통해 여러 인터럽트의 우선순위를 관리할 수 있다.
- Timer, UART, ADC, DMA 등 대부분의 MCU 주변장치와 연결된다.

---

# Polling vs Interrupt

## 3. Polling 방식

Polling은 CPU가 주변장치의 상태를 반복적으로 직접 확인하는 방식이다.

예를 들어 PC13 버튼을 Polling 방식으로 읽으면 다음과 같이 작성할 수 있다.

```c
while (1)
{
    if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_SET)
    {
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
    }
}
```

CPU 입장에서는 다음 과정을 반복한다.

```text
PC13 확인
   ↓
버튼 눌렸나?
   ↓
아니오

PC13 확인
   ↓
버튼 눌렸나?
   ↓
아니오

PC13 확인
   ↓
버튼 눌림 발견
   ↓
처리
```

즉 Polling은 CPU가 하드웨어에게 계속 질문하는 방식이다.

> CPU → 하드웨어 상태 확인

---

## 4. Interrupt 방식

Interrupt 방식에서는 CPU가 GPIO 상태를 계속 읽지 않는다.

```c
while (1)
{
    // CPU는 다른 작업을 수행할 수 있다.
}
```

버튼의 전압 변화가 발생하면 EXTI 하드웨어가 이를 감지하여 CPU에게 인터럽트를 요청한다.

```text
PC13 전압 변화
      ↓
EXTI13
      ↓
IRQ
      ↓
NVIC
      ↓
CPU
      ↓
ISR
```

즉 Interrupt 방식은 사건이 발생했을 때 하드웨어가 CPU에게 알려주는 구조이다.

> 하드웨어 → CPU에게 이벤트 발생 알림

---

## 5. Polling과 Interrupt 비교

| 항목 | Polling | Interrupt |
|---|---|---|
| 이벤트 확인 주체 | CPU | 주변장치 + Interrupt Hardware |
| CPU가 반복 확인 | 필요 | 불필요 |
| 구현 난이도 | 비교적 단순 | 상대적으로 복잡 |
| 비동기 이벤트 처리 | 불편할 수 있음 | 적합 |
| CPU 자원 사용 | 반복 검사 비용 발생 | 이벤트 발생 시 처리 |
| Priority | 직접 구조 설계 필요 | NVIC 사용 가능 |
| 대표 예시 | 단순 상태 확인 | UART RX, Timer, DMA, EXTI |

Polling이 항상 나쁜 것은 아니다. 예를 들어 온도 센서를 1초에 한 번 읽는 정도라면 Polling이 더 단순하고 적합할 수 있다.

반대로 UART 수신, Timer 만료, Encoder Pulse처럼 언제 발생할지 모르거나 빠른 반응이 필요한 경우에는 Interrupt가 더 적합하다.

---

# STM32F446ZE EXTI 구조

## 6. EXTI란?

EXTI는 **External Interrupt/Event Controller**의 약자이다.

STM32 내부에 존재하는 주변 하드웨어 블록으로, GPIO 등의 외부 신호 변화를 감지하여 Interrupt Request 또는 Event를 생성한다.

중요한 점은 EXTI와 NVIC의 소속이 다르다는 것이다.

```text
┌────────────── STM32F446ZE ──────────────┐
│                                         │
│             Cortex-M4 Core              │
│          ┌────────────────┐             │
│          │ CPU            │             │
│          │ Registers      │             │
│          │ NVIC           │             │
│          └───────▲────────┘             │
│                  │ IRQ                  │
│                  │                      │
│          ┌───────┴────────┐             │
│          │      EXTI      │             │
│          └───────▲────────┘             │
│                  │                      │
│               SYSCFG                    │
│                  │                      │
│                GPIO                     │
│                                         │
│        TIM / USART / ADC / SPI ...      │
│                                         │
└─────────────────────────────────────────┘
```

- **NVIC**: ARM Cortex-M4 코어에 포함된 인터럽트 컨트롤러
- **EXTI**: STMicroelectronics가 STM32에 구현한 주변장치
- **SYSCFG**: STM32 시스템 설정용 하드웨어 블록

즉 **SYSCFG는 EXTI 내부 장치가 아니다.** `SYSCFG`와 `EXTI`는 서로 다른 하드웨어 블록이며, EXTI 입력 소스를 설정할 때 SYSCFG의 EXTICR 레지스터를 사용한다.

---

# EXTI Line과 GPIO Port

## 7. 하나의 EXTI Line에는 여러 GPIO Port 후보가 존재한다

GPIO의 핀 번호와 EXTI Line 번호는 대응한다.

예를 들어 13번 핀 계열은 EXTI13의 입력 후보가 된다.

```text
            ┌─────────────┐
PA13 ───────┤             │
PB13 ───────┤             │
PC13 ───────┤    MUX      ├──── EXTI13
PD13 ───────┤             │
PE13 ───────┤             │
            └──────▲──────┘
                   │
              Port 선택
```

즉 다음과 같다.

```text
PA13 ┐
PB13 ├── EXTI13 후보
PC13 ┤
PD13 ┘
```

하지만 동시에 여러 Port를 EXTI13에 연결하는 것이 아니라, 그중 하나를 선택한다.

현재 프로젝트에서는 PC13 USER 버튼을 사용하므로:

```text
EXTI13 Input Source = GPIOC Pin 13
```

으로 설정한다.

이 Port 선택을 담당하는 것이 `SYSCFG->EXTICR`이다.

---

# SYSCFG

## 8. SYSCFG란?

SYSCFG는 시스템 수준의 여러 설정 기능을 제공하는 하드웨어 블록이다.

EXTI 관점에서 가장 중요한 역할은 다음과 같다.

> 특정 EXTI Line에 어떤 GPIO Port를 연결할 것인지 선택한다.

현재 PC13을 EXTI13에 연결하는 흐름은 다음과 같다.

```text
PC13
  │
  │ GPIO 입력
  ↓
SYSCFG EXTICR
  │
  │ "EXTI13 입력 Source = Port C"
  ↓
EXTI13
  ↓
Edge Detector
  ↓
Pending
  ↓
IRQ
  ↓
NVIC
  ↓
CPU
```

따라서 다음과 같이 이해하면 된다.

```text
핀 번호 13
    ↓
EXTI13

Port C
    ↓
SYSCFG EXTICR에서 선택
```

---

## 9. EXTICR 구조

STM32F4의 SYSCFG에는 `EXTICR` 레지스터 배열이 존재한다.

각 EXTICR은 4개의 EXTI Line에 대한 GPIO Port 선택 정보를 가진다.

```text
EXTICR[0] → EXTI0  ~ EXTI3
EXTICR[1] → EXTI4  ~ EXTI7
EXTICR[2] → EXTI8  ~ EXTI11
EXTICR[3] → EXTI12 ~ EXTI15
```

따라서 EXTI13은 `EXTICR[3]`에 포함된다.

HAL 코드에서는 다음 연산으로 EXTICR 번호를 계산한다.

```c
SYSCFG->EXTICR[position >> 2U]
```

PC13의 경우:

```text
position = 13

13 >> 2
= 3
```

따라서:

```c
SYSCFG->EXTICR[3]
```

을 사용한다.

EXTICR 하나 내부에서는 각 EXTI Line이 4bit씩 사용된다.

```text
SYSCFG EXTICR[3]

bit 15      12 11       8 7        4 3        0
┌─────────────┬────────────┬───────────┬───────────┐
│   EXTI15    │   EXTI14   │  EXTI13   │  EXTI12   │
└─────────────┴────────────┴───────────┴───────────┘
                                  ↑
                            GPIO Port 선택
```

PC13이라면 EXTI13 필드에 GPIOC를 의미하는 값이 설정된다.

---

# EXTI 주요 레지스터

## 10. IMR - Interrupt Mask Register

`IMR`은 특정 EXTI Line의 Interrupt Request를 허용할지 결정한다.

예를 들어:

```text
IMR bit13 = 1
```

이면 EXTI13 Interrupt가 허용된다.

반대로:

```text
IMR bit13 = 0
```

이면 EXTI13에서 Edge가 발생하더라도 Interrupt Request가 NVIC 방향으로 전달되지 않는다.

개념적인 흐름:

```text
Edge 발생
   ↓
Pending 생성
   ↓
IMR
 ├─ 1 → Interrupt Request 전달
 └─ 0 → Interrupt Request 차단
```

EXTI의 IMR과 NVIC의 Enable은 서로 다른 단계이다.

```text
EXTI IMR
   ↓
NVIC Enable
   ↓
CPU
```

---

## 11. EMR - Event Mask Register

`EMR`은 EXTI의 Event 출력을 허용할지 결정한다.

EXTI는 Interrupt뿐 아니라 Event도 만들 수 있기 때문에 Interrupt와 Event 설정이 분리되어 있다.

```text
EXTI
 ├─ Interrupt → NVIC → CPU ISR
 │
 └─ Event     → Event 경로
```

현재 PC13 버튼 실습에서는 Interrupt를 사용하므로 EMR은 핵심 관심 대상이 아니다.

---

## 12. RTSR - Rising Trigger Selection Register

`RTSR`은 Rising Edge를 감지할지 결정한다.

```text
LOW → HIGH
```

변화가 Rising Edge이다.

예를 들어:

```text
RTSR bit13 = 1
```

이면 EXTI13은 LOW에서 HIGH로 변하는 순간을 감지한다.

현재 프로젝트에서는 다음 설정을 사용했다.

```c
GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
```

HAL은 이 값을 해석하여 내부적으로 EXTI의 RTSR을 설정한다.

---

## 13. FTSR - Falling Trigger Selection Register

`FTSR`은 Falling Edge를 감지할지 결정한다.

```text
HIGH → LOW
```

변화가 Falling Edge이다.

예를 들어:

```text
FTSR bit13 = 1
```

이면 EXTI13은 HIGH에서 LOW로 변하는 순간을 감지한다.

---

## 14. PR - Pending Register

`PR`은 EXTI Event가 발생했다는 상태를 기록하는 Pending Register이다.

예를 들어 EXTI13 Edge가 감지되면:

```text
EXTI->PR bit13 = 1
```

이 된다.

HAL에서는 다음 매크로를 이용하여 Pending 여부를 확인한다.

```c
__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_13)
```

내부적으로는 다음과 같은 연산을 수행한다.

```c
EXTI->PR & GPIO_PIN_13
```

`GPIO_PIN_13`은 13번째 bit만 1인 비트마스크이다.

```c
GPIO_PIN_13 = 0x2000
```

이는 다음과 동일한 의미이다.

```c
1U << 13
```

이진수로 표현하면:

```text
0010 0000 0000 0000
```

따라서:

```c
EXTI->PR & 0x2000
```

연산을 통해 PR의 bit13이 1인지 확인할 수 있다.

---

## 15. Pending Clear - Write 1 to Clear

Interrupt 처리가 끝나면 Pending 상태를 Clear해야 한다.

HAL에서는:

```c
__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_13);
```

를 사용한다.

내부적으로는:

```c
EXTI->PR = GPIO_PIN_13;
```

형태가 된다.

처음 보면 bit를 Clear하면서 왜 `1`을 쓰는지 의문이 들 수 있다.

EXTI의 PR은 **Write 1 to Clear(W1C)** 방식이기 때문이다.

```text
해당 bit에 0 Write
→ 변화 없음

해당 bit에 1 Write
→ 해당 Pending bit Clear
```

---

## 16. SWIER - Software Interrupt Event Register

`SWIER`는 Software Interrupt Event Register이다.

실제 GPIO Edge가 발생하지 않더라도 소프트웨어에서 EXTI 요청을 발생시키는 데 사용할 수 있다.

개념적으로:

```text
Software
   ↓
SWIER
   ↓
EXTI Pending
   ↓
Interrupt Request
   ↓
NVIC
   ↓
ISR
```

테스트 또는 특정 소프트웨어 기반 Interrupt Event 발생에 활용할 수 있다.

---

# EXTI와 NVIC의 차이

## 17. EXTI 역할

EXTI는 외부 신호의 변화를 감지하고 Interrupt Request의 원인을 생성하는 하드웨어이다.

주요 역할:

- Rising / Falling Edge 감지
- Pending 상태 생성
- Interrupt Request 생성
- Event 생성

```text
GPIO Edge
   ↓
EXTI
   ↓
IRQ
```

---

## 18. NVIC 역할

NVIC는 EXTI를 포함한 여러 주변장치가 생성한 Interrupt Request를 관리한다.

주요 역할:

- Interrupt Enable / Disable
- Pending 관리
- Active 관리
- Priority 관리
- Nested Interrupt 관리
- CPU에 Interrupt 전달

```text
EXTI ─┐
TIM ──┤
UART ─┼── NVIC ── CPU
ADC ──┘
```

따라서 다음과 같이 구분할 수 있다.

> EXTI = GPIO 변화 등을 감지하여 Interrupt Request를 생성하는 하드웨어

> NVIC = 발생한 Interrupt Request 중 무엇을 CPU가 처리할지 관리하는 인터럽트 컨트롤러

---

# HAL 내부의 EXTI 설정 코드 분석

## 19. `HAL_GPIO_Init()`에서 EXTI 설정

CubeMX에서 다음과 같이 설정했다.

```c
GPIO_InitStruct.Pin = GPIO_PIN_13;
GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
GPIO_InitStruct.Pull = GPIO_NOPULL;

HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
```

이 설정은 `HAL_GPIO_Init()` 내부에서 실제 EXTI 및 SYSCFG 레지스터 설정으로 변환된다.

핵심 코드는 다음과 같다.

```c
/*--------------------- EXTI Mode Configuration ------------------------*/
if((GPIO_Init->Mode & EXTI_MODE) != 0x00U)
{
    /* Enable SYSCFG Clock */
    __HAL_RCC_SYSCFG_CLK_ENABLE();

    temp = SYSCFG->EXTICR[position >> 2U];
    temp &= ~(0x0FU << (4U * (position & 0x03U)));
    temp |= ((uint32_t)(GPIO_GET_INDEX(GPIOx))
            << (4U * (position & 0x03U)));
    SYSCFG->EXTICR[position >> 2U] = temp;

    /* Rising Edge */
    temp = EXTI->RTSR;
    temp &= ~((uint32_t)iocurrent);

    if((GPIO_Init->Mode & TRIGGER_RISING) != 0x00U)
    {
        temp |= iocurrent;
    }

    EXTI->RTSR = temp;

    /* Falling Edge */
    temp = EXTI->FTSR;
    temp &= ~((uint32_t)iocurrent);

    if((GPIO_Init->Mode & TRIGGER_FALLING) != 0x00U)
    {
        temp |= iocurrent;
    }

    EXTI->FTSR = temp;

    /* Event Mask */
    temp = EXTI->EMR;
    temp &= ~((uint32_t)iocurrent);

    if((GPIO_Init->Mode & EXTI_EVT) != 0x00U)
    {
        temp |= iocurrent;
    }

    EXTI->EMR = temp;

    /* Interrupt Mask */
    temp = EXTI->IMR;
    temp &= ~((uint32_t)iocurrent);

    if((GPIO_Init->Mode & EXTI_IT) != 0x00U)
    {
        temp |= iocurrent;
    }

    EXTI->IMR = temp;
}
```

---

## 20. PC13 기준으로 HAL 코드를 해석

PC13을 사용하는 경우:

```text
GPIOx = GPIOC
position = 13
iocurrent = 1 << 13 = 0x2000
```

### 20.1 SYSCFG EXTICR 설정

```c
temp = SYSCFG->EXTICR[position >> 2U];
```

PC13에서는:

```text
13 >> 2 = 3
```

이므로:

```c
SYSCFG->EXTICR[3]
```

을 사용한다.

다음 코드는 EXTI13에 연결할 GPIO Port 정보를 설정한다.

```c
temp |= ((uint32_t)(GPIO_GET_INDEX(GPIOx))
        << (4U * (position & 0x03U)));
```

즉:

```text
EXTI13 Input Source = GPIOC
```

가 된다.

### 20.2 Rising Edge 설정

`EXTI->RTSR`의 bit13을 설정하여 Rising Edge를 감지한다.

```text
RTSR bit13 = 1
```

### 20.3 Falling Edge 설정

현재 설정은 `GPIO_MODE_IT_RISING`이므로:

```text
FTSR bit13 = 0
```

이 된다.

### 20.4 Interrupt Mask 설정

`EXTI->IMR`의 bit13을 설정한다.

```text
IMR bit13 = 1
```

따라서 EXTI13에서 발생한 Interrupt Request를 Interrupt 경로로 전달할 수 있게 된다.

---

# PC13 버튼 인터럽트 전체 흐름

## 21. 하드웨어에서 Callback까지

NUCLEO-F446ZE의 USER 버튼 PC13을 누르면 다음 과정을 거친다.

```text
① PC13 전압 변화
        ↓
② SYSCFG를 통해 PC13이 EXTI13 입력으로 연결
        ↓
③ EXTI13 Rising Edge 감지
        ↓
④ PR bit13 = 1
        ↓
⑤ IMR bit13 확인
        ↓
⑥ Interrupt Request 발생
        ↓
⑦ NVIC가 Enable / Priority 확인
        ↓
⑧ Cortex-M4가 Interrupt 수락
        ↓
⑨ 현재 실행 Context를 Stack에 저장
        ↓
⑩ Vector Table에서 EXTI15_10_IRQHandler 주소 확인
        ↓
⑪ EXTI15_10_IRQHandler 실행
        ↓
⑫ HAL_GPIO_EXTI_IRQHandler 호출
        ↓
⑬ Pending Flag Clear
        ↓
⑭ HAL_GPIO_EXTI_Callback 호출
        ↓
⑮ 사용자 코드 실행
        ↓
⑯ ISR 종료
        ↓
⑰ Exception Return
        ↓
⑱ 이전 Context 복원
        ↓
⑲ 원래 프로그램 실행 재개
```

---

# 실제 실습 코드

## 22. PC13 USER Button → PB0 LD1 Toggle

```c
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_13)
    {
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
    }
}
```

`while(1)`에서는 버튼 상태를 Polling하지 않는다.

```c
while (1)
{
}
```

동작 결과:

```text
USER 버튼 1회
↓
LD1 OFF → ON

USER 버튼 다시 1회
↓
LD1 ON → OFF
```

이 실습을 통해 GPIO Interrupt가 단순히 Callback 함수가 직접 실행되는 것이 아니라 다음 경로를 거친다는 것을 확인했다.

```text
PC13
 ↓
SYSCFG
 ↓
EXTI13
 ↓
NVIC
 ↓
Cortex-M4
 ↓
Vector Table
 ↓
EXTI15_10_IRQHandler
 ↓
HAL_GPIO_EXTI_IRQHandler
 ↓
HAL_GPIO_EXTI_Callback
 ↓
PB0 LED Toggle
```

---

# 핵심 정리

## Interrupt

```text
Polling
= CPU가 하드웨어 상태를 계속 확인

Interrupt
= 하드웨어에서 사건이 발생하면 CPU에게 알림
```

## EXTI

```text
GPIO
 ↓
SYSCFG
 ↓
EXTI
 ↓
NVIC
 ↓
CPU
```

## 역할 구분

| 구성 요소 | 역할 |
|---|---|
| GPIO | 실제 외부 전압 입력 |
| SYSCFG | 특정 EXTI Line에 어느 GPIO Port를 연결할지 선택 |
| EXTI | Edge 감지, Pending 생성, IRQ/Event 생성 |
| NVIC | Interrupt Enable, Priority, Pending, Nested 관리 |
| Vector Table | Exception 번호와 ISR 주소 연결 |
| ISR | 실제 Interrupt 처리 진입점 |
| HAL Handler | HAL 수준의 공통 Interrupt 처리 |
| Callback | 사용자가 구현하는 동작 |

---

# 학습하면서 확인한 핵심 포인트

단순히 다음 코드를 사용하는 것만으로도 GPIO Interrupt를 구현할 수 있다.

```c
GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
```

하지만 HAL 내부를 따라가 보면 이 한 줄이 실제로는 다음 하드웨어 설정으로 이어진다는 것을 확인할 수 있다.

```text
GPIO_MODE_IT_RISING
        ↓
HAL_GPIO_Init()
        ↓
SYSCFG EXTICR
        ↓
EXTI RTSR / FTSR
        ↓
EXTI IMR
        ↓
NVIC
        ↓
Cortex-M4
```

이를 통해 STM32 HAL은 별도의 마법 같은 기능이 아니라, MCU 내부 레지스터를 사용하기 쉽게 추상화한 라이브러리라는 점을 이해할 수 있었다.
