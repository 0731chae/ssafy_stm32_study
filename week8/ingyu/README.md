# PWM의 개념과 STM32F446RE에서의 PWM 제어

## 1. PWM이란?

PWM(Pulse Width Modulation, 펄스 폭 변조)은 디지털 신호의 **HIGH와 LOW가 반복되는 비율**을 조절하여
LED 밝기, 모터 속도, 부저 음높이 등 다양한 장치를 제어하는 방법이다.

디지털 GPIO는 기본적으로 두 가지 상태만 가진다.

```text
HIGH = 3.3V
LOW  = 0V
```

PWM은 이 HIGH와 LOW를 매우 빠르게 반복해서 출력한다.

예를 들어 다음과 같은 신호가 있다고 하자.

```text
HIGH ────┐    ┌────┐    ┌────┐
         │    │    │    │    │
LOW      └────┘    └────┘    └────
```

이처럼 일정한 주기로 HIGH와 LOW를 반복하는 것이 PWM이다.

---

## 2. PWM의 핵심 개념

PWM을 이해할 때 가장 중요한 값은 다음 두 가지이다.

### 2.1 주파수(Frequency)

주파수는 PWM 신호가 **1초 동안 몇 번 반복되는지**를 의미한다.

단위는 Hz(헤르츠)를 사용한다.

예를 들어:

```text
1 kHz = 1초에 1000번 반복
2 kHz = 1초에 2000번 반복
```

주기(Period)와 주파수의 관계는 다음과 같다.

```text
f = 1 / T
```

- `f` : 주파수(Hz)
- `T` : 한 주기의 시간(s)

예를 들어 1 kHz PWM이라면:

```text
T = 1 / 1000
  = 0.001초
  = 1ms
```

즉, 한 주기가 1ms이다.

---

### 2.2 Duty Cycle

Duty Cycle은 한 주기 중에서 HIGH 상태가 차지하는 비율이다.

```text
Duty Cycle = HIGH 시간 / 전체 주기 × 100
```

예를 들어:

```text
50% Duty Cycle

HIGH █████-----
LOW  -----█████
```

한 주기의 절반이 HIGH이고 절반이 LOW이다.

대표적인 예시는 다음과 같다.

```text
25% Duty
HIGH ██--------
LOW  --████████

50% Duty
HIGH █████-----
LOW  -----█████

75% Duty
HIGH ███████---
LOW  -------███
```

LED를 PWM으로 제어하면 일반적으로 Duty Cycle이 높을수록 더 밝게 보인다.

---

## 3. Passive Buzzer와 PWM

Passive Buzzer는 내부에 자체 발진 회로가 없기 때문에
마이크로컨트롤러가 직접 주기적인 신호를 만들어줘야 한다.

예를 들어:

```text
262 Hz → 도(C4)
294 Hz → 레(D4)
330 Hz → 미(E4)
349 Hz → 파(F4)
392 Hz → 솔(G4)
440 Hz → 라(A4)
494 Hz → 시(B4)
523 Hz → 도(C5)
```

즉 Passive Buzzer에서는 PWM의 **주파수 자체가 음높이**를 결정한다.

Duty Cycle은 보통 50% 정도를 사용한다.

---

# 4. STM32F446RE에서 PWM은 어떻게 만들어지는가?

STM32에서는 일반적으로 Timer(TIM)를 사용해서 PWM을 생성한다.

PWM을 만들 때 중요한 값은 다음 세 가지이다.

```text
PSC = Prescaler
ARR = Auto Reload Register
CCR = Capture/Compare Register
```

각각의 역할은 다음과 같다.

---

## 4.1 Prescaler(PSC)

Prescaler는 Timer로 들어오는 클럭을 나누는 역할을 한다.

공식은 다음과 같다.

```text
Timer Counter Clock
=
Timer Input Clock / (PSC + 1)
```

예를 들어 TIM1의 입력 클럭이 168 MHz이고:

```text
PSC = 167
```

이라면:

```text
168 MHz / (167 + 1)
= 168 MHz / 168
= 1 MHz
```

따라서 Timer Counter는 1초에 1,000,000번 증가한다.

즉 한 Tick은:

```text
1 / 1,000,000초
= 1us
```

가 된다.

---

## 4.2 ARR(Auto Reload Register)

ARR은 Timer가 어디까지 숫자를 센 뒤 다시 0으로 돌아갈지를 결정한다.

예를 들어:

```text
ARR = 999
```

라면 Timer는:

```text
0 → 1 → 2 → ... → 998 → 999 → 0
```

을 반복한다.

총 1000번의 카운트가 한 주기가 된다.

Timer Counter Clock이 1 MHz라면:

```text
1,000,000 / 1000
= 1000 Hz
```

따라서 1 kHz PWM이 만들어진다.

---

## 4.3 CCR(Capture/Compare Register)

CCR은 PWM 한 주기에서 HIGH/LOW가 전환되는 시점을 결정한다.

예를 들어:

```text
ARR = 999
CCR = 500
```

이라면 약 50% Duty Cycle이 된다.

```text
Duty ≒ CCR / (ARR + 1)
```

따라서:

```text
500 / 1000
= 0.5
= 50%
```

이다.

---

# 5. PWM 주파수 공식

STM32 Timer의 PWM 주파수는 기본적으로 다음 식으로 계산한다.

```text
PWM Frequency
=
Timer Clock
/
((PSC + 1) × (ARR + 1))
```

예를 들어:

```text
Timer Clock = 168 MHz
PSC         = 167
ARR         = 999
```

이면:

```text
168,000,000
/
(168 × 1000)

= 1000 Hz
```

즉 1 kHz PWM이 된다.

---

# 6. STM32F446RE의 Timer Clock

STM32F446RE에서는 Timer마다 연결된 APB 버스가 다르다.

대표적으로:

```text
TIM1 → APB2
TIM5 → APB1
```

Timer Clock은 단순히 PCLK 값과 항상 같은 것은 아니다.

APB Prescaler가 1보다 크면 Timer Clock이 일반적으로 다음과 같이 2배가 된다.

```text
APB Prescaler = 1
→ Timer Clock = PCLK

APB Prescaler > 1
→ Timer Clock = PCLK × 2
```

예를 들어:

```text
SYSCLK = 168 MHz

APB1 Prescaler = /4
PCLK1 = 42 MHz
→ APB1 Timer Clock = 84 MHz

APB2 Prescaler = /2
PCLK2 = 84 MHz
→ APB2 Timer Clock = 168 MHz
```

따라서:

```text
TIM5 Clock = 84 MHz
TIM1 Clock = 168 MHz
```

가 된다.

CubeMX의 `Clock Configuration` 화면에서 실제 Timer Clock을 확인하는 것이 가장 정확하다.

---

# 7. STM32CubeMX에서 PWM 설정

예를 들어 TIM1 Channel 1을 PWM으로 사용한다고 하자.

## 7.1 Pinout 설정

PWM을 출력할 핀을 선택한다.

예시:

```text
TIM1_CH1
```

STM32F446RE에서는 사용할 핀의 Alternate Function(AF)을 확인하여
해당 Timer Channel에 연결해야 한다.

---

## 7.2 TIM1 설정

CubeMX에서:

```text
Timers
→ TIM1
```

으로 이동한다.

다음과 같이 설정한다.

```text
Clock Source
→ Internal Clock

Channel 1
→ PWM Generation CH1
```

예를 들어 TIM1 Clock이 168 MHz이고 1 kHz PWM을 만들고 싶다면:

```text
Prescaler      = 167
Counter Period = 999
Pulse          = 500
```

으로 설정할 수 있다.

결과는:

```text
Timer Counter Clock = 1 MHz
PWM Frequency       = 1 kHz
Duty Cycle          = 50%
```

이다.

---

# 8. HAL 코드에서 PWM 시작하기

CubeMX에서 코드를 생성하면 다음과 같은 Timer Handle이 생성된다.

```c
TIM_HandleTypeDef htim1;
```

Timer 초기화 이후 PWM을 시작한다.

```c
HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
```

예:

```c
int main(void)
{
    HAL_Init();

    SystemClock_Config();

    MX_GPIO_Init();
    MX_TIM1_Init();

    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);

    while (1)
    {
    }
}
```

---

# 9. Duty Cycle 변경하기

PWM의 Duty Cycle은 CCR 값을 변경하면 된다.

HAL 매크로를 사용하면 다음과 같다.

```c
__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, value);
```

예를 들어:

```text
ARR = 999
```

일 때:

```c
// 0%
__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);

// 약 25%
__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 250);

// 약 50%
__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 500);

// 약 75%
__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 750);
```

처럼 Duty Cycle을 변경할 수 있다.

---

# 10. PWM 주파수 변경하기

Passive Buzzer에서는 주파수를 변경해야 음높이가 바뀐다.

Timer Counter Clock을 1 MHz로 맞춰두었다고 하자.

그러면 원하는 주파수에 맞는 ARR 값은:

```text
ARR
=
1,000,000 / Frequency - 1
```

로 계산할 수 있다.

예를 들어 440 Hz를 만들려면:

```text
ARR
=
1,000,000 / 440 - 1

≈ 2272
```

50% Duty Cycle을 사용한다면 CCR은:

```text
CCR
≈ (ARR + 1) / 2
≈ 1136
```

이 된다.

---

# 11. Passive Buzzer용 함수

다음과 같이 주파수를 변경하는 함수를 만들 수 있다.

```c
void Buzzer_SetFrequency(uint32_t freq)
{
    if (freq == 0)
    {
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
        return;
    }

    uint32_t period;

    /*
     * Timer Counter Clock = 1 MHz
     */
    period = (1000000 / freq) - 1;

    /*
     * ARR 변경
     */
    __HAL_TIM_SET_AUTORELOAD(&htim1, period);

    /*
     * Duty Cycle 50%
     */
    __HAL_TIM_SET_COMPARE(
        &htim1,
        TIM_CHANNEL_1,
        (period + 1) / 2
    );

    /*
     * 현재 Timer Counter 초기화
     */
    __HAL_TIM_SET_COUNTER(&htim1, 0);
}
```

부저를 끌 때는:

```c
void Buzzer_Stop(void)
{
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
}
```

처럼 사용할 수 있다.

---

# 12. 도레미파솔라시도 출력 예제

음계 주파수를 정의한다.

```c
#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_C5  523
```

재생 함수:

```c
void Buzzer_PlayScale(void)
{
    Buzzer_SetFrequency(NOTE_C4);
    HAL_Delay(500);

    Buzzer_SetFrequency(NOTE_D4);
    HAL_Delay(500);

    Buzzer_SetFrequency(NOTE_E4);
    HAL_Delay(500);

    Buzzer_SetFrequency(NOTE_F4);
    HAL_Delay(500);

    Buzzer_SetFrequency(NOTE_G4);
    HAL_Delay(500);

    Buzzer_SetFrequency(NOTE_A4);
    HAL_Delay(500);

    Buzzer_SetFrequency(NOTE_B4);
    HAL_Delay(500);

    Buzzer_SetFrequency(NOTE_C5);
    HAL_Delay(500);

    Buzzer_Stop();
}
```

---

# 13. PWM 시작 직후 부저가 울리는 문제

CubeMX에서 다음과 같이 설정했다고 하자.

```text
Period = 999
Pulse  = 500
```

이 상태에서:

```c
HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
```

를 실행하면 PWM이 즉시 시작되기 때문에
버튼을 누르기 전에도 부저가 울릴 수 있다.

이를 방지하려면 초기 Pulse 값을 0으로 설정하는 것이 좋다.

```text
Pulse = 0
```

또는 PWM을 시작한 직후:

```c
HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
Buzzer_Stop();
```

처럼 초기 상태를 OFF로 만들 수 있다.

---

# 14. PWM Mode 1

STM32에서 많이 사용하는 PWM 모드는 `PWM Mode 1`이다.

일반적인 Active High 설정에서는 Counter 값이 CCR보다 작을 때 출력이 Active 상태가 된다.

개념적으로:

```text
CNT < CCR
→ Active

CNT >= CCR
→ Inactive
```

예:

```text
ARR = 999
CCR = 500
```

이면 한 주기의 약 절반이 Active 상태가 되어 50% Duty Cycle이 된다.

출력 Polarity를 Low로 설정했다면 실제 핀의 HIGH/LOW 논리는 반대로 보일 수 있다.

---

# 15. PWM 제어 전체 흐름

STM32에서 PWM이 만들어지는 과정을 정리하면 다음과 같다.

```text
Timer Input Clock
        ↓
    Prescaler
      (PSC)
        ↓
Timer Counter Clock
        ↓
   Counter 증가
        ↓
  0 ~ ARR 반복
        ↓
CCR과 Counter 비교
        ↓
PWM 출력 HIGH / LOW
```

즉:

```text
PSC
→ Timer의 기본 Tick 속도 결정

ARR
→ PWM 주파수 결정

CCR
→ Duty Cycle 결정
```

이라고 기억하면 된다.

---

# 16. 핵심 공식 정리

## Timer Counter Clock

```text
Counter Clock
=
Timer Clock / (PSC + 1)
```

## PWM Frequency

```text
PWM Frequency
=
Timer Clock
/
((PSC + 1) × (ARR + 1))
```

## Duty Cycle

```text
Duty Cycle
≈ CCR / (ARR + 1) × 100
```

## 원하는 주파수에서 ARR 계산

Counter Clock이 1 MHz라면:

```text
ARR
=
1,000,000 / 원하는 주파수 - 1
```

## 50% Duty Cycle

```text
CCR
≈ (ARR + 1) / 2
```

---

# 17. 요약

PWM을 STM32에서 사용할 때 핵심은 다음과 같다.

```text
PWM
= HIGH/LOW를 빠르게 반복하는 디지털 신호
```

```text
Frequency
= 반복 속도
```

```text
Duty Cycle
= 한 주기에서 Active 상태가 차지하는 비율
```

STM32 Timer에서는:

```text
PSC → Counter 속도 조절
ARR → PWM 주파수 조절
CCR → Duty Cycle 조절
```

STM32F446RE에서는 Timer가 APB1 또는 APB2에 연결되어 있기 때문에
PWM 계산을 하기 전에 반드시 CubeMX의 `Clock Configuration`에서
해당 Timer의 실제 입력 Clock을 확인해야 한다.

Passive Buzzer를 제어할 경우 PWM 주파수가 음높이를 결정하며,
보통 약 50% Duty Cycle을 사용한다.
