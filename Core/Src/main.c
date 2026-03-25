/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"


/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "sensors.h"
#include "stdio.h"
#include "math.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define R25	10000.0
#define T25	298.15
#define BETA   3900
#define HIGH_PRIORITY (( configMAX_PRIORITIES - 1 )>>1)+1
#define NORMAL_PRIORITY (( configMAX_PRIORITIES - 1 )>>1)
#define LOW_PRIORITY (( configMAX_PRIORITIES - 1 )>>1)-1

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

UART_HandleTypeDef huart2;

/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_ADC1_Init(void);
void StartDefaultTask(void *argument);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_USART2_UART_Init();
  MX_ADC1_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler */
  osKernelStart();

  void init_sensores(struct sensor_t *sensor_ldr, struct sensor_t * sensor_ntc){
 	// Configurar o LDR (0% a 100%)
 	sensor_ldr->valor = 0.0;
 	sensor_ldr->minimo = 0.0;
 	sensor_ldr->maximo = 100.0;
 	sensor_ldr->nivel_alarma = 50.0;
 	sensor_ldr->activated = 0;
 	sensor_ldr->time_activation = 0;
 	sensor_ldr->value_flashing = 0;
 	sensor_ldr->flashing_last_time_activation = 0;

 	// Configurar o NTC (25ºC a 30ºC)
 	sensor_ntc->valor = 0.0;
 	sensor_ntc->minimo = 25.0;
 	sensor_ntc->maximo = 30.0;
 	sensor_ntc->nivel_alarma = 28.0;
 	sensor_ntc->activated = 0;
 	sensor_ntc->time_activation = 0;
 	sensor_ntc->value_flashing = 0;
 	sensor_ntc->flashing_last_time_activation = 0;

  }

/*   void print_state_captors(){

 	  // Structure de configuration locale pour changer de canal ADC
 	  ADC_ChannelConfTypeDef sConfig = {0};

 	  // Configuration commune pour toutes les lectures
 	  sConfig.Rank = 1;
 	  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;


 	  while(1){
 		  sConfig.Channel = ADC_CHANNEL_4;
 		  HAL_ADC_ConfigChannel(&hadc1, &sConfig);
 		  HAL_ADC_Start(&hadc1);
 		  HAL_ADC_PollForConversion(&hadc1, 10000);
 		  printf("Valor of the pot is %ld \n",HAL_ADC_GetValue(&hadc1));

 		  sConfig.Channel = ADC_CHANNEL_0;
 		  HAL_ADC_ConfigChannel(&hadc1, &sConfig);
 		  HAL_ADC_Start(&hadc1);
 		  HAL_ADC_PollForConversion(&hadc1, 10000);
 		  printf("Valor of the ldr is %ld \n",HAL_ADC_GetValue(&hadc1));

 		  sConfig.Channel = ADC_CHANNEL_1;
 		  HAL_ADC_ConfigChannel(&hadc1, &sConfig);
 		  HAL_ADC_Start(&hadc1);
 		  HAL_ADC_PollForConversion(&hadc1, 10000);
 		  printf("Valor of the ntc is %ld \n",HAL_ADC_GetValue(&hadc1));


 		  vTaskDelay(50 / portTICK_RATE_MS);
 	  }
   }*/

  void show_value_on_leds(float value, float min, float max) {
      GPIO_TypeDef* led_ports[8] = {LED1_GPIO_Port, LED2_GPIO_Port, LED3_GPIO_Port, LED4_GPIO_Port, LED5_GPIO_Port, LED6_GPIO_Port, LED7_GPIO_Port, LED8_GPIO_Port};
      uint16_t led_pins[8] = {LED1_Pin, LED2_Pin, LED3_Pin, LED4_Pin, LED5_Pin, LED6_Pin, LED7_Pin, LED8_Pin};
      int n_led = 8;
      int leds_on = (int)(((value - min) / (max - min)) * n_led + 0.5f);
      if (leds_on < 0) leds_on = 0;
      if (leds_on > n_led) leds_on = n_led;
      for (int i = 0; i < n_led; ++i) {
          if (i < leds_on)
              HAL_GPIO_WritePin(led_ports[i], led_pins[i], GPIO_PIN_SET);
          else
              HAL_GPIO_WritePin(led_ports[i], led_pins[i], GPIO_PIN_RESET);
      }
  }

  // Funzione per lampeggiare il LED corrispondente al livello di allarme
  void flash_alarm_led(float nivel_alarma, float min, float max, int tick) {
      GPIO_TypeDef* led_ports[8] = {LED1_GPIO_Port, LED2_GPIO_Port, LED3_GPIO_Port, LED4_GPIO_Port, LED5_GPIO_Port, LED6_GPIO_Port, LED7_GPIO_Port, LED8_GPIO_Port};
      uint16_t led_pins[8] = {LED1_Pin, LED2_Pin, LED3_Pin, LED4_Pin, LED5_Pin, LED6_Pin, LED7_Pin, LED8_Pin};
      int n_led = 8;
      int led_idx = (int)(((nivel_alarma - min) / (max - min)) * n_led);
      if (led_idx < 0) led_idx = 0;
      if (led_idx >= n_led) led_idx = n_led - 1;
      HAL_GPIO_WritePin(led_ports[led_idx], led_pins[led_idx], (tick % 2) ? GPIO_PIN_RESET : GPIO_PIN_SET);
  }

   void Task_HW(void *pvParameters) {
       struct sensor_t sensor_ldr;
       struct sensor_t sensor_ntc;
       float pot_value = 0.0f;
       int tick = 0;
       int sensorSelector = 0; // 0 = LDR, 1 = NTC (puoi cambiare con pulsante)
       init_sensores(&sensor_ldr, &sensor_ntc);
       ADC_ChannelConfTypeDef sConfig = {0};
       sConfig.Rank = 1;
       sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;

       while (1) {
           // Lettura potenziometro (ADC_CHANNEL_4)
           sConfig.Channel = ADC_CHANNEL_4;
           HAL_ADC_ConfigChannel(&hadc1, &sConfig);
           HAL_ADC_Start(&hadc1);
           HAL_ADC_PollForConversion(&hadc1, 10000);
           uint32_t pot_raw = HAL_ADC_GetValue(&hadc1);
           pot_value = (float)pot_raw / 4095.0f; // Normalizzato 0.0–1.0

           // Lettura LDR (ADC_CHANNEL_0)
           sConfig.Channel = ADC_CHANNEL_0;
           HAL_ADC_ConfigChannel(&hadc1, &sConfig);
           HAL_ADC_Start(&hadc1);
           HAL_ADC_PollForConversion(&hadc1, 10000);
           uint32_t ldr_raw = HAL_ADC_GetValue(&hadc1);
           sensor_ldr.valor = (ldr_raw / 4095.0f) * 100.0f; // %

           // Lettura NTC (ADC_CHANNEL_1)
           sConfig.Channel = ADC_CHANNEL_1;
           HAL_ADC_ConfigChannel(&hadc1, &sConfig);
           HAL_ADC_Start(&hadc1);
           HAL_ADC_PollForConversion(&hadc1, 10000);
           uint32_t ntc_raw = HAL_ADC_GetValue(&hadc1);
           // Conversione semplificata, da sostituire con formula BETA se necessario
           sensor_ntc.valor = BETA/ (log((-10000.0 * 3.3 / (ntc_raw * 3.3 / 4095.9 - 3.3)
               	- 10000.0) / R25) + BETA / T25) - 273.18;


           // Visualizzazione a barra LED del sensore selezionato
           if (sensorSelector == 0) {
               show_value_on_leds(sensor_ldr.valor, sensor_ldr.minimo, sensor_ldr.maximo);
               flash_alarm_led(sensor_ldr.nivel_alarma, sensor_ldr.minimo, sensor_ldr.maximo, tick);
           } else {
               show_value_on_leds(sensor_ntc.valor, sensor_ntc.minimo, sensor_ntc.maximo);
               flash_alarm_led(sensor_ntc.nivel_alarma, sensor_ntc.minimo, sensor_ntc.maximo, tick);
           }

           tick++;
           vTaskDelay(50 / portTICK_RATE_MS); // 20 Hz
       }
   }


   auto res_task=xTaskCreate( Task_HW,"Tarea_HW",2048,NULL,	NORMAL_PRIORITY,NULL);
   if( res_task != pdPASS ){
   			printf("PANIC: Error al crear Tarea Visualizador\r\n");
   			fflush(NULL);
   			while(1);
   	}


  /* We should never get here as control is now taken by the scheduler */
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
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

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

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LED8_Pin|LED6_Pin|BUZZER_Pin|LED3_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LED7_Pin|LED2_Pin|LED5_Pin|LED1_Pin
                          |LED4_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LED8_Pin LED6_Pin BUZZER_Pin LED3_Pin */
  GPIO_InitStruct.Pin = LED8_Pin|LED6_Pin|BUZZER_Pin|LED3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : LED7_Pin LED2_Pin LED5_Pin LED1_Pin
                           LED4_Pin */
  GPIO_InitStruct.Pin = LED7_Pin|LED2_Pin|LED5_Pin|LED1_Pin
                          |LED4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : BTN_IZQ_Pin */
  GPIO_InitStruct.Pin = BTN_IZQ_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(BTN_IZQ_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : BTN_DER_Pin */
  GPIO_InitStruct.Pin = BTN_DER_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(BTN_DER_GPIO_Port, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
int _write(int file, char *ptr, int len){
    int DataIdx;
    for (DataIdx = 0; DataIdx < len; DataIdx++){
   	 HAL_UART_Transmit(&huart2, (uint8_t*)ptr++,1,1000);
    }
    return len;
}
/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN 5 */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END 5 */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM1 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM1) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
