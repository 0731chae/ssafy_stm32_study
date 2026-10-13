# 미니 자동차 후진 보조 시스템 — CPU 버전 vs DMA 버전 (NUCLEO-F446RE)

같은 하드웨어, 같은 동작을 **CPU가 직접 하는 버전**과 **DMA에 맡기는 버전** 두 개의 프로젝트로 만들어 비교합니다.

- 가변저항(가속 페달)으로 모터 속도를 조절합니다.
- 초음파 센서로 뒤쪽 장애물까지의 거리를 잽니다.
- 가까워질수록 부저가 빠르게 울리고, LED가 초록 → 노랑 → 빨강으로 바뀝니다.
- 위험 거리에 들어오면 모터가 자동으로 멈춥니다.

> **읽기 전에**
> - CubeMX 메뉴 이름과 HAL 함수는 4개 PDF에 나오지 않는 **일반 지식**입니다. STM32CubeIDE/CubeMX 버전에 따라 메뉴 위치가 조금 다를 수 있습니다.
> - 이 가이드의 코드는 **실제 보드에서 아직 실행해 보지 않았습니다.** 처음 실행할 때는 모터를 바퀴가 뜬 상태로 두고, 저속으로 시험하세요.
> - 출처 표기: 〔RM〕 = RM0390, 〔DS〕 = STM32F446 데이터시트, 〔UM〕 = UM1724 (Nucleo-64)

---

## 목차
- [0. 두 버전은 무엇이 다른가](#0-두-버전은-무엇이-다른가)
- [1. 준비물과 배선](#1-준비물과-배선)
- [2. 핀 · 타이머 · DMA 배정표](#2-핀--타이머--dma-배정표)
- [3. 공통 CubeMX 설정 (두 프로젝트 모두)](#3-공통-cubemx-설정-두-프로젝트-모두)
- [4. CPU 버전](#4-cpu-버전)
- [5. DMA 버전](#5-dma-버전)
- [6. 비교 실험 방법](#6-비교-실험-방법)
- [7. 문제 해결](#7-문제-해결)
- [8. 심화 과제](#8-심화-과제)

---

## 0. 두 버전은 무엇이 다른가

| 기능 | CPU 버전 | DMA 버전 |
|---|---|---|
| **가속 페달 (가변저항)** | 루프마다 ADC를 1번 변환하고 **끝날 때까지 기다림** (폴링) | ADC 연속 변환 → **DMA2가 64칸 버퍼를 계속 채움** (Circular). CPU는 평균만 계산 |
| **거리 측정 (초음파)** | TRIG 펄스를 CPU가 만들고, **에코가 끝날 때까지 기다리며** 시간을 잼 | TIM4가 TRIG 펄스를 **자동 발생**하고, 에코의 상승/하강 시각을 **입력 캡처 → DMA1**이 저장 |
| **경고음 (부저)** | 루프가 돌 때마다 "지금 시각에 맞는 칸"의 값을 **CPU가 TIM3에 씀** | **TIM6(50ms 박자) → DMA1**이 패턴 표의 다음 칸을 TIM3에 씀. CPU는 표만 바꿈 |
| **PC 로그 (UART)** | 전송이 **끝날 때까지 기다림** (블로킹) | **DMA1**이 보내고, CPU는 바로 다음 일을 함 |
| LED, 모터 PWM, 버튼 | 같음 | 같음 |

**핵심**: 두 버전은 **같은 패턴 표, 같은 판단 로직**을 씁니다. 다른 것은 **"누가 데이터를 옮기느냐(CPU냐 DMA냐)"** 하나뿐입니다. 그래서 차이가 생기면 그것은 순수하게 DMA 때문입니다.

---

## 1. 준비물과 배선

### 1-1. 준비물
| 부품 | 수량 | 비고 |
|---|---|---|
| NUCLEO-F446RE | 1 | USB 케이블 포함 |
| 패시브 부저 모듈 MH-FMD (3핀) | 1 | VCC / I/O / GND |
| LED (초록, 노랑, 빨강) | 3 | + 220~330Ω 저항 3개 |
| 푸시 버튼 | 2 | 비상 정지, 음소거 |
| L298N 모터 드라이버 + DC 모터 | 1 | 모터용 **별도 전원** (예: 6~12V 배터리/어댑터) |
| 초음파 센서 (HC-SR04 계열) | 1 | 5V 전원 |
| 가변저항 (10kΩ 내외) | 1 | |
| 브레드보드, 점퍼선 | | |

### 1-2. 배선표

Nucleo의 Arduino 커넥터 이름(D0~D15, A0~A5)과 STM32 핀의 대응은 〔UM p.36 Figure 24, p.48–49 Table 19〕에서 확인했습니다.

| 부품 | 부품 핀 | 연결할 곳 | STM32 핀 | 용도 |
|---|---|---|---|---|
| **가변저항** | 한쪽 끝 | 3V3 | – | ⚠ **5V에 연결 금지** |
| | 가운데(와이퍼) | **A0** | PA0 | ADC1_IN0 |
| | 다른 쪽 끝 | GND | – | |
| **부저 MH-FMD** | VCC | 3V3 | – | |
| | I/O | **D12** | PA6 | TIM3_CH1 (PWM) |
| | GND | GND | – | |
| **초음파 센서** | VCC | **5V** | – | |
| | TRIG | **D15** | PB8 | CPU: GPIO 출력 / DMA: TIM4_CH3 |
| | ECHO | **D10** | PB6 | CPU: GPIO 입력 / DMA: TIM4_CH1. **5V 허용(FT) 핀** 〔DS p.56〕 |
| | GND | GND | – | |
| **L298N** | ENA | **D7** | PA8 | TIM1_CH1 (속도 PWM). ⚠ ENA 점퍼 캡을 **빼야** 함 |
| | IN1 | **D8** | PA9 | 방향 |
| | IN2 | **D9** | PC7 | 방향 |
| | GND | GND | – | ⚠ **모터 전원 GND와 보드 GND를 반드시 공통**으로 |
| | 12V(VS) | 모터용 외부 전원 + | – | ⚠ 보드 5V로 모터를 돌리지 말 것 |
| | OUT1/OUT2 | DC 모터 | – | |
| **LED 초록** | 애노드(+) → 저항 | **D2** | PA10 | 캐소드(−)는 GND |
| **LED 노랑** | 애노드(+) → 저항 | **D6** | PB10 | |
| **LED 빨강** | 애노드(+) → 저항 | **D11** | PA7 | |
| **버튼1 (비상 정지)** | 한쪽 | **A3** | PB0 | 다른 쪽은 GND (내부 풀업 사용) |
| **버튼2 (음소거)** | 한쪽 | **A4** | PC1 | 다른 쪽은 GND |
| (보드 내장) | LD2 | – | PA5 | 루프 속도 표시용 |
| (보드 내장) | USART2 | – | PA2/PA3 | ST-LINK 가상 COM 포트 〔UM p.26〕 |

> ⚠ **안전 체크**
> - MH-FMD는 보통 **I/O가 LOW일 때 울리는(active-low)** 모듈입니다. 반대로 동작하면 코드의 `BUZZER_ACTIVE_LOW`를 0으로 바꾸세요.
> - L298N의 입력(ENA/IN1/IN2)은 보통 3.3V 신호로도 동작하지만, 모듈마다 다를 수 있습니다.
> - 초음파 ECHO는 5V 신호입니다. PB6은 FT 핀이지만, 다른 핀으로 바꿀 때는 그 핀이 FT인지 〔DS p.46–57〕 핀 표에서 확인하거나 저항 분배로 낮추세요.

---

## 2. 핀 · 타이머 · DMA 배정표

### 2-1. 클럭 (두 버전 공통)
| 항목 | 값 |
|---|---|
| SYSCLK / HCLK | 180 MHz |
| APB1 (PCLK1) | 45 MHz → **APB1 타이머 클럭 90 MHz** (TIM3, TIM4, TIM6) |
| APB2 (PCLK2) | 90 MHz → **APB2 타이머 클럭 180 MHz** (TIM1) |
| ADC 클럭 | PCLK2 / 4 = 22.5 MHz |

APB 분주비가 1이 아니면 타이머 클럭은 PCLK의 2배가 됩니다. CubeMX Clock Configuration 화면의 "APB1 Timer clocks", "APB2 Timer clocks" 칸에서 실제 값을 꼭 확인하세요.

### 2-2. 타이머 계산

| 타이머 | 클럭 | Prescaler (PSC) | 카운터 주파수 | Period (ARR) | 결과 |
|---|---|---|---|---|---|
| **TIM1** 모터 PWM | 180 MHz | 179 | 1 MHz | 999 | **1 kHz** PWM, 듀티 0~999 |
| **TIM3** 부저 PWM | 90 MHz | 89 | 1 MHz | 499 | **2 kHz** 소리 |
| **TIM4** (CPU 버전) | 90 MHz | 89 | 1 MHz | 65535 | 1µs 단위 자유 카운터 |
| **TIM4** (DMA 버전) | 90 MHz | 89 | 1 MHz | 59999 | **60 ms** 측정 주기, CH3 펄스 10 → TRIG **10 µs** |
| **TIM6** (DMA 버전) | 90 MHz | 8999 | 10 kHz | 499 | **50 ms** 박자 |

공식: `출력 주파수 = 타이머 클럭 / ((PSC + 1) × (ARR + 1))`

### 2-3. DMA 배정 (DMA 버전)

〔RM p.204 Table 28, 29〕에서 골랐습니다. 스트림이 서로 겹치지 않습니다.

| 기능 | DMA 요청 | 컨트롤러 | Stream | Channel | 방향 | 모드 |
|---|---|---|---|---|---|---|
| 가변저항 | ADC1 | **DMA2** | Stream0 | 0 | 주변장치 → 메모리 | Circular |
| 에코 상승 시각 | TIM4_CH1 | DMA1 | Stream0 | 2 | 주변장치 → 메모리 | Circular |
| 에코 하강 시각 | TIM4_CH2 | DMA1 | Stream3 | 2 | 주변장치 → 메모리 | Circular |
| 경고음 패턴 | TIM6_UP | DMA1 | Stream1 | 7 | 메모리 → 주변장치 | Circular |
| PC 로그 | USART2_TX | DMA1 | Stream6 | 4 | 메모리 → 주변장치 | Normal |

- TIM3, TIM4, TIM6, USART2는 모두 **APB1** 주변장치라서 **DMA1**을 씁니다.
- ADC1은 **APB2** 주변장치라서 **DMA2**를 씁니다. (README 2부 4절)
- 경고음 DMA는 **요청은 TIM6가 내지만, 쓰는 곳은 TIM3의 CCR1**입니다. DMA는 요청을 낸 장치와 상관없이 `PAR`에 적힌 주소에 씁니다.

---

## 3. 공통 CubeMX 설정 (두 프로젝트 모두)

> 프로젝트를 **두 개** 만듭니다: `Parking_CPU`, `Parking_DMA`.
> 이 3장의 설정은 **두 프로젝트에 똑같이** 합니다. 그다음 4장(CPU)과 5장(DMA)의 추가 설정을 각각 합니다.

### 3-1. 프로젝트 만들기
1. STM32CubeIDE → **File > New > STM32 Project**
2. **Board Selector** 탭 → `NUCLEO-F446RE` 검색 → 선택 → Next
3. Project Name: `Parking_CPU` (두 번째는 `Parking_DMA`) → Finish
4. "Initialize all peripherals with their default Mode?" → **No**
   - 모든 설정을 이 가이드대로 직접 합니다. Yes를 눌러도 되지만, 그러면 B1 버튼 등이 미리 잡혀 있으니 아래 표와 다른 부분은 맞춰 주세요.
5. `.ioc` 파일이 열리면 **Pinout & Configuration** 화면에서 시작합니다.

### 3-2. System Core → SYS
| 항목 | 값 |
|---|---|
| Debug | **Serial Wire** ← ⚠ 이걸 안 하면 다음부터 디버거 연결이 어려워집니다 |
| Timebase Source | SysTick |

### 3-3. System Core → RCC
| 항목 | 값 |
|---|---|
| High Speed Clock (HSE) | Disable |
| Low Speed Clock (LSE) | Disable |

내부 HSI(16 MHz)를 씁니다. 보드 버전과 상관없이 동작합니다.

### 3-4. Clock Configuration 탭
1. **PLL Source Mux**: HSI
2. **PLLM**: /8 → 2 MHz
3. **PLLN**: ×180 → 360 MHz
4. **PLLP**: /2 → 180 MHz
5. **System Clock Mux**: PLLCLK
6. **AHB Prescaler**: /1 → HCLK **180 MHz**
7. **APB1 Prescaler**: /4 → PCLK1 **45 MHz** (APB1 Timer clocks **90 MHz**)
8. **APB2 Prescaler**: /2 → PCLK2 **90 MHz** (APB2 Timer clocks **180 MHz**)

> 팁: HCLK 칸에 `180`을 입력하고 Enter를 치면 CubeMX가 자동으로 맞춰 줍니다. 그 후 위 값과 같은지 확인하세요. 빨간색 칸이 없어야 합니다.

### 3-5. GPIO (Pinout 화면에서 핀 클릭)

핀을 클릭해 모드를 고르고, 핀을 **오른쪽 클릭 → Enter User Label**로 이름을 붙입니다. ⚠ **코드가 이 이름을 그대로 쓰므로 철자를 정확히** 맞추세요.

| 핀 | 모드 | User Label |
|---|---|---|
| PA5 | GPIO_Output | `LD2` |
| PA10 | GPIO_Output | `LED_G` |
| PB10 | GPIO_Output | `LED_Y` |
| PA7 | GPIO_Output | `LED_R` |
| PA9 | GPIO_Output | `MOTOR_IN1` |
| PC7 | GPIO_Output | `MOTOR_IN2` |
| PB0 | **GPIO_EXTI0** | `BTN_ESTOP` |
| PC1 | **GPIO_EXTI1** | `BTN_MUTE` |

**System Core → GPIO** 화면에서 세부 설정:

| 핀 | GPIO mode | Pull-up/Pull-down | Output level |
|---|---|---|---|
| 출력 6개 | Output Push Pull | No pull | Low |
| PB0, PC1 | **External Interrupt Mode with Falling edge trigger detection** | **Pull-up** | – |

**System Core → GPIO → NVIC 탭** (또는 System Core → NVIC):
- ☑ **EXTI line0 interrupt**
- ☑ **EXTI line1 interrupt**

### 3-6. Connectivity → USART2
| 항목 | 값 |
|---|---|
| Mode | **Asynchronous** |
| Baud Rate | 115200 |
| Word Length | 8 Bits |
| Parity | None |
| Stop Bits | 1 |

핀이 **PA2 (TX), PA3 (RX)**로 잡혔는지 확인합니다. 다른 핀으로 잡혔다면 PA2/PA3을 직접 클릭해 USART2_TX/RX로 지정하세요.

### 3-7. Timers → TIM1 (모터 PWM)
| 항목 | 값 |
|---|---|
| Clock Source | Internal Clock |
| Channel1 | **PWM Generation CH1** → 핀 **PA8** 확인 |
| Prescaler | **179** |
| Counter Period | **999** |
| auto-reload preload | Enable |
| PWM Generation Channel 1 → Mode | PWM mode 1 |
| Pulse | **0** |
| CH Polarity | High |

PA8에 User Label `MOTOR_PWM`을 붙입니다(선택).

### 3-8. Timers → TIM3 (부저 PWM)
| 항목 | 값 |
|---|---|
| Clock Source | Internal Clock |
| Channel1 | **PWM Generation CH1** → 핀 **PA6** 확인 |
| Prescaler | **89** |
| Counter Period | **499** |
| auto-reload preload | Enable |
| Mode | PWM mode 1 |
| Pulse | **500** ← ARR보다 큰 값이라 출력이 항상 HIGH = 무음 (active-low 모듈 기준). CubeMX가 이 값을 받지 않으면 0으로 두세요. 코드가 시작할 때 무음 값으로 다시 씁니다. |
| CH Polarity | High |

PA6에 User Label `BUZZER`(선택).

### 3-9. Analog → ADC1 (가변저항)
- **IN0** 체크 → 핀 **PA0** 확인 (User Label `POT`, 선택)

공통 Parameter Settings:

| 항목 | 값 |
|---|---|
| Clock Prescaler | **PCLK2 divided by 4** (22.5 MHz) |
| Resolution | 12 bits |
| Data Alignment | Right alignment |
| Scan Conversion Mode | Disabled |
| End Of Conversion Selection | EOC flag at the end of single channel conversion |
| Rank 1 → Channel | Channel 0 |
| Rank 1 → Sampling Time | **480 Cycles** |

**Continuous Conversion Mode**와 **DMA Continuous Requests**는 버전마다 다릅니다(4장, 5장).

### 3-10. Project Manager
| 항목 | 값 |
|---|---|
| Toolchain / IDE | STM32CubeIDE |
| Code Generator → Generate peripheral initialization as a pair of '.c/.h' files per peripheral | **체크 해제** (모든 핸들이 `main.c`에 있어야 아래 코드가 그대로 동작) |

---

## 4. CPU 버전

### 4-1. CubeMX 추가 설정 (`Parking_CPU`)

**① 초음파 핀은 그냥 GPIO**

| 핀 | 모드 | User Label | GPIO 설정 |
|---|---|---|---|
| PB8 | GPIO_Output | `TRIG` | Push Pull, No pull, Low |
| PB6 | GPIO_Input | `ECHO` | No pull |

**② TIM4 = 1 µs 자유 카운터** (시간 측정, `delay_us`용)

| 항목 | 값 |
|---|---|
| Clock Source | Internal Clock |
| Channel1~4 | Disable |
| Prescaler | **89** |
| Counter Period | **65535** |

**③ ADC1**

| 항목 | 값 |
|---|---|
| Continuous Conversion Mode | **Disabled** |
| DMA Continuous Requests | **Disabled** |

**④ DMA는 아무것도 추가하지 않습니다.**

**⑤ 코드 생성**: `Ctrl + S` → "Do you want to generate Code?" → Yes

### 4-2. 생성된 코드 확인
`Core/Src/main.c`에 다음 핸들이 있어야 합니다:
`hadc1`, `htim1`, `htim3`, `htim4`, `huart2`

그리고 `main.h`에 `LED_G_Pin`, `TRIG_Pin`, `ECHO_GPIO_Port` 같은 매크로가 있어야 합니다(User Label에서 만들어짐).

### 4-3. 코드 (`main.c`의 USER CODE 영역에 붙여넣기)

> ⚠ 반드시 `/* USER CODE BEGIN xxx */` ~ `/* USER CODE END xxx */` **사이**에 넣으세요. 그 밖에 쓰면 CubeMX로 코드를 다시 생성할 때 지워집니다.

#### ① `USER CODE BEGIN Includes`
```c
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */
```

#### ② `USER CODE BEGIN PTD`
```c
/* USER CODE BEGIN PTD */
typedef enum { ZONE_SAFE = 0, ZONE_MID, ZONE_NEAR, ZONE_CRITICAL } zone_t;
/* USER CODE END PTD */
```

#### ③ `USER CODE BEGIN PD`
```c
/* USER CODE BEGIN PD */
/* ---- 부저: TIM3 CH1, 1MHz 카운터, ARR=499 → 2kHz ---- */
#define BUZZER_ACTIVE_LOW   1          /* MH-FMD: LOW일 때 울림. HIGH일 때 울리는 모듈이면 0 */
#define BUZZER_ARR          499U
#define BUZZER_ON_CCR       ((BUZZER_ARR + 1U) / 2U)   /* 듀티 50% → 소리 남 */
#if BUZZER_ACTIVE_LOW
#define BUZZER_OFF_CCR      (BUZZER_ARR + 1U)          /* CCR > ARR → 항상 HIGH → 무음 */
#else
#define BUZZER_OFF_CCR      0U                         /* 항상 LOW → 무음 */
#endif
#define BEEP_SLOT_MS        50U        /* 패턴 한 칸 = 50ms */
#define BEEP_SLOTS          20U        /* 20칸 = 1초 주기 */

/* ---- 거리 구간 (mm) ---- */
#define DIST_MID_MM         400
#define DIST_NEAR_MM        200
#define DIST_CRITICAL_MM    100

/* ---- 모터: TIM1 CH1, 1MHz 카운터, ARR=999 → 1kHz ---- */
#define MOTOR_PWM_MAX       999U
#define MOTOR_DEADZONE      150U       /* 이보다 작은 듀티는 모터가 돌지 못하므로 0으로 */

/* ---- 초음파 ---- */
#define ULTRA_PERIOD_MS     60U        /* 측정 간격 (센서 권장 60ms 이상) */

/* ---- 로그와 부하 조절 (6장 실험에서 바꿔 봅니다) ---- */
#define REPORT_PERIOD_MS    200U       /* 로그 출력 간격 */
#define LOG_REPEAT          4          /* 로그 줄 반복 수. 늘릴수록 UART 전송이 길어짐 */
#define EXTRA_WORK_MS       0U         /* 루프마다 추가하는 '무거운 작업' 시간 (예: 화면 그리기 흉내) */
/* USER CODE END PD */
```

#### ④ `USER CODE BEGIN PV`
```c
/* USER CODE BEGIN PV */
static volatile uint8_t estop = 0;          /* 버튼1: 비상 정지 (토글) */
static volatile uint8_t mute  = 0;          /* 버튼2: 음소거 (토글) */
static uint32_t beep_pattern[BEEP_SLOTS];   /* 칸마다 TIM3->CCR1에 쓸 값 */
static zone_t   cur_zone = ZONE_SAFE;
static uint8_t  cur_mute = 0;
static char     tx_buf[1024];
/* USER CODE END PV */
```

#### ⑤ `USER CODE BEGIN PFP`
```c
/* USER CODE BEGIN PFP */
static void     delay_us(uint16_t us);
static int32_t  ultrasonic_read_mm(void);
static uint16_t pot_read(void);
static zone_t   get_zone(int32_t mm);
static void     leds_update(zone_t z);
static void     motor_update(uint16_t pedal, zone_t z);
static void     beep_build_pattern(zone_t z, uint8_t muted);
static int      make_report(int32_t dist_mm, uint16_t pedal, zone_t z, uint32_t loops_per_sec);
/* USER CODE END PFP */
```

#### ⑥ `USER CODE BEGIN 2` (모든 `MX_xxx_Init()` 호출 바로 뒤)
```c
  /* USER CODE BEGIN 2 */
  HAL_TIM_Base_Start(&htim4);                        /* 1MHz 자유 카운터 시작 */

  beep_build_pattern(ZONE_SAFE, 0);
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, BUZZER_OFF_CCR);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);          /* 부저 PWM 시작 (처음엔 무음) */

  HAL_GPIO_WritePin(MOTOR_IN1_GPIO_Port, MOTOR_IN1_Pin, GPIO_PIN_SET);    /* 회전 방향 고정 */
  HAL_GPIO_WritePin(MOTOR_IN2_GPIO_Port, MOTOR_IN2_Pin, GPIO_PIN_RESET);
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);          /* 모터 PWM 시작 (처음엔 정지) */

  const char *hello = "\r\n===== Parking assist: CPU version =====\r\n";
  HAL_UART_Transmit(&huart2, (uint8_t *)hello, strlen(hello), 100);

  uint32_t loops = 0, loops_per_sec = 0;
  uint32_t last_sec = HAL_GetTick();
  uint32_t last_report = HAL_GetTick();
  uint32_t last_measure = 0;
  int32_t  dist_mm = -1;
  /* USER CODE END 2 */
```

#### ⑦ `USER CODE BEGIN 3` (while 루프 안)

생성된 코드는 다음 모양입니다. `USER CODE BEGIN 3`과 `END 3` 사이에 넣으세요.
```c
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint32_t now = HAL_GetTick();

    /* (1) 초음파: 60ms마다. 에코가 끝날 때까지 CPU가 여기서 기다림 */
    if (now - last_measure >= ULTRA_PERIOD_MS) {
      last_measure = now;
      dist_mm = ultrasonic_read_mm();
    }

    /* (2) 가변저항: 1번 변환하고 끝날 때까지 기다림 */
    uint16_t pedal = pot_read();

    /* (3) 판단: 구간, LED, 모터 */
    zone_t zone = get_zone(dist_mm);
    leds_update(zone);
    motor_update(pedal, zone);

    /* (4) 경고음: 패턴이 바뀌면 표를 다시 만들고,
           '지금 시각에 해당하는 칸'의 값을 CPU가 직접 TIM3에 씀.
           → 루프가 늦게 돌면 칸이 늦게 바뀌어 박자가 흐트러짐 */
    now = HAL_GetTick();
    if (zone != cur_zone || mute != cur_mute) {
      cur_zone = zone;
      cur_mute = mute;
      beep_build_pattern(cur_zone, cur_mute);
    }
    uint32_t slot = (now / BEEP_SLOT_MS) % BEEP_SLOTS;
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, beep_pattern[slot]);

    /* (5) 로그: 전송이 끝날 때까지 기다림 (블로킹) */
    if (now - last_report >= REPORT_PERIOD_MS) {
      last_report = now;
      int n = make_report(dist_mm, pedal, zone, loops_per_sec);
      HAL_UART_Transmit(&huart2, (uint8_t *)tx_buf, (uint16_t)n, 1000);
    }

    /* (6) 무거운 작업 흉내 (6장 실험용) */
    if (EXTRA_WORK_MS > 0U) {
      HAL_Delay(EXTRA_WORK_MS);
    }

    /* (7) 루프 속도 측정: 1초 동안 몇 번 돌았는지 */
    loops++;
    if (now - last_sec >= 1000U) {
      loops_per_sec = loops;
      loops = 0;
      last_sec = now;
    }
    HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);       /* 루프가 느리면 LD2가 눈에 보이게 깜빡임 */
  }
  /* USER CODE END 3 */
```

#### ⑧ `USER CODE BEGIN 4` (main 함수 아래, 함수 정의)
```c
/* USER CODE BEGIN 4 */

/* ===== 시간 ===== */
static void delay_us(uint16_t us)
{
  uint16_t start = (uint16_t)__HAL_TIM_GET_COUNTER(&htim4);
  while ((uint16_t)(__HAL_TIM_GET_COUNTER(&htim4) - start) < us) { }
}

/* ===== 초음파 (CPU가 직접 펄스를 만들고 기다리며 잼) ===== */
static int32_t ultrasonic_read_mm(void)
{
  /* TRIG: 10us HIGH 펄스 */
  HAL_GPIO_WritePin(TRIG_GPIO_Port, TRIG_Pin, GPIO_PIN_SET);
  delay_us(10);
  HAL_GPIO_WritePin(TRIG_GPIO_Port, TRIG_Pin, GPIO_PIN_RESET);

  /* ECHO가 HIGH가 될 때까지 기다림 (최대 30ms) */
  uint16_t t0 = (uint16_t)__HAL_TIM_GET_COUNTER(&htim4);
  while (HAL_GPIO_ReadPin(ECHO_GPIO_Port, ECHO_Pin) == GPIO_PIN_RESET) {
    if ((uint16_t)(__HAL_TIM_GET_COUNTER(&htim4) - t0) > 30000U) return -1;
  }

  /* ECHO가 LOW가 될 때까지 기다리며 시간을 잼 (최대 30ms) */
  uint16_t rise = (uint16_t)__HAL_TIM_GET_COUNTER(&htim4);
  while (HAL_GPIO_ReadPin(ECHO_GPIO_Port, ECHO_Pin) == GPIO_PIN_SET) {
    if ((uint16_t)(__HAL_TIM_GET_COUNTER(&htim4) - rise) > 30000U) return -1;
  }
  uint16_t width_us = (uint16_t)(__HAL_TIM_GET_COUNTER(&htim4) - rise);

  /* 소리는 왕복하므로 1cm ≈ 58us → mm = us * 10 / 58 */
  return (int32_t)width_us * 10 / 58;
}

/* ===== 가변저항 (폴링) ===== */
static uint16_t pot_read(void)
{
  HAL_ADC_Start(&hadc1);
  HAL_ADC_PollForConversion(&hadc1, 10);
  uint16_t v = (uint16_t)HAL_ADC_GetValue(&hadc1);
  HAL_ADC_Stop(&hadc1);
  return v;
}

/* ===== 공통 로직 (DMA 버전과 똑같음) ===== */
static zone_t get_zone(int32_t mm)
{
  if (mm < 0)                return ZONE_SAFE;      /* 에코 없음 = 장애물이 멀다 */
  if (mm < DIST_CRITICAL_MM) return ZONE_CRITICAL;
  if (mm < DIST_NEAR_MM)     return ZONE_NEAR;
  if (mm < DIST_MID_MM)      return ZONE_MID;
  return ZONE_SAFE;
}

static void leds_update(zone_t z)
{
  HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin,
                    (z == ZONE_SAFE) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LED_Y_GPIO_Port, LED_Y_Pin,
                    (z == ZONE_MID || z == ZONE_NEAR) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin,
                    (z == ZONE_NEAR || z == ZONE_CRITICAL || estop) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void motor_update(uint16_t pedal, zone_t z)
{
  static const uint8_t limit_pct[] = { 100, 60, 30, 0 };   /* SAFE, MID, NEAR, CRITICAL */
  uint32_t duty = (uint32_t)pedal * MOTOR_PWM_MAX / 4095U; /* 12bit ADC → 0~999 */
  duty = duty * limit_pct[z] / 100U;                       /* 거리에 따라 속도 제한 */
  if (estop || duty < MOTOR_DEADZONE) {
    duty = 0;
  }
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, duty);
}

static void beep_build_pattern(zone_t z, uint8_t muted)
{
  for (uint32_t i = 0; i < BEEP_SLOTS; i++) {
    uint8_t on;
    switch (z) {
      case ZONE_MID:      on = (i % 10U) < 2U; break;   /* 100ms 울림 / 400ms 쉼 */
      case ZONE_NEAR:     on = (i % 5U)  < 2U; break;   /* 100ms 울림 / 150ms 쉼 */
      case ZONE_CRITICAL: on = 1;              break;   /* 연속음 */
      default:            on = 0;              break;   /* 안전: 무음 */
    }
    beep_pattern[i] = (on && !muted) ? BUZZER_ON_CCR : BUZZER_OFF_CCR;
  }
}

static int make_report(int32_t dist_mm, uint16_t pedal, zone_t z, uint32_t loops_per_sec)
{
  static const char *zname[] = { "SAFE", "MID", "NEAR", "CRIT" };
  int n = 0;
  for (int i = 0; i < LOG_REPEAT; i++) {
    int w = snprintf(tx_buf + n, sizeof(tx_buf) - (size_t)n,
                     "[%d] dist=%5ld mm pedal=%4u zone=%-4s duty=%3lu estop=%u mute=%u loop/s=%lu\r\n",
                     i, (long)dist_mm, pedal, zname[z],
                     (unsigned long)__HAL_TIM_GET_COMPARE(&htim1, TIM_CHANNEL_1),
                     estop, mute, (unsigned long)loops_per_sec);
    if (w < 0 || n + w >= (int)sizeof(tx_buf)) {    /* 버퍼가 차면 멈춤 */
      n = (int)strlen(tx_buf);
      break;
    }
    n += w;
  }
  return n;
}

/* ===== 버튼 (EXTI 인터럽트) ===== */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  static uint32_t last_estop = 0, last_mute = 0;
  uint32_t now = HAL_GetTick();

  if (GPIO_Pin == BTN_ESTOP_Pin && now - last_estop > 200U) {   /* 200ms 채터링 무시 */
    last_estop = now;
    estop = !estop;
    if (estop) {
      __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);         /* 루프를 기다리지 않고 즉시 정지 */
    }
  }
  if (GPIO_Pin == BTN_MUTE_Pin && now - last_mute > 200U) {
    last_mute = now;
    mute = !mute;
  }
}

/* USER CODE END 4 */
```

> 버튼은 두 버전 모두 **EXTI 인터럽트**를 씁니다. 비상 정지는 안전 기능이라, CPU 버전에서도 루프가 막혀 있을 때 바로 반응해야 하기 때문입니다.

### 4-4. 빌드와 실행
1. **Project > Build Project** (망치 아이콘). 에러 없이 끝나는지 확인합니다.
2. **Run > Debug As > STM32 C/C++ Application** → 디버그 시작 → Resume(F8)
3. PC에서 시리얼 터미널(Tera Term, PuTTY, CubeIDE 내장 터미널 등)을 열고, ST-LINK의 COM 포트에 **115200, 8N1**로 연결합니다.
4. 다음 로그가 보이면 성공입니다.
   ```
   ===== Parking assist: CPU version =====
   [0] dist=  532 mm pedal=1830 zone=SAFE duty=447 estop=0 mute=0 loop/s=...
   ```

---

## 5. DMA 버전

### 5-1. CubeMX 추가 설정 (`Parking_DMA`)

3장의 공통 설정을 먼저 마친 뒤, 아래를 추가합니다.

#### ① TIM4 = 초음파 자동 측정기
| 항목 | 값 |
|---|---|
| Clock Source | Internal Clock |
| Channel1 | **Input Capture direct mode** → 핀 **PB6** |
| Channel2 | **Input Capture indirect mode** (PB6 신호를 CH2에서도 받음, 별도 핀 없음) |
| Channel3 | **PWM Generation CH3** → 핀 **PB8** |
| Prescaler | **89** |
| Counter Period | **59999** (60ms) |
| Input Capture Channel 1 → Polarity Selection | **Rising Edge** |
| Input Capture Channel 1 → IC Selection | Direct |
| Input Capture Channel 2 → Polarity Selection | **Falling Edge** |
| Input Capture Channel 2 → IC Selection | Indirect |
| Input Filter (CH1, CH2) | 0 (노이즈가 심하면 3 정도로) |
| PWM Generation Channel 3 → Mode | PWM mode 1 |
| PWM Generation Channel 3 → Pulse | **10** (= 10µs TRIG 펄스) |

**어떻게 동작하나**
```
TIM4 카운터  0 ──────────────────────────────── 59999 → 0 (60ms마다 반복)
CH3 (TRIG)   ‾|_________________________________   ← 0~9us 동안 HIGH (자동)
ECHO (PB6)   ______/‾‾‾‾‾‾‾‾‾‾‾\________________
                   ↑            ↑
              CH1 캡처(상승)   CH2 캡처(하강)  → 각각 DMA가 변수에 저장
거리 = (하강 - 상승) us × 10 / 58  [mm]
```
- CH2의 "indirect"는 **CH1의 입력 핀(TI1) 신호를 CH2에서도 받는다**는 뜻입니다. 그래서 핀 하나로 상승과 하강을 따로 캡처할 수 있습니다.

#### ② TIM6 = 경고음 박자 (50ms)
| 항목 | 값 |
|---|---|
| Activated | ☑ |
| Prescaler | **8999** |
| Counter Period | **499** |

#### ③ ADC1
| 항목 | 값 |
|---|---|
| Continuous Conversion Mode | **Enabled** |
| DMA Continuous Requests | **Enabled** ← 마지막 전송 후에도 계속 DMA 요청을 냄 (`ADC_CR2.DDS = 1`) 〔RM p.382〕 |

#### ④ System Core → DMA

**DMA1** 탭에서 **Add**를 눌러 하나씩 추가합니다. ADC는 **DMA2** 탭에서 추가합니다.

| DMA Request | Stream | Direction | Priority | Mode | Increment (Peripheral / Memory) | Data Width (Peripheral / Memory) |
|---|---|---|---|---|---|---|
| **ADC1** (DMA2 탭) | DMA2 Stream0 | Peripheral To Memory | Medium | **Circular** | ☐ / ☑ | Half Word / Half Word |
| **TIM4_CH1** | DMA1 Stream0 | Peripheral To Memory | High | **Circular** | ☐ / **☐** | Word / Word |
| **TIM4_CH2** | DMA1 Stream3 | Peripheral To Memory | High | **Circular** | ☐ / **☐** | Word / Word |
| **TIM6_UP** | DMA1 Stream1 | **Memory To Peripheral** | Medium | **Circular** | ☐ / ☑ | Word / Word |
| **USART2_TX** | DMA1 Stream6 | Memory To Peripheral | Low | **Normal** | ☐ / ☑ | Byte / Byte |

- 모든 항목의 **Use Fifo**는 체크하지 않습니다 (Direct 모드). 양쪽 데이터 폭이 같아서 FIFO가 필요 없습니다. 〔RM p.211, p.216〕
- TIM4_CH1/CH2는 변수 **하나**에 계속 덮어쓰므로 Memory Increment를 **끕니다**.
- 스트림 번호가 표와 다르게 잡히면 드롭다운에서 바꾸세요. 〔RM p.204〕
- TIM6_UP 요청이 목록에 없으면, TIM6의 **Activated**가 체크되어 있는지 확인하세요.

#### ⑤ NVIC
**System Core → NVIC**에서:
- ☑ **USART2 global interrupt** ← DMA 송신이 끝났을 때 HAL이 상태를 "준비됨"으로 되돌리려면 필요합니다.
- DMA 스트림 인터럽트는 CubeMX가 자동으로 켜 둡니다(해제 불가). 그대로 둡니다.
- EXTI line0, line1은 3장에서 이미 켰습니다.

#### ⑥ 코드 생성
`Ctrl + S` → Yes

### 5-2. 생성된 코드 확인
`main.c`에 다음이 있어야 합니다.
- 핸들: `hadc1`, `htim1`, `htim3`, `htim4`, `htim6`, `huart2`
- DMA 핸들: `hdma_adc1`, `hdma_tim4_ch1`, `hdma_tim4_ch2`, `hdma_tim6_up`, `hdma_usart2_tx`
- ⚠ `main()` 안에서 **`MX_DMA_Init()`이 `MX_ADC1_Init()`, `MX_TIM4_Init()` 등보다 먼저** 호출되는지 확인하세요. 일부 CubeMX 버전은 순서를 거꾸로 생성하는데, 그러면 DMA가 동작하지 않습니다. 순서가 틀렸다면 Project Manager → Advanced Settings에서 순서를 바꾸세요.

### 5-3. 코드 (`main.c`의 USER CODE 영역)

#### ① `USER CODE BEGIN Includes`
```c
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */
```

#### ② `USER CODE BEGIN PTD`
```c
/* USER CODE BEGIN PTD */
typedef enum { ZONE_SAFE = 0, ZONE_MID, ZONE_NEAR, ZONE_CRITICAL } zone_t;
/* USER CODE END PTD */
```

#### ③ `USER CODE BEGIN PD`
CPU 버전의 ③과 **똑같이** 붙여 넣고, 맨 아래에 두 줄을 추가합니다.
```c
/* USER CODE BEGIN PD */
/* ... CPU 버전 ③의 내용 전부 ... */

#define ADC_BUF_LEN         64U        /* DMA가 채우는 가변저항 샘플 수 */
#define ULTRA_STALE_MS      200U       /* 이 시간 동안 측정값 갱신이 없으면 센서 이상 */
/* USER CODE END PD */
```
(`ULTRA_PERIOD_MS`는 DMA 버전에서 쓰지 않습니다. 측정 주기는 TIM4의 ARR이 정합니다.)

#### ④ `USER CODE BEGIN PV`
```c
/* USER CODE BEGIN PV */
static volatile uint8_t estop = 0;          /* 버튼1: 비상 정지 */
static volatile uint8_t mute  = 0;          /* 버튼2: 음소거 */
static uint32_t beep_pattern[BEEP_SLOTS];   /* DMA가 50ms마다 한 칸씩 TIM3->CCR1로 보냄 */
static zone_t   cur_zone = ZONE_SAFE;
static uint8_t  cur_mute = 0;
static char     tx_buf[1024];               /* UART DMA 송신 버퍼 (전송 중에는 건드리지 않음) */

static uint16_t adc_buf[ADC_BUF_LEN];       /* ADC1 → DMA2 Stream0 (Circular) */
static uint32_t echo_rise;                  /* TIM4 CCR1 → DMA1 Stream0 (상승 시각) */
static uint32_t echo_fall;                  /* TIM4 CCR2 → DMA1 Stream3 (하강 시각) */
static volatile int32_t  dist_mm   = -1;    /* 캡처 콜백에서 계산 */
static volatile uint32_t dist_tick = 0;     /* 마지막으로 계산한 시각 */
/* USER CODE END PV */
```

#### ⑤ `USER CODE BEGIN PFP`
```c
/* USER CODE BEGIN PFP */
static uint16_t pot_read(void);
static zone_t   get_zone(int32_t mm);
static void     leds_update(zone_t z);
static void     motor_update(uint16_t pedal, zone_t z);
static void     beep_build_pattern(zone_t z, uint8_t muted);
static int      make_report(int32_t dist_mm, uint16_t pedal, zone_t z, uint32_t loops_per_sec);
/* USER CODE END PFP */
```

#### ⑥ `USER CODE BEGIN 2`
```c
  /* USER CODE BEGIN 2 */

  /* (1) 가변저항: ADC 연속 변환 → DMA2 Stream0 → adc_buf[64] (Circular, 끝없이 반복) */
  HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_buf, ADC_BUF_LEN);
  /* HAL은 절반/완료 인터럽트를 켜 두지만 여기서는 콜백이 필요 없으므로 끈다.
     → 이후 ADC 쪽에서는 CPU 인터럽트가 전혀 발생하지 않음 */
  __HAL_DMA_DISABLE_IT(&hdma_adc1, DMA_IT_HT | DMA_IT_TC);

  /* (2) 초음파: 캡처 DMA를 먼저 켜고, 그다음 TRIG 펄스(CH3)를 시작 */
  HAL_TIM_IC_Start_DMA(&htim4, TIM_CHANNEL_1, &echo_rise, 1);   /* 상승 시각 → echo_rise */
  HAL_TIM_IC_Start_DMA(&htim4, TIM_CHANNEL_2, &echo_fall, 1);   /* 하강 시각 → echo_fall */
  HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_3);                     /* 60ms마다 10us TRIG */

  /* (3) 부저: TIM3 PWM 시작(무음) → DMA 준비 → TIM6 업데이트마다 DMA 요청 */
  beep_build_pattern(ZONE_SAFE, 0);
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, BUZZER_OFF_CCR);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
  HAL_DMA_Start(&hdma_tim6_up,
                (uint32_t)beep_pattern,               /* 소스: 메모리의 패턴 표 */
                (uint32_t)&htim3.Instance->CCR1,      /* 목적지: TIM3의 비교 레지스터 */
                BEEP_SLOTS);
  __HAL_TIM_ENABLE_DMA(&htim6, TIM_DMA_UPDATE);       /* TIM6 업데이트 → DMA 요청 */
  HAL_TIM_Base_Start(&htim6);                         /* 50ms 박자 시작 */

  /* (4) 모터 */
  HAL_GPIO_WritePin(MOTOR_IN1_GPIO_Port, MOTOR_IN1_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(MOTOR_IN2_GPIO_Port, MOTOR_IN2_Pin, GPIO_PIN_RESET);
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);

  const char *hello = "\r\n===== Parking assist: DMA version =====\r\n";
  HAL_UART_Transmit(&huart2, (uint8_t *)hello, strlen(hello), 100);   /* 시작 메시지만 블로킹 */

  uint32_t loops = 0, loops_per_sec = 0;
  uint32_t last_sec = HAL_GetTick();
  uint32_t last_report = HAL_GetTick();
  /* USER CODE END 2 */
```

> `HAL_DMA_Start()`는 **인터럽트를 켜지 않는** 시작 함수입니다. 그래서 경고음 재생 중에는 CPU 인터럽트가 **한 번도** 발생하지 않습니다. 순수하게 TIM6 → DMA1 → TIM3만 동작합니다.

#### ⑦ `USER CODE BEGIN 3`
```c
    /* USER CODE BEGIN 3 */
    uint32_t now = HAL_GetTick();

    /* (1) 초음파: DMA와 캡처 콜백이 이미 계산해 둔 값을 읽기만 함 (기다림 없음) */
    int32_t d = dist_mm;
    if (now - dist_tick > ULTRA_STALE_MS) {
      d = -1;                                          /* 갱신이 끊김 = 센서 이상 → '멀다'로 처리 */
    }

    /* (2) 가변저항: DMA가 채워 둔 64개의 평균 (기다림 없음, 노이즈도 줄어듦) */
    uint16_t pedal = pot_read();

    /* (3) 판단: CPU 버전과 똑같음 */
    zone_t zone = get_zone(d);
    leds_update(zone);
    motor_update(pedal, zone);

    /* (4) 경고음: 패턴이 바뀔 때만 표 내용을 다시 씀. 재생(박자 맞추기)은 DMA가 함 */
    if (zone != cur_zone || mute != cur_mute) {
      cur_zone = zone;
      cur_mute = mute;
      beep_build_pattern(cur_zone, cur_mute);
    }

    /* (5) 로그: 이전 전송이 끝났을 때만 새로 만들어 DMA로 보냄 (기다리지 않음) */
    if (now - last_report >= REPORT_PERIOD_MS && huart2.gState == HAL_UART_STATE_READY) {
      last_report = now;
      int n = make_report(d, pedal, zone, loops_per_sec);
      HAL_UART_Transmit_DMA(&huart2, (uint8_t *)tx_buf, (uint16_t)n);
    }

    /* (6) 무거운 작업 흉내 (6장 실험용) */
    if (EXTRA_WORK_MS > 0U) {
      HAL_Delay(EXTRA_WORK_MS);
    }

    /* (7) 루프 속도 측정 */
    loops++;
    if (now - last_sec >= 1000U) {
      loops_per_sec = loops;
      loops = 0;
      last_sec = now;
    }
    HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
  }
  /* USER CODE END 3 */
```

#### ⑧ `USER CODE BEGIN 4`
```c
/* USER CODE BEGIN 4 */

/* ===== 가변저항: DMA가 채운 버퍼의 평균 ===== */
static uint16_t pot_read(void)
{
  uint32_t sum = 0;
  for (uint32_t i = 0; i < ADC_BUF_LEN; i++) {
    sum += adc_buf[i];
  }
  return (uint16_t)(sum / ADC_BUF_LEN);
}

/* ===== 초음파: CH2(하강) 캡처가 DMA로 저장될 때마다 호출됨 (60ms에 한 번) ===== */
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM4 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2) {
    /* 같은 60ms 주기 안에서 상승(CCR1) → 하강(CCR2) 순서로 캡처됨 */
    if (echo_fall > echo_rise) {
      uint32_t width_us = echo_fall - echo_rise;
      dist_mm = (width_us < 30000U) ? (int32_t)(width_us * 10U / 58U) : -1;   /* 30ms 이상 = 장애물 없음 */
    }
    dist_tick = HAL_GetTick();
  }
}

/* ===== 공통 로직: CPU 버전 ⑧의 아래 함수들을 그대로 복사 =====
   get_zone(), leds_update(), motor_update(), beep_build_pattern(),
   make_report(), HAL_GPIO_EXTI_Callback()
   (delay_us(), ultrasonic_read_mm(), CPU용 pot_read()는 복사하지 않음) */

/* USER CODE END 4 */
```

> 💡 솔직한 메모: 초음파는 DMA 없이 **입력 캡처 인터럽트만으로도** 기다림 없이 측정할 수 있습니다. 여기서 DMA를 쓴 이유는 "캡처 값 옮기기"를 DMA로 해 보는 연습을 하기 위해서입니다. 이 프로젝트에서 DMA가 **가장 확실하게** 차이를 만드는 곳은 **경고음(박자)**, **가변저항(연속 샘플링)**, **UART 로그** 세 곳입니다.

### 5-4. 빌드와 실행
CPU 버전과 같습니다. 터미널에 `===== Parking assist: DMA version =====`이 보이면 성공입니다.

---

## 6. 비교 실험 방법

두 보드를 따로 쓸 수 없다면, 같은 보드에 두 프로젝트를 번갈아 올리며 비교합니다.

### 6-1. 관찰 포인트

| 관찰 대상 | 무엇을 보나 |
|---|---|
| **부저 박자** | "삐-삐-삐" 간격이 일정한가 |
| **페달 반응** | 가변저항을 돌렸을 때 모터가 즉시, 부드럽게 반응하는가 |
| **로그의 `loop/s`** | 메인 루프가 1초에 몇 번 도는가 = CPU 여유 |
| **LD2** | 루프가 느리면 **깜빡임이 보이고**, 빠르면 **희미하게 켜진 것처럼** 보임 |
| **로그의 `pedal`** | 손을 대지 않았을 때 값이 얼마나 흔들리는가 (노이즈) |

### 6-2. 실험 순서 (두 버전 모두 같은 값으로)

`#define` 값만 바꿔 다시 빌드하면서 진행합니다.

| 단계 | `LOG_REPEAT` | `EXTRA_WORK_MS` | 장애물 거리 | 예상 결과 |
|---|---|---|---|---|
| ① 기본 | 1 | 0 | 15~20cm (NEAR) | 두 버전 모두 거의 정상. CPU 버전도 부하가 작으면 괜찮음 |
| ② 로그 부하 | **10** | 0 | 15~20cm | **CPU**: 로그를 보내는 순간(약 70ms)마다 박자가 **한 번씩 늘어짐**. `loop/s` 감소. **DMA**: 변화 없음 |
| ③ 무거운 작업 | 4 | **80** | 15~20cm | **CPU**: 50ms 칸을 놓쳐 **박자가 완전히 엉망**. 페달 반응이 둔해짐. **DMA**: 경고음 박자는 **완벽하게 유지**됨 (모터/LED 갱신은 둘 다 느려짐) |
| ④ 노이즈 | 1 | 0 | – | 가변저항을 가만히 두고 `pedal` 값의 흔들림 비교. **DMA**(64개 평균)가 훨씬 안정적 |

- ③에서 DMA 버전도 모터와 LED는 느려집니다. 그 갱신은 CPU가 하기 때문입니다. **"DMA로 옮긴 일만 CPU 부하와 무관해진다"**는 것을 정확히 보여 주는 결과입니다.
- `LOG_REPEAT=10`이면 약 850바이트를 보냅니다. 115200bps(초당 약 11,520바이트)에서는 **약 74ms** 동안 CPU 버전이 멈춥니다.

### 6-3. 보너스: 디버거로 CPU를 멈춰 보기 🎭
DMA 버전을 실행하다가 CubeIDE에서 **Suspend(일시정지)**를 눌러 보세요.
- CPU는 멈췄는데 **경고음은 계속 같은 박자로** 울릴 수 있습니다. 디버그 중 타이머를 멈출지는 `DBGMCU_APB1_FZ`의 `DBG_TIMx_STOP` 비트가 정합니다. 〔RM p.1307–1308〕
- ⚠ **모터도 마지막 속도로 계속 돕니다.** CPU가 멈춰서 **비상 정지 버튼도 동작하지 않습니다.** 이 실험은 **모터 전원을 뺀 상태**에서 하세요.

---

## 7. 문제 해결

| 증상 | 확인할 것 |
|---|---|
| 빌드 에러: `LED_G_Pin` 등이 없음 | CubeMX의 User Label 철자. 코드 생성을 다시 했는지 |
| 빌드 에러: `hdma_tim6_up` 없음 | DMA 탭에서 TIM6_UP을 추가했는지. 핸들 이름은 생성된 `main.c` 위쪽에서 확인 |
| 터미널에 아무것도 안 나옴 | COM 포트 번호, 115200 설정, USART2 핀이 PA2/PA3인지 |
| 부저가 계속 울림 / 안 울림 | `BUZZER_ACTIVE_LOW` 값을 반대로 바꿔 보기 |
| 부저 소리가 작거나 이상함 | 패시브 부저는 2~4kHz에서 소리가 큼. TIM3 ARR을 바꿔 주파수 조절 (`1MHz / (ARR+1)`) |
| 거리가 항상 -1 | 센서 5V 전원, TRIG/ECHO 배선. DMA 버전은 TIM4 CH1=PB6, CH3=PB8 핀 배정 확인 |
| 거리가 이상하게 튐 | 센서 앞 2cm 이내는 측정 불가. TIM4 Input Filter를 3 정도로 |
| DMA 버전에서 거리가 항상 -1 (배선은 정상) | 오래된 STM32CubeF4 펌웨어 패키지의 HAL은 같은 타이머의 두 번째 채널에 `HAL_TIM_IC_Start_DMA()`를 호출하면 `HAL_BUSY`를 돌려줄 수 있습니다. 반환값을 확인하고, Help → Manage Embedded Software Packages에서 F4 패키지를 최신으로 업데이트하세요 |
| 모터가 안 돎 | L298N **ENA 점퍼 제거**, 외부 전원, **GND 공통**, `MOTOR_DEADZONE` 값 낮추기 |
| 모터가 반대로 돎 | `MOTOR_IN1`/`MOTOR_IN2` 출력 값을 서로 바꾸기 |
| DMA 버전에서 `pedal`이 0에서 안 변함 | `MX_DMA_Init()` 호출 순서, ADC의 DMA Continuous Requests = Enabled |
| DMA 버전 로그가 첫 줄 이후 안 나옴 | NVIC에서 **USART2 global interrupt** 켰는지 |
| 버튼이 한 번에 두 번 눌림 | 채터링. 콜백의 200ms 값을 늘리기 |
| 디버거 연결이 안 됨 | SYS → Debug = Serial Wire 설정 누락. 보드의 RESET을 누른 채 연결 시도 |

---

## 8. 심화 과제

1. **모터 부드러운 출발 (DMA2)**
   페달을 갑자기 밟아도 듀티가 서서히 올라가도록, **램프 표**를 TIM1 업데이트마다 DMA로 `TIM1->CCR1`에 씁니다.
   - 요청: TIM1_UP → **DMA2 Stream5 Channel6** 〔RM p.204〕
   - TIM1은 APB2 타이머라서 DMA2를 써야 합니다.
   - 생각할 점: `motor_update()`도 CCR1에 쓰고 있어서 둘이 충돌합니다. 램프 중에는 누가 CCR1을 쓸지 정해야 합니다.

2. **빨간 LED 숨쉬기 효과**
   빨간 LED를 PWM 핀으로 옮기고, 밝기 표(사인파)를 타이머 업데이트 DMA로 재생합니다.
   - 핀과 타이머를 고를 때 RM p.204에서 **스트림 충돌**이 없는지 직접 확인해 보세요.

3. **멜로디 알림**
   출발할 때 "학교종" 멜로디를 재생합니다. TIM3의 **ARR**(음 높이)을 DMA로 바꾸는 방식입니다. 이때 CCR도 ARR의 절반으로 같이 바뀌어야 하는 문제를 어떻게 풀지 생각해 보세요.
   - 힌트: 출력 비교 **Toggle 모드**를 쓰면 CCR을 바꾸지 않아도 듀티가 50%로 유지됩니다.

4. **CPU 부하율 측정**
   `loop/s`를 "부하 없음" 기준값과 비교해 CPU 사용률(%)로 표시해 보세요.

5. **두 버전을 하나로**
   버튼으로 CPU 모드와 DMA 모드를 실시간 전환하는 펌웨어를 만들어 보세요.
   - 어려운 점: TRIG/ECHO 핀을 GPIO와 타이머 사이에서 **실행 중에 다시 설정**해야 합니다.
