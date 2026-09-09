# STM32CubeIDE 림커 스크립트(STM32F446RETX_FLASH.ld) 주요 내용


## 1. 엔트리 포인트 및 스택/힙 정의

```ld
/* Entry Point */ 
ENTRY(Reset_Handler)
```
* **의미:** 프로그램이 전원이 켜지거나 리셋될 때 가장 먼저 실행할 함수의 이름을 Reset_Handler로 지정합니다. 칩이 켜지면 제일 먼저 Reset_Handler부터 실행됩니다.  

```ld
/* Highest address of the user mode stack */
_estack = ORIGIN(RAM) + LENGTH(RAM);    /* end of "RAM" Ram type memory */
```
* **의미:** 메인 스택의 시작 주소(스택의 최상단 위치)인 _estack을 정의합니다.  
* **원리:** ARM Cortex-M의 스택은 메모리의 높은 주소에서 낮은 주소로 줄어드는 Full-Descending 방식을 사용하므로, RAM 메모리의 맨 끝 주소(ORIGIN(RAM) + LENGTH(RAM))를 스택의 시작점으로 설정합니다.  

```ld
_Min_Heap_Size = 0x200 ;  /* required amount of heap */
_Min_Stack_Size = 0x800 ; /* required amount of stack */
```
* **의미:**
  * **_Min_Heap_Size (0x200 = 512 Bytes):** 동적 할당(malloc 등)에 사용할 최소 힙 영역 크기입니다.  
  * **_Min_Stack_Size (0x800 = 2048 Bytes):** 지역 변수 및 함수 호출에 필요한 최소 스택 영역 크기입니다.  

---

## 2. 메모리 영역 정의 (MEMORY)

```ld
/* Memories definition */ 
MEMORY 
{ 
  RAM    (xrw)    : ORIGIN = 0x20000000,   LENGTH = 128K 
  FLASH    (rx)    : ORIGIN = 0x8000000,   LENGTH = 512K 
}
```
* **의미:** STM32F446RE 칩의 물리적인 메모리 주소와 크기를 링커에게 알려줍니다.  
  * **RAM:** 시작 주소는 0x20000000, 크기는 128Kbyte입니다. 읽기/쓰기/실행(xrw: eXecute, Read, Write) 권한을 가집니다.  
  * **FLASH:** 시작 주소는 0x08000000, 크기는 512Kbyte입니다. 읽기/실행(rx: Read, eXecute) 권한을 가집니다.  

---

## 3. 섹션배치 정의 (SECTIONS)

컴파일된 코드와 데이터들을 FLASH와 RAM에 나누어 배치하는 구간입니다.  

### (1) 인터럽트 벡터 테이블 (.isr_vector)

```ld
.isr_vector :
{
  . = ALIGN(4);
  KEEP(*(.isr_vector)) /* Startup code */
  . = ALIGN(4);
} >FLASH
```
* **. = ALIGN(4);:** 주소를 4바이트 단위(Word alignment)로 맞춥니다.  
* **KEEP(*(.isr_vector)):** 최적화(Dead code elimination) 중에 사용되지 않는 것처럼 보이더라도 이 섹션(isr_vector)은 절대 삭제하지 말고 포함시키라는 의미입니다.  
* **>FLASH:** 이 섹션을 FLASH 메모리에 배치합니다.  
* **역할:** 칩이 켜지자마자 가장 먼저 참조하는 인터럽트 벡터 테이블을 FLASH 메모리의 시작 부분에 배치합니다.  

### (2) 코드 및 실행 명령어 영역 (.text)

```ld
.text :
{
  . = ALIGN(4);
  *(.text)           /* .text sections (code) */
  *(.text*)          /* .text* sections (code) */
  *(.glue_7)         /* glue arm to thumb code */
  *(.glue_7t)        /* glue thumb to arm code */
  *(.eh_frame)

  KEEP (*(.init))
  KEEP (*(.fini))

  . = ALIGN(4);
  _etext = .;        /* define a global symbols at end of code */
} >FLASH
```
* ***(.text) 및 *(.text*):** 모든 C/C++ 소스코드의 실제 실행 코드(기능/함수 등)들을 포함합니다.  
* ***(.glue_7), *(.glue_7t):** ARM 코드와 Thumb 명령어 간 호환을 위한 링커 전용 코드입니다.  
* **KEEP (*(.init)), KEEP (*(.fini)):** C++ 객체 생성자(constructor) 및 소멸자(destructor) 초기화 코드입니다.  
* **_etext = .;:** .text 섹션이 끝나는 지점의 주소를 _etext라는 심볼에 저장합니다.  
* **>FLASH:** 이 구역 전체를 FLASH 메모리에 저장합니다.  

### (3) 읽기 전용 상수 데이터 (.rodata)

```ld
.rodata :
{
  . = ALIGN(4);
  *(.rodata)         /* .rodata sections (constants, strings, etc.) */
  *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
  . = ALIGN(4);
} >FLASH
```
* ***(.rodata) 및 *(.rodata*):** const로 선언된 변수나 문자열 리터럴("Hello World" 등) 같은 변경되지 않는 읽기 전용 데이터를 담는 섹션입니다.  
* **>FLASH:** 읽기 전용이므로 FLASH 메모리에 보관됩니다.  

### (4) ARM 예외 처리 및 C++ 초기화 섹션들  

```ld
.ARM.extab   : { . = ALIGN(4);  *(.ARM.extab* .gnu.linkonce.armextab.*) . = ALIGN(4); } >FLASH
.ARM : { . = ALIGN(4); __exidx_start = .;  *(.ARM.exidx* ) __exidx_end = .; . = ALIGN(4); } >FLASH
.preinit_array     : { . = ALIGN(4); PROVIDE_HIDDEN (__preinit_array_start = .); KEEP ( *(.preinit_array* )) PROVIDE_HIDDEN (__preinit_array_end = .); . = ALIGN(4); } >FLASH
.init_array : { . = ALIGN(4); PROVIDE_HIDDEN (__init_array_start = .); KEEP ( *(SORT(.init_array.* ))) KEEP ( *(.init_array* )) PROVIDE_HIDDEN (__init_array_end = .); . = ALIGN(4); } >FLASH
.fini_array : { . = ALIGN(4); PROVIDE_HIDDEN (__fini_array_start = .); KEEP ( *(SORT(.fini_array.* ))) KEEP ( *(.fini_array* )) PROVIDE_HIDDEN (__fini_array_end = .); . = ALIGN(4); } >FLASH
```
* **.ARM.extab / .ARM:** C++ 예외 처리(Exception handling) 및 스택 언와인딩을 위한 주소 테이블입니다.  
* **.preinit_array / .init_array / .fini_array:** main 함수가 실행되기 전후에 실행되어야 하는 C/C++ 시스템 함수 및 글로벌 객체 초기화 배열입니다.  
* **PROVIDE_HIDDEN (...):** 내부 런타임 라이브러리에서 사용할 심볼을 정의하며, 전역적으로 외부로 노출되지 않도록 숨깁니다.  
* **>FLASH:** 프로그램 시작 직후 읽혀야 하므로 모두 FLASH 메모리에 저장됩니다.  

---

## 요약 정리

| 메모리 구조 | 섹션 이름 | 저장되는 데이터 유형 |
| :--- | :--- | :--- |
| **FLASH (0x08000000)** | .isr_vector | 인터럽트 벡터 테이블 |
| | .text | C/C++ 실행 코드 (기능 함수) |
| | .rodata | const 상수, 문자열 리터럴 |
| | .init_array 등 | C++ 및 시스템 초기화 루틴 |
| **RAM (0x20000000)** | Stack (_estack) | 지역변수, 함수 반환 주소 (높은 주소에서 아래로 확장) |
| | Heap (_Min_Heap_Size) | 동적 메모리 할당 영역 (malloc) |