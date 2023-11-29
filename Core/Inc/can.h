/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    can.h
 * @brief   This file contains all the function prototypes for
 *          the can.c file
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
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __CAN_H__
#define __CAN_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

extern CAN_HandleTypeDef hcan;

/* USER CODE BEGIN Private defines */
#define BXCAN_MAX_DATA_SIZE   (8u)
#define BXCAN_FILTER_BANK_MAX (14u)
#define BXCAN_MAX_TX_FIFO     (3u)
#define BXCAN_MAX_RX_FIFO     (3u)

#define BXCAN_TX_MB0          (0u)
#define BXCAN_TX_MB1          (1u)
#define BXCAN_TX_MB2          (2u)

typedef union bxCAN_Filter_t {
    struct {
        uint32_t __const: 1;
        uint32_t RTR:     1;
        uint32_t IDE:     1;
        uint32_t ExtId:   18;
        uint32_t StdId:   11;
    } as_struct;
    struct {
        uint16_t low;
        uint16_t high;
    } as_u16;
    uint32_t as_u32;
} bxCAN_Filter_t;

typedef enum {
    BXCAN_RX_FIFO0 = CAN_RX_FIFO0,
    BXCAN_RX_FIFO1 = CAN_RX_FIFO1,
} bxCAN_RxFifo_t;

typedef bxCAN_Filter_t bxCAN_Mask_t;

typedef void (* bxCAN_TxCompleteCallback_t)(void);

/* USER CODE END Private defines */

void MX_CAN_Init(void);

/* USER CODE BEGIN Prototypes */
HAL_StatusTypeDef bxCAN_Initialize(void);
HAL_StatusTypeDef bxCAN_SetFilterPolicy(uint8_t policy_number, uint8_t filter_fifo, bxCAN_Filter_t filter_id, bxCAN_Mask_t filter_mask);
HAL_StatusTypeDef bxCAN_Transmit(const uint8_t *const data, uint8_t len, uint16_t std_id, bxCAN_TxCompleteCallback_t callback);
HAL_StatusTypeDef bxCAN_Receive(bxCAN_RxFifo_t rx_fifo, uint8_t *data, uint8_t *len, uint16_t *std_id);
void bxCAN_TxCompleteCallback(CAN_HandleTypeDef * hcan, uint32_t mailbox);
/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __CAN_H__ */
