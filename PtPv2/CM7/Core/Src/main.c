/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
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
#include "string.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_eth.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define PTP_ETHType 0x88F7
#define PTP_CLOCK_FREQ 50000000
#define RX_BUFFER_SIZE 1524


#ifndef HSEM_ID_0
#define HSEM_ID_0 (0U) /* HW semaphore 0*/
#endif

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
#if defined ( __ICCARM__ ) /*!< IAR Compiler */
#pragma location=0x30000000
ETH_DMADescTypeDef  DMARxDscrTab[ETH_RX_DESC_CNT]; /* Ethernet Rx DMA Descriptors */
#pragma location=0x30000080
ETH_DMADescTypeDef  DMATxDscrTab[ETH_TX_DESC_CNT]; /* Ethernet Tx DMA Descriptors */

#elif defined ( __CC_ARM )  /* MDK ARM Compiler */

__attribute__((at(0x30000000))) ETH_DMADescTypeDef  DMARxDscrTab[ETH_RX_DESC_CNT]; /* Ethernet Rx DMA Descriptors */
__attribute__((at(0x30000080))) ETH_DMADescTypeDef  DMATxDscrTab[ETH_TX_DESC_CNT]; /* Ethernet Tx DMA Descriptors */

#elif defined ( __GNUC__ ) /* GNU Compiler */

ETH_DMADescTypeDef DMARxDscrTab[ETH_RX_DESC_CNT] __attribute__((section(".RxDescripSection"))); /* Ethernet Rx DMA Descriptors */
ETH_DMADescTypeDef DMATxDscrTab[ETH_TX_DESC_CNT] __attribute__((section(".TxDescripSection")));   /* Ethernet Tx DMA Descriptors */
#endif

ETH_TxPacketConfig TxConfig;

ETH_HandleTypeDef heth;

TIM_HandleTypeDef htim2;

/* USER CODE BEGIN PV */
uint8_t RxBuffer[ETH_RX_DESC_CNT][RX_BUFFER_SIZE]; // Allocate Rx buffers

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void MX_ETH_Init(void);
/* USER CODE BEGIN PFP */
static void InitETH_PTP(void);
void HandleSyncMessage(uint8_t *packetBuffer);
void HandleFollowUpMessage(uint8_t *packetBuffer);
void HandleDelayReqMessage(uint8_t *packetBuffer);
void HandleDelayRespMessage(uint8_t *packetBuffer);
void HAL_ETH_BuildRxDescriptors(ETH_HandleTypeDef *);
static void AdjustLocalClock(uint32_t seconds, uint32_t nanoseconds);
void ConfigurePTPMACFilter(ETH_HandleTypeDef *heth);
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
/* USER CODE BEGIN Boot_Mode_Sequence_0 */
  int32_t timeout;
/* USER CODE END Boot_Mode_Sequence_0 */

/* USER CODE BEGIN Boot_Mode_Sequence_1 */
  /* Wait until CPU2 boots and enters in stop mode or timeout*/
  timeout = 0xFFFF;
  while((__HAL_RCC_GET_FLAG(RCC_FLAG_D2CKRDY) != RESET) && (timeout-- > 0));
  if ( timeout < 0 )
  {
  Error_Handler();
  }
/* USER CODE END Boot_Mode_Sequence_1 */
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();
/* USER CODE BEGIN Boot_Mode_Sequence_2 */
/* When system initialization is finished, Cortex-M7 will release Cortex-M4 by means of
HSEM notification */
/*HW semaphore Clock enable*/
__HAL_RCC_HSEM_CLK_ENABLE();
/*Take HSEM */
HAL_HSEM_FastTake(HSEM_ID_0);
/*Release HSEM in order to notify the CPU2(CM4)*/
HAL_HSEM_Release(HSEM_ID_0,0);
/* wait until CPU2 wakes up from stop mode */
timeout = 0xFFFF;
while((__HAL_RCC_GET_FLAG(RCC_FLAG_D2CKRDY) == RESET) && (timeout-- > 0));
if ( timeout < 0 )
{
Error_Handler();
}
/* USER CODE END Boot_Mode_Sequence_2 */

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM2_Init();
  MX_ETH_Init();
  /* USER CODE BEGIN 2 */
  InitETH_PTP();

  HAL_ETH_Start_IT(&heth);

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
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_CRSInitTypeDef RCC_CRSInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_EXTERNAL_SOURCE_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48|RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 60;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 5;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enable the SYSCFG APB clock
  */
  __HAL_RCC_CRS_CLK_ENABLE();

  /** Configures CRS
  */
  RCC_CRSInitStruct.Prescaler = RCC_CRS_SYNC_DIV1;
  RCC_CRSInitStruct.Source = RCC_CRS_SYNC_SOURCE_PIN;
  RCC_CRSInitStruct.Polarity = RCC_CRS_SYNC_POLARITY_RISING;
  RCC_CRSInitStruct.ReloadValue = __HAL_RCC_CRS_RELOADVALUE_CALCULATE(48000000,1);
  RCC_CRSInitStruct.ErrorLimitValue = 34;
  RCC_CRSInitStruct.HSI48CalibrationValue = 32;

  HAL_RCCEx_CRSConfig(&RCC_CRSInitStruct);
}

/**
  * @brief ETH Initialization Function
  * @param None
  * @retval None
  */
static void MX_ETH_Init(void)
{

  /* USER CODE BEGIN ETH_Init 0 */

  /* USER CODE END ETH_Init 0 */

   static uint8_t MACAddr[6];

  /* USER CODE BEGIN ETH_Init 1 */

  /* USER CODE END ETH_Init 1 */
  heth.Instance = ETH;
  MACAddr[0] = 0x00;
  MACAddr[1] = 0x80;
  MACAddr[2] = 0xE1;
  MACAddr[3] = 0x00;
  MACAddr[4] = 0x00;
  MACAddr[5] = 0x00;
  heth.Init.MACAddr = &MACAddr[0];
  heth.Init.MediaInterface = HAL_ETH_RMII_MODE;
  heth.Init.TxDesc = DMATxDscrTab;
  heth.Init.RxDesc = DMARxDscrTab;
  heth.Init.RxBuffLen = 1524;

  /* USER CODE BEGIN MACADDRESS */
  ConfigurePTPMACFilter(&heth);

  /* USER CODE END MACADDRESS */

  if (HAL_ETH_Init(&heth) != HAL_OK)
  {
    Error_Handler();
  }

  memset(&TxConfig, 0 , sizeof(ETH_TxPacketConfig));
  TxConfig.Attributes = ETH_TX_PACKETS_FEATURES_CSUM | ETH_TX_PACKETS_FEATURES_CRCPAD;
  TxConfig.ChecksumCtrl = ETH_CHECKSUM_IPHDR_PAYLOAD_INSERT_PHDR_CALC;
  TxConfig.CRCPadCtrl = ETH_CRC_PAD_INSERT;
  /* USER CODE BEGIN ETH_Init 2 */
  /* USER CODE END ETH_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_SlaveConfigTypeDef sSlaveConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 4294967295;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sSlaveConfig.SlaveMode = TIM_SLAVEMODE_TRIGGER;
  sSlaveConfig.InputTrigger = TIM_TS_ITR4;
  if (HAL_TIM_SlaveConfigSynchro(&htim2, &sSlaveConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

static void InitETH_PTP(void) {
	ETH_PTP_ConfigTypeDef ptpConfig;

	ptpConfig.Timestamp = ENABLE;
	ptpConfig.TimestampUpdate = ENABLE;
	ptpConfig.TimestampAddendUpdate = ENABLE;
	ptpConfig.TimestampAll = ENABLE;
	ptpConfig.TimestampEthernet = ENABLE;
	ptpConfig.TimestampSnapshots = ENABLE;
	ptpConfig.TimestampFilter = 0x00000300; // Timestamp all PTP packets
	ptpConfig.TimestampSubsecondInc = (uint32_t)(1e9 / PTP_CLOCK_FREQ); // Adjust for your clock
	ptpConfig.TimestampAddend = 0x80000000; // Default addend

	if (HAL_ETH_PTP_SetConfig(&heth, &ptpConfig) != HAL_OK) {
		printf("PTP Configuration failed\n");
		Error_Handler();
	} else {
		printf("PTP Configuration succeeded\n");
	}
}

void HAL_ETH_BuildRxDescriptors(ETH_HandleTypeDef *heth) {
    for (uint32_t i = 0; i < ETH_RX_DESC_CNT; i++) {
        // Clear existing descriptor fields
        DMARxDscrTab[i].DESC0 = 0;
        // Set the buffer address
        DMARxDscrTab[i].BackupAddr0  = (uint32_t)RxBuffer[i];
        // Enable OWN bit for DMA ownership
        DMARxDscrTab[i].DESC0 |= ETH_DMARXNDESCWBF_OWN;
    }
}

void HAL_ETH_RxCpltCallback(ETH_HandleTypeDef *heth) {
    uint8_t *packetBuffer;
    uint16_t ethertype;
    uint8_t ptpMessageType;

    // Read the received packet
    HAL_ETH_ReadData(heth, (void **)&packetBuffer);

    // Extract the EtherType (bytes 12-13 of the Ethernet frame)
    ethertype = (packetBuffer[12] << 8) | packetBuffer[13];

    if (ethertype == PTP_ETHType) // PTP EtherType
    {
        // Extract the PTP message type (byte 0 of the PTP payload)
        ptpMessageType = packetBuffer[14] & 0x0F;

        // Handle different PTP message types
        switch (ptpMessageType)
        {
        case 0x00: // Sync Message
            HandleSyncMessage(packetBuffer);
            break;
        case 0x01: // Delay_Req Message
            HandleDelayReqMessage(packetBuffer);
            break;
        case 0x02: // Follow_Up Message
            HandleFollowUpMessage(packetBuffer);
            break;
        case 0x03: // Delay_Resp Message
            HandleDelayRespMessage(packetBuffer);
            break;
        default:
            printf("Unknown PTP message type: %d\n", ptpMessageType);
            break;
        }
    }
    // Re-enable the DMA descriptor for the next packet
    HAL_ETH_BuildRxDescriptors(heth);
}

void HandleSyncMessage(uint8_t *packetBuffer)
{
    printf("Received Sync message.\n");

    // Extract the OriginTimestamp (bytes 34–41)
    uint32_t seconds = (packetBuffer[34] << 24) |
                       (packetBuffer[35] << 16) |
                       (packetBuffer[36] << 8)  |
                       (packetBuffer[37]);

    uint32_t nanoseconds = (packetBuffer[38] << 24) |
                           (packetBuffer[39] << 16) |
                           (packetBuffer[40] << 8)  |
                           (packetBuffer[41]);

    printf("Sync OriginTimestamp: %lu seconds, %lu nanoseconds\n", seconds, nanoseconds);

    // Use the extracted timestamp for synchronization or conversion to ToD later
}


void HandleFollowUpMessage(uint8_t *packetBuffer)
{
    printf("Received Follow-Up message.\n");

    // Extract the PreciseOriginTimestamp (bytes 34–41)
    uint32_t seconds = (packetBuffer[34] << 24) |
                       (packetBuffer[35] << 16) |
                       (packetBuffer[36] << 8)  |
                       (packetBuffer[37]);

    uint32_t nanoseconds = (packetBuffer[38] << 24) |
                           (packetBuffer[39] << 16) |
                           (packetBuffer[40] << 8)  |
                           (packetBuffer[41]);

    printf("Follow-Up PreciseOriginTimestamp: %lu seconds, %lu nanoseconds\n", seconds, nanoseconds);

    // Use the extracted timestamp for further processing
}


void HandleDelayReqMessage(uint8_t *packetBuffer)
{
    printf("Received Delay_Req message.\n");

    // Process Delay Request message
}

void HandleDelayRespMessage(uint8_t *packetBuffer)
{
    printf("Received Delay_Resp message.\n");

    // Process Delay Response message
}

static void AdjustLocalClock(uint32_t seconds, uint32_t nanoseconds) {
    __HAL_TIM_SET_COUNTER(&htim2, seconds * 1000000000 + nanoseconds);
    printf("Local clock synchronized: %lu seconds, %lu nanoseconds\n", seconds, nanoseconds);
}


void ConfigurePTPMACFilter(ETH_HandleTypeDef *heth) {
	ETH_MACFilterConfigTypeDef macFilterConfig;
	memset(&macFilterConfig, 0, sizeof(macFilterConfig));
	macFilterConfig.PassAllMulticast = ENABLE;
	if (HAL_ETH_SetMACFilterConfig(heth, &macFilterConfig) != HAL_OK) {
        printf("Failed to configure MAC filters.\n");
        Error_Handler();
    } else {
        printf("MAC filters configured for PTP.\n");
	}
}
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
