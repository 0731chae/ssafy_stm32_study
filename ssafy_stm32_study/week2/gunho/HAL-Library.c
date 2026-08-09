int main(void)
{
  /* MCU 및 시스템 클록, GPIO 초기화 (CubeMX가 자동 생성) */
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();

  /* 무한 루프 */
  while (1)
  {
    /* USER CODE BEGIN WHILE */
    
    // PA5 핀 상태 반전 (1초마다 ON <-> OFF)
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
    
    // 1000ms (1초) 지연
    HAL_Delay(1000);
    
    /* USER CODE END WHILE */
  }
}