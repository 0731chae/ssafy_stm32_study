# HAL방식과 BareMetal방식

## HAL 과 Baremetal이란?
STM32 개발 방식을 단순화 하면 다음 세가지 이다.
    
|방식|예시|특징|
|---|---|---|
|HAL|HAL_GPIO_WritePIN()|설정이 쉽지만 내부 동작이 추상화됨|
|LL|LL_GPIO_SetOutputPin()|HAL보다 하드웨어에 가까움|
|Bare-metal/Register-level|GPIOC->BSRR = (1U << 8)|레지스터 주소와 비트를 직접 제어|

### 각 방식의 간단한 설명과 장단점
1. **HAL**
    - **특징**
    하드웨어의 복잡한 구조를 숨기고 개발자가 쉽게 사용할 수 있도록 ST에서 제공하는 고수준 API이다.
    특징으로는 하드웨어를 몰라도 함수 이름과 매개변수만 알면 제어가 가능하다.
    - **사용하는 이유**
        1. **빠른개발속도**: 프로젝트 프로토타입 제작시 매우 유리함
        2. **이식성**: 같은 STM32 칩군 안에서는 이식성이 높음
    - **단점**
        코드가 무겁고 실행 속도가 느리다. 함수 하나를 호출할 때 내부적으로 수많은 예외처리와 상태 검사 로직으로 인해 오버헤드가 발생한다.

2. **LL**
    - **특징**
    HAL보다는 하드웨어에 더 가까운 저수준 API 라이브러리이다.
    - **사용이유**
        1. **HAL과 Baremetal의 중간점**: HAL의 편리함(가독성 좋은 함수명)과 베어 메탈의 성능(빠른 속도, 적은 메모리)을 타협한
        2. **레지스터 제어의 부담감이 적어짐**: 최적화가 필요하지만 모든 레지스터의 비트를 직접 연산하기에 부담스러울 때 사용
    - **단점**
        MCU의 하드웨어 구조를 꽤 깊이 이해하고 있어야 제대로 사용가능(진입장벽)
3. **Bare-Meatal**
    - **특징**
        ST에서 제공하는 라이브러리를 쓰지 않고, MCU의 데이터 시트를 보며 메모리주소에 직접 비트연산을 수행하는 방식.
    - **사용 이유**
        1. **극한의 성능과 실시간성**: 함수 호출에 따른 오버헤드가 없다.
        2. **메모리 최적화**: 칩의 플래시 메모리(ROM)나 RAM 크기가 매우 작을 때 무거운 라이브러리를 뺄 수 있다.
    - **단점**
        가독성이 낮고 정말 칩에 대한 보드에 대한 매우 깊은 수준의 이해가 필요하다.

### 그럼 Bare-Metal을 알면 뭐가좋은데?
HAL과 같은 라이브러리가 있어도 전문적인 임베디드/미션 크리티컬 소프트웨어 엔지니어에게는 Bare-Metal이 **필수 역량**으로 꼽힌다.
Bare-Metal 방식을 이해하고 있다면 HAL 라이브러리를 사용하던 도중에 문제가 발생 시 단순 구글링이나, AI를 통한 원인찾기 뿐만 아닌 직접 레지스터를 까보며 어디서 문제가 생겼는지 확인할 수 있는 선택지가 생긴다.
또한, 앞서 말했듯 오베헤드가 없어 정말 마이크로초 단위의 실시간성을 확보할 수 있다. 모터 제어, 드론 제어, 로봇 팔 구동과 같이 실시간으로 데이터를 처리해 다음 동작을 진행해야하는 경우 Bare-Metal 방식을 고려해야할 것이다.
마지막으로 Bare-Metal방식에 대한 자세한 이해가 있다면, 이슈가 생겨(예산 문제, 또는 공급망 문제) 칩을 바꿔야 하는 경우에도 데이터 시트를 읽으며 드라이버를 밑바닥부터 새로 작성할 수 있다.
    

### HAL방식 코드
- 초기화 코드
    ```
    HAL_Init();
    ```
    HAL **"라이브러리"** 를 초기화한다.
    
    ```
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();
    ```
    시스템 클럭을 설정하고, MX_GPIO_Init()에서 우리가 CubeMS를 통해 설정한 핀의 IN/OUT 어떤 GPIO핀 사용할 건지 등을 설정한다.
    MX_USART2_UART_Init()에서는 UART 통신 관련 설정을 진행한다.
    이후 while문안에서
    ```
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8, SET);
	HAL_Delay(1000);
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8, RESET);
	HAL_Delay(1000);
    ```
    해당 코드를 통해 Pin을 제어하고, 딜레이를 준다.

### BareMetal방식 핵심 코드
- 초기화 코드

    ```
    #define PC8_PIN     (1U << 8)

    static void GPIOC_PC8_INIT(void)
    {
        RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
        (void)RCC->AHB1ENR;

        GPIOC->MODER &= ~(3U << (8U * 2U));
        GPIOC->MODER |=  (1U << (8U * 2U));

        GPIOC->OTYPER &= ~PC8_PIN;

        GPIOC->PUPDR &= ~(3U << (8U * 2U));

        GPIOC->OSPEEDR &= ~(3U << (8U * 2U));

        GPIOC->BSRR = PC8_PIN;
    }
    ```

    #### Line별 설명
    1. **RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;**
        - GPIOC 클록 활성화
            STM32는 전력 절약을 위해 기본적으로 클록을 꺼둔다. GPIO 레지스터에 값을 쓰기 전 RCC의 AHB1ENR 레지스터에서 GPIOC 클록을 켜야한다.
            그리고 레지스터명을 보면 알 수 있듯 RCC의 AHB1ENR에서 GPIOC를 Enable 하라고 하는 것이다. (해당 bit에 1을 넣음)

        ```
        RCC
        └─ AHB1ENR
            ├─ GPIOAEN : GPIOA 클록
            ├─ GPIOBEN : GPIOB 클록
            └─ GPIOCEN : GPIOC 클록
        ```
        
    2. **(void)RCC->AHB1ENR;**
        - AHB1ENR의 레지스터 값 읽기
            처음 초기화 코드를 통해 GPIOC를 사용할 것이라고 레지스터에 적었으나, CPU가 write 명령을 한 후 활성 상태라 가정하지 않고 값을 읽는 것이다.
        - (void)를 붙여 값을 따로 저장하진 않고 그냥 읽기 명령만 실행한 상태이다.
        - 다음 코드처럼 GPIOC가 진짜 활성화 되었는지 확인할 수도 있다.
        ```
        RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;

        while ((RCC->AHB1ENR & RCC_AHB1ENR_GPIOCEN) == 0U)
        {
        }
        ```
        <span>
            다만, 정상적인 STM32설정에서는 보통 무한 대기는 하지 않고 관례적으로 해당 
        <span>
        
        ```
        (void)RCC->AHB1ENR;
        ```
        <span>
        코드만 실행한다.
        <br><br>
    3. **GPIOC->MODER : 핀 동작 모드 설정**
        - GPIOx_MODER는 각 핀마다 2비트를 사용한다.<br>
            
            
            |값|모드|
            |---|---|
            |00|Input|
            |01|Output|
            |10|Alternate function|
            |11|Analog|
            
            
            해당 모드에 맞춰 설정을 진행하면 된다.
            이제 다음 코드를 봐보자

            ```
            GPIOC->MODER &= ~(3U << (8U * 2U));
            GPIOC->MODER |=  (1U << (8U * 2U));
            ```
            
            여기서 GPIOC->MODER &= ~(3U << (8U * 2U)) 해당 코드는
            GPIOC->MODER에 ~(11 00 00 00 00 00 00 00) 즉, (00 11 11 11 11 11 11 11) 와 &연산을 통해서 GPIOC의 8번에 해당 하는 레지스터값만을 0으로 초기화 하는 코드이다.
            이후, GPIOC->MODER |=  (1U << (8U * 2U)) 해당 코드를 통해 GPIOC의 8번핀에 01 비트를 넣어 해당 핀을 Output으로 사용하도록 초기화 하는 코드이다.

    4. **GPIOC->OTYPER &= ~PC8_PIN;**
        - OTYPER는 출력 전기 방식을 지정하는 레지스터이다.
        
            |OTYPER값|출력방식|
            |---|---|
            |0|Push-pull|
            |1|Open-drain|
        
            push-pull은 MCU가 HIGH와 LOW를 직접 제어한다. LED ON/OFF와 같은 일반적인 Enable 신호, 디지털 출력에 사용한다.
            Open-drain은 LOW만 MCU가 만들고 High는 외부 pull-up 저항으로 만든다. I2C같은 공유 버스에서 주로 사용한다

    5. **GPIOC->PUPDR &= ~(3U << (8U * 2U));**
        - GPIOx_PUPDR는 MODER처럼 핀당 2비트를 가진다.
            해당 PUPDR 레지스터는 내부 pull-up, pull-down을 설정한다.
        
            |값|설정|
            |---|---|
            |00|No pull|
            |01|Pull-up|
            |10|Pull-down|
            |11|Reserved|

            pull-up/down은 해당 핀의 기본 상태를 무엇으로 둘지이다. 즉 MCU가 직접 컨트롤 하고 있지 않을때는 기본적으로 Up상태 또는 Down상태로 두는것을 의미한다.

            현재 LED Control에서는 직접 HIGH/LOW를 주기에 No Pull상태로 만들었다.

            Reserved는 그냥 사용하지 말라는거다. 3가지 모드를 구현하기 위해서는 2비트를 사용하는데, 그럼 결국 1개의 값(3)이 남게된다. 그러니 그냥 사용하지 말라는거다. 또한 혹시 나중에 추가적인 설정을 만들때가 있으면 사용한다는거다.

    6. **GPIOC->OSPEEDR &= ~(3U << (8U * 2U));**
        - OSPEEDR은 출력 전환 속도이다.

        |값|속도|
        |---|---|
        |00|Low Speed|
        |01|Medium Speed|
        |10|Fast Speed|
        |11|High/Very High speed|

        Led ON/OFF, 전원 Enable 처럼 느린 제어 목적이면 00을 쓰면 된다. 속도를 불필요하게 높이면 전력소모와 노이즈가 증가할 수 있다.

    7. **GPIOC->BSRR = PC8_PIN;**
        - GPIOx_BSRR은 출력 레지스터의 특정 비트를 원자적으로 set/reset하는 레지스터이다.
        
        |BSRR영역|의미|PC8예시|
        |---|---|---|
        |bit[15:0]|해당 핀 HIGH 설정|1U << 8|
        |bit[31:16]|해당 핀 LOW 설정|1U << (8 + 16)|

        현재 코드에서는 GPIOC 8번 핀에 High상태를 주겠다는 의미이다.
        while문 안에서도 해당 레지스터 값을 조절해 LED에 신호를 넣거나 뺄 수 있다.