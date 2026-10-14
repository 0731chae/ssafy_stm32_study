# UART 로그 폭주 실험 — CPU vs DMA (NUCLEO-F446RE)

**1초마다 깜빡이는 LED** 하나와 **매우 긴 랜덤 로그 문자열**을 UART로 주고받으면서, 그 전송을 **CPU가 직접 할 때**와 **DMA에 맡길 때**를 비교합니다.

> **읽기 전에**
> - 이 가이드는 **프로젝트 하나**만 만듭니다. CPU 버전과 DMA 버전은 `#define USE_DMA` 한 줄로 바꿉니다.
> - CubeMX 메뉴 이름과 HAL 함수는 일반 지식입니다. STM32CubeIDE/CubeMX 버전에 따라 메뉴 위치가 조금 다를 수 있습니다.
> - **이 가이드의 코드는 실제 보드에서 아직 실행해 보지 않았습니다.** 예측값(0-4)과 실제 측정값을 비교하는 것 자체가 이 실험의 일부입니다.
> - 출처 표기: 〔RM〕 = RM0390, 〔DS〕 = STM32F446 데이터시트, 〔UM〕 = UM1724 (Nucleo-64)

---

## 목차
- [0. 이 프로젝트가 보여주는 것](#0-이-프로젝트가-보여주는-것)
- [1. 준비물과 배선](#1-준비물과-배선)
- [2. 핀 · 타이머 · DMA 배정표](#2-핀--타이머--dma-배정표)
- [3. CubeMX 설정](#3-cubemx-설정)
- [4. 코드](#4-코드)
- [5. 빌드와 실행](#5-빌드와-실행)
- [6. 비교 실험](#6-비교-실험)
- [7. 문제 해결](#7-문제-해결)
- [8. 심화 과제](#8-심화-과제)

---

## 0. 이 프로젝트가 보여주는 것

### 0-1. 한 줄 요약

> **115200 bps로 4 KB를 보내는 데 0.36초가 걸립니다. 그 0.36초 동안 CPU가 무엇을 하고 있는지가 전부입니다.**

| | CPU 버전 | DMA 버전 |
|---|---|---|
| 전송 함수 | `HAL_UART_Transmit()` — **끝날 때까지 안 돌아옴** | `HAL_UART_Transmit_DMA()` — **즉시 돌아옴** |
| 그 0.36초 동안 CPU는 | 바이트를 하나씩 레지스터에 **손으로 넣고 있음** | **다른 일을 함** |
| 1초 LED | **늦게** 깜빡임 | 정확히 1초 |
| 터미널에서 타이핑하면 | **문자가 사라짐** | 전부 받음 |

### 0-2. 장치 구성

```
                   ┌─────────────────────────────────────────┐
                   │            STM32F446RE                  │
  [PC 터미널]      │                                         │
   115200 8N1      │  ┌──────────┐                           │
       │           │  │ 메인 루프 │                           │
       │  TX ◀─────┼──┤          │── 4 KB 랜덤 로그 전송      │
       │           │  │          │   (450 ms마다)             │
       │  RX ─────▶┼──┤          │── 수신 1바이트씩 확인       │
       │           │  │          │                           │
       │           │  │          │── 1초마다 LED 토글 ────────┼──▶ 🔴 외부 LED (PA6)
       │           │  └──────────┘                           │     = 관찰 대상
       │           │                                         │
       │           │  ┌──────────┐                           │
       │           │  │ TIM2 (HW)│── 1초마다 자동 토글 ───────┼──▶ 🟢 LD2 (PA5, 내장)
       │           │  └──────────┘                           │     = 기준 메트로놈
                   └─────────────────────────────────────────┘
```

**LED가 두 개인 이유**가 이 실험의 핵심 장치입니다.

| LED | 누가 깜빡이게 하나 | 결과 |
|---|---|---|
| 🔴 **외부 LED (PA6)** | **메인 루프**가 `HAL_GetTick()`을 보고 `TogglePin()` | 루프가 막히면 **늦어진다** ← 관찰 대상 |
| 🟢 **LD2 (PA5, 내장)** | **TIM2 하드웨어**가 핀을 직접 토글 | CPU와 무관하게 **항상 정확** ← 기준 |

두 LED를 나란히 두고 보세요.

- **DMA 버전**: 두 개가 **딱 맞춰** 깜빡입니다.
- **CPU 버전**: 🔴가 🟢보다 **눈에 보이게 늦게** 깜빡이고, 늦는 정도가 **매번 달라집니다**(지터).

오실로스코프 없이 "타이밍이 깨졌다"를 눈으로 보는 장치입니다.

### 0-3. 동작 명세

| # | 동작 | 주기 | 담당 |
|---|---|---|---|
| ① | 🔴 외부 LED 토글 | **1,000 ms** | 메인 루프 (소프트웨어) |
| ② | 🟢 LD2 토글 | **1,000 ms** | TIM2 OC Toggle (하드웨어) |
| ③ | 랜덤 로그 전송 | **450 ms** | CPU 또는 DMA ← **비교 대상** |
| ④ | 수신 바이트 확인 | 루프마다 | CPU 폴링 또는 DMA 링버퍼 ← **비교 대상** |

**전송하는 로그의 구조** — 매번 **정확히 같은 바이트 수**를 보냅니다.

```
┌──────────── 96 바이트 ────────────┬───── 4096 바이트 ─────┬── 2 ──┐
│ #12 mode=CPU jit=314ms rx=37      │ Xk2Pq9... (랜덤)      │ CRLF  │
│ ore=4 loop/s=11840 len=4096       │ 62자마다 줄바꿈       │       │
│ (남는 자리는 공백으로 채움)        │                       │       │
└───────────────────────────────────┴───────────────────────┴───────┘
                        합계 4,194 바이트
```

- 헤더는 **길이를 96바이트로 고정**합니다(남는 자리는 공백). 숫자가 커져도 전송량이 변하지 않아야 두 버전의 부하가 정확히 같습니다.
- 본문은 **부팅할 때 한 번만** 생성합니다. 생성 비용이 루프에 들어가면 비교가 오염됩니다.
- 랜덤 문자는 **선형 합동 생성기(LCG)**로 만듭니다. 62자마다 CRLF를 넣어 터미널에서 읽을 수 있게 했습니다.

**수신 동작 (④)**

터미널에서 아무 글자나 치면 `rx` 카운터가 올라갑니다. `r`을 치면 모든 통계가 **초기화**됩니다.

### 0-4. 숫자로 미리 계산하기

실험 전에 **답을 먼저 계산**해 두고, 실측값과 맞는지 확인하는 것이 이 프로젝트의 목적입니다.

**① 전송 시간**

```
115200 bps, 8N1 → 1바이트 = 10비트 (시작 1 + 데이터 8 + 정지 1)
                → 11,520 바이트/초
                → 1바이트 = 86.8 µs

전송 바이트 = 96 + 4096 + 2 = 4,194 바이트
전송 시간   = 4,194 ÷ 11,520 = 0.364 초 = 364 ms
```

**② CPU 점유율 (CPU 버전)**

```
450 ms마다 364 ms씩 묶임 → 364 / 450 = 81 %
```

CPU가 **81% 를 UART에 쓰고 있습니다.** 남는 19%로만 LED와 수신을 처리합니다.

**③ LED 최대 지연 (`jit`)** — 이게 가장 재미있는 예측입니다.

전송은 450 ms 격자, LED 마감은 1000 ms 격자입니다. LED 마감이 전송 구간 안에 들어가면, 전송이 끝날 때까지 밀립니다.

```
두 격자의 최소 간격 = gcd(450, 1000) = 50 ms
→ 마감이 전송 시작 직후 50 ms 지점에 걸리는 경우가 최악
→ 최대 지연 = 364 - 50 = 314 ms
```

**예측: CPU 버전의 `jit`은 약 314 ms에서 멈춥니다.** 그 이상 올라가지 않습니다. 실측값이 310~315 범위면 계산이 맞은 것입니다.

**④ 루프 속도 비율**

```
CPU 버전은 19%의 시간에만 루프를 돈다
DMA 버전은 100% 돈다
→ loop/s 비율 = 1 / 0.19 ≈ 5.3 배
```

절대값은 컴파일러 최적화 옵션에 따라 달라지지만, **비율 약 5배**는 맞아야 합니다.

**예측 종합**

| 지표 | CPU 버전 | DMA 버전 |
|---|---|---|
| `jit` (🔴 LED 최대 지연) | **약 314 ms** | **0 ~ 1 ms** |
| `loop/s` | 기준값 | **약 5배** |
| `ore` (수신 오버런) | **발생함** | **0** |
| 타이핑한 50글자 중 `rx` | **50보다 작음** | **정확히 50** |
| 🔴와 🟢의 깜빡임 | **눈에 보이게 엇나감** | **일치** |

### 0-5. 두 버전의 차이는 코드 3곳

`#define USE_DMA`를 `0`에서 `1`로 바꾸면 아래 세 군데만 달라집니다. **나머지는 완전히 같은 코드입니다.**

| 곳 | CPU 버전 (`USE_DMA 0`) | DMA 버전 (`USE_DMA 1`) |
|---|---|---|
| 초기화 | (없음) | `HAL_UART_Receive_DMA()` 로 수신 링버퍼 가동 |
| 전송 | `HAL_UART_Transmit(..., HAL_MAX_DELAY)` | `HAL_UART_Transmit_DMA(...)` |
| 수신 | `HAL_UART_Receive(..., 0)` 1바이트 폴링 + 오버런 플래그 처리 | 링버퍼에서 `NDTR`로 읽기 |

> **왜 프로젝트를 둘로 나누지 않나**
> CubeMX에서 DMA를 켜 두어도, `HAL_UART_Transmit()`(블로킹)은 **DMA를 쓰지 않습니다.** 핸들에 `hdmatx`가 연결돼 있든 없든 동작이 같습니다. 그래서 **설정 하나로 두 버전을 모두 돌릴 수 있고**, 그 덕에 "전송 방식 말고는 아무것도 다르지 않다"가 보장됩니다. 빌드도 한 번씩만 하면 됩니다.

---

## 1. 준비물과 배선

### 1-1. 준비물

| 부품 | 수량 | 비고 |
|---|---|---|
| NUCLEO-F446RE | 1 | USB 케이블 포함 |
| LED (아무 색) | 1 | **관찰 대상.** 🟢 기준 LED는 보드 내장(LD2)을 씁니다 |
| 저항 220~330Ω | 1 | LED와 직렬 |
| 브레드보드, 점퍼선 | | |

외부 전원이 필요 없습니다. **USB 케이블 하나로 전부 동작합니다.**

### 1-2. 배선표

| 부품 | 연결할 곳 | STM32 핀 | 용도 |
|---|---|---|---|
| **LED 애노드(+)** → 저항 | **D12** | PA6 | 🔴 소프트웨어 1초 LED (관찰 대상) |
| **LED 캐소드(−)** | GND | – | |
| (보드 내장) | – | PA5 | 🟢 LD2. TIM2_CH1 하드웨어 1초 LED (기준) |
| (보드 내장) | – | PA2 / PA3 | USART2 TX / RX. ST-LINK 가상 COM 포트 〔UM p.26〕 |

- Arduino 커넥터 이름과 STM32 핀의 대응은 〔UM p.36 Figure 24, p.48–49 Table 19〕에서 확인했습니다.
- ⚠ **저항을 반드시 직렬로** 넣으세요. 없이 꽂으면 LED와 GPIO가 상합니다.
- 🔴 외부 LED를 **보드의 🟢 LD2 바로 옆에** 두세요. 두 개를 동시에 봐야 엇나가는 게 보입니다.

> 🔴 외부 LED가 없으면 역할을 바꿔도 됩니다. LD2(PA5)를 소프트웨어 LED로 쓰고 TIM2는 빼세요. 다만 **비교 기준이 사라져서** 늦어지는 것을 눈으로 확인하기 어려워집니다. 그 경우 로그의 `jit` 숫자에만 의존해야 합니다.

---

## 2. 핀 · 타이머 · DMA 배정표

### 2-1. 클럭

| 항목 | 값 |
|---|---|
| SYSCLK / HCLK | 180 MHz |
| APB1 (PCLK1) | 45 MHz → **APB1 타이머 클럭 90 MHz** (TIM2) |
| APB2 (PCLK2) | 90 MHz |

첫 프로젝트와 같은 클럭 구성입니다. HSI(16 MHz) → PLL → 180 MHz. 180 MHz를 쓰는 이유는 `PROJECT_GUIDE.md` 2-1을 보세요.

### 2-2. 타이머 계산

| 타이머 | 클럭 | PSC | 카운터 주파수 | ARR | 결과 |
|---|---|---|---|---|---|
| **TIM2** 기준 LED | 90 MHz | **8999** | 10 kHz | **9999** | **1초**마다 토글 (깜빡임 주기 2초) |

```
토글 주기 = 90,000,000 / ((8999 + 1) × (9999 + 1)) = 1 Hz
```

> OC **Toggle** 모드는 카운터가 CCR에 도달할 때마다 출력을 뒤집습니다. 주기가 1초이므로 **1초마다 한 번** 뒤집힙니다. 소프트웨어 LED(`TogglePin`)와 동작이 정확히 1:1로 대응합니다.

### 2-3. DMA 배정 (DMA 버전)

〔RM p.204 Table 28〕에서 골랐습니다.

| 기능 | DMA 요청 | 컨트롤러 | Stream | Channel | 방향 | 모드 |
|---|---|---|---|---|---|---|
| 로그 전송 | USART2_TX | **DMA1** | Stream6 | 4 | 메모리 → 주변장치 | **Normal** |
| 문자 수신 | USART2_RX | **DMA1** | Stream5 | 4 | 주변장치 → 메모리 | **Circular** |

- USART2는 **APB1** 주변장치라서 **DMA1**을 씁니다.
- 전송은 **Normal**: 한 번 보내고 끝. 매번 `HAL_UART_Transmit_DMA()`가 다시 걸어 줍니다.
- 수신은 **Circular**: 링버퍼를 끝없이 채웁니다. 그래서 **CPU가 한 번도 끼어들지 않아도 문자가 쌓입니다.** 이게 수신 유실이 사라지는 이유입니다.

---

## 3. CubeMX 설정

### 3-1. 프로젝트 만들기
1. STM32CubeIDE → **File > New > STM32 Project**
2. **Board Selector** 탭 → `NUCLEO-F446RE` 검색 → 선택 → Next
3. Project Name: `Uart_Stress` → Finish
4. "Initialize all peripherals with their default Mode?" → **No**

### 3-2. System Core → SYS
| 항목 | 값 |
|---|---|
| Debug | **Serial Wire** ← ⚠ 안 하면 다음부터 디버거 연결이 어려워집니다 |
| Timebase Source | SysTick |

### 3-3. System Core → RCC
| 항목 | 값 |
|---|---|
| High Speed Clock (HSE) | Disable |
| Low Speed Clock (LSE) | Disable |

### 3-4. Clock Configuration 탭
**HCLK 칸에 `180`을 입력하고 Enter**를 치면 CubeMX가 자동으로 맞춰 줍니다. 그다음 아래와 같은지 확인하세요.

| 항목 | 값 |
|---|---|
| PLL Source Mux | HSI |
| PLLM / PLLN / PLLP | /8 → ×180 → /2 |
| System Clock Mux | PLLCLK |
| AHB Prescaler | /1 → HCLK **180 MHz** |
| APB1 Prescaler | /4 → PCLK1 **45 MHz** (**APB1 Timer clocks 90 MHz**) |
| APB2 Prescaler | /2 → PCLK2 **90 MHz** |

빨간색 칸이 없어야 합니다. **APB1 Timer clocks가 90 MHz**인지 꼭 확인하세요 — TIM2 계산이 이 값을 씁니다.

### 3-5. GPIO — 🔴 소프트웨어 LED

Pinout 화면에서 **PA6**을 클릭 → `GPIO_Output`. 핀을 **오른쪽 클릭 → Enter User Label** → `LED_SW`.

⚠ **코드가 이 이름을 그대로 쓰므로 철자를 정확히** 맞추세요.

**System Core → GPIO** 화면에서:

| 핀 | GPIO mode | Pull-up/Pull-down | Output level |
|---|---|---|---|
| PA6 | Output Push Pull | No pull | Low |

### 3-6. Timers → TIM2 — 🟢 기준 LED

| 항목 | 값 |
|---|---|
| Clock Source | **Internal Clock** |
| Channel1 | **Output Compare CH1** → 핀 **PA5**로 잡히는지 확인 |
| Prescaler | **8999** |
| Counter Period | **9999** |
| auto-reload preload | Enable |
| Output Compare Channel 1 → Mode | **Toggle on match** |
| Output Compare Channel 1 → Pulse | **0** |
| CH Polarity | High |

> PA5는 보드의 **LD2가 달려 있는 핀**이고, 동시에 **TIM2_CH1**입니다. 그래서 부품을 추가하지 않고 하드웨어 기준 LED를 만들 수 있습니다.
> PA5가 TIM2_CH1으로 안 잡히면 PA5를 직접 클릭해 `TIM2_CH1`을 고르세요.

### 3-7. Connectivity → USART2

| 항목 | 값 |
|---|---|
| Mode | **Asynchronous** |
| Hardware Flow Control | Disable |
| Baud Rate | **115200** |
| Word Length | 8 Bits |
| Parity | None |
| Stop Bits | 1 |
| Data Direction | **Receive and Transmit** ← 송신만이 아니라 **수신도** 씁니다 |

핀이 **PA2 (TX), PA3 (RX)**로 잡혔는지 확인합니다.

### 3-8. System Core → DMA

**DMA1** 탭에서 **Add**를 눌러 두 개를 추가합니다.

| DMA Request | Stream | Direction | Priority | Mode | Increment (Periph / Mem) | Data Width (Periph / Mem) |
|---|---|---|---|---|---|---|
| **USART2_TX** | DMA1 Stream6 | Memory To Peripheral | Low | **Normal** | ☐ / ☑ | Byte / Byte |
| **USART2_RX** | DMA1 Stream5 | Peripheral To Memory | Low | **Circular** | ☐ / ☑ | Byte / Byte |

- **Use Fifo는 체크하지 않습니다** (Direct 모드). 양쪽 데이터 폭이 같아서 FIFO가 필요 없습니다. 〔RM p.211, p.216〕
- 스트림 번호가 표와 다르게 잡히면 드롭다운에서 바꾸세요. 〔RM p.204〕
- ⚠ **RX는 반드시 Circular**입니다. Normal로 두면 256바이트를 받고 멈춰서, 그 뒤 문자는 모두 사라집니다.

### 3-9. System Core → NVIC

- ☑ **USART2 global interrupt** ← ⚠ **필수.** DMA 송신이 끝났을 때 HAL이 상태를 "준비됨"으로 되돌리려면 필요합니다. 이걸 빼면 **DMA 버전이 로그를 한 줄만 보내고 멈춥니다.**
- DMA 스트림 인터럽트는 CubeMX가 자동으로 켜 둡니다. 그대로 둡니다.

### 3-10. Project Manager

| 항목 | 값 |
|---|---|
| Toolchain / IDE | STM32CubeIDE |
| Code Generator → Generate peripheral initialization as a pair of '.c/.h' files per peripheral | **체크 해제** (모든 핸들이 `main.c`에 있어야 아래 코드가 그대로 동작) |

### 3-11. 코드 생성
`Ctrl + S` → "Do you want to generate Code?" → **Yes**

**생성된 `main.c`에 다음이 있어야 합니다.**
- 핸들: `htim2`, `huart2`
- DMA 핸들: `hdma_usart2_tx`, `hdma_usart2_rx`
- `main.h`에 매크로: `LED_SW_Pin`, `LED_SW_GPIO_Port`
- ⚠ `main()` 안에서 **`MX_DMA_Init()`이 `MX_USART2_UART_Init()`보다 먼저** 호출되는지 확인하세요. 거꾸로 생성되면 DMA가 동작하지 않습니다.

---

## 4. 코드

> ⚠ 반드시 `/* USER CODE BEGIN xxx */` ~ `/* USER CODE END xxx */` **사이**에 넣으세요. 그 밖에 쓰면 CubeMX로 코드를 다시 생성할 때 지워집니다.

### ① `USER CODE BEGIN Includes`
```c
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */
```

### ② `USER CODE BEGIN PD`
```c
/* USER CODE BEGIN PD */

/* ★★★ 이 한 줄이 CPU 버전과 DMA 버전을 가른다 ★★★ */
#define USE_DMA         0U       /* 0 = CPU 버전,  1 = DMA 버전 */

#define LOG_LEN         4096U    /* 랜덤 로그 본문 길이 (바이트) */
#define HDR_LEN         96U      /* 헤더 고정 길이. 전송량을 매번 같게 하려고 고정한다 */
#define TX_PERIOD_MS    450U     /* 로그 전송 주기 */
#define BLINK_MS        1000U    /* 소프트웨어 LED 토글 주기 */
#define RX_BUF_LEN      256U     /* DMA 수신 링버퍼 크기 */

/* USER CODE END PD */
```

> **`TX_PERIOD_MS` 값을 바꿀 때 지켜야 할 규칙 두 가지** (6-3에서 다시 설명합니다)
> 1. **전송 시간보다 커야 합니다.** 작으면 DMA 버전이 전송을 건너뛰어, 두 버전의 전송량이 달라집니다.
> 2. **`BLINK_MS`의 약수여서는 안 됩니다.** 500처럼 딱 나눠지는 값을 쓰면 LED 마감과 전송 시작이 항상 어긋나지 않게 맞물려서, **CPU 버전에서도 지연이 0으로 나옵니다.** 450은 이 두 조건을 모두 만족합니다.

### ③ `USER CODE BEGIN PV`
```c
/* USER CODE BEGIN PV */
/* [헤더 96][랜덤 본문 4096][CRLF 2][여분 1] */
static char     log_buf[HDR_LEN + LOG_LEN + 3U];
static uint16_t log_total;              /* 실제 전송 바이트 수 */

static uint8_t  rx_buf[RX_BUF_LEN];     /* DMA 수신 링버퍼 */
static uint16_t rx_tail;                /* 내가 읽은 위치 */

static uint32_t seq;                    /* 전송 번호 */
static uint32_t blink_jit_max;          /* ★ LED 토글 최대 지연 (ms) */
static uint32_t rx_count;               /* 수신 바이트 누적 */
static uint32_t ore_count;              /* ★ 오버런 발생 횟수 */
static uint32_t loops, loops_per_sec;
/* USER CODE END PV */
```

### ④ `USER CODE BEGIN PFP`
```c
/* USER CODE BEGIN PFP */
static uint32_t rnd(void);
static void     log_fill_body(void);
static void     log_make_header(void);
static void     rx_poll(void);
/* USER CODE END PFP */
```

### ⑤ `USER CODE BEGIN 2` (모든 `MX_xxx_Init()` 호출 바로 뒤)
```c
  /* USER CODE BEGIN 2 */

  /* (1) 랜덤 로그 본문을 '딱 한 번' 만든다.
         ★ 생성 비용이 루프에 들어가면 두 버전의 비교가 오염된다 */
  log_fill_body();
  log_buf[HDR_LEN + LOG_LEN]      = '\r';
  log_buf[HDR_LEN + LOG_LEN + 1U] = '\n';
  log_total = (uint16_t)(HDR_LEN + LOG_LEN + 2U);

  /* (2) 기준 LED: TIM2가 PA5(LD2)를 1초마다 직접 토글한다.
         이 순간 이후 CPU는 이 LED에 관여하지 않는다 */
  HAL_TIM_OC_Start(&htim2, TIM_CHANNEL_1);

#if USE_DMA
  /* (3) DMA 버전만: 수신 링버퍼 가동. CPU가 안 끼어들어도 문자가 쌓인다 */
  HAL_UART_Receive_DMA(&huart2, rx_buf, RX_BUF_LEN);
#endif

  /* (4) 시작 메시지 (두 버전 모두 블로킹. 한 번뿐이라 영향 없음) */
  const char *hello = USE_DMA ? "\r\n===== UART stress: DMA version =====\r\n"
                              : "\r\n===== UART stress: CPU version =====\r\n";
  HAL_UART_Transmit(&huart2, (uint8_t *)hello, (uint16_t)strlen(hello), 100);

  uint32_t last_blink = HAL_GetTick();
  uint32_t last_tx    = last_blink;
  uint32_t last_sec   = last_blink;
  /* USER CODE END 2 */
```

### ⑥ `USER CODE BEGIN 3` (while 루프 안)
```c
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint32_t now = HAL_GetTick();

    /* (1) 🔴 1초 LED — 소프트웨어 타이머.
           루프가 막혀 있으면 마감을 놓치고, 풀린 다음에야 토글된다 */
    if (now - last_blink >= BLINK_MS) {
      uint32_t jit = (now - last_blink) - BLINK_MS;      /* 늦은 만큼 */
      if (jit > blink_jit_max) { blink_jit_max = jit; }
      HAL_GPIO_TogglePin(LED_SW_GPIO_Port, LED_SW_Pin);
      last_blink += BLINK_MS;      /* ★ now가 아니라 += : 1초 격자를 유지한다 */
    }

    /* (2) 수신 확인 */
    rx_poll();

    /* (3) ★ 긴 로그 전송 — 여기가 두 버전의 유일한 차이 */
    if (now - last_tx >= TX_PERIOD_MS) {
      last_tx = now;
      seq++;
      log_make_header();
#if USE_DMA
      /* 바로 리턴한다. 이전 전송이 아직 안 끝났으면 이번 건 건너뛴다 */
      if (huart2.gState == HAL_UART_STATE_READY) {
        HAL_UART_Transmit_DMA(&huart2, (uint8_t *)log_buf, log_total);
      }
#else
      /* 4,194바이트를 다 내보낼 때까지 CPU가 이 줄에 묶인다 (약 364 ms) */
      HAL_UART_Transmit(&huart2, (uint8_t *)log_buf, log_total, HAL_MAX_DELAY);
#endif
    }

    /* (4) 루프 속도 = CPU 여유 */
    loops++;
    if (now - last_sec >= 1000U) {
      loops_per_sec = loops;
      loops = 0;
      last_sec += 1000U;
    }
  }
  /* USER CODE END 3 */
```

### ⑦ `USER CODE BEGIN 4` (main 함수 아래, 함수 정의)
```c
/* USER CODE BEGIN 4 */

/* ===== 랜덤: 선형 합동 생성기 (LCG) =====
   표준 rand()를 쓰지 않는 이유는, 어느 라이브러리를 링크해도
   항상 같은 수열이 나와야 실험을 재현할 수 있기 때문이다 */
static uint32_t rnd(void)
{
  static uint32_t st = 0x12345678U;
  st = st * 1103515245U + 12345U;
  return st >> 16;                 /* 상위 비트가 더 고르게 섞인다 */
}

/* ===== 랜덤 로그 본문 생성 (부팅 때 한 번만) ===== */
static void log_fill_body(void)
{
  static const char tbl[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  char *p = log_buf + HDR_LEN;

  for (uint32_t i = 0; i < LOG_LEN; i++) {
    /* 64칸마다 CRLF를 넣어 터미널에서 읽을 수 있게 한다 (랜덤 62자 + CRLF) */
    if      ((i % 64U) == 62U) { p[i] = '\r'; }
    else if ((i % 64U) == 63U) { p[i] = '\n'; }
    else                       { p[i] = tbl[rnd() % 62U]; }
  }
}

/* ===== 헤더 생성: 길이를 HDR_LEN으로 '고정'한다 =====
   숫자 자릿수가 변해도 전송 바이트 수가 같아야,
   두 버전에 걸리는 부하가 정확히 동일해진다 */
static void log_make_header(void)
{
  int n = snprintf(log_buf, HDR_LEN,
                   "\r\n#%lu mode=%s jit=%lums rx=%lu ore=%lu loop/s=%lu len=%u\r\n",
                   (unsigned long)seq,
                   (USE_DMA ? "DMA" : "CPU"),
                   (unsigned long)blink_jit_max,
                   (unsigned long)rx_count,
                   (unsigned long)ore_count,
                   (unsigned long)loops_per_sec,
                   (unsigned)LOG_LEN);
  if (n < 0)             { n = 0; }
  if (n > (int)HDR_LEN)  { n = (int)HDR_LEN; }   /* 잘렸으면 반환값이 HDR_LEN보다 클 수 있다 */
  while (n < (int)HDR_LEN) { log_buf[n++] = ' '; }  /* 남는 자리는 공백으로 (NUL도 덮어씀) */
}

/* ===== 수신 ===== */
static void rx_poll(void)
{
#if USE_DMA

  /* DMA가 링버퍼를 계속 채운다. NDTR(남은 전송 개수)로 머리 위치를 역산해
     내가 읽은 꼬리부터 따라잡는다. CPU가 늦게 와도 데이터는 이미 메모리에 있다 */
  uint16_t head = (uint16_t)(RX_BUF_LEN - __HAL_DMA_GET_COUNTER(huart2.hdmarx));
  while (rx_tail != head) {
    uint8_t ch = rx_buf[rx_tail];
    rx_tail = (uint16_t)((rx_tail + 1U) % RX_BUF_LEN);
    rx_count++;
    if (ch == 'r') { blink_jit_max = 0; ore_count = 0; rx_count = 0; }
  }

#else

  /* 레지스터를 직접 폴링한다. 폴링 수신기가 실제로 하는 일이 이 세 줄이다.
     ★ 전송 중(364 ms)에는 이 함수에 도달조차 못 한다
        → 그사이 도착한 문자는 DR에서 덮어써져 영원히 사라진다 */
  uint32_t sr = huart2.Instance->SR;        /* SR은 한 번만 읽는다 */

  /* ★ 오버런을 '먼저' 센다.
     DR을 읽는 순간 RXNE와 ORE가 함께 지워지므로, 수신 뒤에 확인하면 놓친다 */
  if (sr & USART_SR_ORE) { ore_count++; }

  if (sr & USART_SR_RXNE) {
    uint8_t ch = (uint8_t)(huart2.Instance->DR & 0xFFU);  /* 이 읽기가 RXNE/ORE를 지운다 */
    rx_count++;
    if (ch == 'r') { blink_jit_max = 0; ore_count = 0; rx_count = 0; }
  } else if (sr & USART_SR_ORE) {
    (void)huart2.Instance->DR;              /* ORE만 섰을 때: DR을 읽어 플래그를 지운다 */
  }

#endif
}

/* USER CODE END 4 */
```

> **`ore`를 정확히 읽는 법**
> ORE는 **"플래그가 섰다"**를 뜻할 뿐, **잃어버린 바이트 수가 아닙니다.** 전송 중에 10글자가 들어와도 플래그는 한 번만 섭니다. 그래서 `ore`는 "유실이 일어났다"는 **증거**로만 쓰고, **얼마나 잃었는지는 `rx` 값을 보낸 글자 수와 비교**해서 판단하세요(6-2 실험 ③).

> **왜 `HAL_UART_Receive()`를 쓰지 않았나**
> `HAL_UART_Receive(&huart2, &ch, 1, 0)`(타임아웃 0)도 비블로킹 폴링으로 동작합니다. 하지만 **HAL 버전에 따라** 타임아웃 경로에서 `RxState`를 `READY`로 되돌리지 않는 경우가 있어, 그러면 두 번째 호출부터 영구히 `HAL_BUSY`를 돌려주고 **수신이 아예 멈춥니다.**
> 위 코드처럼 `SR`/`DR`을 직접 읽으면 HAL 버전과 무관하게 동작하고, "폴링이란 무엇인가"도 그대로 드러납니다. `USART_SR_ORE` · `USART_SR_RXNE`는 CMSIS 헤더에 이미 정의돼 있어 추가 include가 필요 없습니다.

---

## 5. 빌드와 실행

1. **Project > Build Project** (망치). 에러 없이 끝나는지 확인합니다.
2. **Run > Debug As > STM32 C/C++ Application** → Resume(F8)
3. PC에서 시리얼 터미널(Tera Term, PuTTY, CubeIDE 내장 터미널)을 열고 ST-LINK COM 포트에 **115200, 8N1**로 연결합니다.
4. 이렇게 나오면 성공입니다.

```
===== UART stress: CPU version =====

#1 mode=CPU jit=0ms rx=0 ore=0 loop/s=0 len=4096
kQ2mXp9vBnR4tLsZ...(62자)
aW7eR3tY8uI2oP5q...
    ... 64줄 ...
#7 mode=CPU jit=314ms rx=0 ore=0 loop/s=11840 len=4096
...
```

- 🔴 외부 LED와 🟢 LD2가 깜빡이기 시작합니다.
- `jit` 값이 10초 안에 **300 근처까지 올라가서 멈추는지** 보세요. (예측: 314 ms)

> **헤더 숫자는 한 박자 늦습니다.** 헤더는 전송을 **시작하기 전에** 만들기 때문에, 방금 일어난 일은 다음 전송의 헤더에 나타납니다. `#1`의 `jit=0`은 "아직 LED 마감이 한 번도 안 왔다"는 뜻이고, `loop/s=0`은 "아직 1초가 안 지났다"는 뜻입니다. 정상입니다.

> 터미널에 로그가 쏟아져서 읽기 힘들면 `LOG_LEN`을 `256U`로 임시로 낮추고 동작만 확인한 뒤 되돌리세요. 다만 256바이트는 전송이 **3 ms**밖에 안 걸려서 **두 버전의 차이가 나타나지 않습니다.**

---

## 6. 비교 실험

### 6-1. 실험 순서

`USE_DMA`를 `0` → `1`로 바꿔 빌드·업로드하는 것이 전부입니다. **총 2번 업로드.**

| | `USE_DMA` | 보는 것 |
|---|---|---|
| 1 | `0U` (CPU) | 아래 ①②③을 전부 측정하고 적기 |
| 2 | `1U` (DMA) | 같은 것을 다시 측정해 비교 |

### 6-2. 측정 항목 세 가지

**① 🔴와 🟢의 깜빡임 — 계측기 없이 눈으로**

두 LED를 나란히 보세요. 보드를 리셋한 직후에는 둘이 맞지 않을 수 있으니, **10초쯤 지난 뒤**부터 보세요.

| | 보이는 것 |
|---|---|
| **CPU** | 🔴가 🟢보다 **늦게** 켜지고, 늦는 정도가 **매번 달라집니다.** 🔴만 보면 "대충 1초"지만, 🟢와 나란히 보면 **확실히 흔들립니다** |
| **DMA** | 두 개가 **거의 동시에** 바뀝니다 |

**② 로그 헤더의 `jit` — 숫자로**

30초쯤 돌린 뒤 헤더의 `jit` 값을 적습니다. 이 값은 **최댓값 누적**이라 계속 커지다가 어느 선에서 멈춥니다.

| | 예측 | 의미 |
|---|---|---|
| **CPU** | **약 314 ms** | 1초마다 깜빡여야 할 LED가 **최대 0.3초**까지 늦는다 |
| **DMA** | **0 ~ 1 ms** | 마감을 놓치지 않는다 |

0-4 ③의 계산(`364 - gcd(450,1000)` = 314)과 맞는지 확인하세요. 맞으면 **왜 늦는지를 정확히 이해한 것**입니다.

**③ 타이핑한 문자가 살아남는가 — 수신 비교**

이게 "입출력"을 둘 다 쓰는 이유입니다.

1. 터미널에서 **`r`을 한 번** 눌러 통계를 초기화합니다.
2. **`a`를 정확히 50번** 또박또박 누릅니다. (로그가 쏟아지는 중에 눌러야 합니다)
3. 다음 헤더의 `rx`와 `ore`를 봅니다.

| | `rx` | `ore` | 왜 |
|---|---|---|---|
| **CPU** | **50보다 작음** | **0보다 큼** | 364 ms 동안 `rx_poll()`에 도달하지 못합니다. 그사이 들어온 문자는 수신 레지스터에서 **덮어써져 사라집니다** |
| **DMA** | **정확히 50** | **0** | DMA가 CPU와 무관하게 링버퍼에 쌓습니다. CPU는 나중에 천천히 읽어도 됩니다 |

> 터미널의 **로컬 에코를 끄면** 내가 누른 글자가 화면에 안 보입니다. 그래도 `rx` 숫자로 확인되니 괜찮습니다. 켜져 있으면 로그와 섞여서 더 헷갈립니다.

**④ `loop/s` — CPU 여유**

| | 예측 |
|---|---|
| **CPU** | 기준값 |
| **DMA** | **약 5배** (0-4 ④의 `1 / (1 - 0.81)` = 5.3) |

### 6-3. 값을 바꿔 보기

`LOG_LEN`을 바꾸면 전송 시간이 바뀌고, 모든 결과가 따라 바뀝니다.

| `LOG_LEN` | 전송 바이트 | 전송 시간 | 안전한 `TX_PERIOD_MS` | CPU 점유율 | **예측 `jit`** |
|---|---|---|---|---|---|
| 512 | 610 | 53 ms | 150 | 35 % | 3 ms ← 너무 작아 눈에 안 보임 |
| 1024 | 1,122 | 97 ms | 150 | 65 % | 47 ms |
| 2048 | 2,146 | 186 ms | 350 | 53 % | 136 ms |
| **4096** | **4,194** | **364 ms** | **450** | **81 %** | **314 ms** ← 기본값 |
| 8192 | 8,290 | 720 ms | 850 | 85 % | 670 ms |

두 공식만 알면 모든 칸을 직접 채울 수 있습니다.

```
전송 시간  = (HDR_LEN + LOG_LEN + 2) ÷ 11,520  [초]
예측 jit   = 전송 시간 − gcd(TX_PERIOD_MS, BLINK_MS)
```

표의 `TX_PERIOD_MS`는 모두 **`gcd(TX_PERIOD_MS, 1000) = 50`**이 되도록 골랐습니다. 250이나 500처럼 1000을 딱 나누는 값은 **일부러 피했습니다** — 이유는 바로 아래 규칙 ②에 있습니다.

> ⚠ **지켜야 할 규칙 두 가지**
>
> **① `TX_PERIOD_MS` > 전송 시간**
> 어기면 DMA 버전은 `gState` 검사에서 **전송을 건너뛰고**(로그가 띄엄띄엄), CPU 버전은 건너뛰지 않고 계속 묶입니다. 두 버전이 **다른 양을 보내게 되어 비교가 깨집니다.**
>
> **② `TX_PERIOD_MS`가 `BLINK_MS`의 약수여서는 안 됨**
> 500이나 250처럼 1000을 딱 나누는 값을 쓰면, LED 마감과 전송 시작이 항상 같은 순간에 걸립니다. 코드가 LED를 먼저 처리하므로 **CPU 버전에서도 `jit`이 0으로 나옵니다.** "DMA가 필요 없다"는 틀린 결론이 나오니 주의하세요.
> 이 함정을 직접 확인해 보는 것도 좋은 실험입니다. `TX_PERIOD_MS`를 `500U`로 바꾸고 CPU 버전을 돌려 보세요. `loop/s`는 떨어지는데 `jit`은 0입니다.

### 6-4. 보너스: 디버거로 CPU를 멈춰 보기 🎭

DMA 버전을 실행하다가 CubeIDE에서 **Suspend(일시정지)**를 눌러 보세요.

- 🟢 LD2는 **계속 1초마다 깜빡입니다.** TIM2가 하드웨어니까요. (디버그 중 타이머를 멈출지는 `DBGMCU_APB1_FZ`의 `DBG_TIM2_STOP` 비트가 정합니다 〔RM p.1307–1308〕)
- 🔴 외부 LED는 **멈춥니다.** 소프트웨어니까요.
- **보내던 로그는 끝까지 나갑니다.** DMA가 이미 주소와 개수를 받아 뒀기 때문에, CPU가 죽어도 남은 바이트를 다 내보냅니다.

"CPU가 하던 일은 전부 멈추지만, DMA에 넘긴 일은 끝까지 간다"를 한 화면에서 볼 수 있습니다.

### 6-5. 결과 적어 두기

리셋 후 **30초**씩 돌리고 채우세요.

| 지표 | 예측 (CPU) | 실측 (CPU) | 예측 (DMA) | 실측 (DMA) |
|---|---|---|---|---|
| `jit` | 약 314 ms | | 0 ~ 1 ms | |
| `loop/s` | 기준 | | 약 5배 | |
| `a` 50번 → `rx` | 50 미만 | | 50 | |
| `ore` | 0 초과 | | 0 | |
| 🔴 vs 🟢 | 엇나감 | | 일치 | |

**결론은 한 줄입니다**: CPU 버전은 **81%의 시간을 UART에 빼앗겨** 1초 LED를 놓치고 수신 문자를 흘립니다. DMA 버전은 **같은 양을 같은 속도로 보내면서도** 둘 다 지킵니다.

---

## 7. 문제 해결

| 증상 | 확인할 것 |
|---|---|
| 빌드 에러: `LED_SW_Pin`이 없음 | CubeMX의 PA6 User Label 철자(`LED_SW`). 코드 생성을 다시 했는지 |
| 빌드 에러: `hdma_usart2_rx`가 없음 | DMA1 탭에서 **USART2_RX**를 추가했는지. 핸들 이름은 생성된 `main.c` 위쪽에서 확인 |
| 터미널에 아무것도 안 나옴 | COM 포트 번호, **115200** 설정, USART2 핀이 PA2/PA3인지 |
| **DMA 버전이 첫 줄만 보내고 멈춤** | NVIC에서 **USART2 global interrupt**를 켰는지. 가장 흔한 실수입니다 |
| DMA 버전 로그가 띄엄띄엄 빠짐 | 전송 시간 > `TX_PERIOD_MS`. `gState` 검사에서 건너뛰는 중입니다. 6-3 표의 값을 쓰세요 |
| **두 버전 다 `jit`이 0** | `TX_PERIOD_MS`가 `BLINK_MS`의 약수인지 확인(6-3 규칙 ②). `450U`를 쓰세요 |
| `jit`이 끝없이 커짐 | 전송 시간이 **1초를 넘었습니다.** `last_blink += BLINK_MS`가 따라잡지 못합니다. `LOG_LEN`을 줄이세요 |
| `rx`가 안 올라감 | PA3이 USART2_RX인지. USART2 Data Direction이 **Receive and Transmit**인지. 터미널이 로컬 에코만 하고 실제로 안 보내는 것은 아닌지 |
| **CPU 버전인데 `ore`가 0** | 전송이 **쉬는 틈에** 타이핑했을 수 있습니다. 로그가 쏟아지는 중에 빠르게 여러 번 누르세요 |
| DMA 버전인데 `rx`가 틀림 | RX가 **Circular**인지(Normal이면 256바이트 뒤 멈춤). `RX_BUF_LEN`과 `HAL_UART_Receive_DMA()`의 개수가 같은지 |
| 🟢 LD2가 안 깜빡임 | PA5가 **TIM2_CH1 / Output Compare**인지. `HAL_TIM_OC_Start()`를 호출했는지. Mode가 **Toggle on match**인지 |
| 🔴 외부 LED가 안 켜짐 | 애노드(+)가 PA6 쪽인지, 저항이 직렬로 들어갔는지 |
| 로그 문자가 깨져 나옴 | 보드와 터미널의 보드레이트 불일치. 클럭 설정(HCLK 180 MHz)이 맞는지 |
| 디버거 연결이 안 됨 | SYS → Debug = **Serial Wire** 누락. 보드의 RESET을 누른 채 연결 시도 |

---

## 8. 심화 과제

1. **세 번째 버전: 인터럽트 방식 (`USE_IT`)**
   `HAL_UART_Transmit_IT()`를 쓰는 버전을 추가해 **3자 비교**를 해 보세요.
   - 이것도 블로킹이 아니라서 `jit`은 DMA처럼 0에 가깝습니다.
   - 하지만 **바이트마다 인터럽트가 한 번** 걸립니다. 4,194바이트 → **4,194번**.
   - `loop/s`를 DMA 버전과 비교해 보세요. **"논블로킹"과 "CPU를 안 쓴다"는 다른 말**임을 알 수 있습니다.

2. **버퍼를 덮어쓰면 어떻게 깨지는가**
   DMA 버전에서 `huart2.gState == HAL_UART_STATE_READY` 검사를 **지우고**, `TX_PERIOD_MS`를 `200U`로 낮춰 보세요.
   - DMA가 읽고 있는 `log_buf`를 CPU가 중간에 고쳐 쓰게 됩니다.
   - 로그가 어떻게 섞이는지 관찰하고, **왜 CPU 버전에는 이 검사가 필요 없는지** 설명해 보세요.

3. **핑퐁 버퍼 (이중 버퍼)**
   `log_buf`를 2개로 만들어, DMA가 A를 보내는 동안 CPU가 B를 채우게 하세요.
   - 과제 2의 문제를 **건너뛰지 않고** 해결하는 방법입니다.

4. **보드레이트를 올려 보기**
   USART2를 **921600 bps**로 바꾸면 전송 시간이 1/8(약 45 ms)이 됩니다.
   - CPU 버전의 `jit`이 얼마로 줄어들지 **먼저 계산**하고 확인하세요.
   - 생각할 점: 그렇다면 "속도를 올리면 DMA가 필요 없나요?" 로그 길이를 8배로 늘리면 어떻게 되나요?

5. **LED를 타이머 인터럽트로 옮기기**
   🔴 LED 토글을 메인 루프에서 빼서 **TIM 업데이트 인터럽트**로 옮기세요.
   - CPU 버전에서도 `jit`이 0이 됩니다. **인터럽트는 블로킹을 뚫기 때문**입니다.
   - 그렇다면 왜 DMA를 쓰나요? 과제 1의 결과와 함께 생각해 보세요.

6. **수신한 명령으로 실시간 제어**
   터미널에서 `+` / `-`를 눌러 `LOG_LEN`을 실행 중에 바꿔 보세요.
   - CPU 버전에서는 이 명령조차 **자주 씹힙니다.** 그게 핵심입니다.

---

## 부록: 첫 프로젝트와 무엇이 다른가

| | `PROJECT_GUIDE.md` (후진 보조) | 이 프로젝트 (UART 폭주) |
|---|---|---|
| 프로젝트 수 | 2개 (`Parking_CPU`, `Parking_DMA`) | **1개** (`#define USE_DMA`로 전환) |
| 비교 대상 | ADC, 입력 캡처, 타이머→DMA, UART (4곳) | **UART 송신 + 수신 (2곳)** |
| 부하를 만드는 법 | `EXTRA_WORK_MS`로 **인공 지연** 삽입 | **UART 전송 자체가 부하** (인공 지연 없음) |
| 깨지는 것 | 부저 박자 (귀) | **1초 LED (눈) + 수신 문자 (숫자)** |
| 기준(대조군) | 없음 — CPU에 남겨 둔 일로 간접 비교 | **하드웨어 LED**가 항상 정확한 기준을 제공 |

첫 프로젝트는 "DMA를 여러 주변장치에 써 보는" 폭넓은 연습이고, 이 프로젝트는 **"블로킹 전송 하나가 시스템을 어떻게 망치는지"**를 좁고 깊게 보는 실험입니다. 인공 지연(`HAL_Delay`)이 없어서 **"일부러 느리게 만든 거 아니냐"는 반론이 통하지 않는다**는 점이 이 실험의 강점입니다.
