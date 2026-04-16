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
osMutexId_t sensorMutex;

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define R25	10000.0
#define T25	298.15
#define BETA   3900
#define HIGH_PRIORITY (( configMAX_PRIORITIES - 1 )>>1)+1
#define NORMAL_PRIORITY (( configMAX_PRIORITIES - 1 )>>1)
#define LOW_PRIORITY (( configMAX_PRIORITIES - 1 )>>1)-1
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
/* Definitions for Task_HW */
osThreadId_t Task_HWHandle;
const osThreadAttr_t Task_HW_attributes = {
  .name = "Task_HW",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for myTask_Render */
osThreadId_t myTask_RenderHandle;
const osThreadAttr_t myTask_Render_attributes = {
  .name = "myTask_Render",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityLow,
};

/* USER CODE BEGIN PV */
bool button_pressed_left = false; // False for off and true for on, set to true in inputs tasks and off in the outputs tasks
bool button_pressed_right = false;
int selectedSensor;
uint32_t alarm_off_timestamp = 0; // Stores the timestamp when the alarm was manually disabled


uint16_t leds[] = {LED1_Pin,LED2_Pin,LED3_Pin,LED4_Pin,LED5_Pin,LED6_Pin,LED7_Pin,LED8_Pin};

GPIO_TypeDef* leds_ports[] = {GPIOB, GPIOB, GPIOA, GPIOB, GPIOB, GPIOA, GPIOB, GPIOA};

struct sensor_t sensor_ldr;
struct sensor_t sensor_ntc;
float pot_value = 0.0f;
bool alarm_triggered = false;


float last_pot_position_ntc ;
float last_pot_position_ldr ;


/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_ADC1_Init(void);
void StartDefaultTask(void *argument);
void StartTask_HW(void *argument);
void StartTask_Render(void *argument);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

int _write(int file, char *ptr, int len){
    int DataIdx;
    for (DataIdx = 0; DataIdx < len; DataIdx++){
   	 HAL_UART_Transmit(&huart2, (uint8_t*)ptr++,1,1000);
    }
    return len;
}


void init_sensores(struct sensor_t *sensor_ldr, struct sensor_t * sensor_ntc){
	// Configure the LDR sensor (0% to 100%)
	sensor_ldr->valor = 0.0;
	sensor_ldr->minimo = 0.0;
	sensor_ldr->maximo = 100.0;
	sensor_ldr->nivel_alarma = 7;

	// Configure the NTC sensor (20°C to 35°C)
	sensor_ntc->valor = 0.0;
	sensor_ntc->minimo = 20.;
	sensor_ntc->maximo = 35.0;
	sensor_ntc->nivel_alarma = 7;

}

void render_leds(int number_leds, int led_alarm,long int * last_blink){
	for (int i =0;i<8;i++){
		if (i != led_alarm){
			if (i<number_leds){
				HAL_GPIO_WritePin(leds_ports[i], leds[i], GPIO_PIN_SET);
			}
			else {
				HAL_GPIO_WritePin(leds_ports[i], leds[i], GPIO_PIN_RESET);
			}
		}
	}
	if(xTaskGetTickCount()- *last_blink > 200){
		*last_blink = xTaskGetTickCount();
		HAL_GPIO_TogglePin(leds_ports[led_alarm], leds[led_alarm]);
	}
}

int clamp(int value, int min, int max){
	if (value > max){
		return max;
	}
	else if (value < min){
		return min;
	}
	return value;
}

float absf(float a){
	if (a>0){
		return a;
	}
	else {
		return -a;
	}
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
  sensorMutex = osMutexNew(NULL);
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

  /* creation of Task_HW */
  Task_HWHandle = osThreadNew(StartTask_HW, NULL, &Task_HW_attributes);

  /* creation of myTask_Render */
  myTask_RenderHandle = osThreadNew(StartTask_Render, NULL, &myTask_Render_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  init_sensores(&sensor_ldr, &sensor_ntc);
  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler */
  osKernelStart();

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
    osDelay(1000000);
  }
  /* USER CODE END 5 */
}

/* USER CODE BEGIN Header_StartTask_HW */
/**
* @brief Function implementing the Task_HW thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask_HW */
void StartTask_HW(void *argument)
{
  /* USER CODE BEGIN StartTask_HW */

  GPIO_PinState last_button_state = GPIO_PIN_SET;
  GPIO_PinState last_state_right = GPIO_PIN_SET;

  ADC_ChannelConfTypeDef sConfig = {0};
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;

  /* Infinite loop */
  for(;;)
  {
      // Read potentiometer (ADC_CHANNEL_4)
      sConfig.Channel = ADC_CHANNEL_4;
      HAL_ADC_ConfigChannel(&hadc1, &sConfig);
      HAL_ADC_Start(&hadc1);
      HAL_ADC_PollForConversion(&hadc1, 10000);
      uint32_t pot_raw = HAL_ADC_GetValue(&hadc1);

      // Read LDR (ADC_CHANNEL_0)
      sConfig.Channel = ADC_CHANNEL_0;
      HAL_ADC_ConfigChannel(&hadc1, &sConfig);
      HAL_ADC_Start(&hadc1);
      HAL_ADC_PollForConversion(&hadc1, 10000);
      uint32_t ldr_raw = HAL_ADC_GetValue(&hadc1);

      // Read NTC (ADC_CHANNEL_1)
      sConfig.Channel = ADC_CHANNEL_1;
      HAL_ADC_ConfigChannel(&hadc1, &sConfig);
      HAL_ADC_Start(&hadc1);
      HAL_ADC_PollForConversion(&hadc1, 10000);
      uint32_t ntc_raw = HAL_ADC_GetValue(&hadc1);

      osMutexAcquire(sensorMutex, osWaitForever);

      // Protection against extreme ADC values (0 and 4095)
      if (ntc_raw > 0 && ntc_raw < 4095) {
          float voltage     = (ntc_raw * 3.3f) / 4095.0f;        // Voltage on the voltage divider [V]
          float r_ntc       = (10000.0f * voltage) / (3.3f - voltage); // NTC Resistance [Ohm]

          if (r_ntc > 0.0f) {
              sensor_ntc.valor = BETA / (log(r_ntc / R25) + BETA / T25) - 273.15f;
          }
      }

      sensor_ldr.valor = 100.0 - (ldr_raw / 4095.0f) * 100.0f; // %

      pot_value = 1.0f -  (float)pot_raw / 4095.0f; // Normalize to 0.0 - 1.0

	  if (selectedSensor == 0){
		   if(absf(pot_value - last_pot_position_ntc) > 0.05){
			   sensor_ntc.nivel_alarma = (int) (pot_value*8);
		   }
	  }
	  else{
		   if(absf(pot_value - last_pot_position_ldr) > 0.05){
			   sensor_ldr.nivel_alarma = (int) (pot_value*8);
		   }
	  }

      osMutexRelease(sensorMutex);

      // Read current left pin state
      GPIO_PinState current_button_state = HAL_GPIO_ReadPin(BTN_IZQ_GPIO_Port, BTN_IZQ_Pin);

      // Detect falling edge (button press)
      if (last_button_state == GPIO_PIN_SET && current_button_state == GPIO_PIN_RESET) {

    	  if (selectedSensor == 0) {
    		  selectedSensor = 1; // Switch to NTC
        	  last_pot_position_ldr = pot_value;
    	  } else {
    		  selectedSensor = 0; // Switch back to LDR
        	  last_pot_position_ntc = pot_value;
    	  }

    	  button_pressed_left = true;

    	  printf("\r\n[EVENT] Left Button Pressed! Selected sensor: %s\r\n",
    			  (selectedSensor == 1) ? "LDR (Light)" : "NTC (Temperature)");

    	  // Delay for mechanical debounce
    	  osDelay(50);
      }

      // Update previous state
      last_button_state = current_button_state;

      // Handle right button (Alarm Reset)
      GPIO_PinState current_state_right = HAL_GPIO_ReadPin(BTN_DER_GPIO_Port, BTN_DER_Pin);
      if (last_state_right == GPIO_PIN_SET && current_state_right == GPIO_PIN_RESET) {
          button_pressed_right = true;
          alarm_off_timestamp = osKernelGetTickCount();
          printf("\r\n[RESET] Alarm silenced.\r\n");
          HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);
          alarm_triggered = 0;
      }
      last_state_right = current_state_right;

      // Update buzzer state
      if (alarm_triggered) {
    	  HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_SET);
      }

      // Output values to serial port
      printf("LDR: %d%% | NTC: %d C | POT: %d \r\n", (int) sensor_ldr.valor, (int) sensor_ntc.valor,(int) (pot_value*100));

      osDelay(50);
  }
  /* USER CODE END StartTask_HW */
}

/* USER CODE BEGIN Header_StartTask_Render */
/**
* @brief Function implementing the myTask_Render thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask_Render */
void StartTask_Render(void *argument)
{
  /* USER CODE BEGIN StartTask_Render */

	long int last_blinking =0;
	int nb_leds = 0;
	int led_alarm = 0;

  /* Infinite loop */
  for(;;)
  {
	if (selectedSensor == 0){
		osMutexAcquire(sensorMutex, osWaitForever);
		nb_leds = (int) ((sensor_ntc.valor-sensor_ntc.minimo)/(sensor_ntc.maximo - sensor_ntc.minimo)*8);
		led_alarm = (int) sensor_ntc.nivel_alarma;
		osMutexRelease(sensorMutex);
		nb_leds = clamp(nb_leds,0,7);
		led_alarm = clamp(led_alarm,0,7);
		render_leds(nb_leds,led_alarm,&last_blinking);

	}
	else {
		osMutexAcquire(sensorMutex, osWaitForever);
		nb_leds = (int) ((sensor_ldr.valor-sensor_ldr.minimo)/(sensor_ldr.maximo - sensor_ldr.minimo)*8);
		led_alarm = (int) sensor_ldr.nivel_alarma;
		osMutexRelease(sensorMutex);
		nb_leds = clamp(nb_leds,0,7);
		led_alarm = clamp(led_alarm,0,7);
		render_leds(nb_leds,led_alarm,&last_blinking);
	}

	if (led_alarm <= nb_leds && (osKernelGetTickCount() - alarm_off_timestamp) >= 5000 ){
		alarm_triggered = 1;
	};


    osDelay(20);
  }
  /* USER CODE END StartTask_Render */
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
  * where the assert_param error has occurred.
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
