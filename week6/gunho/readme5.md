```markdown
# STM32F446RET6 기반 C/C++ 프로그램 컴파일·링크 및 부팅 과정 분석

STM32F446RET6 기반 C/C++ 프로그램이 GCC 툴체인으로 어떻게 컴파일·링크되어 바이너리로 배포되고, MCU 전원 인가 후 초기 부팅(Boot Sequence) 과정을 거쳐 `main()`에 도달하는지 핵심 코드 라인 및 구조 분석입니다.

---

## 1. GCC 기반 컴파일 및 빌드 과정 (Compilation & Linking Process)

GCC 툴체인은 소스 코드와 설정 파일(`LinkerScript.ld`, `startup_stm32f446xx.s`)을 거쳐 실행 가능한 ELF/HEX/BIN 파일으로 만듭니다.

### A. 소스 전처리, 컴파일 및 어셈블 (Source Build)
* C/C++ 소스 코드는 기계어 오브젝트 파일(`.o`)로 변환됩니다.
* 어셈블리 소스인 `startup_stm32f446xx.s` 역시 툴체인에 의해 어셈블되어 프로젝트의 최우선 메모리 배치 오브젝트로 준비됩니다.

### B. 링킹 과정 (Linking with Linker Script)
링커(`arm-none-eabi-gcc` / `ld`)는 여러 오브젝트 파일들을 메모리 맵(`STM32F446RETX_FLASH.ld`)에 지정된 섹션 규칙에 맞춰 결합합니다.

* **엔트리 포인트 지정:**  
  ```ld
  ENTRY(Reset_Handler)
  ```
  전원 복구 및 리셋 이벤트 시 CPU가 가장 먼저 시작해야 할 기호(Symbol)로 `Reset_Handler`를 정의합니다.  

* **메모리 영역 정의 (MEMORY):**  
  ```ld
  MEMORY {
      RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
      FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
  }
  ```
  FLASH는 `0x08000000`부터 512KB, RAM은 `0x20000000`부터 128KB 크기를 가짐을 명시합니다.  

* **스택 최상단 심볼 계산 (_estack):**  
  ```ld
  _estack = ORIGIN(RAM) + LENGTH(RAM); /* 0x20000000 + 128K = 0x20020000 */
  ```
  RAM 메모리의 가장 마지막 끝 지점을 초기 Stack Pointer(SP)의 주소로 할당합니다.  

* **주요 섹션 배치 (SECTIONS):**  
  * `.isr_vector`: 인터럽트 벡터 테이블. Flash 시작 위치인 `0x08000000`에 최우선 배치됩니다 (`KEEP(*(.isr_vector))`).  
  * `.text`: C 및 어셈블리 실행 코드 영역. Flash에 저장됩니다.  
  * `.data`: 초기값이 설정된 전역/정적 변수 섹션. LMA(Load Memory Address; Flash)에 보관된 후 초기화 시 VMA(Virtual Memory Address; RAM)로 복사됩니다.  
  * `.bss`: 초기화되지 않거나 0으로 초기화된 전역/정적 변수 섹션. RAM에만 할당됩니다.  

---

## 2. STM32F446 부팅 과정 (Boot Sequence)

MCU에 전원이 들어온 순간부터 유저 코드 `main()` 함수까지 진행되는 세부 과정입니다.

### [단계 1] 하드웨어 래칭 및 에이리어싱 (Hardware Boot Selection)
* **BOOT 핀 샘플링:**  
  리셋 신호 해제 직후 SYSCLK의 4번째 상승 에지 시점에 BOOT0, BOOT1 핀 값을 샘플링합니다.  
  * Normal Flash Boot: `BOOT0 = 0` 일 때 Main Flash Memory 선택.  
* **0x00000000 매핑:**  
  Cortex-M4 CPU는 언제나 `0x00000000` 주소에서 첫 2개 단어(Word)를 읽어옵니다. `BOOT0 = 0` 설정에 의해 `0x08000000` (Flash 영역)이 `0x00000000` 주소로 앨리어싱(Aliasing)됩니다.  

### [단계 2] 벡터 테이블 읽기 (Vector Table Fetch)
CPU는 `0x00000000` 영역에서 다음 두 값을 하드웨어적으로 페치(Fetch)합니다:  

* `0x00000000` (주소 0x0): Initial Main Stack Pointer (`_estack`)  
* `0x00000004` (주소 0x4): Reset Vector (`Reset_Handler` 주소값)  

이 값들은 `startup_stm32f446xx.s`의 벡터 테이블 구문에 배치되어 있습니다.  

```assembly
/* startup_stm32f446xx.s */
.section .isr_vector,"a",%progbits
.type g_pfnVectors, %object
g_pfnVectors:
    .word _estack        /* 0x00000000: Initial SP */
    .word Reset_Handler  /* 0x00000004: Reset Vector */
    .word NMI_Handler
    .word HardFault_Handler
    ...
```

### [단계 3] Reset_Handler 실행 (Software Setup)
CPU는 SP 레지스터에 `_estack` 주소를 로드하고, PC 레지스터를 `Reset_Handler`로 점프시킨 뒤 어셈블리 루틴을 수행합니다.  

* **Stack Pointer 재설정:**  
  ```assembly
  ldr sp, =_estack
  ```
  명시적으로 SP 레지스터에 스택 메모리 끝 주소를 설정합니다.  

* **.data 섹션 복사 (Flash -> SRAM):**  
  Flash 메모리에 저장되어 있는 초기화 데이터 변수값들을 RAM 영역으로 복사합니다.  
  ```assembly
  /* Copy the data segment initializers from flash to SRAM */
    movs r1, #0
    b LoopCopyDataInit

  CopyDataInit:
    ldr r3, =_sidata    /* Flash 내 data 초기값 시작 주소 */
    ldr r3, [r3, r1]
    str r3, [r0, r1]    /* SRAM 내 data 시작 주소(_sdata) */
    adds r1, r1, #4

  LoopCopyDataInit:
    ldr r0, =_sdata
    ldr r3, =_edata
    adds r2, r0, r1
    cmp r2, r3
    bcc CopyDataInit
  ```

* **.bss 섹션 Zero 초기화:**  
  초기화값이 없는 변수들이 위치할 SRAM의 BSS 영역을 전부 0으로 채웁니다.  
  ```assembly
    ldr r2, =_sbss
    b LoopFillZerobss

  FillZerobss:
    movs r3, #0
    str r3, [r2], #4

  LoopFillZerobss:
    ldr r3, = _ebss
    cmp r2, r3
    bcc FillZerobss
  ```

* **클럭 및 시스템 초기화 (SystemInit):**  
  시스템 클럭, FPU, Vector Table Offset 등의 기본 하드웨어 환경을 설정하는 C 함수인 `SystemInit()`을 호출합니다.  
  ```assembly
  bl SystemInit
  ```

* **C runtime / C++ 생성자 초기화 (__libc_init_array):**  
  C 표준 라이브러리 및 C++ 전역 객체 생성자(Global Constructors)를 실행합니다.  
  ```assembly
  bl __libc_init_array
  ```

### [단계 4] 애플리케이션 진입 (main)
모든 런타임 환경 설정이 정상적으로 완료되면, 최종적으로 유저의 진입점인 `main()` 함수로 분기합니다.  

```assembly
bl main
```

만약 `main()` 함수가 종료되는 특수 상황이 발생하면 `bx lr`을 통해 복귀를 시도하거나 무한 루프에 빠지게 됩니다.

```