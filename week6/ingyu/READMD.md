# STM32F446ZE: NVIC부터 부팅, ISR 복귀까지

> 보드: NUCLEO-F446ZE / MCU: STM32F446ZETx, Arm Cortex-M4F  
> 환경: STM32CubeIDE + GCC + ST-LINK(SWD)  
> 프로젝트: `F446_EXTI_LED`

NVIC, Vector Table, Startup, Linker, Build, Boot, ISR 진입·복귀를 하나의 흐름으로 정리했다. 기본 예시는 내부 Flash에서 부팅하는 비 RTOS 프로그램이다.

> **검증 범위**  
> 파일명·프로젝트 구조와 Debug 수치는 제공된 초안의 기록이다. 실제 `.s`, `.ld`, `.c`, `.elf`, `.map`은 제공되지 않아 로컬 코드 대조나 보드 재실행은 하지 않았다. 초기화 순서는 ST 공식 GCC startup에서 확인했으며, 로컬 파일과 동일하다고 단정하지 않는다. 설명용 코드와 공식 비교 자료를 구분했고, 참고 자료는 2026-09-09에 확인했다.

---

## 0. 프로젝트에서 살펴볼 파일

초안에 기록된 주요 파일은 다음과 같다. 로컬 파일명은 **ZETX 기준**이다.

```text
F446_EXTI_LED/
├─ Core/Startup/startup_stm32f446zetx.s
├─ Core/Src/
│  ├─ main.c
│  ├─ system_stm32f4xx.c
│  ├─ stm32f4xx_it.c
│  └─ sysmem.c
├─ Drivers/
├─ Debug/ 또는 Release/
├─ F446_EXTI_LED.ioc
├─ STM32F446ZETX_FLASH.ld
└─ STM32F446ZETX_RAM.ld
```

공식 비교 자료인 `startup_stm32f446xx.s`는 F446 계열 공통 템플릿이다. 이를 참고한다고 로컬 파일명을 바꿀 필요는 없다. 또한 FLASH/RAM linker script가 둘 다 있어도, 실제 사용하는 파일은 빌드의 `-T` 옵션으로 결정된다.[^startup][^gnu-ld]

## 1. 전체 흐름

```text
Power ON / Reset
       ↓
부팅 메모리 선택 및 0x00000000 영역 매핑
       ↓
CPU: Vector[0] → 초기 MSP / Vector[1] → Reset_Handler
       ↓
Reset_Handler                       ← 공식 비교 템플릿의 순서
 ├─ SP를 _estack으로 다시 설정
 ├─ SystemInit()
 ├─ .data: Flash → RAM 복사
 ├─ .bss: 0으로 초기화
 ├─ __libc_init_array()
 └─ main()
       ↓
HAL_Init → SystemClock_Config → 주변장치 초기화 → 일반 코드
                                                       │
                                                주변장치 IRQ 발생
                                                       ↓
                                      NVIC 상태·우선순위와 CPU 마스크 확인
                                                       ↓
                                       Exception Entry: context 저장
                                            벡터에서 ISR 주소 읽기
                                                       ↓
                                                    ISR 실행
                                                       ↓
                                       Exception Return: context 복원
                                                       ↓
                                            중단된 코드 이어서 실행
```

논리적 순서를 나타낸 그림이며, 실제 하드웨어의 스택 저장과 벡터 읽기는 겹쳐 진행될 수 있다. `main()` 내부 흐름은 일반적인 CubeMX/HAL 프로젝트 예시다.[^startup][^pm]

---

## 2. NVIC: 어떤 인터럽트를 처리할지 관리한다

**NVIC(Nested Vectored Interrupt Controller)**는 Cortex-M4 코어에 통합된 인터럽트 컨트롤러다. Timer, UART, EXTI, DMA 등의 IRQ에 대해 Enable, Pending, Active, Priority를 관리한다. 여기서 외부 IRQ는 코어 밖 주변장치의 IRQ라는 뜻이지, 반드시 칩 바깥 신호라는 뜻은 아니다.[^pm]

| 상태 | 의미 |
|---|---|
| Inactive | 대기 중이지도, 처리 중이지도 않음 |
| Pending | 요청이 접수되어 처리를 기다림 |
| Active | 처리를 시작했고 아직 종료하지 않음. 다른 ISR에 선점된 경우도 포함 |
| Active + Pending | 처리 중인 IRQ에 추가 요청이 대기 중임 |

Enable은 이 상태와 별개다. **Disable해도 Pending은 자동으로 지워지지 않는다.** Pending 비트는 횟수를 세는 큐도 아니므로, 여러 이벤트의 발생 횟수가 모두 보관되지는 않는다.[^pm]

| 레지스터 | 역할 |
|---|---|
| `ISER` / `ICER` | IRQ 허용 / 금지 |
| `ISPR` / `ICPR` | Pending 설정 / 해제 |
| `IABR` | Active 확인 |
| `IPR` | IRQ 우선순위 설정 |

### 우선순위와 선점

STM32F446의 `__NVIC_PRIO_BITS`는 **4**이므로 16개 priority 값을 표현한다. **숫자가 작을수록 우선순위가 높으며**, grouping에 따라 이 4비트를 나눠 쓴다.[^device][^hal]

| 구분 | 역할 |
|---|---|
| 선점 우선순위, preemption priority | 실행 중인 ISR을 중단하고 먼저 실행할 수 있는지 판단 |
| 서브 우선순위, subpriority | 선점 우선순위가 같은 Pending IRQ 중 처리 순서 결정 |

**서브 우선순위만으로 실행 중인 ISR을 선점할 수는 없다.** ST HAL의 `HAL_Init()`은 기본적으로 선점 4비트·서브 0비트인 `NVIC_PRIORITYGROUP_4`를 설정하지만, 이후 변경될 수 있다. 이 기본 그룹에서의 예시는 다음과 같다.[^hal]

```c
/* 설명용 예시: 선점 우선순위 5, 서브 우선순위는 사용하지 않음 */
HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
```

CMSIS의 `NVIC_SetPriority()`에는 비트 위치에 맞춰 미리 시프트한 값이 아니라 논리적 priority 값을 전달한다.[^cmsis-core]

### 요청부터 실행까지

```text
주변장치 인터럽트 조건 충족 → NVIC Pending
  → IRQ Enable 확인
  → PRIMASK / BASEPRI 등 마스크 확인
  → 현재 실행 우선순위보다 선점 우선순위가 높은가?
  → 조건을 만족하면 CPU가 Exception Entry 수행
```

`PRIMASK=1`도 NMI와 HardFault까지 막지는 않는다. 또한 NVIC Enable만으로 주변장치 설정이 끝나지는 않는다. EXTI라면 입력·트리거·EXTI 마스크 설정도 맞아야 한다.[^pm][^hal-gpio]

---

## 3. Vector Table과 Reset 직후의 동작

### 벡터는 정해진 위치의 주소를 읽는 테이블이다

공식 startup의 `.isr_vector`는 다음 순서로 구성된다. **첫 항목은 함수 주소가 아니라 초기 MSP 값**이다.[^startup]

```text
Vector[0]       초기 MSP, 보통 _estack
Vector[1]       Reset_Handler 주소
Vector[2]       NMI_Handler 주소
Vector[3]       HardFault_Handler 주소
   ...
Vector[15]      SysTick_Handler 주소
Vector[16 + n]  외부 IRQ n의 handler 주소
```

실행 중 IRQ의 벡터 항목은 `SCB->VTOR + 4 × (16 + IRQn)`에 있다. STM32F446의 `EXTI15_10_IRQn=40`이면 예외 번호는 `56`, 오프셋은 `0xE0`이다. CPU는 함수 이름을 검색하지 않고 이 항목의 주소로 분기한다.[^pm][^device]

### 최초 MSP 설정은 하드웨어가 한다

내부 Flash 부팅에서는 Flash 시작 영역이 `0x00000000`에도 보이도록 매핑된다.[^ds][^system-old]

```text
Flash의 원래 주소               Reset 직후 읽는 주소
0x08000000  Vector[0] ────────→ 0x00000000 → 초기 MSP 로드
0x08000004  Vector[1] ────────→ 0x00000004 → Reset_Handler 진입
```

**CPU가 먼저 MSP와 시작 위치를 얻은 뒤 startup 명령어를 실행한다.** 공식 startup의 `ldr sp, =_estack`은 그 다음에 SP를 다시 설정하는 소프트웨어 동작이다.[^pm][^startup]

벡터의 handler 주소는 Thumb 상태를 나타내는 bit 0이 1이어야 한다. 실제 명령어 주소는 정렬된 주소이므로, 벡터의 홀수 값과 디버거가 보여주는 PC의 짝수 값이 다르게 보일 수 있다.[^pm]

### VTOR 설정은 코드 버전에 따라 다르다

`SCB->VTOR`는 실행 중 사용할 벡터 테이블의 기준 주소다. 메모리 자체의 부팅 주소 매핑과 VTOR 변경은 다른 동작이다.[^pm]

확인한 `cmsis-device-f4 v2.6.10`은 `USER_VECT_TAB_ADDRESS`가 정의되었을 때만 VTOR를 쓴다. 반면 2026-09-09에 확인한 `master`는 그 설정이 없으면 `FLASH_BASE`를 쓴다. Flash 부팅에서 VTOR를 0으로 유지하는 코드도 부팅 주소 매핑을 통해 동작할 수 있다.[^system-old][^system-new]

따라서 “SystemInit이 항상 VTOR를 `0x08000000`으로 바꾼다”라고 일반화하지 않는다. 현재 프로젝트는 로컬 `system_stm32f4xx.c`와 실행 중 VTOR 값으로 확인해야 한다.

---

## 4. Startup Code: `main()` 전에 실행 환경을 만든다

### 확인한 초기화 순서

ST 공식 `startup_stm32f446xx.s`의 `v2.6.10`과 확인 시점 `master`에서 아래 순서를 확인했다. 초안의 순서와 일치하지만, 제공되지 않은 로컬 `.s`까지 직접 대조한 결과는 아니다.[^startup]

```text
Reset_Handler
  → SP ← _estack
  → SystemInit()
  → .data 초기값을 Flash에서 RAM으로 복사
  → .bss를 0으로 채움
  → __libc_init_array()
  → main()
```

`SystemInit()`은 조건에 따른 FPU 접근 허용, 선택적인 외부 메모리 초기화, 버전·설정에 따른 VTOR 처리를 담당한다. `main()` 안에서 애플리케이션 클럭을 구성하는 `SystemClock_Config()`와는 다른 함수다.[^system-new]

```text
Reset_Handler → SystemInit() → 메모리·런타임 준비 → main()
                                                    ├─ HAL_Init()
                                                    ├─ SystemClock_Config()
                                                    └─ 주변장치 초기화
```

이 순서에서는 `SystemInit()`이 `.data`·`.bss` 초기화보다 먼저 실행되므로, 이 함수를 수정할 때 전역변수가 이미 C 초기값으로 준비되어 있다고 가정하면 안 된다. `__libc_init_array()`는 초기화 함수·정적 생성자 등을 실행하며, 앞의 메모리 초기화까지 혼자 수행하는 함수는 아니다.[^startup]

### `.data`, `.bss`와 linker symbol

```c
/* 일반적인 GCC 배치 예시. 최적화·section 설정에 따라 달라질 수 있음 */
int count = 10;        // .data: 초기값은 Flash, 실행 중 값은 RAM
int button_count;     // .bss: 시작 시 0
static int state = 0; // 명시적으로 0을 써도 보통 .bss
```

| Symbol | 의미 |
|---|---|
| `_estack` | 초기 스택 상단 경계 |
| `_sidata` | Flash의 .data 초기값 시작 주소 |
| `_sdata`, `_edata` | RAM의 .data 시작 주소와 끝 다음 주소 |
| `_sbss`, `_ebss` | RAM의 .bss 시작 주소와 끝 다음 주소 |

Linker script가 정의한 이 주소를 startup이 사용한다. **Linker가 배치하고, startup이 복사·초기화한다.**[^ld][^startup]

```text
Flash: _sidata부터 .data 크기만큼 → RAM: [_sdata, _edata)에 복사
RAM:   [_sbss, _ebss)            → 0으로 채움
```

`.bss` 설명은 전역/static 저장 영역에 대한 것이다. 초기화하지 않은 일반 지역변수까지 자동으로 0이 된다는 뜻은 아니다.

### weak handler와 Default_Handler

공식 startup은 IRQ handler를 weak symbol로 `Default_Handler`에 연결한다. `stm32f4xx_it.c` 등에 같은 이름의 일반 정의를 제공하면 링크할 때 그 구현이 선택된다.[^startup]

```text
벡터의 EXTI15_10_IRQHandler
  ├─ 실제 구현 있음 → 해당 ISR
  └─ 구현 없음      → Default_Handler의 무한 루프
```

---

## 5. Linker Script: 코드와 데이터를 어디에 배치할지 정한다

### 5.1 메모리 영역과 `_estack`

STM32F446ZE의 내부 Flash는 512 KiB, 일반 SRAM은 128 KiB다. ST의 NUCLEO-F446ZE용 `STM32F446ZETX_FLASH.ld`도 다음 영역을 사용한다. [^ds][^ld]

| 영역 | 시작 주소 | 마지막 유효 바이트 | 크기 |
|---|---|---|---|
| Flash | `0x08000000` | `0x0807FFFF` | 512 KiB |
| SRAM | `0x20000000` | `0x2001FFFF` | 128 KiB |

```text
_estack = ORIGIN(RAM) + LENGTH(RAM)
        = 0x20000000 + 0x20000
        = 0x20020000
```

`0x20020000`은 RAM의 마지막 유효 바이트가 아니라 **RAM 끝 다음 경계**다. Cortex-M의 스택은 full descending 방식이므로 공간을 확보할 때 SP를 낮춘 뒤 저장한다.[^ld][^arm-stack]

```text
높은 주소
0x20020000  ← 초기 SP / _estack: 아직 저장하지 않은 상단 경계
0x2001FFFF  ← RAM의 마지막 유효 바이트
0x2001FFFC  ← 4바이트를 처음 push한다면 사용하는 word 시작 주소
0x2001FFF8
     ↓ 스택은 낮은 주소 방향으로 성장
0x20000000  ← RAM 시작
낮은 주소
```

`_Min_Stack_Size`는 최소 공간 확보·링크 시 용량 점검을 돕지만, **실행 중 스택 크기를 제한하거나 overflow를 막는 장치는 아니다.**[^ld][^arm-stack]

### 5.2 주요 section

| Section 또는 영역 | 대표 내용 | 일반적인 Flash 실행 구성 |
|---|---|---|
| `.isr_vector` | 초기 MSP와 예외 handler 주소 | Flash |
| `.text` | 기계어 코드 | Flash |
| `.rodata` | 읽기 전용 상수 데이터·문자열 | Flash |
| `.data` | 0이 아닌 초기값 등을 가진 전역/static 저장 영역 | 사용 위치 RAM, 초기값 이미지 Flash |
| `.bss` | 0으로 초기화할 전역/static 저장 영역 | RAM, startup이 0으로 채움 |
| Heap | 동적 메모리 할당에 사용하는 영역 | RAM |
| Stack | 함수 호출, 필요한 지역 저장 공간, 예외 context | RAM |

위 표는 일반적인 배치다. 지역변수는 레지스터에 놓일 수도 있고, 상수·코드도 최적화나 section 설정에 따라 배치가 달라질 수 있다.[^ld][^arm-stack]

```text
낮은 주소

Flash: 0x08000000           RAM: 0x20000000
┌────────────────────┐    ┌────────────────────┐
│ .isr_vector        │    │ .data              │
│ .text              │    │ .bss               │
│ .rodata 등         │    │ Heap               │
│ .data 초기값       │    │   ↓ 높은 주소 방향 │
└────────────────────┘    │   남은 공간        │
                          │   ↑ 낮은 주소 방향 │
                          │ Stack              │
                          └────────────────────┘
                          0x20020000: 상단 경계
높은 주소
```

이는 개념도다. 실제 section 순서, 정렬 간격과 heap 관리는 linker script 및 `sysmem.c` 등의 구현을 따른다.[^ld]

### 5.3 `.data`의 LMA와 VMA

**VMA**는 실행 중 section을 사용하는 주소, **LMA**는 초기 이미지를 저장·로드할 주소다. VMA라는 용어가 여기서 운영체제의 가상 메모리를 사용한다는 뜻은 아니다.[^gnu-lma]

```text
.data의 LMA: Flash — 펌웨어가 보관하는 초기값
.data의 VMA: RAM   — 프로그램이 읽고 쓰는 실제 변수 위치
```

다음은 **축약 예시이며 교체용 전체 script가 아니다.** 공식 파일의 Flash 영역 이름은 `ROM`이지만, 여기서는 설명을 위해 `FLASH`로 적었다.[^ld][^gnu-lma]

```ld
_sidata = LOADADDR(.data);

.data :
{
    . = ALIGN(4);
    _sdata = .;
    *(.data .data.*)
    . = ALIGN(4);
    _edata = .;
} > RAM AT > FLASH
```

`> RAM`은 VMA, `AT > FLASH`는 LMA를 지정한다. **Linker가 배치·초기값 이미지를 만들고 startup이 복사한다.** `.data`가 RAM에서 “실행된다”는 표현은 여기서는 변수를 RAM에서 사용한다는 의미다.[^gnu-lma][^startup]

벡터 테이블에는 보통 `KEEP(*(.isr_vector))`를 사용해 미사용 section 제거의 영향을 받지 않도록 한다. 또한 `ENTRY(Reset_Handler)`는 ELF의 entry point 지정이며, CPU가 Reset 때 벡터를 읽는 하드웨어 동작을 대신하는 명령이 아니다.[^gnu-ld][^pm]

---

## 6. 컴파일·빌드와 결과 파일

```text
.c → 전처리 → 컴파일 → 어셈블 → .o ─┐
                                  │
.s ───────────────→ 어셈블 → .o ───┤
                                  ▼
                       Linker + .ld + 라이브러리
                                  │
                                  ▼
                           .elf / .map(선택)
                                  │
                            objcopy, 선택 사항
                             ┌────┴────┐
                             ▼         ▼
                            .hex      .bin
```

C 소스도 최종적으로는 어셈블 단계를 거쳐 object file이 된다. 중간 어셈블리 파일이 디스크에 별도로 남지 않아도 단계 자체가 없어지는 것은 아니다.[^gcc]

| 파일 | 용도와 특징 |
|---|---|
| `.o` | 기계어·symbol·재배치 정보 등을 가진 링크 전 object file |
| `.ld` | 메모리 영역과 section 배치 규칙을 정하는 linker script |
| `.elf` | 링크된 코드·데이터·주소·symbol 등을 포함. 디버그 정보가 있으면 소스 줄과 기계어 주소 연결 가능 |
| `.hex` | 주소와 checksum 등이 담긴 Intel HEX 텍스트 형식의 펌웨어 이미지 |
| `.bin` | 주소 메타데이터가 없는 바이너리 이미지. 기록 도구에 올바른 시작 주소를 별도로 지정해야 함 |
| `.map` | section·symbol 등의 최종 배치를 기록한 linker 출력. MCU가 실행할 펌웨어는 아님 |

`.map`과 HEX/BIN은 관련 옵션을 설정했을 때 생성하는 선택 산출물이다. ELF를 이용하는 디버깅·다운로드 흐름에서 두 파일이 항상 필요한 것은 아니다. ELF의 모든 디버그 정보가 MCU Flash에 기록되는 것도 아니다.[^binutils]

### 빌드 결과를 직접 확인하는 명령

다음 명령은 프로젝트 최상위 폴더와 Arm GNU 도구가 PATH에 잡힌 환경을 가정한 **확인용 예시**다. 실제 산출물이 Release에 있다면 경로를 바꾼다. 이 문서 작성 과정에서 프로젝트 ELF로 실행한 결과는 아니다.[^binutils]

```bash
# section별 VMA/LMA와 크기 확인
arm-none-eabi-objdump -h Debug/F446_EXTI_LED.elf

# 주소 순서로 symbol 확인: _estack, _sdata, Reset_Handler 등
arm-none-eabi-nm -n Debug/F446_EXTI_LED.elf

# Reset_Handler의 실제 명령어 확인
arm-none-eabi-objdump -d --disassemble=Reset_Handler Debug/F446_EXTI_LED.elf

# 벡터 테이블의 실제 저장 내용 확인
arm-none-eabi-objdump -s -j .isr_vector Debug/F446_EXTI_LED.elf
```

---

## 7. ISR 진입: 무엇이 스택에 저장되는가?

### 7.1 CPU가 저장하는 기본 exception frame

IRQ가 받아들여지면 CPU는 기존 실행을 재개하는 데 필요한 다음 8개 레지스터를 하드웨어로 저장한다. **기본 frame은 8 word, 32바이트**다.[^pm]

| 기본 frame 시작 주소 기준 offset | 저장 값 |
|---|---|
| `+0x00` | R0 |
| `+0x04` | R1 |
| `+0x08` | R2 |
| `+0x0C` | R3 |
| `+0x10` | R12 |
| `+0x14` | 중단된 코드의 LR |
| `+0x18` | 복귀 후 실행을 재개할 PC |
| `+0x1C` | 중단된 코드의 xPSR |

스택의 LR은 **중단된 코드의 LR**이며, handler 진입 시 LR에 들어가는 `EXC_RETURN`과는 다른 값이다.[^pm]

### 7.2 MSP와 PSP

Thread Mode에서는 MSP 또는 PSP를 사용할 수 있고, **Handler Mode에서는 MSP를 사용한다.** 예외 frame은 예외 직전에 사용하던 스택에 저장된다.[^pm][^arm-stack]

```text
일반적인 비 RTOS 예시
  main: MSP 사용 → frame도 MSP에 저장 → ISR도 MSP 사용

PSP를 쓰는 Thread 예시
  Thread: PSP 사용 → frame은 PSP에 저장 → ISR 자체는 MSP 사용
```

따라서 ISR의 현재 SP가 언제나 중단된 thread의 frame을 가리키지는 않는다.[^pm]

### 7.3 SP 변화가 항상 32바이트는 아닌 이유

기본 frame 외에 스택 정렬용 공간, FPU context, 컴파일러가 ISR에서 추가 저장한 레지스터와 지역 저장 공간이 필요할 수 있다. R4~R11은 기본 하드웨어 frame에 포함되지 않으며, C ISR에서 보존이 필요한 레지스터는 컴파일러가 함수 진입·종료 코드를 통해 처리한다.[^pm][^arm-stack]

FPU 확장 frame은 기본 8 word에 S0~S15, FPSCR, 예약 word가 추가되어 **26 word, 104바이트**가 된다. 정렬과 소프트웨어 저장 공간은 별도다. FPU가 있는 MCU라고 모든 IRQ에 확장 frame이 생기는 것은 아니다. **Lazy stacking은 필요한 공간을 확보하되 FP 레지스터의 실제 저장을 필요 시점까지 미루는 방식**이며, S16~S31은 이 하드웨어 확장 frame에 포함되지 않는다.[^pm][^arm-fpu]

현재 SP 차이만으로 하드웨어 frame 크기를 단정하지 않는다.

---

## 8. 벡터에서 ISR, HAL callback까지

공식 HAL 기반 EXTI 처리 흐름은 다음과 같다. 현재 프로젝트의 버튼 핀·callback 구현은 제공된 코드로 확인하지 못했다.[^hal-gpio]

```text
GPIO 입력 변화 → EXTI 트리거·마스크 조건 충족 → NVIC 요청
  → Exception Entry → 벡터 주소 읽기
  → EXTI15_10_IRQHandler()
  → HAL_GPIO_EXTI_IRQHandler(해당 핀)
  → 해당 EXTI pending 확인·해제
  → HAL_GPIO_EXTI_Callback(해당 핀) → 사용자 로직
```

**NVIC Pending과 EXTI 주변장치의 pending flag는 별개다.** CPU가 IRQ를 받아들여도 주변장치 원인까지 자동 해결되는 것은 아니다. 공식 HAL handler는 해당 EXTI flag를 확인·해제한 뒤 callback을 호출한다.[^pm][^hal-gpio]

NVIC Pending만 지우고 원인을 남기면 다시 IRQ가 요청될 수 있다. `EXTI15_10`은 10~15번 선이 공유하므로, 여러 선을 사용한다면 관련 원인을 모두 처리해야 한다.[^device][^hal-gpio]

---

## 9. ISR이 끝나면 어떻게 원래 코드로 돌아오는가?

### EXC_RETURN은 복귀 주소가 아니라 복귀 방법이다

일반 함수 호출의 `BL`은 LR에 코드 복귀 주소를 기록한다. 반면 예외 진입 시 CPU는 LR에 **EXC_RETURN**이라는 특수 값을 넣는다. 이는 원래 코드 주소가 아니라 복귀 모드·스택·frame 종류를 나타낸다.[^pm]

| 기본 frame의 EXC_RETURN | 복귀 대상 | frame을 복원할 스택 |
|---|---|---|
| `0xFFFFFFF1` | 선점되었던 ISR, Handler Mode | MSP |
| `0xFFFFFFF9` | Thread Mode | MSP |
| `0xFFFFFFFD` | Thread Mode | PSP |

FPU 확장 frame의 값은 각각 `0xFFFFFFE1`, `0xFFFFFFE9`, `0xFFFFFFED`다. EXC_RETURN의 bit 4는 기본 frame이면 1, 확장 frame이면 0이다.[^pm]

```text
ISR 코드 종료
  → 컴파일러가 추가 저장한 레지스터·스택 공간 정리
  → 보존된 EXC_RETURN으로 복귀
  → CPU가 Exception Return으로 인식
  → 선택된 스택의 R0~R3, R12, LR, PC, xPSR 등 복원
  → 복귀 모드·스택 상태 전환
  → 저장된 PC에서 실행 재개
```

`BX LR`이나 해당 값을 PC로 복원하는 종료 명령이 유효한 EXC_RETURN을 사용하면 CPU가 예외 복귀를 수행한다. “main의 몇 번째 줄로 돌아가라”는 코드를 작성할 필요가 없는 이유는 **스택의 PC와 processor state가 복구되기 때문**이다.[^pm]

### ISR 내부의 LR이 항상 EXC_RETURN은 아니다

ISR이 다른 함수를 `BL`로 호출하면 LR은 일반 함수 복귀 주소로 바뀐다. C 컴파일러는 원래 복귀 정보를 보존해 최종 ISR 종료 때 사용하도록 처리한다.[^pm][^arm-stack]

```text
ISR 첫 명령어의 LR       → EXC_RETURN
내부 함수를 호출한 후 LR → 일반 코드 주소일 수 있음
최종 ISR 종료에 쓰는 값  → 보존해 둔 EXC_RETURN
```

따라서 callback 내부에서 멈췄는데 LR이 `0xFFFFFFF9`가 아니라고 오류는 아니다. **ISR 첫 명령어와 C 함수 호출 이후의 중단점을 구분한다.**[^pm]

---

## 10. 중첩 인터럽트와 Tail-Chaining

선점 우선순위가 더 높은 IRQ가 허용된 상태로 들어오면 현재 ISR도 중단될 수 있다. 이때 복귀 대상은 곧바로 `main()`이 아니라 선점되었던 ISR이다.[^pm]

```text
main → IRQ A → 더 높은 우선순위 IRQ B
                        ↓ 종료
                   IRQ A 재개 → 종료 → main 재개
```

같은 IRQ는 자기 자신을 선점하지 않는다. 실행 중 새 요청이 Pending되면 종료 후 다시 처리될 수 있다.[^pm]

**Tail-Chaining**은 ISR 종료 때 이어서 처리 가능한 Pending 예외가 있으면 불필요한 context 복원·재저장을 생략하고 다음 handler로 넘어가는 최적화다. 매번 `ISR → main → 다음 ISR` 순서를 거치지는 않는다.[^pm]

---

## 11. 초안에 기록된 Debug 관찰값 해석

초안에는 `main.c` 약 92번째 줄에서 중단했을 때 다음 값을 보았다고 기록되어 있다. **아래는 그 시점의 관찰 기록이지, 이 문서 작성 중 보드에서 재현한 결과나 고정된 초기값이 아니다.**

| 항목 | 기록된 값 | 해석 |
|---|---|---|
| PC | `0x08000536` | 주소 범위상 내부 Flash 영역 |
| SP | `0x2001FFF0` | 초기 상단 경계보다 낮은 스택 위치 |
| R7 | `0x2001FFF0` | SP와 같지만, 이유는 실제 함수 진입 코드 확인 필요 |
| xPSR | `0x61000000` | IPSR 필드가 0이므로 Thread Mode |
| LR | `0x08000A47` | EXC_RETURN 값이 아닌 일반 코드 주소 형태 |

초안의 `_estack = 0x20020000`을 기준으로 계산하면 다음과 같다.

```text
0x20020000 - 0x2001FFF0 = 0x10 = 16바이트
```

이는 해당 시점 SP가 상단 경계보다 16바이트 낮다는 뜻이다. **프로그램 전체의 최대 스택 사용량이 16바이트라는 뜻도, 인터럽트 진입으로 16바이트가 저장되었다는 뜻도 아니다.** 함수 진입 코드, 최적화, 중단 위치에 따라 값은 달라진다.[^arm-stack]

`xPSR & 0x1FF`로 읽는 IPSR 값은 이 기록에서 0이고, Thumb 상태를 나타내는 bit 24는 1이다. `PC=0x08000536`이 실제로 `main.c`의 어느 줄에 대응하는지는 해당 빌드의 ELF·디버그 정보 없이는 독립적으로 확인할 수 없다.[^pm][^binutils]

---

## 12. 다음 Debug 실습에서 확인할 것

| 실습 | 확인할 내용 |
|---|---|
| Reset_Handler 첫 명령어에서 중단 | 벡터 첫 두 word와 MSP·PC를 비교하고, 이후 `ldr sp, =_estack` 실행 관찰 |
| SystemInit 및 startup single-step | VTOR 전후 값, `.data` 복사, `.bss` 초기화, `BL` 전후 PC·LR 확인 |
| EXTI handler 첫 어셈블리 명령어에서 중단 | LR의 EXC_RETURN, MSP/PSP, 현재 PC, IPSR 확인 |
| 하드웨어 frame 확인 | 현재 ISR의 PC와 스택에 저장된 복귀 PC 비교 |

디버거가 이미 `main()`까지 실행했다면 Reset 직후 값으로 해석하지 않는다. 실제 중단점 주소와 실행된 함수 진입 코드를 함께 기록한다.[^startup][^pm]

EXTI15_10 처리 중 IPSR, 즉 `xPSR & 0x1FF`는 `40 + 16 = 56(0x38)`이다. 다른 IRQ에는 이 값을 그대로 적용하지 않는다.[^device]

**기본 frame이고, compiler prologue 실행 전이며, 중단된 코드가 MSP를 사용한 경우** 진입 시 MSP가 기본 frame 시작점이다. 이때 저장된 PC는 `MSP + 0x18`, xPSR은 `MSP + 0x1C`에서 확인한다.[^pm]

PSP를 사용했다면 그 스택을 확인해야 하고, FP 확장 frame이나 이미 실행된 prologue가 있다면 추가 분석이 필요하다. **C callback의 현재 SP에 무조건 `0x18`을 더하는 방식은 피한다.**[^pm][^arm-stack]

---

## 13. 면접·구두 설명용 요약

STM32F446ZE는 Cortex-M4 내부의 NVIC로 인터럽트의 허용·대기·우선순위를 관리합니다. 인터럽트가 처리 가능한 상태가 되면 CPU가 R0~R3, R12, LR, PC, xPSR을 기본 스택 frame에 저장하고, 벡터 테이블에서 해당 ISR 주소를 읽어 실행합니다.

Reset 때도 `main()`으로 바로 가는 것은 아닙니다. CPU가 벡터 첫 항목에서 초기 MSP를, 다음 항목에서 Reset_Handler 주소를 얻습니다. 확인한 ST startup은 SP 재설정, SystemInit, `.data` 복사, `.bss` 초기화, 런타임 초기화 후 `main()`을 호출합니다. 이때 사용하는 메모리 주소와 section 배치는 linker script가 정합니다.

ISR 종료 시에는 보존된 EXC_RETURN으로 예외 복귀를 수행합니다. CPU가 스택의 PC와 레지스터 상태를 복원하므로, 프로그래머가 복귀 위치를 직접 지정하지 않아도 중단되었던 코드가 이어집니다. 중첩 인터럽트였다면 먼저 선점되었던 ISR로 돌아갑니다.[^pm][^startup][^ld]

---

## 14. 참고 자료

공식 코드의 `master`는 바뀔 수 있다. 로컬 동작은 사용 중인 패키지 버전·소스·빌드 옵션·ELF를 함께 확인한다.

[^pm]: STMicroelectronics, **PM0214 — STM32 Cortex-M4 MCUs and MPUs Programming Manual**. 레지스터, NVIC, 예외 진입·복귀, stack frame·EXC_RETURN 표 참고. `https://www.st.com/resource/en/programming_manual/pm0214-stm32-cortexm4-mcus-and-mpus-programming-manual-stmicroelectronics.pdf`

[^ds]: STMicroelectronics, **DS10693 — STM32F446xC/xE Datasheet**. 메모리 용량·주소와 boot options 참고. `https://www.st.com/resource/en/datasheet/stm32f446ze.pdf`

[^startup]: STMicroelectronics, **cmsis-device-f4**, `Source/Templates/gcc/startup_stm32f446xx.s`. `v2.6.10`·확인 시점 `master`의 초기화 순서와 벡터·weak handler 확인. `https://github.com/STMicroelectronics/cmsis-device-f4/blob/v2.6.10/Source/Templates/gcc/startup_stm32f446xx.s`

[^system-old]: STMicroelectronics, **cmsis-device-f4 v2.6.10**, `Source/Templates/system_stm32f4xx.c`. 조건부 VTOR 설정과 부팅 주소 매핑 유지 설명 참고. `https://github.com/STMicroelectronics/cmsis-device-f4/blob/v2.6.10/Source/Templates/system_stm32f4xx.c`

[^system-new]: STMicroelectronics, **cmsis-device-f4 master**, `Source/Templates/system_stm32f4xx.c`, 2026-09-09 확인. 기본 `SCB->VTOR = FLASH_BASE` 분기 참고. `https://github.com/STMicroelectronics/cmsis-device-f4/blob/master/Source/Templates/system_stm32f4xx.c`

[^device]: STMicroelectronics, **cmsis-device-f4**, `Include/stm32f446xx.h`. `__NVIC_PRIO_BITS = 4U`, `__FPU_PRESENT = 1U`, `EXTI15_10_IRQn = 40` 참고. `https://github.com/STMicroelectronics/cmsis-device-f4/blob/master/Include/stm32f446xx.h`

[^ld]: STMicroelectronics, **STM32CubeF4**, NUCLEO-F446ZE의 공식 `STM32F446ZETX_FLASH.ld` 예시. 메모리·symbol·VMA/LMA·heap/stack 설정 참고. 로컬 파일과 동일 여부는 미확인. `https://github.com/STMicroelectronics/STM32CubeF4/blob/master/Projects/STM32F446ZE-Nucleo/Templates/STM32CubeIDE/STM32F446ZETX_FLASH.ld`

[^hal]: STMicroelectronics, **stm32f4xx-hal-driver**, `HAL_Init()`과 priority grouping·NVIC 함수 참고. `https://github.com/STMicroelectronics/stm32f4xx-hal-driver/blob/master/Src/stm32f4xx_hal.c` / `https://github.com/STMicroelectronics/stm32f4xx-hal-driver/blob/master/Src/stm32f4xx_hal_cortex.c`

[^hal-gpio]: STMicroelectronics, **stm32f4xx-hal-driver**, `Src/stm32f4xx_hal_gpio.c`. EXTI 설정 및 `HAL_GPIO_EXTI_IRQHandler()`의 flag 확인·해제·callback 호출 순서 참고. `https://github.com/STMicroelectronics/stm32f4xx-hal-driver/blob/master/Src/stm32f4xx_hal_gpio.c`

[^cmsis-core]: Arm, **CMSIS-Core**, `core_cm4.h`의 `__NVIC_SetPriority()` 및 NVIC priority API. 우선순위 인자의 내부 시프트 구현 참고. `https://github.com/ARM-software/CMSIS_5/blob/develop/CMSIS/Core/Include/core_cm4.h`

[^arm-stack]: Arm, **How much stack memory do I need for my Arm Cortex-M applications?**, Joseph Yiu. Descending stack, MSP/PSP, 하드웨어·소프트웨어 저장량과 스택 사용 분석 참고. `https://developer.arm.com/community/arm-community-blogs/b/architectures-and-processors-blog/posts/how-much-stack-memory-do-i-need-for-my-arm-cortex--m-applications`

[^arm-fpu]: Arm, **10 useful tips to using the Floating Point Unit on the Arm Cortex-M4 processor**, Ian Johnson. FPU context와 lazy stacking 설명 참고. `https://developer.arm.com/community/arm-community-blogs/b/architectures-and-processors-blog/posts/10-useful-tips-to-using-the-floating-point-unit-on-the-arm-cortex--m4-processor`

[^gnu-lma]: GNU Binutils, **GNU ld — Output Section LMA**. VMA/LMA와 `AT`, `AT>`의 의미 참고. `https://sourceware.org/binutils/docs/ld/Output-Section-LMA.html`

[^gnu-ld]: GNU Binutils, **GNU ld**. Scripts, Entry Point, Input Section Keep에 설명된 `-T`, `ENTRY`, `KEEP` 참고. `https://sourceware.org/binutils/docs/ld/Scripts.html` / `https://sourceware.org/binutils/docs/ld/Entry-Point.html` / `https://sourceware.org/binutils/docs/ld/Input-Section-Keep.html`

[^gcc]: GNU, **GCC — Options Controlling the Kind of Output**. 전처리·컴파일·어셈블·링크 단계 참고. `https://gcc.gnu.org/onlinedocs/gcc/Overall-Options.html`

[^binutils]: GNU, **Binutils**. `objcopy`, `objdump`, `nm`의 파일 형식·주소·symbol 처리와 확인 명령 참고. `https://sourceware.org/binutils/docs/binutils/objcopy.html` / `https://sourceware.org/binutils/docs/binutils/objdump.html` / `https://sourceware.org/binutils/docs/binutils/nm.html`

추가 원문: **RM0390 — STM32F446xx Reference Manual**의 Memory and bus architecture, Boot configuration, EXTI 항목. 공식 원문 위치는 `https://www.st.com/resource/en/reference_manual/dm00135183.pdf`이다. 이번 검토에서는 대용량 원문 전체를 직접 열람하지 못했다. 관련 설명은 PM0214·DS10693 및 공식 CMSIS/HAL 코드로 교차 확인했다.
