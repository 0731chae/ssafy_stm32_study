/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : HC-SR04 + SSD1306 OLED Distance Meter
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* =========================================================
 * HC-SR04
 * ========================================================= */
#define TRIG_PORT       GPIOD
#define TRIG_PIN        GPIO_PIN_15

#define ECHO_PORT       GPIOF
#define ECHO_PIN        GPIO_PIN_12


/* =========================================================
 * SSD1306 OLED
 * ========================================================= */
#define OLED_WIDTH      128
#define OLED_HEIGHT     64

/* HAL에서는 7bit 주소를 왼쪽으로 1비트 shift해서 사용 */
#define OLED_ADDR_3C    (0x3C << 1)
#define OLED_ADDR_3D    (0x3D << 1)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim14;

/* USER CODE BEGIN PV */

/*
 * Live Expressions에서도 확인 가능
 */
volatile float distance_cm = 0.0f;
volatile uint32_t echo_time_us = 0;


/*
 * OLED framebuffer
 *
 * 128 x 64 / 8
 * = 1024 bytes
 */
static uint8_t oled_buffer[OLED_WIDTH * OLED_HEIGHT / 8];


/*
 * 실제 감지된 OLED 주소
 */
static uint16_t oled_address = OLED_ADDR_3C;


/*
 * OLED 통신 성공 여부
 */
static uint8_t oled_ready = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/

void SystemClock_Config(void);

static void MX_GPIO_Init(void);
static void MX_TIM14_Init(void);
static void MX_I2C1_Init(void);

/* USER CODE BEGIN PFP */

/* HC-SR04 */
static void delay_us(uint16_t us);
static float HCSR04_ReadDistance(void);


/* OLED */
static uint8_t OLED_Init(void);

static HAL_StatusTypeDef OLED_Command(uint8_t cmd);

static void OLED_Clear(void);

static void OLED_UpdateScreen(void);

static void OLED_DrawPixel(
        uint8_t x,
        uint8_t y
);

static void OLED_GetGlyph(
        char c,
        uint8_t glyph[5]
);

static void OLED_DrawChar(
        uint8_t x,
        uint8_t y,
        char c,
        uint8_t scale
);

static void OLED_DrawString(
        uint8_t x,
        uint8_t y,
        const char *str,
        uint8_t scale
);

static uint8_t UIntToString(
        uint32_t value,
        char *buffer
);

static void OLED_ShowDistance(float distance);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */


/* =========================================================
 *
 *                  HC-SR04
 *
 * ========================================================= */


/**
 * @brief microsecond delay
 *
 * TIM14
 *
 * Clock = 16 MHz
 * Prescaler = 15
 *
 * 16MHz / 16 = 1MHz
 *
 * 따라서
 *
 * Counter 1 증가 = 1 us
 */
static void delay_us(uint16_t us)
{
    __HAL_TIM_SET_COUNTER(&htim14, 0);

    while (__HAL_TIM_GET_COUNTER(&htim14) < us)
    {
        /* wait */
    }
}


/**
 * @brief HC-SR04 거리 측정
 *
 * @retval 거리(cm)
 *
 * 정상 : 0 이상
 * 실패 : 음수
 */
static float HCSR04_ReadDistance(void)
{
    uint32_t timeout_start;
    uint32_t pulse_time;


    /* ---------------------------------------------
     * TRIG LOW
     * --------------------------------------------- */
    HAL_GPIO_WritePin(
            TRIG_PORT,
            TRIG_PIN,
            GPIO_PIN_RESET
    );

    delay_us(2);


    /* ---------------------------------------------
     * TRIG HIGH 10 us
     * --------------------------------------------- */
    HAL_GPIO_WritePin(
            TRIG_PORT,
            TRIG_PIN,
            GPIO_PIN_SET
    );

    delay_us(10);

    HAL_GPIO_WritePin(
            TRIG_PORT,
            TRIG_PIN,
            GPIO_PIN_RESET
    );


    /* ---------------------------------------------
     * ECHO가 HIGH가 될 때까지 대기
     * --------------------------------------------- */

    timeout_start = HAL_GetTick();

    while (
        HAL_GPIO_ReadPin(
            ECHO_PORT,
            ECHO_PIN
        ) == GPIO_PIN_RESET
    )
    {
        /*
         * 30ms 동안 ECHO가 오지 않으면 실패
         */
        if ((HAL_GetTick() - timeout_start) > 30)
        {
            return -1.0f;
        }
    }


    /* ---------------------------------------------
     * ECHO HIGH 시간 측정 시작
     * --------------------------------------------- */

    __HAL_TIM_SET_COUNTER(&htim14, 0);


    while (
        HAL_GPIO_ReadPin(
            ECHO_PORT,
            ECHO_PIN
        ) == GPIO_PIN_SET
    )
    {
        /*
         * 30ms 이상 HIGH면 비정상
         */
        if (__HAL_TIM_GET_COUNTER(&htim14) > 30000)
        {
            return -2.0f;
        }
    }


    pulse_time = __HAL_TIM_GET_COUNTER(&htim14);

    echo_time_us = pulse_time;


    /*
     * 음속 약
     *
     * 0.0343 cm/us
     *
     * 초음파는
     *
     * 센서 → 물체 → 센서
     *
     * 왕복하므로 2로 나눔
     */
    return ((float)pulse_time * 0.0343f) / 2.0f;
}



/* =========================================================
 *
 *                    SSD1306 OLED
 *
 * ========================================================= */


/**
 * @brief OLED 명령 전송
 */
static HAL_StatusTypeDef OLED_Command(uint8_t cmd)
{
    uint8_t data[2];

    /*
     * SSD1306 control byte
     *
     * 0x00 = Command
     */
    data[0] = 0x00;
    data[1] = cmd;

    return HAL_I2C_Master_Transmit(
            &hi2c1,
            oled_address,
            data,
            2,
            100
    );
}


/**
 * @brief SSD1306 초기화
 *
 * 0x3C와 0x3D를 자동 탐색
 */
static uint8_t OLED_Init(void)
{
    /* ---------------------------------------------
     * 먼저 0x3C 확인
     * --------------------------------------------- */

    if (
        HAL_I2C_IsDeviceReady(
            &hi2c1,
            OLED_ADDR_3C,
            3,
            100
        ) == HAL_OK
    )
    {
        oled_address = OLED_ADDR_3C;
    }

    /* ---------------------------------------------
     * 0x3C가 아니면 0x3D 확인
     * --------------------------------------------- */

    else if (
        HAL_I2C_IsDeviceReady(
            &hi2c1,
            OLED_ADDR_3D,
            3,
            100
        ) == HAL_OK
    )
    {
        oled_address = OLED_ADDR_3D;
    }

    else
    {
        /*
         * OLED가 발견되지 않음
         */
        return 0;
    }


    HAL_Delay(100);


    /* ---------------------------------------------
     * SSD1306 Initial Sequence
     * 128 x 64
     * --------------------------------------------- */

    if (OLED_Command(0xAE) != HAL_OK)
        return 0;

    /* Display OFF */


    OLED_Command(0x20);

    /*
     * Page Addressing Mode
     */
    OLED_Command(0x02);


    /* Start line = 0 */
    OLED_Command(0x40);


    /*
     * Segment remap
     */
    OLED_Command(0xA1);


    /*
     * COM scan direction
     */
    OLED_Command(0xC8);


    /*
     * Multiplex ratio
     * 64 rows
     */
    OLED_Command(0xA8);
    OLED_Command(0x3F);


    /*
     * Display offset
     */
    OLED_Command(0xD3);
    OLED_Command(0x00);


    /*
     * Display clock
     */
    OLED_Command(0xD5);
    OLED_Command(0x80);


    /*
     * Pre-charge period
     */
    OLED_Command(0xD9);
    OLED_Command(0xF1);


    /*
     * COM pin configuration
     */
    OLED_Command(0xDA);
    OLED_Command(0x12);


    /*
     * Contrast
     */
    OLED_Command(0x81);
    OLED_Command(0xCF);


    /*
     * VCOM detect
     */
    OLED_Command(0xDB);
    OLED_Command(0x40);


    /*
     * Entire display ON from RAM
     */
    OLED_Command(0xA4);


    /*
     * Normal display
     */
    OLED_Command(0xA6);


    /*
     * Charge pump ON
     */
    OLED_Command(0x8D);
    OLED_Command(0x14);


    /*
     * Display ON
     */
    OLED_Command(0xAF);


    OLED_Clear();

    OLED_UpdateScreen();


    return 1;
}


/**
 * @brief OLED framebuffer 초기화
 */
static void OLED_Clear(void)
{
    memset(
        oled_buffer,
        0,
        sizeof(oled_buffer)
    );
}


/**
 * @brief OLED 실제 화면 업데이트
 */
static void OLED_UpdateScreen(void)
{
    uint8_t page;
    uint8_t tx_buffer[129];


    /*
     * 첫 byte = OLED Data Control Byte
     *
     * 0x40 = Display Data
     */
    tx_buffer[0] = 0x40;


    for (page = 0; page < 8; page++)
    {
        /*
         * Page 선택
         */
        OLED_Command(0xB0 + page);


        /*
         * Column address = 0
         */
        OLED_Command(0x00);
        OLED_Command(0x10);


        /*
         * framebuffer → 전송 buffer
         */
        memcpy(
            &tx_buffer[1],
            &oled_buffer[page * 128],
            128
        );


        /*
         * 한 페이지 = 128 byte
         */
        HAL_I2C_Master_Transmit(
                &hi2c1,
                oled_address,
                tx_buffer,
                129,
                200
        );
    }
}


/**
 * @brief OLED 한 점 켜기
 */
static void OLED_DrawPixel(
        uint8_t x,
        uint8_t y
)
{
    if (x >= OLED_WIDTH)
        return;

    if (y >= OLED_HEIGHT)
        return;


    oled_buffer[
        x +
        (y / 8) * OLED_WIDTH
    ] |= (1 << (y % 8));
}


/**
 * @brief
 * 필요한 문자만 구현한 5x7 font
 */
static void OLED_GetGlyph(
        char c,
        uint8_t glyph[5]
)
{
    memset(glyph, 0, 5);


    switch (c)
    {

    /* =========================
     * 숫자
     * ========================= */

    case '0':

        glyph[0] = 0x3E;
        glyph[1] = 0x51;
        glyph[2] = 0x49;
        glyph[3] = 0x45;
        glyph[4] = 0x3E;

        break;


    case '1':

        glyph[0] = 0x00;
        glyph[1] = 0x42;
        glyph[2] = 0x7F;
        glyph[3] = 0x40;
        glyph[4] = 0x00;

        break;


    case '2':

        glyph[0] = 0x42;
        glyph[1] = 0x61;
        glyph[2] = 0x51;
        glyph[3] = 0x49;
        glyph[4] = 0x46;

        break;


    case '3':

        glyph[0] = 0x21;
        glyph[1] = 0x41;
        glyph[2] = 0x45;
        glyph[3] = 0x4B;
        glyph[4] = 0x31;

        break;


    case '4':

        glyph[0] = 0x18;
        glyph[1] = 0x14;
        glyph[2] = 0x12;
        glyph[3] = 0x7F;
        glyph[4] = 0x10;

        break;


    case '5':

        glyph[0] = 0x27;
        glyph[1] = 0x45;
        glyph[2] = 0x45;
        glyph[3] = 0x45;
        glyph[4] = 0x39;

        break;


    case '6':

        glyph[0] = 0x3C;
        glyph[1] = 0x4A;
        glyph[2] = 0x49;
        glyph[3] = 0x49;
        glyph[4] = 0x30;

        break;


    case '7':

        glyph[0] = 0x01;
        glyph[1] = 0x71;
        glyph[2] = 0x09;
        glyph[3] = 0x05;
        glyph[4] = 0x03;

        break;


    case '8':

        glyph[0] = 0x36;
        glyph[1] = 0x49;
        glyph[2] = 0x49;
        glyph[3] = 0x49;
        glyph[4] = 0x36;

        break;


    case '9':

        glyph[0] = 0x06;
        glyph[1] = 0x49;
        glyph[2] = 0x49;
        glyph[3] = 0x29;
        glyph[4] = 0x1E;

        break;



    /* =========================
     * 기호
     * ========================= */

    case '.':

        glyph[0] = 0x00;
        glyph[1] = 0x60;
        glyph[2] = 0x60;
        glyph[3] = 0x00;
        glyph[4] = 0x00;

        break;


    case '-':

        glyph[0] = 0x08;
        glyph[1] = 0x08;
        glyph[2] = 0x08;
        glyph[3] = 0x08;
        glyph[4] = 0x08;

        break;


    case ' ':

        break;



    /* =========================
     * DISTANCE
     * ========================= */

    case 'D':

        glyph[0] = 0x7F;
        glyph[1] = 0x41;
        glyph[2] = 0x41;
        glyph[3] = 0x22;
        glyph[4] = 0x1C;

        break;


    case 'I':

        glyph[0] = 0x00;
        glyph[1] = 0x41;
        glyph[2] = 0x7F;
        glyph[3] = 0x41;
        glyph[4] = 0x00;

        break;


    case 'S':

        glyph[0] = 0x46;
        glyph[1] = 0x49;
        glyph[2] = 0x49;
        glyph[3] = 0x49;
        glyph[4] = 0x31;

        break;


    case 'T':

        glyph[0] = 0x01;
        glyph[1] = 0x01;
        glyph[2] = 0x7F;
        glyph[3] = 0x01;
        glyph[4] = 0x01;

        break;


    case 'A':

        glyph[0] = 0x7E;
        glyph[1] = 0x11;
        glyph[2] = 0x11;
        glyph[3] = 0x11;
        glyph[4] = 0x7E;

        break;


    case 'N':

        glyph[0] = 0x7F;
        glyph[1] = 0x02;
        glyph[2] = 0x04;
        glyph[3] = 0x08;
        glyph[4] = 0x7F;

        break;


    case 'C':

        glyph[0] = 0x3E;
        glyph[1] = 0x41;
        glyph[2] = 0x41;
        glyph[3] = 0x41;
        glyph[4] = 0x22;

        break;


    case 'E':

        glyph[0] = 0x7F;
        glyph[1] = 0x49;
        glyph[2] = 0x49;
        glyph[3] = 0x49;
        glyph[4] = 0x41;

        break;



    /* =========================
     * cm
     * ========================= */

    case 'c':

        glyph[0] = 0x38;
        glyph[1] = 0x44;
        glyph[2] = 0x44;
        glyph[3] = 0x44;
        glyph[4] = 0x20;

        break;


    case 'm':

        glyph[0] = 0x7C;
        glyph[1] = 0x04;
        glyph[2] = 0x18;
        glyph[3] = 0x04;
        glyph[4] = 0x78;

        break;


    default:

        /*
         * 지원하지 않는 문자는 공백
         */
        break;
    }
}


/**
 * @brief 문자 하나 출력
 */
static void OLED_DrawChar(
        uint8_t x,
        uint8_t y,
        char c,
        uint8_t scale
)
{
    uint8_t glyph[5];

    uint8_t column;
    uint8_t row;

    uint8_t sx;
    uint8_t sy;


    OLED_GetGlyph(
        c,
        glyph
    );


    for (column = 0; column < 5; column++)
    {
        for (row = 0; row < 7; row++)
        {
            if (
                glyph[column] &
                (1 << row)
            )
            {
                /*
                 * 문자 확대
                 */
                for (sx = 0; sx < scale; sx++)
                {
                    for (sy = 0; sy < scale; sy++)
                    {
                        OLED_DrawPixel(
                                x +
                                column * scale +
                                sx,

                                y +
                                row * scale +
                                sy
                        );
                    }
                }
            }
        }
    }
}


/**
 * @brief 문자열 출력
 */
static void OLED_DrawString(
        uint8_t x,
        uint8_t y,
        const char *str,
        uint8_t scale
)
{
    while (*str)
    {
        OLED_DrawChar(
                x,
                y,
                *str,
                scale
        );

        /*
         * 글자 폭 5 + 공백 1
         */
        x += 6 * scale;

        str++;
    }
}


/**
 * @brief unsigned int → 문자열
 */
static uint8_t UIntToString(
        uint32_t value,
        char *buffer
)
{
    char reverse[10];

    uint8_t length = 0;
    uint8_t i;


    if (value == 0)
    {
        buffer[0] = '0';
        buffer[1] = '\0';

        return 1;
    }


    while (value > 0)
    {
        reverse[length++] =
            '0' + (value % 10);

        value /= 10;
    }


    for (i = 0; i < length; i++)
    {
        buffer[i] =
            reverse[length - 1 - i];
    }


    buffer[length] = '\0';


    return length;
}


/**
 * @brief OLED에 거리 출력
 *
 * 예:
 *
 *      DISTANCE
 *
 *      23.4 cm
 */
static void OLED_ShowDistance(float distance)
{
    char text[16];

    char integer_string[10];

    uint32_t distance10;
    uint32_t integer_part;
    uint32_t decimal_part;

    uint8_t integer_length;
    uint8_t index;

    uint8_t text_length;
    uint16_t pixel_width;

    uint8_t start_x;


    OLED_Clear();


    /* =====================================================
     * 위쪽 제목
     * ===================================================== */

    OLED_DrawString(
            40,
            7,
            "DISTANCE",
            1
    );


    /* =====================================================
     * 거리값 문자열 생성
     *
     * float printf를 사용하지 않음
     *
     * STM32의 printf float 옵션이 없어도 동작
     * ===================================================== */

    if (distance < 0.0f)
    {
        strcpy(
            text,
            "---.- cm"
        );
    }

    else
    {
        /*
         * 소수점 한 자리까지 반올림
         *
         * 예
         *
         * 23.46
         * ↓
         * 235
         * ↓
         * 23.5
         */
        distance10 =
            (uint32_t)(distance * 10.0f + 0.5f);


        integer_part =
            distance10 / 10;


        decimal_part =
            distance10 % 10;


        integer_length =
            UIntToString(
                integer_part,
                integer_string
            );


        index = 0;


        /*
         * 정수 부분 복사
         */
        memcpy(
            &text[index],
            integer_string,
            integer_length
        );


        index += integer_length;


        /*
         * .
         */
        text[index++] = '.';


        /*
         * 소수점 한 자리
         */
        text[index++] =
            '0' + decimal_part;


        /*
         * 공백
         */
        text[index++] = ' ';


        /*
         * cm
         */
        text[index++] = 'c';
        text[index++] = 'm';


        text[index] = '\0';
    }


    /* =====================================================
     * 문자열 중앙 정렬
     * ===================================================== */

    text_length =
        strlen(text);


    /*
     * scale 2
     *
     * 한 글자당
     *
     * 6 pixel × 2
     */
    pixel_width =
        text_length * 12;


    if (pixel_width < OLED_WIDTH)
    {
        start_x =
            (OLED_WIDTH - pixel_width) / 2;
    }

    else
    {
        start_x = 0;
    }


    /*
     * 큰 글씨 거리 출력
     */
    OLED_DrawString(
            start_x,
            32,
            text,
            2
    );


    /*
     * framebuffer → 실제 OLED
     */
    OLED_UpdateScreen();
}


/* USER CODE END 0 */


/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */


  /* MCU Configuration--------------------------------------------------------*/


  /*
   * Reset of all peripherals,
   * Initializes Flash interface and Systick
   */
  HAL_Init();


  /* USER CODE BEGIN Init */

  /* USER CODE END Init */


  /*
   * Configure system clock
   */
  SystemClock_Config();


  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */


  /*
   * Initialize all configured peripherals
   */
  MX_GPIO_Init();

  MX_TIM14_Init();

  MX_I2C1_Init();


  /* USER CODE BEGIN 2 */


  /* =====================================================
   * TIM14 시작
   * ===================================================== */

  if (
      HAL_TIM_Base_Start(&htim14)
      != HAL_OK
  )
  {
      Error_Handler();
  }


  /*
   * 처음 TRIG = LOW
   */
  HAL_GPIO_WritePin(
          TRIG_PORT,
          TRIG_PIN,
          GPIO_PIN_RESET
  );


  /* =====================================================
   * OLED 초기화
   * ===================================================== */

  oled_ready = OLED_Init();


  /*
   * OLED가 정상 연결됐으면
   * 처음 화면에 0.0 cm 표시
   */
  if (oled_ready)
  {
      OLED_ShowDistance(
          distance_cm
      );
  }


  /* USER CODE END 2 */


  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  while (1)
  {

      float measured_distance;


      /* =================================================
       * HC-SR04 거리 측정
       * ================================================= */

      measured_distance =
          HCSR04_ReadDistance();


      if (measured_distance >= 0.0f)
      {
          /*
           * Live Expressions에서도 확인 가능
           */
          distance_cm =
              measured_distance;


          /*
           * OLED 갱신
           */
          if (oled_ready)
          {
              OLED_ShowDistance(
                  distance_cm
              );
          }
      }


      /*
       * HC-SR04 권장 측정 간격 확보
       */
      HAL_Delay(60);
  }

  /* USER CODE END WHILE */

  /* USER CODE BEGIN 3 */

  /* USER CODE END 3 */
}


/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};

  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};


  /*
   * Configure the main
   * internal regulator output voltage
   */
  __HAL_RCC_PWR_CLK_ENABLE();


  __HAL_PWR_VOLTAGESCALING_CONFIG(
      PWR_REGULATOR_VOLTAGE_SCALE3
  );


  /*
   * HSI = 16 MHz
   */
  RCC_OscInitStruct.OscillatorType =
      RCC_OSCILLATORTYPE_HSI;


  RCC_OscInitStruct.HSIState =
      RCC_HSI_ON;


  RCC_OscInitStruct.HSICalibrationValue =
      RCC_HSICALIBRATION_DEFAULT;


  RCC_OscInitStruct.PLL.PLLState =
      RCC_PLL_NONE;


  if (
      HAL_RCC_OscConfig(
          &RCC_OscInitStruct
      )
      != HAL_OK
  )
  {
      Error_Handler();
  }


  /*
   * CPU / AHB / APB Clock
   */
  RCC_ClkInitStruct.ClockType =
      RCC_CLOCKTYPE_HCLK |
      RCC_CLOCKTYPE_SYSCLK |
      RCC_CLOCKTYPE_PCLK1 |
      RCC_CLOCKTYPE_PCLK2;


  RCC_ClkInitStruct.SYSCLKSource =
      RCC_SYSCLKSOURCE_HSI;


  RCC_ClkInitStruct.AHBCLKDivider =
      RCC_SYSCLK_DIV1;


  RCC_ClkInitStruct.APB1CLKDivider =
      RCC_HCLK_DIV1;


  RCC_ClkInitStruct.APB2CLKDivider =
      RCC_HCLK_DIV1;


  if (
      HAL_RCC_ClockConfig(
          &RCC_ClkInitStruct,
          FLASH_LATENCY_0
      )
      != HAL_OK
  )
  {
      Error_Handler();
  }
}


/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{
  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */


  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */


  hi2c1.Instance =
      I2C1;


  hi2c1.Init.ClockSpeed =
      100000;


  hi2c1.Init.DutyCycle =
      I2C_DUTYCYCLE_2;


  hi2c1.Init.OwnAddress1 =
      0;


  hi2c1.Init.AddressingMode =
      I2C_ADDRESSINGMODE_7BIT;


  hi2c1.Init.DualAddressMode =
      I2C_DUALADDRESS_DISABLE;


  hi2c1.Init.OwnAddress2 =
      0;


  hi2c1.Init.GeneralCallMode =
      I2C_GENERALCALL_DISABLE;


  hi2c1.Init.NoStretchMode =
      I2C_NOSTRETCH_DISABLE;


  if (
      HAL_I2C_Init(
          &hi2c1
      )
      != HAL_OK
  )
  {
      Error_Handler();
  }


  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */
}


/**
  * @brief TIM14 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM14_Init(void)
{

  /* USER CODE BEGIN TIM14_Init 0 */

  /* USER CODE END TIM14_Init 0 */


  /* USER CODE BEGIN TIM14_Init 1 */

  /* USER CODE END TIM14_Init 1 */


  /*
   * TIM14 Clock = 16 MHz
   *
   * Prescaler 15
   *
   * 16MHz / 16
   *
   * = 1 MHz
   *
   * Counter 1 = 1 us
   */

  htim14.Instance =
      TIM14;


  htim14.Init.Prescaler =
      15;


  htim14.Init.CounterMode =
      TIM_COUNTERMODE_UP;


  htim14.Init.Period =
      65535;


  htim14.Init.ClockDivision =
      TIM_CLOCKDIVISION_DIV1;


  htim14.Init.AutoReloadPreload =
      TIM_AUTORELOAD_PRELOAD_DISABLE;


  if (
      HAL_TIM_Base_Init(
          &htim14
      )
      != HAL_OK
  )
  {
      Error_Handler();
  }


  /* USER CODE BEGIN TIM14_Init 2 */

  /* USER CODE END TIM14_Init 2 */
}


/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};


  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */


  /*
   * GPIO Ports Clock Enable
   */
  __HAL_RCC_GPIOA_CLK_ENABLE();

  __HAL_RCC_GPIOF_CLK_ENABLE();

  __HAL_RCC_GPIOD_CLK_ENABLE();

  __HAL_RCC_GPIOB_CLK_ENABLE();


  /*
   * PA5 initial LOW
   */
  HAL_GPIO_WritePin(
      GPIOA,
      GPIO_PIN_5,
      GPIO_PIN_RESET
  );


  /*
   * TRIG initial LOW
   */
  HAL_GPIO_WritePin(
      GPIOD,
      GPIO_PIN_15,
      GPIO_PIN_RESET
  );


  /* =====================================================
   * PA5
   * ===================================================== */

  GPIO_InitStruct.Pin =
      GPIO_PIN_5;


  GPIO_InitStruct.Mode =
      GPIO_MODE_OUTPUT_PP;


  GPIO_InitStruct.Pull =
      GPIO_NOPULL;


  GPIO_InitStruct.Speed =
      GPIO_SPEED_FREQ_LOW;


  HAL_GPIO_Init(
      GPIOA,
      &GPIO_InitStruct
  );


  /* =====================================================
   * PF12 = HC-SR04 ECHO
   * ===================================================== */

  GPIO_InitStruct.Pin =
      GPIO_PIN_12;


  GPIO_InitStruct.Mode =
      GPIO_MODE_INPUT;


  GPIO_InitStruct.Pull =
      GPIO_NOPULL;


  HAL_GPIO_Init(
      GPIOF,
      &GPIO_InitStruct
  );


  /* =====================================================
   * PD15 = HC-SR04 TRIG
   * ===================================================== */

  GPIO_InitStruct.Pin =
      GPIO_PIN_15;


  GPIO_InitStruct.Mode =
      GPIO_MODE_OUTPUT_PP;


  GPIO_InitStruct.Pull =
      GPIO_NOPULL;


  GPIO_InitStruct.Speed =
      GPIO_SPEED_FREQ_LOW;


  HAL_GPIO_Init(
      GPIOD,
      &GPIO_InitStruct
  );


  /*
   * PB8 / PB9 I2C 설정은
   *
   * stm32f4xx_hal_msp.c
   *
   * HAL_I2C_MspInit()
   *
   * 에서 CubeMX가 자동 생성
   */


  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}


/**
  * @brief  This function is executed
  *         in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */

  __disable_irq();


  while (1)
  {
  }

  /* USER CODE END Error_Handler_Debug */
}


#ifdef USE_FULL_ASSERT

/**
  * @brief Reports the name of the source file
  *        and line number
  */
void assert_failed(
        uint8_t *file,
        uint32_t line
)
{
  /* USER CODE BEGIN 6 */

  /* USER CODE END 6 */
}

#endif
