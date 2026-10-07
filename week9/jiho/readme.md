# ADC?

ADC는 Analog to Digital Convertor 즉 아날로그 전압을 디지털 숫자로 변환하는 장치

MCU의 CPU는 결국 0/1로 표현되는 디지털 값만 처리할 수 있음. 하지만 현실의 센서들은 보통 연속적인 전압을 출력함

예를 들어 조고 센서가 다음과 같이 출력한다고 하면

```
어두움               밝음

0V ----------------- 3.3V
```

STM32가 이 값을 직접 1.75V 이런식으로 읽을 수 없음

그래서 ADC를 사용함 ADC를 거치게 되면

```
1.73V
↓
ADC
↓
2146
```

이렇게 정수로 변환이 됨

## ADC 해상도

ADC는 해상도를 가지는데 우리가 사용하는 f446re 의 ADC는 대표적으로 12bit ADC로 사용할 수 있음

12bit 라는 것은 ADC 결과를

$2^{12} = 4096$ 단계 로 나눈다는 것을 의미함

따라서 ADC 결과값은 0~4095의 값을 갖게 됨

기준 전압이 3.3V라면 대략

```
0V     → 0
1.65V  → 약 2048
3.3V   → 4095
```

이렇게 됨

기본 공식은

$ADC=\frac{V_{in}}{V_{ref}}\times(2^N-1)$ 이다

## ADC는 완벽한가?

당연히 절대아님 앞서 말한 해상도의 개념과 이어지는데

12bit에 3.3V라면 한 단계당 (값이 1차이나면)

$\frac{3.3V}{4096} \approx 0.000805V$

즉 약 0.805mV임

그래서 실제 입력이 매우 낮게 차이나면

예를들어

```
1.000000V
1.000001V
1.000002V
```

이런경우 전부 같은 ADC값으로 표기될 수 있음

이런걸 양자화 라고 함

## ADC Channel

ADC에는 여러 입력 채널이 존재함

예를들어 특정 GPIO가 ADC 채널과 연결 되어 있다고 할 때

```
PA0
 │
 └── ADC1_IN0
```

센서를 PA0에 연결하면

```
센서 전압
↓
PA0
↓
ADC1 Channel 0
↓
ADC 변환
```

이 이루어짐

중요한 점은

> GPIO핀 번호와 ADC Channel 번호는 같은 개념이 아님

다음과 같은 연결을 가지니 확인 해볼 것
| ADC 채널 | 연결 GPIO 핀 | 지원 ADC 인스턴스 | Arduino 커넥터 (NUCLEO) |
|---|---|---|---|
| IN0 | PA0 | ADC1, ADC2, ADC3 | A0 |
| IN1 | PA1 | ADC1, ADC2, ADC3 | A1 |
| IN2 | PA2 | ADC1, ADC2, ADC3 | (기본 USART2_TX 연결) |
| IN3 | PA3 | ADC1, ADC2, ADC3 | (기본 USART2_RX 연결) |
| IN4 | PA4 | ADC1, ADC2 (DAC1_OUT1 겸용) | A2 |
| IN5 | PA5 | ADC1, ADC2 (DAC1_OUT2 겸용) | D13 (내장 LED 연결) |
| IN6 | PA6 | ADC1, ADC2 | D12 |
| IN7 | PA7 | ADC1, ADC2 | D11 |
| IN8 | PB0 | ADC1, ADC2 | A3 |
| IN9 | PB1 | ADC1, ADC2 | Morpho CN10 |
| IN10 | PC0 | ADC1, ADC2, ADC3 | A5 |
| IN11 | PC1 | ADC1, ADC2, ADC3 | A4 |
| IN12 | PC2 | ADC1, ADC2, ADC3 | Morpho CN7 |
| IN13 | PC3 | ADC1, ADC2, ADC3 | Morpho CN7 |
| IN14 | PC4 | ADC1, ADC2 | Morpho CN10 |
| IN15 | PC5 | ADC1, ADC2 | Morpho CN10 |

## Sample & Hold

ADC가 변환을 시작해도 입력 전압을 계속 보면서 전환하는 게 아님

먼저 입력 전환을 **샘플링** 함

내부적으로 간단하게 표현하면 캐패시터가 있다고 함

```
센서

Vin
 │
 ├──── Switch ──── Capacitor
 │
 │                 ┌───┐
 └─────────────────│ C │
                   └───┘
```

ADC가 샘플링을 시작하면 스위치가 닫힘

캐패시터가 입력 전압까지 충전이 됨

그 다음 스위치를 끊음

```
Vin       C = 2.1V
          │
          ▼
        ADC 변환
```

## Sampling Time이 중요한 이유

STM32 ADC 설정을 하다 보면 이런 값이 있음

```
ADC_SAMPLETIME_3CYCLES
ADC_SAMPLETIME_15CYCLES
ADC_SAMPLETIME_28CYCLES
ADC_SAMPLETIME_56CYCLES
ADC_SAMPLETIME_84CYCLES
ADC_SAMPLETIME_112CYCLES
ADC_SAMPLETIME_144CYCLES
ADC_SAMPLETIME_480CYCLES
```

이게 캐패시터를 입력 전압으로 충전시키는 시간임

너무 짧다면

```
실제 입력: 3.0V

capacitor:

0V
│
│     /
│   /
│ /
└────────────
   sampling 종료

2.6V까지만 충전됨
```

ADC가 3.0V가 아닌 약 2.6V 라고 생각할 수 있음

특히 센서의 출력 임피던스가 크다면 충전 시간이 더 필요함

따라서

```
Sampling Time 증가
→ 정확도 증가 가능
→ ADC 측정 속도 감소
```

이런 트레이드 오프가 존재함

## STM32F446RE의 ADC는 SAR 방식

SAR는

> Successive Approximation Register

즉 **순차 비교 방식**이다

이분 탐색 처럼 동작한다고 보면 된다

예를 들어

```
Vin = 2.0V
Vref = 3.3V
일 때

1.65V보다 큰가?

2.0 > 1.65
→ YES

2.475V보다 큰가?

2.0 < 2.475
→ NO

이 과정을 반복함

bit11 결정
bit10 결정
bit9 결정
...
bit1 결정
bit0 결정
```

최종적으로 011110... 이런식의 12bit 결과가 만들어짐

## ADC Clock

ADC역시 내부 회로이기 떄문에 Clock이 필요함

개념적으로

```
System Clock

   ↓

APB2

   ↓

ADC Prescaler

   ↓

ADC Clock
```

이런 형태로 들어감

ADC의 Sampling과 Conversion이 모두 이 ADC Clock을 기준으로 진행됨

예를 들어 Sampling Time이 15 Cycles라면

ADC Clock 15개 동안 입력 전압을 샘플링 하는 것을 의미함.

## 변환 시간

ADC 전체 변환 시간은 대략

```
Sampling 시간
+
Conversion 시간



ADC Start

│<--- Sampling --->│<--- Conversion --->│

                   SAR
                   변환

                                      Done
```

Sampling Time을 크게 설정하면 더 안정적으로 입력을 받을 수 있지만 그만큼 전체 측정 속도는 느려진다.

## Conversion

ADC 설정이 끝났다면 ADC에게 측정하라고 명령 해야함

명령이 들어오면 ADC에서

```
Channel 선택
↓
Sampling
↓
Conversion
```

해당 과정을 진행함

## EOC

변환이 끝나면 ADC는 EOC를 발생시킴

EOC는 End Of Conversion 이라는 의미다

CPU에서는

```
CPU : ADC 변환 끝났어?

ADC : 아직

CPU : 끝났어?

ADC : 아직

ADC : EOC = 1

CPU : 끝났구나
```

이렇게 되는거임

> HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);

이런 함수로 변환 완료를 기다릴 수 있음

**여기서 주의 할 점은 EOC는 인터럽트가 아님**

EOC는 Flag임

개념적으로

```
ADC 변환 중

EOC = 0

      ↓

ADC 변환 완료

EOC = 1
```

그냥 하드웨어 상태값임 그 다음 CPU가 어떻게 처리하느냐에 따라 Polling/Interrupt가 됨

## ADC_DR

최종 값은 ADC의 Data Register에 저장된다
