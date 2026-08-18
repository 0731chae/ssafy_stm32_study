# 3주차 과제 — STM32 GPIO와 HC-SR04 초음파 센서

> 대상 보드: **NUCLEO-F446ZE / STM32F446ZE (Cortex-M4)**  
> 참고 매뉴얼: **STM32F446xx Reference Manual RM0390**  
> 초음파 센서: **HC-SR04**

---

## 0. 과제 목표

이번 과제의 핵심은 크게 세 가지이다.

1. GPIO의 Input / Output Mode와 Push-Pull, Open-Drain, Pull-up, Pull-down을 이해한다.
2. `MODER`, `ODR`, `BSRR`을 제외한 GPIO 레지스터의 역할과 Reference Manual을 읽는 방법을 이해한다.
3. HC-SR04 초음파 센서를 GPIO와 DWT Cycle Counter를 이용해 구동하고 거리를 측정한다.

---

# 1. GPIO란?

GPIO는 **General Purpose Input/Output**의 약자이다.

마이크로컨트롤러의 핀을 프로그램에서

- 입력(Input)으로 사용하거나
- 출력(Output)으로 사용하거나
- UART, I2C, SPI 같은 주변장치 기능(Alternate Function)으로 사용하거나
- Analog 입력으로 사용하는

기능이다.

STM32에서는 각 GPIO 포트가 보통 다음과 같이 표현된다.

```text
GPIOA
GPIOB
GPIOC
...
```

각 포트에는 최대 16개의 핀이 존재한다.

```text
PA0, PA1, ... PA15
PB0, PB1, ... PB15
```

GPIO의 동작 방법은 여러 개의 **Memory-Mapped Register**를 설정하여 결정한다.

---

# 2. GPIO Input Mode

## 2.1 Input Mode란?

Input Mode는 외부에서 들어오는 전압을 MCU가 읽는 모드이다.

예를 들면 다음과 같다.

- 버튼
- 적외선 센서
- 초음파 센서의 ECHO
- 디지털 센서 출력

외부 핀의 상태가 LOW인지 HIGH인지 읽을 때 사용한다.

STM32에서는 입력값을 `GPIOx_IDR` 레지스터에서 읽는다.

```c
if (GPIOA->IDR & (1 << 5))
{
    // PA5가 HIGH
}
```

HAL에서는 다음과 같이 사용할 수 있다.

```c
HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5);
```

---

# 3. Floating Input

입력 핀에 아무것도 연결하지 않으면 핀의 전압이 HIGH인지 LOW인지 정해지지 않을 수 있다.

이를 **Floating 상태**라고 한다.

```text
외부 신호 없음

        ?
        |
GPIO ---+
```

이 상태에서는 주변의 전기적 노이즈 때문에 입력값이

```text
0 → 1 → 0 → 1
```

처럼 불안정하게 변할 수 있다.

따라서 필요에 따라 Pull-up 또는 Pull-down을 사용한다.

---

# 4. Pull-up

Pull-up은 입력 핀을 내부 또는 외부 저항을 통해 VDD에 연결하여, 외부 입력이 없을 때 기본값을 HIGH로 만드는 방식이다.

```text
3.3 V
  |
 [R]
  |
  +------ GPIO Input
  |
 Switch
  |
 GND
```

스위치를 누르지 않으면 HIGH, 스위치를 누르면 LOW가 된다.

STM32에서는 `GPIOx_PUPDR` 레지스터로 설정한다.

```text
00 : No pull-up / pull-down
01 : Pull-up
10 : Pull-down
11 : Reserved
```

---

# 5. Pull-down

Pull-down은 입력 핀을 저항을 통해 GND 쪽으로 연결하여, 외부 입력이 없을 때 기본값을 LOW로 만드는 방식이다.

```text
3.3 V
  |
Switch
  |
  +------ GPIO Input
  |
 [R]
  |
 GND
```

스위치를 누르지 않으면 LOW, 스위치를 누르면 HIGH가 된다.

---

# 6. GPIO Output Mode

Output Mode는 MCU가 외부 장치로 HIGH 또는 LOW 신호를 출력하는 모드이다.

예를 들면 다음과 같다.

- LED 켜기/끄기
- 모터 드라이버 제어
- 초음파 센서 TRIG 신호
- 릴레이 제어

STM32에서는 출력 방식으로 크게 다음 두 가지를 선택할 수 있다.

1. Push-Pull
2. Open-Drain

---

# 7. Push-Pull

Push-Pull은 MCU가 핀을 **HIGH와 LOW 모두 직접 구동**하는 방식이다.

개념적으로는 다음과 같다.

```text
         VDD
          |
      [상단 MOSFET]
          |
GPIO PIN -+
          |
      [하단 MOSFET]
          |
         GND
```

HIGH를 출력하면 상단 트랜지스터가 동작하여 핀을 VDD 쪽으로 연결한다.

LOW를 출력하면 하단 트랜지스터가 동작하여 핀을 GND 쪽으로 연결한다.

즉,

```text
HIGH → MCU가 직접 HIGH를 만들어 줌
LOW  → MCU가 직접 LOW를 만들어 줌
```

### 장점

- HIGH와 LOW를 모두 적극적으로 출력할 수 있다.
- 일반적인 LED, TRIG 신호 등에 사용하기 좋다.
- Open-Drain보다 일반적인 GPIO 출력에 많이 사용된다.

### 예

HC-SR04의 `TRIG` 핀은 보통 Push-Pull Output으로 설정하면 된다.

---

# 8. Open-Drain

Open-Drain은 MCU가 **LOW만 직접 만들 수 있는 방식**이다.

```text
VDD
 |
[R]  ← Pull-up 저항
 |
 +---------- GPIO PIN
 |
[트랜지스터]
 |
GND
```

트랜지스터가 ON이면 LOW가 된다.

트랜지스터가 OFF이면 MCU는 HIGH를 직접 출력하지 않고, 선을 놓아준다(High-Impedance).

이때 외부 Pull-up 저항 때문에 HIGH가 된다.

```text
LOW  → MCU가 직접 GND로 당김
HIGH → MCU가 직접 만들지 않음
       Pull-up 저항이 HIGH로 만들어 줌
```

대표적인 사용 예가 **I2C의 SDA / SCL**이다.

여러 장치가 하나의 선을 같이 사용할 때 한 장치가 HIGH를 강제로 출력하고 다른 장치가 LOW를 출력하는 충돌을 방지하기 유리하다.

> 참고: STM32 내부 Pull-up도 존재하지만, I2C에서는 신호 상승시간과 버스 조건 때문에 일반적으로 적절한 **외부 Pull-up 저항**을 사용한다.

---

# 9. Push-Pull vs Open-Drain 정리

| 구분 | Push-Pull | Open-Drain |
|---|---|---|
| LOW 출력 | 직접 출력 | 직접 출력 |
| HIGH 출력 | 직접 출력 | Pull-up에 의존 |
| 외부 Pull-up | 보통 불필요 | 일반적으로 필요 |
| 대표 사용 | LED, TRIG, 일반 출력 | I2C |
| 장점 | 빠르고 단순 | 여러 장치가 선을 공유하기 좋음 |

---

# 10. GPIO Output Speed

STM32의 GPIO에는 Output Speed 설정이 존재한다.

STM32F446에서는 `GPIOx_OSPEEDR`로 설정한다.

```text
00 : Low speed
01 : Medium speed
10 : Fast speed
11 : High speed
```

여기서 Speed는 프로그램의 실행 속도가 아니라 **GPIO 출력 신호의 상승/하강 특성(Slew Rate)**에 가깝다.

무조건 High Speed로 설정하는 것이 좋은 것은 아니다.

속도가 너무 높으면

- EMI 증가
- 링잉
- 불필요한 전력 소비
- 신호 무결성 문제

등이 발생할 수 있다.

따라서 필요한 정도의 속도를 사용하는 것이 좋다.

HC-SR04의 TRIG 같은 저속 디지털 신호는 일반적으로 Low 또는 Medium Speed로 충분하다.

---

# 11. STM32F446 GPIO 레지스터 전체 구조

STM32F446의 GPIO에서 대표적으로 사용하는 레지스터는 다음과 같다.

| Offset | Register | 역할 |
|---:|---|---|
| `0x00` | `GPIOx_MODER` | Input / Output / AF / Analog 모드 선택 |
| `0x04` | `GPIOx_OTYPER` | Push-Pull / Open-Drain 선택 |
| `0x08` | `GPIOx_OSPEEDR` | Output Speed 선택 |
| `0x0C` | `GPIOx_PUPDR` | Pull-up / Pull-down 선택 |
| `0x10` | `GPIOx_IDR` | 입력값 읽기 |
| `0x14` | `GPIOx_ODR` | 출력값 읽기/쓰기 |
| `0x18` | `GPIOx_BSRR` | 출력 비트를 Atomic Set / Reset |
| `0x1C` | `GPIOx_LCKR` | GPIO 설정 잠금 |
| `0x20` | `GPIOx_AFRL` | Pin 0~7 Alternate Function 선택 |
| `0x24` | `GPIOx_AFRH` | Pin 8~15 Alternate Function 선택 |

이번 과제에서는 이미 다룬 `MODER`, `ODR`, `BSRR`을 제외하고 나머지를 자세히 본다.

---

# 12. GPIOx_OTYPER

## 역할

GPIO Output Type을 결정한다.

```text
0 : Push-Pull
1 : Open-Drain
```

각 GPIO 핀당 1bit씩 사용한다.

예를 들어 PA5를 Open-Drain으로 설정한다면 PA5에 해당하는 `OT5`를 1로 설정한다.

```c
GPIOA->OTYPER |= (1 << 5);
```

Push-Pull로 설정하려면

```c
GPIOA->OTYPER &= ~(1 << 5);
```

### Reference Manual 정보

```text
Address offset : 0x04
Reset value    : 0x0000 0000
Access         : Read / Write
```

Reset value가 모두 0이므로 Reset 직후 Output Type bit들은 Push-Pull 값으로 초기화되어 있다.

단, 실제 핀 동작 여부는 `MODER` 등 다른 설정도 함께 결정한다.

---

# 13. GPIOx_OSPEEDR

## 역할

GPIO 출력 속도를 설정한다.

핀 하나당 2bit를 사용한다.

```text
00 : Low speed
01 : Medium speed
10 : Fast speed
11 : High speed
```

예를 들어 Pin 5의 설정 비트 위치는

```text
2 × 5 = 10

OSPEEDR[11:10]
```

이다.

### Reference Manual 정보

```text
Address offset : 0x08
Reset values
- GPIOA : 0x0C00 0000
- GPIOB : 0x0000 00C0
- Other ports : 0x0000 0000
```

GPIOA와 GPIOB의 Reset value가 다른 이유는 Reset 직후 Debug 관련 핀 등 일부 핀에 이미 정해진 초기 설정이 존재하기 때문이다.

---

# 14. GPIOx_PUPDR

## 역할

GPIO 내부 Pull-up / Pull-down을 설정한다.

핀 하나당 2bit이다.

```text
00 : No pull-up / pull-down
01 : Pull-up
10 : Pull-down
11 : Reserved
```

### Reference Manual 정보

```text
Address offset : 0x0C

Reset values
- GPIOA : 0x6400 0000
- GPIOB : 0x0000 0100
- Other ports : 0x0000 0000
```

예를 들어 PA5에 Pull-up을 설정한다면 PA5는 5번 핀이므로 10~11번 비트를 사용한다.

```c
GPIOA->PUPDR &= ~(3U << (5 * 2));
GPIOA->PUPDR |=  (1U << (5 * 2));
```

---

# 15. GPIOx_IDR

## 역할

현재 GPIO 입력 핀의 실제 논리 상태를 읽는 레지스터이다.

```text
IDR0  → Px0
IDR1  → Px1
...
IDR15 → Px15
```

예를 들어 PA5 입력 상태를 읽는다면

```c
if (GPIOA->IDR & (1U << 5))
{
    // HIGH
}
else
{
    // LOW
}
```

### Reference Manual 정보

```text
Address offset : 0x10
Reset value    : 0x0000 XXXX
Access         : Read Only
```

여기서 `X`는 값이 정해져 있지 않다는 의미이다.

즉, Reset 순간 외부 핀 상태에 따라 입력값이 달라질 수 있기 때문에 고정된 0 또는 1이라고 표현할 수 없다.

---

# 16. GPIOx_LCKR

## 역할

GPIO 설정이 실수로 변경되는 것을 방지하기 위해 GPIO 설정을 잠그는 레지스터이다.

```text
LCK0 ~ LCK15 : 각 GPIO 핀의 Lock
LCKK         : Lock Key
```

한 번 정상적인 Lock Sequence를 수행하면 **MCU Reset 전까지 해당 GPIO 설정을 변경할 수 없다.**

### Reference Manual 정보

```text
Address offset : 0x1C
Reset value    : 0x0000 0000
Access         : 32-bit Word Read / Write
```

Lock Sequence는 다음과 같은 순서이다.

```text
1. LCKK = 1 + 잠글 핀 값 Write
2. LCKK = 0 + 동일한 핀 값 Write
3. LCKK = 1 + 동일한 핀 값 Write
4. LCKR Read
5. LCKK가 1인지 확인
```

일반적인 간단한 프로젝트에서는 자주 사용하지 않지만, 제품 수준에서는 중요한 핀의 설정이 프로그램 오류로 변경되는 것을 막는 데 사용할 수 있다.

---

# 17. GPIOx_AFRL

AF는 **Alternate Function**의 약자이다.

GPIO 핀을 단순 Input/Output이 아니라

- UART
- SPI
- I2C
- Timer PWM
- CAN

등 MCU 내부 주변장치와 연결할 때 사용한다.

`AFRL`은 **Pin 0 ~ Pin 7**을 담당한다.

각 핀당 4bit를 사용한다.

```text
AF0  = 0000
AF1  = 0001
...
AF15 = 1111
```

### Reference Manual 정보

```text
Address offset : 0x20
Reset value    : 0x0000 0000
Access         : Read / Write
```

예를 들어 PA5의 Alternate Function을 AF5로 선택한다고 가정하면,

PA5는 Pin 0~7 범위이므로 `AFRL`을 사용한다.

Pin 5의 위치는

```text
5 × 4 = bit 20
```

부터 시작한다.

```c
GPIOA->AFR[0] &= ~(0xFU << 20);
GPIOA->AFR[0] |=  (5U   << 20);
```

---

# 18. GPIOx_AFRH

`AFRH`는 **Pin 8 ~ Pin 15**의 Alternate Function을 설정한다.

### Reference Manual 정보

```text
Address offset : 0x24
Reset value    : 0x0000 0000
Access         : Read / Write
```

예를 들어 PA9를 설정한다면 PA9는 8 이상이므로 AFRH를 사용한다.

AFRH 내부에서는 Pin 8이 첫 번째 필드이므로

```text
(Pin 번호 - 8) × 4
```

로 위치를 계산할 수 있다.

---

# 19. Reference Manual의 Register 설명 읽는 방법

STM32 Reference Manual에서 레지스터를 보면 처음에는 복잡해 보이지만 아래 순서로 보면 된다.

## 19.1 Address offset

예:

```text
Address offset: 0x04
```

GPIO 포트의 Base Address에 `0x04`를 더한 위치에 해당 레지스터가 존재한다는 뜻이다.

개념적으로

```text
GPIOA Base Address + 0x04 = GPIOA_OTYPER
```

이다.

---

## 19.2 Reset value

예:

```text
Reset value: 0x0000 0000
```

MCU Reset 직후 레지스터가 기본적으로 가지는 값이다.

`OTYPER`의 Reset value가 0이면 각 `OTy` bit가 0이라는 뜻이고, 매뉴얼에

```text
0 : Output Push-Pull
1 : Output Open-Drain
```

이라고 적혀 있으므로 기본 Output Type 값은 Push-Pull임을 알 수 있다.

---

## 19.3 r / w / rw

레지스터 그림 아래의 문자는 접근 권한을 의미한다.

```text
r  : Read Only
w  : Write Only
rw : Read / Write
```

예를 들어 `IDR`은 입력상태를 읽는 레지스터이므로 `r`이다.

---

## 19.4 Reserved

```text
Reserved
```

라고 적힌 비트는 사용하지 않는 비트이다.

Reference Manual에서

```text
must be kept at reset value
```

라고 적혀 있다면 해당 비트는 임의로 변경하면 안 된다.

따라서 레지스터 전체에 아무 값이나 대입하는 것보다 필요한 비트만 마스킹해서 변경하는 습관이 안전하다.

예:

```c
GPIOA->PUPDR &= ~(3U << 10);
GPIOA->PUPDR |=  (1U << 10);
```

---

## 19.5 x와 y의 의미

Reference Manual에서 다음 표현을 자주 볼 수 있다.

```text
GPIOx_OTYPER
OTy
```

여기서

```text
x = GPIO Port
y = GPIO Pin
```

이다.

예:

```text
GPIOA의 5번 핀
→ x = A
→ y = 5
→ GPIOA->OTYPER의 OT5
```

---

# 20. HC-SR04 초음파 센서

HC-SR04는 초음파를 발사하고 물체에 반사되어 돌아오는 시간을 이용해 거리를 측정한다.

핀은 총 4개이다.

| HC-SR04 | 역할 |
|---|---|
| VCC | 5V 전원 |
| TRIG | 측정 시작 신호 입력 |
| ECHO | 초음파 왕복 시간 출력 |
| GND | Ground |

---

# 21. HC-SR04 동작 원리

전체 과정은 다음과 같다.

```text
STM32
  |
  | 10 us HIGH
  v
TRIG
  |
  v
HC-SR04가 40 kHz 초음파 8주기 송신
  |
  v
물체에 반사
  |
  v
ECHO가 왕복 시간만큼 HIGH 유지
  |
  v
STM32가 ECHO HIGH 시간을 측정
  |
  v
거리 계산
```

HC-SR04 User Guide에서는 TRIG에 최소 약 **10 us HIGH pulse**를 넣어 측정을 시작하도록 설명한다.

ECHO가 HIGH로 유지된 시간을 `t`라고 하면 초음파는

```text
센서 → 물체 → 센서
```

로 왕복하므로 실제 거리는 왕복 거리의 절반이다.

간단히 센티미터 단위로 계산할 때 많이 사용하는 식은

```text
distance(cm) = ECHO HIGH 시간(us) / 58
```

이다.

예를 들어 ECHO가 580 us 동안 HIGH였다면

```text
580 / 58 = 10 cm
```

정도로 계산할 수 있다.

---

# 22. HC-SR04와 STM32 연결 시 매우 중요한 점

HC-SR04는 일반적으로 **5V로 동작**하고 ECHO도 5V 레벨이 나올 수 있다.

STM32F446의 GPIO는 3.3V 로직을 사용하므로 가장 안전한 방법은 ECHO 라인에 전압 분배기를 넣는 것이다.

예:

```text
HC-SR04 ECHO
     |
    1 kΩ
     |
     +---------- STM32 ECHO Input
     |
    2 kΩ
     |
    GND
```

전압은 대략

```text
5 V × 2 kΩ / (1 kΩ + 2 kΩ)
≈ 3.33 V
```

가 된다.

따라서 실습에서는 **ECHO를 STM32 핀에 무조건 5V 직결하기보다 레벨을 확인하고 분압 회로를 사용하는 것을 권장한다.**

TRIG는 STM32에서 센서 방향으로 나가는 신호이므로 STM32의 3.3V 출력으로 구동한다.

---

# 23. CubeMX GPIO 설정 예시

핀 이름은 프로젝트에서 자유롭게 정할 수 있다.

예를 들어 CubeMX에서 핀 Label을 다음처럼 지정한다.

```text
TRIG
ECHO
LD1
```

그러면 코드에서 보통 다음 이름이 자동 생성된다.

```c
TRIG_GPIO_Port
TRIG_Pin

ECHO_GPIO_Port
ECHO_Pin
```

## TRIG

```text
Mode        : GPIO_Output
Output type : Push Pull
Pull        : No pull
Speed       : Low 또는 Medium
```

## ECHO

```text
Mode : GPIO_Input
Pull : No pull
```

## LD1

NUCLEO-F446ZE 계열 Nucleo-144 보드에서 사용자 LED LD1은 보드 문서 기준 **PB0**에 연결된 구성으로 사용된다.

```text
PB0 : GPIO_Output
```

단, 실제 프로젝트에서는 보드 Revision과 CubeMX의 Board Pinout / 회로도를 한 번 확인한다.

---

# 24. 왜 DWT를 사용하는가?

초음파 센서는 약 10 us 수준의 짧은 시간을 만들어야 하고 ECHO pulse의 길이도 마이크로초 단위로 측정해야 한다.

하지만

```c
HAL_Delay(1);
```

은 1 ms 단위이므로

```text
10 us = 0.01 ms
```

를 정확하게 만들 수 없다.

이때 Cortex-M4에 존재하는 **DWT(Data Watchpoint and Trace)**의 `CYCCNT`를 사용할 수 있다.

---

# 25. DWT CYCCNT 원리

`DWT->CYCCNT`는 CPU Clock Cycle을 세는 32bit Counter이다.

예를 들어 CPU가 180 MHz로 동작한다면

```text
1초 = 180,000,000 cycle
1ms = 180,000 cycle
1us = 180 cycle
```

이 된다.

따라서 일정한 Cycle 수가 지날 때까지 기다리면 마이크로초 delay를 만들 수 있다.

실제 코드에서는 Clock을 180 MHz라고 하드코딩하지 않고 `SystemCoreClock` 값을 이용하면 더 안전하다.

---

# 26. DWT 초기화 코드

`main.c`의 USER CODE 영역 등에 다음 함수를 추가할 수 있다.

```c
static void DWT_Delay_Init(void)
{
    // DWT / ITM 등의 Trace 기능 사용 허용
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    // Cycle Counter 초기화
    DWT->CYCCNT = 0;

    // Cycle Counter 시작
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}
```

---

# 27. microsecond Delay 함수

```c
static void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;

    uint32_t cycles =
        (SystemCoreClock / 1000000U) * us;

    while ((DWT->CYCCNT - start) < cycles)
    {
        // 원하는 cycle이 지날 때까지 대기
    }
}
```

예:

```c
delay_us(10);
```

이면 약 10 us를 기다린다.

---

# 28. HC-SR04 거리 측정 코드

아래 코드는 **Hardware Timer를 사용하지 않고 DWT만 이용하는 예시**이다.

CubeMX에서 `TRIG`와 `ECHO`라는 User Label을 설정했다고 가정한다.

```c
static float HCSR04_ReadDistanceCm(void)
{
    uint32_t timeout_start;
    uint32_t echo_start;
    uint32_t echo_cycles;
    float echo_us;

    // 1. TRIG를 잠시 LOW로 만든다.
    HAL_GPIO_WritePin(
        TRIG_GPIO_Port,
        TRIG_Pin,
        GPIO_PIN_RESET
    );

    delay_us(2);

    // 2. 최소 10 us HIGH pulse 발생
    HAL_GPIO_WritePin(
        TRIG_GPIO_Port,
        TRIG_Pin,
        GPIO_PIN_SET
    );

    delay_us(10);

    HAL_GPIO_WritePin(
        TRIG_GPIO_Port,
        TRIG_Pin,
        GPIO_PIN_RESET
    );

    // 3. ECHO가 HIGH가 될 때까지 대기
    timeout_start = DWT->CYCCNT;

    while (HAL_GPIO_ReadPin(
               ECHO_GPIO_Port,
               ECHO_Pin
           ) == GPIO_PIN_RESET)
    {
        // 약 30 ms 이상 기다렸는데 신호가 없으면 실패
        if ((DWT->CYCCNT - timeout_start)
            > (SystemCoreClock / 1000U) * 30U)
        {
            return -1.0f;
        }
    }

    // 4. ECHO Rising Edge 시점 기록
    echo_start = DWT->CYCCNT;

    // 5. ECHO가 LOW가 될 때까지 대기
    while (HAL_GPIO_ReadPin(
               ECHO_GPIO_Port,
               ECHO_Pin
           ) == GPIO_PIN_SET)
    {
        if ((DWT->CYCCNT - echo_start)
            > (SystemCoreClock / 1000U) * 30U)
        {
            return -1.0f;
        }
    }

    // 6. ECHO가 HIGH였던 총 Cycle 계산
    echo_cycles = DWT->CYCCNT - echo_start;

    // 7. Cycle → us 변환
    echo_us =
        (float)echo_cycles /
        ((float)SystemCoreClock / 1000000.0f);

    // 8. us → cm
    return echo_us / 58.0f;
}
```

---

# 29. main()에서 사용하는 방법

전역변수로 거리를 하나 만들어 두면 Debugger의 Live Expressions에서도 확인하기 편하다.

```c
volatile float g_distance_cm = 0.0f;
```

`main()`에서는 GPIO와 Clock 초기화 이후 DWT를 초기화한다.

```c
int main(void)
{
    HAL_Init();

    SystemClock_Config();

    MX_GPIO_Init();

    DWT_Delay_Init();

    while (1)
    {
        g_distance_cm = HCSR04_ReadDistanceCm();

        HAL_Delay(70);
    }
}
```

HC-SR04 문서에서는 이전 측정의 영향을 줄이기 위해 측정 사이에 충분한 간격을 두는 것을 권장하므로, 실습에서는 약 `60 ms` 이상 간격을 두면 이해하기 쉽다.

---

# 30. LD1으로 센서 구동 여부 확인

거리값을 UART나 LCD로 아직 출력하지 않더라도 보드의 LED를 사용하면 센서가 동작하는지 확인할 수 있다.

예를 들어 물체가 20 cm보다 가까우면 LD1을 켠다.

```c
while (1)
{
    g_distance_cm = HCSR04_ReadDistanceCm();

    if (g_distance_cm > 0.0f &&
        g_distance_cm < 20.0f)
    {
        HAL_GPIO_WritePin(
            LD1_GPIO_Port,
            LD1_Pin,
            GPIO_PIN_SET
        );
    }
    else
    {
        HAL_GPIO_WritePin(
            LD1_GPIO_Port,
            LD1_Pin,
            GPIO_PIN_RESET
        );
    }

    HAL_Delay(70);
}
```

만약 CubeMX에서 LD1이라는 Label을 만들지 않았다면 NUCLEO-F446ZE에서 사용하는 실제 GPIO 이름에 맞춰 변경한다.

예:

```c
HAL_GPIO_WritePin(
    GPIOB,
    GPIO_PIN_0,
    GPIO_PIN_SET
);
```

---

# 31. STM32CubeIDE Live Expressions로 거리 실시간 확인

`g_distance_cm`를 다음처럼 전역 `volatile` 변수로 선언한다.

```c
volatile float g_distance_cm;
```

그리고 반복문에서 값을 갱신한다.

```c
g_distance_cm = HCSR04_ReadDistanceCm();
```

Debug Mode에서

```text
Window
→ Show View
→ Live Expressions
```

를 열고

```text
g_distance_cm
```

를 등록하면 실행 중 거리값을 확인할 수 있다.

`volatile`을 붙이는 이유는 Compiler가 해당 변수를 최적화해서 Debugger에서 사라지거나 갱신이 잘 보이지 않는 문제를 줄이기 위해서이다.

---

# 32. 전체 동작 순서

```text
[부팅]
   |
   v
HAL_Init()
   |
   v
SystemClock_Config()
   |
   v
MX_GPIO_Init()
   |
   v
DWT_Delay_Init()
   |
   v
TRIG LOW
   |
   v
10 us HIGH Pulse
   |
   v
TRIG LOW
   |
   v
ECHO가 HIGH가 될 때까지 대기
   |
   v
CYCCNT 값 저장
   |
   v
ECHO가 LOW가 될 때까지 대기
   |
   v
경과 Cycle 계산
   |
   v
Cycle → us
   |
   v
us / 58
   |
   v
거리(cm)
   |
   +------> Live Expressions
   |
   +------> LD1
```

---

# 33. GPIO와 HC-SR04의 관계

초음파 센서를 GPIO 관점에서 보면 매우 단순하다.

## TRIG

STM32가 센서에게 신호를 보내므로

```text
GPIO Output
Push-Pull
```

이다.

## ECHO

센서가 STM32에게 신호를 보내므로

```text
GPIO Input
```

이다.

즉,

```text
STM32 Output → HC-SR04 TRIG
HC-SR04 ECHO → STM32 Input
```

구조이다.

---

# 34. 과제 핵심 암기 포인트

### GPIO Input

```text
외부 신호를 읽는다.
IDR에서 값을 읽는다.
Floating을 막기 위해 필요하면 Pull-up / Pull-down을 사용한다.
```

### Push-Pull

```text
HIGH와 LOW 모두 MCU가 직접 출력한다.
일반적인 GPIO 출력에 사용한다.
```

### Open-Drain

```text
LOW만 MCU가 직접 출력한다.
HIGH는 Pull-up에 의해 만들어진다.
I2C에서 대표적으로 사용한다.
```

### OTYPER

```text
0 = Push-Pull
1 = Open-Drain
```

### OSPEEDR

```text
00 Low
01 Medium
10 Fast
11 High
```

### PUPDR

```text
00 No Pull
01 Pull-up
10 Pull-down
11 Reserved
```

### IDR

```text
GPIO 입력값
Read Only
```

### LCKR

```text
GPIO 설정 잠금
Reset 전까지 설정 변경 방지
```

### AFRL

```text
Pin 0~7의 Alternate Function
```

### AFRH

```text
Pin 8~15의 Alternate Function
```

### HC-SR04

```text
TRIG에 최소 10 us HIGH
↓
ECHO HIGH 시간 측정
↓
거리(cm) ≈ ECHO 시간(us) / 58
```

### DWT

```text
CPU Cycle을 세는 CYCCNT 사용
Hardware Timer 없이 us 단위 시간 측정 가능
```

---

# 35. 예상 질문 및 답변

## Q1. Pull-up은 왜 사용하는가?

입력 핀이 연결되지 않았을 때 Floating 상태가 되어 입력값이 불안정해지는 것을 막고 기본 상태를 HIGH로 정하기 위해 사용한다.

---

## Q2. Push-Pull과 Open-Drain의 가장 큰 차이는?

Push-Pull은 HIGH와 LOW 모두 MCU가 직접 출력하지만, Open-Drain은 LOW만 직접 출력하고 HIGH는 Pull-up 저항에 의존한다.

---

## Q3. Open-Drain을 I2C에서 사용하는 이유는?

여러 장치가 하나의 SDA/SCL 선을 공유할 때 각 장치가 HIGH를 강제로 출력하지 않고 LOW만 당기도록 하면 출력 충돌을 방지하면서 버스를 공유할 수 있기 때문이다.

---

## Q4. IDR과 ODR의 차이는?

```text
IDR : 실제 핀에서 들어오는 입력값
ODR : MCU가 출력하도록 저장해 둔 출력값
```

이다.

---

## Q5. BSRR을 사용하는 이유는?

특정 GPIO bit를 다른 bit에 영향을 주지 않고 Atomic하게 Set/Reset하기 좋기 때문이다.

---

## Q6. OSPEEDR의 High Speed가 항상 좋은가?

아니다. 출력 전환이 필요 이상으로 빠르면 EMI, 링잉, 전력소모 등 문제가 증가할 수 있으므로 필요한 속도만 사용한다.

---

## Q7. HC-SR04에서 왜 시간을 2로 나누는가?

측정한 시간은 초음파가

```text
센서 → 물체 → 센서
```

를 왕복한 시간이기 때문이다.

---

## Q8. 왜 HAL_Delay()만으로 HC-SR04를 구현하기 어려운가?

`HAL_Delay()`는 기본적으로 millisecond 단위이지만 HC-SR04는 약 10 microsecond 단위의 Trigger pulse와 microsecond 단위의 Echo 시간 측정이 필요하기 때문이다.

---

## Q9. DWT는 Timer인가?

TIM1, TIM2 같은 STM32 Peripheral Timer와는 다르다.

DWT는 Cortex-M Core 내부의 Debug/Trace 기능이며 `CYCCNT`가 CPU Cycle을 카운트한다.

따라서 별도의 TIM Peripheral 설정 없이 짧은 시간을 측정할 수 있다.

---

# 36. 최종 요약

이번 과제를 통해 GPIO는 단순히 HIGH/LOW를 출력하는 기능만 있는 것이 아니라,

```text
MODER   → GPIO 동작 모드
OTYPER  → Push-Pull / Open-Drain
OSPEEDR → 출력 속도
PUPDR   → Pull-up / Pull-down
IDR     → 입력값
ODR     → 출력값
BSRR    → Atomic Set / Reset
LCKR    → 설정 Lock
AFRL/H  → Alternate Function
```

와 같이 여러 레지스터가 협력하여 하나의 GPIO 핀을 제어한다는 것을 확인할 수 있다.

HC-SR04에서는 이러한 개념이 실제로 다음과 같이 적용된다.

```text
TRIG = Push-Pull GPIO Output
ECHO = GPIO Input
DWT CYCCNT = Pulse 시간 측정
ECHO 시간 / 58 ≈ 거리(cm)
LD1 = 센서 동작 여부 확인
```

즉, 이번 초음파 센서 실습은 GPIO Input/Output과 마이크로초 단위 시간 측정을 실제 하드웨어에 적용하는 예제라고 볼 수 있다.
