/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    can.c
 * @brief   This file provides code for the configuration
 *          of the CAN instances.
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
#include "can.h"

/* USER CODE BEGIN 0 */
#include "FreeRTOS.h"
#include "semphr.h"
#include "event_groups.h"
#include "queue.h"

#define BXCAN_TX_MB0_FLAG     (1u << BXCAN_TX_MB0)
#define BXCAN_TX_MB1_FLAG     (1u << BXCAN_TX_MB1)
#define BXCAN_TX_MB2_FLAG     (1u << BXCAN_TX_MB2)
#define BXCAN_TX_ALL_FLAGS    (BXCAN_TX_MB0_FLAG | BXCAN_TX_MB1_FLAG | BXCAN_TX_MB2_FLAG)

static EventGroupHandle_t bxCAN_TxEventGroupHandle = NULL;
static StaticEventGroup_t bxCAN_TxEventGroup = {0};
static bxCAN_TxCompleteCallback_t bxCAN_TxCompleteCallbacks [BXCAN_MAX_TX_FIFO] = {0};

/* USER CODE END 0 */

CAN_HandleTypeDef hcan;

/* CAN init function */
void MX_CAN_Init(void) {

  /* USER CODE BEGIN CAN_Init 0 */

  /* USER CODE END CAN_Init 0 */

  /* USER CODE BEGIN CAN_Init 1 */

  /* USER CODE END CAN_Init 1 */
  hcan.Instance = CAN1;
  hcan.Init.Prescaler = 1;
  hcan.Init.Mode = CAN_MODE_LOOPBACK;
  hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan.Init.TimeSeg1 = CAN_BS1_6TQ;
  hcan.Init.TimeSeg2 = CAN_BS2_1TQ;
  hcan.Init.TimeTriggeredMode = DISABLE;
  hcan.Init.AutoBusOff = DISABLE;
  hcan.Init.AutoWakeUp = DISABLE;
  hcan.Init.AutoRetransmission = DISABLE;
  hcan.Init.ReceiveFifoLocked = DISABLE;
  hcan.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN_Init 2 */

  bxCAN_TxEventGroupHandle = xEventGroupCreateStatic(&bxCAN_TxEventGroup);
  xEventGroupSetBits(bxCAN_TxEventGroupHandle, BXCAN_TX_MB0_FLAG | BXCAN_TX_MB1_FLAG | BXCAN_TX_MB2_FLAG);

  /* USER CODE END CAN_Init 2 */
}

void HAL_CAN_MspInit(CAN_HandleTypeDef *canHandle) {

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if (canHandle->Instance == CAN1) {
    /* USER CODE BEGIN CAN1_MspInit 0 */

    /* USER CODE END CAN1_MspInit 0 */
    /* CAN1 clock enable */
    __HAL_RCC_CAN1_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**CAN GPIO Configuration
    PA11     ------> CAN_RX
    PA12     ------> CAN_TX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* CAN1 interrupt Init */
    HAL_NVIC_SetPriority(USB_HP_CAN1_TX_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USB_HP_CAN1_TX_IRQn);
    HAL_NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
    HAL_NVIC_SetPriority(CAN1_SCE_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(CAN1_SCE_IRQn);
    /* USER CODE BEGIN CAN1_MspInit 1 */

    /* USER CODE END CAN1_MspInit 1 */
  }
}

void HAL_CAN_MspDeInit(CAN_HandleTypeDef *canHandle) {

  if (canHandle->Instance == CAN1) {
    /* USER CODE BEGIN CAN1_MspDeInit 0 */

    /* USER CODE END CAN1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_CAN1_CLK_DISABLE();

    /**CAN GPIO Configuration
    PA11     ------> CAN_RX
    PA12     ------> CAN_TX
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_11 | GPIO_PIN_12);

    /* CAN1 interrupt Deinit */
    HAL_NVIC_DisableIRQ(USB_HP_CAN1_TX_IRQn);
    HAL_NVIC_DisableIRQ(USB_LP_CAN1_RX0_IRQn);
    HAL_NVIC_DisableIRQ(CAN1_SCE_IRQn);
    /* USER CODE BEGIN CAN1_MspDeInit 1 */

    /* USER CODE END CAN1_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */
static inline int inHandlerMode (void)
{
  return __get_IPSR() != 0;
}

/* Initialize -------------------------------------------------------------- */

HAL_StatusTypeDef bxCAN_Initialize(void) {

  if (HAL_CAN_ActivateNotification(
    &hcan, 
    CAN_IT_TX_MAILBOX_EMPTY 
    | CAN_IT_RX_FIFO0_MSG_PENDING 
    | CAN_IT_RX_FIFO0_FULL 
    | CAN_IT_RX_FIFO0_OVERRUN 
    | CAN_IT_RX_FIFO1_MSG_PENDING 
    | CAN_IT_RX_FIFO1_FULL 
    | CAN_IT_RX_FIFO1_OVERRUN 
    | CAN_IT_ERROR 
    | CAN_IT_ERROR_WARNING 
    | CAN_IT_ERROR_PASSIVE 
    | CAN_IT_LAST_ERROR_CODE 
    | CAN_IT_BUSOFF
  ) != HAL_OK) {
    Error_Handler();
    return HAL_ERROR;
  }

  xEventGroupSetBits(bxCAN_TxEventGroupHandle, 
    BXCAN_TX_MB0_FLAG | BXCAN_TX_MB1_FLAG | BXCAN_TX_MB2_FLAG
  );

  return HAL_CAN_Start(&hcan);
}

HAL_StatusTypeDef bxCAN_SetFilterPolicy(uint8_t policy_number, uint8_t filter_fifo, bxCAN_Filter_t filter_id, bxCAN_Mask_t filter_mask) {
  CAN_FilterTypeDef filter = {0};

  assert_param(policy_number < BXCAN_FILTER_BANK_MAX);
  assert_param((filter_fifo == CAN_FILTER_FIFO0) || (filter_fifo == CAN_FILTER_FIFO1));

  filter.FilterActivation = ENABLE;
  filter.FilterBank = policy_number;
  filter.FilterFIFOAssignment = filter_fifo;
  filter.FilterMode = CAN_FILTERMODE_IDMASK;
  filter.FilterScale = CAN_FILTERSCALE_32BIT;
  filter.FilterMaskIdHigh = filter_mask.as_u16.high;
  filter.FilterMaskIdLow = filter_mask.as_u16.low;
  filter.FilterIdHigh = filter_id.as_u16.high;
  filter.FilterIdLow = filter_id.as_u16.low;

  if (HAL_CAN_ConfigFilter(&hcan, &filter) != HAL_OK) {
    Error_Handler();
    return HAL_ERROR;
  }

  return HAL_OK;
}

/* Blocking Transmit ------------------------------------------------------- */

HAL_StatusTypeDef bxCAN_Transmit(const uint8_t *const data, uint8_t len, uint16_t std_id, bxCAN_TxCompleteCallback_t callback) {
  CAN_TxHeaderTypeDef tx_header = {0};
  uint32_t mailbox = 0;

  tx_header.DLC = len;
  tx_header.StdId = std_id;
  tx_header.IDE = CAN_ID_STD;
  tx_header.RTR = CAN_RTR_DATA;
  tx_header.TransmitGlobalTime = DISABLE;

  // // wait until 1 tx mailbox is free
  // while(HAL_CAN_GetTxMailboxesFreeLevel(&hcan) == 0) {
  // }

  EventBits_t available_mailbox = xEventGroupWaitBits(
    bxCAN_TxEventGroupHandle, 
    BXCAN_TX_MB0_FLAG | BXCAN_TX_MB1_FLAG | BXCAN_TX_MB2_FLAG,
    0,
    pdFALSE,
    portMAX_DELAY
  );

  if((available_mailbox & BXCAN_TX_MB0_FLAG) == BXCAN_TX_MB0_FLAG) {
    bxCAN_TxCompleteCallbacks[BXCAN_TX_MB0] = callback;
    xEventGroupClearBits(bxCAN_TxEventGroupHandle, BXCAN_TX_MB0_FLAG);

  } else if((available_mailbox & BXCAN_TX_MB1_FLAG) == BXCAN_TX_MB1_FLAG) {
    bxCAN_TxCompleteCallbacks[BXCAN_TX_MB1] = callback;
    xEventGroupClearBits(bxCAN_TxEventGroupHandle, BXCAN_TX_MB1_FLAG);

  } else {
    bxCAN_TxCompleteCallbacks[BXCAN_TX_MB2] = callback;
    xEventGroupClearBits(bxCAN_TxEventGroupHandle, BXCAN_TX_MB2_FLAG);
  }

  // add message to mailbox
  if (HAL_CAN_AddTxMessage(&hcan, &tx_header, data, &mailbox) != HAL_OK) {
    Error_Handler();
    return HAL_ERROR;
  }

  return HAL_OK;
}

/* Blocking Receive ------------------------------------------------------- */

HAL_StatusTypeDef bxCAN_Receive(bxCAN_RxFifo_t rx_fifo, uint8_t *data, uint8_t *len, uint16_t *std_id) {
  CAN_RxHeaderTypeDef rx_header = {0};

  // wait a message on RX FIFO 0
  while (HAL_CAN_GetRxFifoFillLevel(&hcan, rx_fifo) == 0) {
  }

  if (HAL_CAN_GetRxMessage(&hcan, rx_fifo, &rx_header, data) != HAL_OK) {
    Error_Handler();
    return HAL_ERROR;
  }

  (*len) = rx_header.DLC;
  (*std_id) = rx_header.StdId;

  return HAL_OK;
}

/* CAN Callbacks ---------------------------------------------------------- */

static inline BaseType_t __bxCAN_TxCompleteCallback(uint32_t mailbox_id) {
  BaseType_t xTaskWoken = pdFALSE;

  xEventGroupSetBitsFromISR(bxCAN_TxEventGroupHandle, (1u << mailbox_id), &xTaskWoken);
  if(bxCAN_TxCompleteCallbacks[mailbox_id] != NULL) {
    bxCAN_TxCompleteCallbacks[mailbox_id]();
  }

  return xTaskWoken;
}

void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef *hcan) {
  BaseType_t xTaskWoken = __bxCAN_TxCompleteCallback(BXCAN_TX_MB0);
  portYIELD_FROM_ISR(xTaskWoken);
}

void HAL_CAN_TxMailbox1CompleteCallback(CAN_HandleTypeDef *hcan) {
  BaseType_t xTaskWoken = __bxCAN_TxCompleteCallback(BXCAN_TX_MB1);
  portYIELD_FROM_ISR(xTaskWoken);
}

void HAL_CAN_TxMailbox2CompleteCallback(CAN_HandleTypeDef *hcan) {
  BaseType_t xTaskWoken = __bxCAN_TxCompleteCallback(BXCAN_TX_MB2);
  portYIELD_FROM_ISR(xTaskWoken);
}

void HAL_CAN_RxFifo0FullCallback(CAN_HandleTypeDef *hcan) {}

void HAL_CAN_RxFifo1FullCallback(CAN_HandleTypeDef *hcan) {}

void HAL_CAN_SleepCallback(CAN_HandleTypeDef *hcan) {}

void HAL_CAN_WakeUpFromRxMsgCallback(CAN_HandleTypeDef *hcan) {}

void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan) {}

/* USER CODE END 1 */
