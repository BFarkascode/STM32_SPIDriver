/* USER CODE BEGIN Header */
/**
 *  Author: BalazsFarkas
 *  Project: STM32_SPIDriver
 *  Processor: STM32L053R8
 *  Compiler: ARM-GCC (STM32 IDE)
 *  Program version: 1.0
 *  File: main.c
 *  Hardware description/pin distribution: SPI pins on PA5, PA6, PA7, CS pin selected in main.c
 *  Modified from: N/A
 *  Change history: N/A
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "SPIDriver_STM32L0x3.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

//retarget printf to the CubeIDE serial port
int _write(int file, char *ptr, int len)
{
	int DataIdx;
	for (DataIdx = 0; DataIdx < len; DataIdx++) {
		HAL_UART_Transmit(&huart2, (uint8_t *)ptr++, 1, 100);			//here we pass the dereferenced pointer, one byte is sent over with a timeout of 100, using the huart2 settings
	}
	return len;
}



//BMP280 temperature compensation calculation for fixed point results
int32_t compensate_temperature(int32_t sensor_adc_readout, uint16_t dig_T1, int16_t dig_T2, int16_t dig_T3) {

	int32_t var1, var2;

	var1 = ((((sensor_adc_readout >> 3) - (dig_T1 << 1)))	* dig_T2) >> 11;
	var2 = (((((sensor_adc_readout >> 4) - dig_T1) * ((sensor_adc_readout >> 4) - dig_T1)) >> 12) * dig_T3) >> 14;

	return ((var1 + var2) * 5 + 128) >> 8;
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
//  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  SysClockConfig();																		//we set up sysclk at 32 MHz using PLL, APB2 at 8 MHz
  TIM6Config();																			//TIM6 set for time measurement

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();

  /* USER CODE BEGIN 2 */
  SPI1MasterInit(GPIOB, 6);																//we initialise SPI1 as master peripheral

  uint8_t reset_sensor_msg[2] = {0x60, 0xB6};											//SPI message array. First element is the address of the register we write to, second is the desired message.
  uint8_t reply_id_msg[2] = {0xD0, 0xFF};												//we store the address at [0] and leave the rest of the buffer empty for incoming data
  uint8_t std_setup[2] = {0x74, 0x27};													//standard function setup
  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	  	//Note: 0x74 is 0xF4 without the MSB

  SPI1MasterRead (reply_id_msg[0], &reply_id_msg[1], 1, GPIOB, 6);						//we read out the sensor ID from the sensor
  printf("Custom readout for device id is 0x%x \r\n", reply_id_msg[1]);

  SPI1MasterWrite(reset_sensor_msg[0], &reset_sensor_msg[1], 1, GPIOB, 6);				//we send a reset sensor message by publishing the reset array. External CS/SS will be on PB6.
  Delay_us(100);
  SPI1MasterWrite(std_setup[0], &std_setup[1], 1, GPIOB, 6);							//we use the standard run mode
  Delay_us(100);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {

	//we define the SPI message matrices (see datasheet)
	//Note: we will be doing a one-by-one swap and use the same matrices to store the extracted values. We can do this since reading from the SPI is a write followed by a read.

	uint8_t T_comp[6] = {0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D};							//this is where the temperature compensation parameters are
	uint8_t T_out[3] = {0xFA, 0xFB, 0xFC};												//this is where the ADC temp values will go

	//BMP280 temperature coefficient readout
	SPI1MasterRead(T_comp[0], &T_comp[0], 1, GPIOB, 6);
	Delay_us(100);
	SPI1MasterRead(T_comp[1], &T_comp[1], 1, GPIOB, 6);
	Delay_us(100);
	SPI1MasterRead(T_comp[2], &T_comp[2], 1, GPIOB, 6);
	Delay_us(100);
	SPI1MasterRead(T_comp[3], &T_comp[3], 1, GPIOB, 6);
	Delay_us(100);
	SPI1MasterRead(T_comp[4], &T_comp[4], 1, GPIOB, 6);
	Delay_us(100);
	SPI1MasterRead(T_comp[5], &T_comp[5], 1, GPIOB, 6);
	Delay_us(100);

	//BMP280 temperature ADC readout
	SPI1MasterRead(T_out[0], &T_out[0], 1, GPIOB, 6);
	Delay_us(100);
	SPI1MasterRead(T_out[1], &T_out[1], 1, GPIOB, 6);
	Delay_us(100);
	SPI1MasterRead(T_out[1], &T_out[2], 1, GPIOB, 6);

	//We rebuild the parameters from the readout
	uint16_t dig_T1 = (T_comp[1] << 8) | T_comp[0];
	int16_t dig_T2 = (T_comp[3] << 8) | T_comp[2];
	int16_t dig_T3 = (T_comp[5] << 8) | T_comp[4];


	int32_t adc_T = (T_out[0] << 12) | (T_out[1] << 4) | (T_out[2]>>4);						//we rebuild the 20 bit temperature value
	int32_t temperature = compensate_temperature(adc_T, dig_T1, dig_T2, dig_T3);

	printf("Temperature measured from the device is %i.%i degrees Celsius \r\n", (temperature / 100), (temperature - (temperature / 100) * 100));
	printf(" \r\n");
	Delay_ms(1000);

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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_5;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART2;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
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
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
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

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

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
