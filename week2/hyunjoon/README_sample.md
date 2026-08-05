- 1. GPIO 하드웨어 구조 및 이해
    - 들어가기 전
        - GPIO란?
            
            **다용도 입출력**[(general-purpose input/output, **GPIO**)은 입력이나 출력을 포함한 동작이 런타임 시에 사용자에 의해 제어될 수 있는, 집적 회로나 전기 회로 기판의 디지털 신호 핀이다.
            
            GPIO는 특정한 목적이 미리 정의되지 않으며 기본적으로는 사용되지 않는다. GPIO는 어셈블리 레벨의 회로망 설계자(집적 회로 GPIO의 경우에는 회로 기판 설계자, 기판 레벨 GPIO의 경우에는 시스템 통합자, S/I)에 의해 구현되어 있으며 사용 시에는 GPIO의 목적과 동작이 정의된다.
            
    - NUCLEO - F429ZI GPIO 주요 사양
        - 16개의 I/O 제어 가능
        - 출력 방식 : push-pull , open-drain + pull -up/down
        - 데이터 출력 : ouput data 레지스터 혹은 peripheral
        - 각 I/O에 대해서 속도 설정 가능
        - 입력 방식 : floating, pull - up / down, analog
        - 데이터 입력 : input data 레지스터, preipheral
    
    - GPIO PIN까지의 회로 구성
        
        !image.png
        
        - BSRR (Bit set/reset Resistor) → write only. 해당 레지스터의 상태에 따라 ODR이 결정됨
    - GPIO 레지스터 목록
        - GPIO port mode register (GPIOx_MODER)
        - GPIO port output type register (GPIOx_OTYPER)
        - GPIO port output speed register (GPIOx_OSPEEDR)
        - GPIO port pull-up/pull-down register (GPIOx_PUPDR)
        - GPIO port input data register (GPIOx_IDR) (x = A..I/J/K)
        - GPIO port output data register (GPIOx_ODR) (x = A..I/J/K)
        - GPIO port bit set/reset register (GPIOx_BSRR) (x = A..I/J/K)
        - GPIO port configuration lock register (GPIOx_LCKR)
        - GPIO alternate function low register (GPIOx_AFRL) (x = A..I/J/K)
        - GPIO alternate function high register (GPIOx_AFRH)
    - 주요 레지스터 설명
        - GPIO port mode register (GPIOx_MODER)
            - port의 mode 설정
                
                00: Input (reset state)
                01: General purpose output mode
                10: Alternate function mode
                11: Analog mode
                
        - GPIO port output type register (GPIOx_OTYPER)
            - port의 출력 방식 설정
                
                0: Output push-pull (reset state)
                1: Output open-drain
                
        - GPIO port output speed register (GPIOx_OSPEEDR)
            - port의 출력 속도 설정
                
                00: Low speed
                01: Medium speed
                10: High speed
                11: Very high speed
                
        - GPIO port pull-up/pull-down register
            - I/O를 풀업으로 인식할지 풀 다운으로 인식할지의 설정(MCU 내부 저항 연결)
                
                00: No pull-up, pull-down
                01: Pull-up
                10: Pull-down
                11: Reserved
                
        - GPIO port input data register (GPIOx_IDR) (x = A..I/J/K)
            - read only
            - I/O의 실제 전기적 상태를 나타냄
        - GPIO port output data register (GPIOx_ODR) (x = A..I/J/K)
            - 해당 핀에 출력할 논리 값 저장
        - GPIO port bit set/reset register (GPIOx_BSRR) (x = A..I/J/K)
            - write only
            - 해당 핀에 출력할 논리 값 설정
                - 비트 단위 제어 가능
- 2. LED 제어코드 분석 및 활용
    1. HAL
        - main 함수
            
            ```c
            
            int main(void)
            {
            
              /* USER CODE BEGIN 1 */
            
              /* USER CODE END 1 */
            
              /* MCU Configuration--------------------------------------------------------*/
            
              /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
              HAL_Init();
            
              /* USER CODE BEGIN Init */
            
              /* USER CODE END Init */
            
              /* Configure the system clock */
              SystemClock_Config();
            
              /* USER CODE BEGIN SysInit */
            
              /* USER CODE END SysInit */
            
              /* Initialize all configured peripherals */
              MX_GPIO_Init();
              MX_ETH_Init();
              MX_USART3_UART_Init();
              MX_USB_OTG_FS_PCD_Init();
              /* USER CODE BEGIN 2 */
              HAL_GPIO_WritePin(GPIOB,GPIO_PIN_7, 1);
              HAL_Delay(5000);
              HAL_GPIO_WritePin(GPIOB,GPIO_PIN_7, 0);
              /* USER CODE END 2 */
            
              /* Infinite loop */
              /* USER CODE BEGIN WHILE */
              while (1)
              {
                /* USER CODE END WHILE */
                /* USER CODE BEGIN 3 */
              }
            
              /* USER CODE END 3 */
            }
            
            /**
              * @brief System Clock Configuration
              * @retval None
              */
            ```
            
            - HAL 라이브러리를 사용하여 HAL_GPIO_WritePin 함수를 이용해 5초동안 LED를 ON
        - HAL_GPIO_WritePin 함수
            
            ```c
            
            void HAL_GPIO_WritePin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, GPIO_PinState PinState)
            {
              /* Check the parameters */
              assert_param(IS_GPIO_PIN(GPIO_Pin));
              assert_param(IS_GPIO_PIN_ACTION(PinState));
            
              if(PinState != GPIO_PIN_RESET)
              {
                GPIOx->BSRR = GPIO_Pin;
              }
              else
              {
                GPIOx->BSRR = (uint32_t)GPIO_Pin << 16U;
              }
            }
            ```
            
            - GPIO의 포트 , 핀 번호, 쓰려는 Pin state를 받는다.
            - 포트와 핀 번호가 유효한지 확인한다.
            - PinState가 논리값(1 또는 0) 인지 확인한다.
            - Pinstate가 0(RESET)이 아니면 GPIO_TypeDef 구조체의 BSRR을 참조해 해당 Pin의 번호를 넣는다. (해당 Pin번호의 BSRR 인가 주소로 접근해 1 넣기)
            - Pinstate가 0이면 GPIO_Pin을 16bit 밀고 BSRR에 대입한다 ( BSRR reset하는 주소에 1 넣기)
    2. 직접 작성
        - BSRR 접근
            
            ```c
            int main(void)
            {
            
              /* USER CODE BEGIN 1 */
            
              /* USER CODE END 1 */
            
              /* MCU Configuration--------------------------------------------------------*/
            
              /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
              HAL_Init();
            
              /* USER CODE BEGIN Init */
            
              /* USER CODE END Init */
            
              /* Configure the system clock */
              SystemClock_Config();
            
              /* USER CODE BEGIN SysInit */
            
              /* USER CODE END SysInit */
            
              /* Initialize all configured peripherals */
              MX_GPIO_Init();
              MX_ETH_Init();
              MX_USART3_UART_Init();
              MX_USB_OTG_FS_PCD_Init();
              /* USER CODE BEGIN 2 */
              /* USER CODE END 2 */
            
              /* Infinite loop */
              /* USER CODE BEGIN WHILE */
              while (1)
              {
                /* USER CODE END WHILE */
            	GPIOB -> BSRR = 1<<7;
            	HAL_Delay(1000);
            	GPIOB -> BSRR = (1<<7)<<16;
            	HAL_Delay(1000);
            
                /* USER CODE BEGIN 3 */
              }
            
              /* USER CODE END 3 */
            }
            ```
            
            ```c
            GPIOB -> BSRR = 1<<7;
            	HAL_Delay(1000);
            	GPIOB -> BSRR = (1<<7)<<16;
            	HAL_Delay(1000);
            ```
            
            - GPIOB : GPIO_Typedef 구조체 포인터
                - GPIOB_BASE → GPIOB포트의 시작 메모리 주소
            - 
            
            !image.png
            
            - BSRR의 메모리 구성은 다음과 같으므로, 하위 16bit에 1을 인가하면 해당 핀에 1을 인가하고 상위 16bit에 1을 인가하면 해당 핀이 reset된다.
            - 따라서 7번 주소에 1할당 → ON
            - 7+16번 째 주소에 1 할당 → RESET(OFF)
        - ODR 접근
            
            ```c
            int main(void)
            {
            
              /* USER CODE BEGIN 1 */
            
              /* USER CODE END 1 */
            
              /* MCU Configuration--------------------------------------------------------*/
            
              /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
              HAL_Init();
            
              /* USER CODE BEGIN Init */
            
              /* USER CODE END Init */
            
              /* Configure the system clock */
              SystemClock_Config();
            
              /* USER CODE BEGIN SysInit */
            
              /* USER CODE END SysInit */
            
              /* Initialize all configured peripherals */
              MX_GPIO_Init();
              MX_ETH_Init();
              MX_USART3_UART_Init();
              MX_USB_OTG_FS_PCD_Init();
              /* USER CODE BEGIN 2 */
              /* USER CODE END 2 */
            
              /* Infinite loop */
              /* USER CODE BEGIN WHILE */
              while (1)
              {
                /* USER CODE END WHILE */
            	GPIOB->MODER = 1<<14; 	// PB7 general output mode 설정
            	GPIOB -> OTYPER = 0<<7;		// PB7 output mode push-pull 설정
            	GPIOB -> ODR = 1<<7;
            	HAL_Delay(1000);
            	GPIOB -> ODR = (0<<7);
            	HAL_Delay(1000);
                /* USER CODE BEGIN 3 */
              }
            
              /* USER CODE END 3 */
            }
            ```
            
            ```c
            	GPIOB->MODER = 1<<14; 	// PB7 general output mode 설정
            	GPIOB -> OTYPER = 0<<7;		// PB7 output mode push-pull 설정
            	GPIOB -> ODR = 1<<7;
            	HAL_Delay(1000);
            	GPIOB -> ODR = (0<<7);
            	HAL_Delay(1000);
            ```
            
            - MODER에 접근해 14번째 메모리를 1로 (7번째 Pin을 01로) 설정하여 해당 Pin을 Output mode로 설정한다.
            - 1초 간격으로 깜빡이는 LED 구현
            
            - GPIO → OTYPER로 Output 방식을 push-pull로 설정한다.
            - ODR에 직접 접근하여 7번 째 주소에 1 인가 → 1초 delay → 0인가(OFF)
        - 매크로 GPIOB 선언부
        
        !image.png
        
        - GPIO_TypeDef  구조체 멤버 선언부
        
        !image.png
