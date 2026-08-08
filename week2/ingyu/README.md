# 2주차 — STM32F446ZE Memory Map 구조 및 GPIO 정리

> 담당: ingyu
> 대상 보드: NUCLEO-F446ZE (STM32F446ZETx, Cortex-M4F, LQFP144)
> 담당 파트: **Bare-metal 방식**

---

## 0. 요약

| 항목 | 값 |
|---|---|
| 코어 | Arm Cortex-M4F (32bit, FPU 탑재) |
| 최대 클럭 | 180 MHz |
| Flash | 512 KB @ `0x0800_0000` |
| SRAM | 128 KB (SRAM1 112KB + SRAM2 16KB) @ `0x2000_0000` |
| GPIO 포트 | GPIOA ~ GPIOH (8개, 포트당 16핀) |
| GPIO 소속 버스 | AHB1 |

본 문서의 주소값은 모두 프로젝트 내
`Drivers/CMSIS/Device/ST/STM32F4xx/Include/stm32f446xx.h` 에서 직접 확인한 값이다.

---

## 1. Cortex-M4 주소 공간 (4 GB)

Cortex-M4는 32비트 주소를 쓰므로 접근 가능한 공간은 2³² = 4 GB이다.
Arm이 이 공간의 용도를 **고정으로 규정**해 두었기 때문에, 모든 Cortex-M4 칩이 동일한 큰 틀을 공유한다.

```
0xFFFF_FFFF ┌──────────────────────────┐
            │  System / Private        │  Cortex-M4 코어 내부 주변장치
            │  Peripheral Bus (PPB)    │  → NVIC, SysTick, SCB, MPU
0xE000_0000 ├──────────────────────────┤
            │  External Device         │
0xA000_0000 ├──────────────────────────┤
            │  External RAM            │
0x6000_0000 ├──────────────────────────┤
            │  Peripheral              │  ★ GPIO, RCC, TIM, UART ...
0x4000_0000 ├──────────────────────────┤
            │  SRAM                    │  변수, 스택, 힙
0x2000_0000 ├──────────────────────────┤
            │  Code                    │  Flash (프로그램, 상수)
0x0000_0000 └──────────────────────────┘
```

### 중요한 구분

`SysTick`, `NVIC`는 **RM0390(ST 레퍼런스 매뉴얼)에 나오지 않는다.**
`0xE000_0000` 이상은 ST가 만든 주변장치가 아니라 **Arm이 코어에 내장한 것**이기 때문이며,
해당 문서는 **PM0214 (STM32 Cortex-M4 Programming Manual)** 이다.

| 영역 | 만든 주체 | 참고 문서 |
|---|---|---|
| `0x4000_0000` ~ (GPIO, RCC 등) | STMicroelectronics | RM0390 |
| `0xE000_0000` ~ (SysTick, NVIC) | Arm | PM0214 |

---

## 2. STM32F446ZE에 실제로 존재하는 메모리

4 GB 공간 중 실제로 물리 메모리가 붙어 있는 구간은 일부뿐이다.

| 영역 | 시작 주소 | 크기 | CMSIS 매크로 |
|---|---|---|---|
| Flash | `0x0800_0000` | 512 KB | `FLASH_BASE` |
| Flash 끝 | `0x0807_FFFF` | — | `FLASH_END` |
| SRAM1 | `0x2000_0000` | 112 KB | `SRAM1_BASE` |
| SRAM2 | `0x2001_C000` | 16 KB | `SRAM2_BASE` |
| Backup SRAM | `0x4002_4000` | 4 KB | `BKPSRAM_BASE` |
| Peripheral | `0x4000_0000` | — | `PERIPH_BASE` |

### 비트 밴딩(bit-banding) 영역

SRAM과 주변장치 영역에는 **별칭(alias) 주소 공간**이 따로 존재한다.

| 영역 | 별칭 시작 주소 | 매크로 |
|---|---|---|
| SRAM1 별칭 | `0x2200_0000` | `SRAM1_BB_BASE` |
| Peripheral 별칭 | `0x4200_0000` | `PERIPH_BB_BASE` |

이 영역의 한 워드(32bit)가 원본 영역의 **비트 하나**에 대응한다.
따라서 read-modify-write 없이 단일 write로 비트 하나를 조작할 수 있다.
다만 GPIO에는 이보다 더 직관적인 `BSRR`이 있어 실제로는 잘 쓰이지 않는다.

---

## 3. 버스 구조와 주변장치 영역

`0x4000_0000` 부터의 Peripheral 영역은 다시 **버스별로** 나뉜다.

| 버스 | 시작 주소 | 오프셋 | 특징 |
|---|---|---|---|
| APB1 | `0x4000_0000` | `PERIPH_BASE + 0x0000_0000` | 저속. 최대 45 MHz |
| APB2 | `0x4001_0000` | `PERIPH_BASE + 0x0001_0000` | 중속. 최대 90 MHz |
| **AHB1** | **`0x4002_0000`** | `PERIPH_BASE + 0x0002_0000` | 고속. HCLK 그대로 (180 MHz) |
| AHB2 | `0x5000_0000` | `PERIPH_BASE + 0x1000_0000` | 고속 |

> **GPIO가 AHB1에 있는 이유**
> GPIO는 토글 속도가 곧 성능인 주변장치다. APB에 두면 버스 분주 때문에 최대 토글
> 주파수가 떨어지므로, 시스템 클럭을 그대로 쓰는 AHB1에 배치되어 있다.

### APB1 (일부)

| 주변장치 | 주소 |
|---|---|
| TIM2 | `0x4000_0000` |
| TIM3 | `0x4000_0400` |
| TIM4 | `0x4000_0800` |
| TIM5 | `0x4000_0C00` |
| SPI2 | `0x4000_3800` |
| USART2 | `0x4000_4400` |
| USART3 | `0x4000_4800` |
| I2C1 | `0x4000_5400` |
| PWR | `0x4000_7000` |

### APB2 (일부)

| 주변장치 | 주소 |
|---|---|
| TIM1 | `0x4001_0000` |
| TIM8 | `0x4001_0400` |
| USART1 | `0x4001_1000` |
| USART6 | `0x4001_1400` |
| ADC1 | `0x4001_2000` |
| SDIO | `0x4001_2C00` |

### AHB1 — GPIO가 있는 곳

| 주변장치 | 주소 | 오프셋 |
|---|---|---|
| **GPIOA** | **`0x4002_0000`** | `+0x0000` |
| **GPIOB** | **`0x4002_0400`** | `+0x0400` |
| **GPIOC** | **`0x4002_0800`** | `+0x0800` |
| **GPIOD** | **`0x4002_0C00`** | `+0x0C00` |
| **GPIOE** | **`0x4002_1000`** | `+0x1000` |
| **GPIOF** | **`0x4002_1400`** | `+0x1400` |
| **GPIOG** | **`0x4002_1800`** | `+0x1800` |
| **GPIOH** | **`0x4002_1C00`** | `+0x1C00` |
| CRC | `0x4002_3000` | `+0x3000` |
| **RCC** | **`0x4002_3800`** | `+0x3800` |
| DMA1 | `0x4002_6000` | `+0x6000` |
| DMA2 | `0x4002_6400` | `+0x6400` |

**포트당 `0x400`(1 KB) 간격**으로 균등 배치되어 있다.
실제 레지스터는 `0x00` ~ `0x24`까지 40바이트뿐이지만, 주소 디코딩 회로를
단순하게 만들기 위해 넉넉한 크기로 정렬한 것이다.

---

## 4. CMSIS의 메모리 맵 표현 방식

CMSIS 헤더는 메모리 맵을 **주소 매크로**와 **구조체 타입** 두 축으로 표현한다.

### 4.1 주소 계산 사슬

```c
#define PERIPH_BASE       0x40000000UL                    /* stm32f446xx.h:920  */
#define AHB1PERIPH_BASE   (PERIPH_BASE + 0x00020000UL)    /* stm32f446xx.h:939  */
#define GPIOA_BASE        (AHB1PERIPH_BASE + 0x0000UL)    /* stm32f446xx.h:999  */
#define GPIOA             ((GPIO_TypeDef *) GPIOA_BASE)   /* stm32f446xx.h:1121 */
```

전처리가 끝나면 `GPIOA`는 `((GPIO_TypeDef *) 0x40020000UL)` 한 덩어리가 된다.

| 심볼 | 정체 | 가능한 연산 |
|---|---|---|
| `GPIOA_BASE` | `unsigned long` 숫자 | 산술 연산만 |
| `GPIOA` | `GPIO_TypeDef *` 포인터 | `->MODER`, `->BSRR` 접근 |

캐스팅이 **"이 주소부터 GPIO_TypeDef 모양의 데이터가 있다"** 고 컴파일러에게 알려주는 단계다.

### 4.2 구조체 타입

```c
typedef struct
{
  __IO uint32_t MODER;    /*!< GPIO port mode register,               Address offset: 0x00      */
  __IO uint32_t OTYPER;   /*!< GPIO port output type register,        Address offset: 0x04      */
  __IO uint32_t OSPEEDR;  /*!< GPIO port output speed register,       Address offset: 0x08      */
  __IO uint32_t PUPDR;    /*!< GPIO port pull-up/pull-down register,  Address offset: 0x0C      */
  __IO uint32_t IDR;      /*!< GPIO port input data register,         Address offset: 0x10      */
  __IO uint32_t ODR;      /*!< GPIO port output data register,        Address offset: 0x14      */
  __IO uint32_t BSRR;     /*!< GPIO port bit set/reset register,      Address offset: 0x18      */
  __IO uint32_t LCKR;     /*!< GPIO port configuration lock register, Address offset: 0x1C      */
  __IO uint32_t AFR[2];   /*!< GPIO alternate function registers,     Address offset: 0x20-0x24 */
} GPIO_TypeDef;
```

`uint32_t` 멤버를 선언 순서대로 나열하면 **구조체 오프셋이 레지스터 맵 오프셋과 저절로 일치**한다.
따라서 아래 두 줄은 완전히 동일한 기계어로 컴파일된다.

```c
GPIOA->BSRR = 0x20;
(*(volatile uint32_t *)(0x40020000UL + 0x18)) = 0x20;
```

### 4.3 `__IO` 의 정체

`core_cm4.h:227` 부근에 정의되어 있다.

```c
#define __I   volatile const   /* read only  */
#define __O   volatile         /* write only */
#define __IO  volatile         /* read/write */
```

`volatile`은 **"컴파일러가 모르는 이유로 값이 바뀔 수 있으니 최적화하지 말라"** 는 지시다.
주변장치 레지스터는 CPU가 아니라 하드웨어가 값을 바꾸므로 반드시 필요하다.
없을 경우 다음이 깨진다.

| 코드 | `volatile` 없으면 |
|---|---|
| `while ((SysTick->CTRL & (1<<16)) == 0) {}` | 조건이 안 변한다고 판단 → 무한루프 |
| `BSRR = ON; BSRR = OFF;` | 첫 write가 무의미하다고 판단 → 삭제 |
| `(void)RCC->AHB1ENR;` | 결과를 안 쓴다고 판단 → 삭제 |

`-O0`(디버그 빌드)에서는 증상이 안 나타나고 `-O2`에서 터지므로 특히 위험하다.

> `__O`와 `__IO`의 정의가 같은 이유: 쓰기 전용을 강제하는 C 문법이 없다.
> 반면 `__I`는 `const`가 붙어 실제로 컴파일 에러가 발생한다.

### 4.4 왜 오프셋이 4씩 증가하는가

메모리는 **바이트 단위로 주소가 매겨진다.**
`uint32_t`는 32bit = **4바이트**를 차지하므로, 다음 레지스터는 4칸 뒤에서 시작한다.
핀 개수나 비트 필드 폭과는 무관하며, 32비트 레지스터를 연달아 배치한 결과다.

---

## 5. GPIO 레지스터 정리

### 전체 목록

| 오프셋 | 이름 | 핀당 비트 | R/W | 역할 |
|---|---|---|---|---|
| `0x00` | **MODER** | 2 | R/W | 핀 모드 (입력/출력/AF/아날로그) |
| `0x04` | **OTYPER** | 1 | R/W | 출력 타입 (Push-Pull / Open-Drain) |
| `0x08` | **OSPEEDR** | 2 | R/W | 출력 속도 (슬루율) |
| `0x0C` | **PUPDR** | 2 | R/W | 내부 풀업/풀다운 |
| `0x10` | **IDR** | 1 | R | 입력 데이터 — 핀의 실제 전압 |
| `0x14` | **ODR** | 1 | R/W | 출력 데이터 — 명령한 값 |
| `0x18` | **BSRR** | 2 | W | 비트 단위 Set/Reset (원자적) |
| `0x1C` | **LCKR** | 1 | R/W | 설정 잠금 |
| `0x20` | **AFRL** (`AFR[0]`) | 4 | R/W | 핀 0~7 의 대체 기능 |
| `0x24` | **AFRH** (`AFR[1]`) | 4 | R/W | 핀 8~15 의 대체 기능 |

### 비트 폭이 다른 이유

핀당 비트 수는 **표현해야 할 경우의 수**가 결정한다.

| 레지스터 | 경우의 수 | 필요 비트 | × 16핀 | 32bit에 수용 |
|---|---|---|---|---|
| OTYPER, IDR, ODR | 2가지 | 1 | 16 bit | ○ (상위 16bit 미사용) |
| MODER, OSPEEDR, PUPDR | 4가지 | 2 | 32 bit | ○ (정확히 꽉 참) |
| BSRR | Set/Reset 각 1 | 2 | 32 bit | ○ (정확히 꽉 참) |
| AFR | **16가지** (AF0~AF15) | **4** | **64 bit** | **× → 레지스터 2개로 분할** |

`AFR`만 배열인 이유가 여기 있다. 16핀 × 4비트 = 64비트라 32비트 레지스터 하나에 담기지 않아
AFRL(핀 0~7) / AFRH(핀 8~15)로 쪼갠 것이다.

### 비트 위치 공식

```
1 bit/pin 레지스터 (OTYPER, IDR, ODR)      →  핀 n 은 비트 n
2 bit/pin 레지스터 (MODER, OSPEEDR, PUPDR) →  핀 n 은 비트 n*2 부터 2칸
4 bit/pin 레지스터 (AFR)                   →  핀 n 은 AFR[n/8] 의 비트 (n%8)*4 부터 4칸
BSRR                                       →  Set: 비트 n  /  Reset: 비트 n+16
```

---

### 5.1 MODER (`0x00`) — 핀 모드

| 값 | 모드 | 설명 |
|---|---|---|
| `00` | Input | 리셋 기본값 (대부분의 핀) |
| `01` | **General purpose output** | 일반 출력 |
| `10` | Alternate function | UART/SPI/TIM 등 주변장치에 연결 |
| `11` | Analog | ADC/DAC. 디지털 입력 버퍼 차단 |

```c
GPIOA->MODER &= ~(3U << (5 * 2));   /* PA5 의 2비트 클리어 */
GPIOA->MODER |=  (1U << (5 * 2));   /* 01 = Output 세팅   */
```

클리어를 먼저 하지 않고 `|=` 만 하면 기존 값이 남아 의도치 않은 모드가 될 수 있다.

### 5.2 OTYPER (`0x04`) — 출력 타입

| 값 | 타입 | 동작 |
|---|---|---|
| `0` | **Push-Pull** | PMOS/NMOS 둘 다 사용. High/Low 모두 능동 구동 |
| `1` | Open-Drain | NMOS만 사용. Low만 구동, High는 외부 풀업 필요 |

LED 구동에는 Push-Pull, I2C 같은 공유 버스에는 Open-Drain을 쓴다.

**Push-Pull 출력 드라이버 구조**

```
              VDD (3.3V)
                 │
              ┌──┴──┐
              │ PMOS│  ← ODR = 1 일 때 ON
              └──┬──┘
                 │
    ODR 비트 ────┼──────────●  핀
                 │
              ┌──┴──┐
              │ NMOS│  ← ODR = 0 일 때 ON
              └──┬──┘
                 │
                GND
```

GPIO 출력 핀은 전원을 새로 만드는 것이 아니라, **3.3 V 레일에 달린 소프트웨어 제어 스위치**다.
그래서 배선 한 가닥으로 전원 공급과 제어가 동시에 이루어진다.
다만 핀당 권장 전류는 약 8 mA, 절대 최대는 25 mA이므로 LED에는 반드시 전류 제한 저항이 필요하다.

### 5.3 OSPEEDR (`0x08`) — 출력 속도

| 값 | 속도 |
|---|---|
| `00` | Low |
| `01` | Medium |
| `10` | Fast |
| `11` | Very High |

출력 파형의 상승/하강 시간(슬루율)을 조절한다.
빠를수록 소비 전류와 EMI가 증가하므로, **필요한 만큼만 올리는 것이 원칙**이다.
LED 점멸(1 Hz)에는 Low로 충분하다.

### 5.4 PUPDR (`0x0C`) — 풀업/풀다운

| 값 | 설정 |
|---|---|
| `00` | 없음 |
| `01` | Pull-up |
| `10` | Pull-down |
| `11` | 예약 |

출력 모드에서는 핀이 능동 구동되므로 보통 `00`으로 둔다.
입력 모드에서 아무것도 연결되지 않은 핀(floating)의 값이 요동치는 것을 막는 용도로 쓴다.

### 5.5 IDR (`0x10`) — 입력 데이터 (읽기 전용)

핀에 **실제로 걸린 전압**을 나타낸다. 출력 모드에서도 계속 동작한다.

```c
uint32_t cmd  = (GPIOA->ODR >> 5) & 1U;   /* 내가 명령한 값 */
uint32_t real = (GPIOA->IDR >> 5) & 1U;   /* 실제 핀의 전압 */
```

`cmd == 1` 인데 `real == 0` 이면 핀이 GND로 단락되었거나 과부하 상태다.
회로 진단에 활용할 수 있다.

### 5.6 ODR (`0x14`) — 출력 데이터

하위 16비트만 사용하며, **각 비트가 핀 하나에 1:1 대응**한다.
이 비트가 출력 드라이버의 PMOS/NMOS를 직접 제어한다.

| ODR 비트 | 핀 상태 |
|---|---|
| 1 | PMOS ON → 3.3 V 출력 |
| 0 | NMOS ON → 0 V 출력 |

### 5.7 BSRR (`0x18`) — Bit Set/Reset

```
 비트 31 ─────────────────── 16 │ 15 ─────────────────── 0
┌─────────────────────────────┬─────────────────────────────┐
│      BR[15:0]  (Reset)      │      BS[15:0]  (Set)        │
└─────────────────────────────┴─────────────────────────────┘
        상위 16비트                     하위 16비트
   1을 쓰면 ODR 해당 비트 → 0     1을 쓰면 ODR 해당 비트 → 1
```

핀 하나가 BSRR에서 **두 자리**를 차지한다.

| 동작 | 비트 위치 | PA5 (핀 5) |
|---|---|---|
| SET (ON) | `n` | 비트 5 → `0x0000_0020` |
| RESET (OFF) | `n + 16` | 비트 21 → `0x0020_0000` |

**성질**

- **Write-only.** 읽으면 항상 0이며, 실제 상태는 `ODR`에 있다
- **1을 쓴 비트만 동작**하고 0을 쓴 비트는 무시된다 → `=` 대입인데도 다른 핀에 영향이 없다
- BS와 BR을 동시에 1로 쓰면 **BS(Set)가 우선**한다

BSRR은 ODR을 저장하는 별도 레지스터가 아니라, **ODR을 조작하는 명령 창구**다.

```
     BSRR (0x18)              ODR (0x14)              물리 핀
  ┌──────────────┐         ┌──────────────┐       ┌──────────┐
  │ BR[31:16]    │         │              │       │  PMOS/   │
  │ BS[15:0]     │ ──────▶ │  비트 0~15   │ ────▶ │  NMOS    │ ──▶ PA5
  │ (write-only) │  하드웨어 │  (상태 저장) │       │  드라이버 │
  └──────────────┘         └──────────────┘       └──────────┘
     "명령"                    "상태"                 "출력"
```

### 5.8 LCKR (`0x1C`) — 설정 잠금

특정 시퀀스를 거쳐 핀 설정을 잠그면, 다음 리셋까지 MODER/OTYPER/OSPEEDR/PUPDR/AFR을
변경할 수 없다. 안전이 중요한 시스템에서 오작동으로 인한 핀 재설정을 막는 용도다.

### 5.9 AFR[2] (`0x20`, `0x24`) — 대체 기능

핀을 UART, SPI, TIM 등 주변장치에 연결할 때 **어느 기능인지**를 4비트로 지정한다.
MODER를 `10`(Alternate function)으로 설정한 뒤에만 의미가 있다.

어떤 핀에 어떤 AF 번호가 배정되어 있는지는 **데이터시트(DS10693)의 Alternate function mapping 표**를 봐야 한다.

---

## 6. BSRR vs ODR — 왜 BSRR을 쓰는가

### 6.1 문제

```c
GPIOA->ODR |= (1U << 5);
```

이 한 줄은 기계어 **3개**로 번역된다.

```asm
ldr   r3, [r2, #20]     ; ① ODR 읽기   (offset 0x14)
orr   r3, r3, #32       ; ② 비트 세우기
str   r3, [r2, #20]     ; ③ ODR 쓰기
```

인터럽트는 **명령어와 명령어 사이**에서 발생하므로, ①과 ③ 사이에 끼어들 수 있다.

### 6.2 사고 시나리오

PA5 = 파란 LED (main), PA6 = 빨간 LED (ISR), 초기 `ODR = 0x0000` 일 때:

| 시각 | 주체 | 동작 | ODR 실제값 | r3 (CPU 레지스터) |
|---|---|---|---|---|
| ① | main | `LDR` ODR 읽기 | `0x0000` | `0x0000` |
| ② | — | **인터럽트 발생** | | |
| ③ | ISR | `ODR \|= (1<<6)` — 빨간 LED ON | `0x0040` | |
| ④ | ISR | 복귀 (r3 = `0x0000` 복원) | `0x0040` | `0x0000` |
| ⑤ | main | `ORR` r3 수정 | `0x0040` | `0x0020` |
| ⑥ | main | `STR` ODR 쓰기 | **`0x0020`** | |

ISR이 켠 6번 비트가 **증발**한다 (lost update).
코드 어디에도 빨간 LED를 끄는 줄이 없는데 빨간 LED가 켜지지 않는다.

원인은 main이 ①에서 읽은 값이 ⑥ 시점에는 이미 낡았다는 것이다.
`|=`를 쓴 것 자체는 "다른 핀을 지우지 않으려는" **올바른 의도**지만,
읽은 값의 **유효기간이 만료**되어 버린다.

### 6.3 해결

```c
GPIOA->BSRR = (1U << 5);
```

```asm
str   r3, [r2, #24]     ; 단일 명령 (offset 0x18)
```

- 명령어가 1개 → 중간에 끊길 구간 자체가 없음 (**원자적**)
- ODR을 **읽지 않음** → 낡은 값이 존재할 수 없음
- 1을 쓴 비트만 반응 → 다른 15개 핀을 덮어쓰지 않음

해법이 "더 잘 읽자"가 아니라 **"읽지 말자"** 라는 점이 핵심이다.
읽어야 하는 이유는 다른 핀을 보존하기 위해서인데, BSRR은 애초에 다른 핀을 건드리지
않으므로 보존할 필요도, 읽을 이유도 사라진다.

| | `ODR \|= x` | `BSRR = x` |
|---|---|---|
| 명령의 의미 | "16비트 전체를 이 값으로 만들어라" | "n번 비트를 1로 만들어라" |
| 다른 핀 상태를 읽는가 | 예 | 아니오 |
| 다른 핀 상태를 덮어쓰는가 | 예 (32bit 전체) | 아니오 |
| 기계어 수 | 3 | 1 |
| 인터럽트 안전성 | ✗ | ○ |

---

## 7. 실험 — 인터럽트 경쟁 조건 실측

### 7.1 하드웨어 구성

| 소자 | 핀 | Zio 커넥터 | 제어 주체 |
|---|---|---|---|
| **파란 LED** | **PA5** | D13 | **main** |
| **빨간 LED** | **PA6** | D12 | **인터럽트(ISR)** |

```
PA5 ── 파란 LED ── 330Ω ── GND
PA6 ── 빨간 LED ── 330Ω ── GND
```

> **두 LED가 반드시 같은 포트(GPIOA)여야 한다.**
> 경쟁 조건은 `GPIOA->ODR` 이라는 **하나의 레지스터**를 공유할 때만 발생한다.
> 빨간 LED를 GPIOB에 연결하면 별개 레지스터이므로 아무리 해도 재현되지 않는다.

### 7.2 시나리오

```
1~3회차   파랑 1초 ON  →  빨강 1초 ON      (평범하게 잘 돌아가는 구간)
4회차     파랑을 켜기 "직전"에 빨강 인터럽트 발생
          → ODR  방식 : 파랑만 켜진다  (빨강 명령이 씹힘)
          → BSRR 방식 : 둘 다 켜진다   (정상)
```

3번 정상 동작을 보여준 뒤 4번째에 사고가 나므로,
**"평소엔 멀쩡한데 어느 순간 터진다"** 는 경쟁 조건의 성격이 그대로 드러난다.

### 7.3 인터럽트를 원하는 순간에 발생시키는 방법

실제 경쟁 조건은 기계어 2~3개 폭의 좁은 창에 인터럽트가 **우연히** 떨어져야 발생한다.
확률이 너무 낮아 시연 중 재현이 불가능하므로, **소프트웨어로 강제 발생**시켰다.

```c
static void Trigger_Interrupt_Now(void)
{
    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;   /* PendSV 를 pending 상태로 */
    __DSB();                               /* 쓰기 완료 보장 */
    __ISB();                               /* 다음 명령 전에 예외 진입 보장 */
}
```

**PendSV**는 Cortex-M 코어에 내장된 예외로, 주변장치 없이 `SCB->ICSR`(`0xE000_ED04`)의
비트 하나를 세우면 CPU가 다음 명령을 실행하기 전에 핸들러로 점프한다.
덕분에 **매 사이클 100% 재현**된다.

### 7.4 핵심 코드

```c
/* [인터럽트 측] 빨간 LED를 켜라는 명령 — PendSV_Handler 에서 호출 */
void Red_ISR_TurnOn(void)
{
    RED_PORT->ODR |= (1U << RED_PIN);
    g_isr_red_on_count++;
}

/* ---------- 방식 A : ODR 직접 조작 (버그) ---------- */
static void LED_On_ODR_Racy(void)
{
    uint32_t tmp;

    tmp = LED_PORT->ODR;         /* ① 읽기 (LDR) — 빨강 비트는 아직 0 */

    Trigger_Interrupt_Now();     /* ★ ISR 이 빨강 비트를 1로 만들고 복귀
                                  *   그러나 tmp 는 여전히 낡은 값        */

    tmp |= (1U << LED_PIN);      /* ② 수정 (ORR) — 낡은 값 기준 */
    LED_PORT->ODR = tmp;         /* ③ 쓰기 (STR) — 빨강 비트가 증발 */

    if (!RED_IS_ON()) { g_red_lost_count++; }
}

/* ---------- 방식 B : BSRR 사용 (정상) ---------- */
static void LED_On_BSRR_Safe(void)
{
    Trigger_Interrupt_Now();     /* 같은 타이밍 — 조건은 완전히 동일 */

    LED_PORT->BSRR = (1U << LED_PIN);   /* 단일 STR. ODR 을 읽지 않는다 */

    if (!RED_IS_ON()) { g_red_lost_count++; }   /* 여기는 절대 증가하지 않음 */
}
```

두 함수의 **인터럽트 발생 시점과 조건은 완전히 동일**하다.
다른 것은 파란 LED를 켜는 방식이 `ODR` 이냐 `BSRR` 이냐 하나뿐이다.

### 7.5 실측 영상

| 방식 | 4회차 결과 |
|---|---|
| **ODR** (`ODR \|= ...`) | **파랑만 켜짐** — 빨강 명령이 씹힘 |
| **BSRR** (`BSRR = ...`) | **파랑 + 빨강 함께 켜짐** — 정상 |

#### ① ODR 방식 — 빨간 LED가 씹힌다

<!-- 영상 첨부 위치 (ODR) -->

[파일 다운로드: `media/led_race_odr.mp4`](media/led_race_odr.mp4)

#### ② BSRR 방식 — 정상 동작

<!-- 영상 첨부 위치 (BSRR) -->

[파일 다운로드: `media/led_race_bsrr.mp4`](media/led_race_bsrr.mp4)

**관찰 포인트**

1. 두 영상 모두 **앞부분 3회차는 완전히 동일**하게 파랑↔빨강이 번갈아 점멸한다
2. **4회차에서만** 차이가 난다
   - ODR 영상: 파란 LED만 켜지고 빨간 LED는 끝까지 어둡다
   - BSRR 영상: 두 LED가 동시에 켜진다
3. ISR은 두 경우 모두 **똑같이 "빨간 LED를 켜라"는 명령을 실행했다.**
   ODR 방식에서는 그 명령이 main의 낡은 write에 의해 덮어써진 것뿐이다

### 7.6 수치로 확인

디버거의 `Live Expressions` 에 두 변수를 등록하면 유실이 숫자로 나온다.

| 변수 | ODR 방식 | BSRR 방식 |
|---|---|---|
| `g_isr_red_on_count` (ISR이 명령한 횟수) | 1, 2, 3, 4 … 증가 | 1, 2, 3, 4 … 증가 |
| `g_red_lost_count` (main이 지운 횟수) | **똑같이 증가** (100% 유실) | **0 고정** |

---

## 8. 실습 — PA5 출력 설정 전체 코드

### 8.1 Bare-metal

```c
#define LED_PIN  5U

void LED_Init(void)
{
    /* [1] RCC_AHB1ENR bit0 = GPIOAEN — GPIOA에 클럭 공급.
     *     클럭이 없으면 이후 레지스터 write가 전부 무시된다. */
    RCC->AHB1ENR |= (1U << 0);
    (void)RCC->AHB1ENR;                              /* 클럭 안정화 더미 리드 */

    /* [2] MODER : 01 = Output */
    GPIOA->MODER   &= ~(3U << (LED_PIN * 2));
    GPIOA->MODER   |=  (1U << (LED_PIN * 2));

    /* [3] OTYPER : 0 = Push-Pull */
    GPIOA->OTYPER  &= ~(1U << LED_PIN);

    /* [4] OSPEEDR : 00 = Low speed */
    GPIOA->OSPEEDR &= ~(3U << (LED_PIN * 2));

    /* [5] PUPDR : 00 = No pull */
    GPIOA->PUPDR   &= ~(3U << (LED_PIN * 2));
}

/* 출력 */
GPIOA->BSRR = (1U << LED_PIN);          /* ON  : 0x0000_0020 */
GPIOA->BSRR = (1U << (LED_PIN + 16));   /* OFF : 0x0020_0000 */
```

### 8.2 1초 지연 (SysTick 직접 구성)

`HAL_Delay()` 를 쓰지 않고 SysTick 레지스터를 직접 다룬다.
SysTick은 `0xE000_E010` 에 있는 **Cortex-M4 코어 내부** 주변장치다.

```c
static void SysTick_Init_BareMetal(void)
{
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
        /* CTRL bit16 = COUNTFLAG : 0까지 셌으면 1이 된다.
         * 이 레지스터를 읽는 순간 자동으로 0으로 클리어된다. */
        while ((SysTick->CTRL & (1U << 16)) == 0U) { }
    }
}
```

> **주의** — `HAL_Init()` 은 SysTick을 `TICKINT=1`(인터럽트 사용)로 설정하고
> `SysTick_Handler()` 에서 `HAL_IncTick()` 을 호출한다.
> 위 코드가 `CTRL` 을 덮어쓰면서 TICKINT가 0이 되므로, 이후 `HAL_Delay()` 를 부르면
> 무한 대기에 빠진다. 두 방식을 한 프로젝트에 섞지 말 것.

### 8.3 HAL과의 대응

| 단계 | HAL | Bare-metal |
|---|---|---|
| 클럭 인가 | `__HAL_RCC_GPIOA_CLK_ENABLE()` | `RCC->AHB1ENR \|= (1<<0)` |
| 모드 설정 | `HAL_GPIO_Init(GPIOA, &cfg)` | MODER/OTYPER/OSPEEDR/PUPDR 직접 |
| 출력 ON | `HAL_GPIO_WritePin(..., GPIO_PIN_SET)` | `GPIOA->BSRR = (1<<5)` |
| 출력 OFF | `HAL_GPIO_WritePin(..., GPIO_PIN_RESET)` | `GPIOA->BSRR = (1<<21)` |
| 1초 지연 | `HAL_Delay(1000)` | SysTick `LOAD`/`VAL`/`CTRL` 직접 구성 |

HAL 소스(`stm32f4xx_hal_gpio.c`)의 `HAL_GPIO_WritePin()` 내부는 다음과 같다.

```c
if (PinState != GPIO_PIN_RESET) { GPIOx->BSRR = GPIO_Pin; }
else                            { GPIOx->BSRR = (uint32_t)GPIO_Pin << 16u; }
```

**HAL은 레지스터 조작을 함수로 감싼 얇은 래퍼**이며, 두 방식은 다른 일을 하는 것이
아니라 같은 일을 다른 추상화 수준에서 수행한다.

| 비교 항목 | HAL | Bare-metal |
|---|---|---|
| 코드량 | 적음 | 많음 |
| 이식성 | 높음 (다른 STM32로 그대로) | 낮음 (칩마다 레지스터 상이) |
| 실행 파일 크기 | 큼 | 작음 |
| 하드웨어 이해 | 감춰짐 | 드러남 |

---

## 9. 검증 방법

### 9.1 SFR 뷰 (STM32CubeIDE)

`Run > Debug` 로 디버그 세션 시작 후
`Window > Show View > SFRs` → `GPIOA` 전개

`LED_Init()` 을 `F6`(Step Over)로 한 줄씩 실행하면 다음이 관찰된다.

| 실행 후 | MODER 값 | 해석 |
|---|---|---|
| 초기 | `0x0000_0000` | 전 핀 Input |
| `[2]` 실행 후 | `0x0000_0400` | `0b01 << 10` — 핀 5의 2비트 자리에 `01` |

`BSRR` 에 write하는 순간 `ODR` 이 `0x00` ↔ `0x20` 으로 토글되는 것도 확인할 수 있다.
**계산한 값과 실제 레지스터 값이 일치**하는지 확인하면 본 문서의 내용이 검증된다.

### 9.2 디스어셈블리

`Window > Show View > Disassembly` 에서 ODR 방식과 BSRR 방식의 명령어 수 차이를
직접 확인할 수 있다.

```asm
; ODR 방식 — 3개
ldr   r3, [r3, #20]     ; #20 = 0x14 = ODR
orr   r3, r3, #32
str   r3, [r2, #20]

; BSRR 방식 — 1개
str   r3, [r2, #24]     ; #24 = 0x18 = BSRR
```

오프셋 `#20`, `#24` 가 5장의 레지스터 맵 표와 정확히 일치한다.
소스가 아니라 **실행 코드 수준에서** 원자성 차이가 증명된다.

---

## 10. 참고 문서

| 문서 | 내용 |
|---|---|
| **RM0390** | STM32F446xx Reference Manual — 메모리 맵, GPIO/RCC 레지스터 비트 정의 |
| **DS10693** | STM32F446xC/E 데이터시트 — 핀 배치, AF 매핑, 전기적 특성 |
| **PM0214** | STM32 Cortex-M4 Programming Manual — SysTick, NVIC, SCB 등 코어 레지스터 |
| **UM1974** | STM32 Nucleo-144 boards — 커넥터 핀맵, 온보드 LED/버튼 회로 |
| `stm32f446xx.h` | CMSIS 디바이스 헤더 — 모든 베이스 주소와 레지스터 구조체 정의 |
| `core_cm4.h` | CMSIS 코어 헤더 — `__IO` 등 매크로, SysTick/NVIC 구조체 |
