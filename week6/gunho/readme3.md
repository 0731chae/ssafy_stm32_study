# STM32F446RE 스타트업 코드(startup_stm32f446retx.s) 분석

[startup_stm32f446retx.s](./source/startup_stm32f446retx.s)

스타트업 코드는 MCU에 전원이 들어오거나 리셋이 발생했을 때 C 언어의 main() 함수가 실행되기 전까지 하드웨어 및 메모리를 초기화하는 전처리 과정입니다.

---

## 1. 지시어 및 핸들러 정의 (환경 설정)

```assembly
.syntax unified
.cpu cortex-m4
.fpu softvfp
.thumb
```
*   **.syntax unified** : ARM과 Thumb 어셈블리 명령어를 통일된 문법으로 사용하겠다는 의미입니다.
*   **.cpu cortex-m4** : 코어가 Cortex-M4임을 명시합니다.
*   **.fpu softvfp** : 부동소수점 연산 방식(소프트웨어 방식)을 설정합니다.
*   **.thumb** : 16비트/32비트 혼합 명령어 세트인 Thumb 모드로 코드를 작성함을 의미합니다.

```assembly
.global g_pfnVectors
.global Default_Handler
```
*   **.global** : g_pfnVectors(인터럽트 벡터 테이블)와 Default_Handler를 다른 파일에서도 참조할 수 있도록 전역 기호로 선언합니다.

```assembly
.section .text.Reset_Handler
.weak Reset_Handler
.type Reset_Handler, %function
Reset_Handler:
```
*   **.section .text.Reset_Handler** : 이 아래 실행 코드들을 메모리의 .text (코드 영역) 섹션 중 Reset_Handler에 배치합니다.
*   **.weak Reset_Handler** : 약한 결합(weak)으로 선언하여, 사용자가 다른 파일에서 Reset_Handler를 재정의하면 그 함수를 우선 사용하도록 합니다.
*   **.type ... %function** : Reset_Handler가 함수 타입임을 알립니다.

--------------------------------------------------------------------------------

## 2. 스택 포인터(SP) 초기화 및 데이터 복사 준비

```assembly
ldr sp, =_estack
```
*   **스택 포인터(SP) 설정** : RAM의 최상단 주소인 _estack 값을 Stack Pointer(sp) 레지스터에 로드하여 스택의 시작 위치를 잡습니다.

```assembly
movs r1, #0
b LoopCopyDataInit
```
*   **movs r1, #0** : 오프셋으로 사용할 레지스터 r1을 0으로 초기화합니다.
*   **b LoopCopyDataInit** : .data 섹션 복사를 검사하는 루프(LoopCopyDataInit)로 바로 이동(branch)합니다.

--------------------------------------------------------------------------------

## 3. .data 섹션 복사 (Flash $\rightarrow$ SRAM)

**이유** : 변수 중 초기값이 있는 전역/정적 변수는 전원이 꺼져도 유지되는 Flash 메모리에 보관되어 있습니다. 실행 속도와 값 변경을 위해 이를 RAM(SRAM)으로 복사해야 합니다.

```assembly
CopyDataInit:
  ldr r3, =_sidata
  ldr r3, [r3, r1]
  str r3, [r0, r1]
  adds r1, r1, #4
```
*   **ldr r3, =_sidata** : Flash에 저장된 .data 섹션의 초기값 시작 주소(_sidata)를 r3에 읽어옵니다.
*   **ldr r3, [r3, r1]** : Flash 메모리주소(_sidata + r1)에 있는 4바이트 데이터를 읽어서 r3에 담습니다.
*   **str r3, [r0, r1]** : r3에 담긴 데이터를 SRAM 주소(_sdata + r1)에 씁니다.
*   **adds r1, r1, #4** : 다음 4바이트(1 단어) 처리를 위해 오프셋 r1을 4만큼 증가시킵니다.

```assembly
LoopCopyDataInit:
  ldr r0, =_sdata
  ldr r3, =_edata
  adds r2, r0, r1
  cmp r2, r3
  bcc CopyDataInit
```
*   **ldr r0, =_sdata** : SRAM에 배치될 .data 시작 주소(_sdata)를 읽어옵니다.
*   **ldr r3, =_edata** : SRAM의 .data 끝 주소(_edata)를 읽어옵니다.
*   **adds r2, r0, r1** : 현재 복사 중인 SRAM 주소(_sdata + r1)를 구하여 r2에 넣습니다.
*   **cmp r2, r3** : 현재 주소(r2)가 끝 주소(r3)에 도달했는지 비교합니다.
*   **bcc CopyDataInit** : 도달하지 않았다면(r2 < r3), CopyDataInit으로 돌아가 계속 복사합니다.

--------------------------------------------------------------------------------

## 4. .bss 섹션 0으로 초기화 (SRAM)

**이유** : 초기값이 0이거나 지정되지 않은 전역/정적 변수 영역(.bss)을 모두 0으로 싹 비워줍니다.

```assembly
ldr r2, =_sbss
b LoopFillZerobss
```
*   **ldr r2, =_sbss** : BSS 영역의 시작 주소(_sbss)를 r2에 로드합니다.
*   **b LoopFillZerobss** : BSS 검사 루프(LoopFillZerobss)로 이동합니다.

```assembly
FillZerobss:
  movs r3, #0
  str r3, [r2], #4
```
*   **movs r3, #0** : 0 값을 r3에 채웁니다.
*   **str r3, [r2], #4** : r2가 가리키는 SRAM 주소에 0을 채우고, r2 주소를 4바이트 증가시킵니다.

```assembly
LoopFillZerobss:
  ldr r3, =_ebss
  cmp r2, r3
  bcc FillZerobss
```
*   **ldr r3, =_ebss** : BSS 영역의 끝 주소(_ebss)를 로드합니다.
*   **cmp r2, r3** : 현재 주소(r2)가 BSS 끝 주소(r3)에 도달했는지 비교합니다.
*   **bcc FillZerobss** : 도달하지 않았다면(r2 < r3), FillZerobss로 이동해 0 채우기를 반복합니다.

--------------------------------------------------------------------------------

## 5. 시스템 초기화 및 main() 호출

```assembly
bl SystemInit
```
*   **bl SystemInit** : 클록(Clock), FPU, 외부 메모리 컨트롤러 등을 설정하는 C 언어 기반 시스템 초기화 함수(SystemInit)를 호출합니다.

```assembly
bl __libc_init_array
```
*   **bl __libc_init_array** : C/C++ 표준 라이브러리 및 정적 생성자(constructor)들을 초기화하는 함수를 호출합니다.

```assembly
bl main
bx lr
```
*   **bl main** : 드디어 우리가 작성한 C 프로그램의 진입점인 main() 함수를 호출합니다!
*   **bx lr** : 만약 main() 함수가 종료되고 돌아오면 이전 링크 레지스터(LR)로 복귀합니다(실제 엠베디드 환경에서는 main이 무한 루프를 돌기 때문에 보통 여기까지 실행되지 않습니다).

```assembly
.size Reset_Handler, .-Reset_Handler
```
*   **.size** : Reset_Handler 심볼의 크기를 계산하여 디버거 등에 알려줍니다.

--------------------------------------------------------------------------------

## 요약 흐름

1. 스택 포인터(SP) 설정
2. Flash에 있던 초기값 데이터를 RAM의 .data 영역으로 복사
3. RAM의 .bss 영역을 0으로 초기화
4. 시스템 클록 초기화 (SystemInit)
5. C 라이브러리/생성자 초기화 (__libc_init_array)
6. main() 함수 실행