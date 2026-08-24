# UART통신이란?

Universal Asynchronous Receiver/Transmitter의 약자

두 장치가 데이터를 직렬(Serial)로 주고받기 위한 통신 방식

기본적인 연결은 2개

```
Device A                 Device B

TX --------------------> RX
RX <-------------------- TX
GND -------------------- GND
```

TX-RX교차한다는 점 확인할 것

UART는 clock 선이 없음. 그래서 비동기 통신이라고 불림

> 다만 F446RE 에서는 USART로 동기 통신도 지원함

## UART 한 글자가 전송되기 까지의 과정

예를 들어 문자 A를 전송한다고 할때

```
ASCII
'A' = 0x41 = 0100 0001
            bit 7 ~ bit 0
```

UART는 보통 다음 형태로 전송

```
Idle     Start      Data bits                 Stop
  1        0       D0 D1 D2 D3 D4 D5 D6 D7     1
-------+       +---------------------------+---------
       |_______|
```

받는쪽에서는

```
Start
  ↓
0 | 1 0 0 0 0 0 1 0 | 1
    └──── data ─────┘   ↑
                        Stop
```

이런느낌

## Baud Rate

115200 Baud (보통 115200 씀) 에서는 1초에 115200개의 bit를 전송함

일반적인 8N1 UART 프레임은

```
Start    1 bit
Data     8 bit
Stop     1 bit
----------------
총       10 bit
```

이므로 11520byte/s를 가짐

UART 양쪽 Baud Rate가 다르면 PC에서는 흔히 이런 식으로 보입니다.

```
Hello World

→

▒╞?x▒?
```

그래서 UART 디버깅이 안 될 때 가장 먼저 Baud Rate를 확인하면 됩니다.

## 그래서 실제 통신은 어떻게 됨?

송신의 경우

```
CPU
 │
 │ 데이터 작성
 ↓
USART_DR
Transmit Data Register
 │
 ↓
Transmit Shift Register
 │
 │ 1bit씩
 ↓
TX Pin
```

수신의 경우

```
RX Pin
 │
 │ 1bit씩
 ↓
Receive Shift Register
 │
 ↓
USART_DR
Receive Data Register
 │
 ↓
CPU
```

> 여기서 DR은 Data Register를 의미

## 송신 과정

```
uint8_t data = 'A';

HAL_UART_Transmit(&huart2, &data, 1, HAL_MAX_DELAY);
```

이런 코드가 있다고 할 때.

개념적으로는 다음 과정

```
CPU
 │
 │ 'A' = 0x41
 ↓
USART2 Data Register
 │
 ↓
Transmit Shift Register
 │
 ↓
Start bit
D0
D1
D2
...
D7
Stop bit
 │
 ↓
PA2 (USART2_TX)
```
