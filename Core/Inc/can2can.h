#ifndef _CAN2CAN_H_
#define _CAN2CAN_H_

#define OPERATION_COMMAND_STD_ID          (0x300u)
#define OPERATION_COMMAND_FREQUENCY       (1u)
#define OPERATION_COMMAND_MSG_SIZE        (1u)
#define OPERATION_COMMAND_POS             (0u)
#define OPERATION_COMMAND_OFF             (0x55u)
#define OPERATION_COMMAND_ON              (0xAAu)

#define OPERATION_STATUS_STD_ID           (0x301u)
#define OPERATION_STATUS_FREQUENCY        (10u)
#define OPERATION_STATUS_MSG_SIZE         (2u)
#define OPERATION_STATUS_POS              (0u)
#define OPERATION_VALUE_POS               (1u)
#define OPERATION_STATUS_OFF              (0u)
#define OPERATION_STATUS_ON               (1u)
#define OPERATION_STATUS_VALUE_MODIFIER   (0x01u)

#define OPERATION_STATUS_COUNT            (OPERATION_STATUS_FREQUENCY/ OPERATION_COMMAND_FREQUENCY)

#define MASTER_NODE_POLICY_NUMBER         (0u)
#define SLAVE_NODE_POLICY_NUMBER          (1u)

#define MASTER_TASK_TASK_PRIORITY         (3u)
#define MASTER_TASK_STACK_DEPTH           (128u)

#define SLAVE_TASK_TASK_PRIORITY          (2u)
#define SLAVE_TASK_STACK_DEPTH            (128u)

#define MASTER_NODE_RX_QUEUE_SIZE         (3u)
#define SLAVE_NODE_RX_QUEUE_SIZE          (3u)

#define MASTER_NODE_MAX_EVENTS            (10u)
#define SLAVE_NODE_MAX_EVENTS             (10u)

#define MASTER_NODE_RX_FIFO               (BXCAN_RX_FIFO0)
#define MASTER_NODE_RX_FIFO_NOTIFICATION  (CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_RX_FIFO0_FULL | CAN_IT_RX_FIFO0_OVERRUN)
#define MASTER_NODE_RX_FIFO_CALLBACK      HAL_CAN_RxFifo0MsgPendingCallback

#define SLAVE_NODE_RX_FIFO                (BXCAN_RX_FIFO1)
#define SLAVE_NODE_RX_FIFO_NOTIFICATION   (CAN_IT_RX_FIFO1_MSG_PENDING | CAN_IT_RX_FIFO1_FULL | CAN_IT_RX_FIFO1_OVERRUN)
#define SLAVE_NODE_RX_FIFO_CALLBACK       HAL_CAN_RxFifo1MsgPendingCallback

#if !(OPERATION_COMMAND_FREQUENCY > 0)
#error OPERATION_COMMAND_FREQUENCY must be > 0
#endif /* !(OPERATION_COMMAND_FREQUENCY > 0) */

#if !(OPERATION_STATUS_FREQUENCY > 0)
#error OPERATION_STATUS_FREQUENCY must be > 0
#endif /* !(OPERATION_STATUS_FREQUENCY > 0) */

#if (OPERATION_COMMAND_FREQUENCY > OPERATION_STATUS_FREQUENCY)
#error OPERATION_STATUS_FREQUENCY must be larger than OPERATION_COMMAND_FREQUENCY
#endif /* (OPERATION_COMMAND_FREQUENCY <= OPERATION_STATUS_FREQUENCY) */

#if ((OPERATION_STATUS_COUNT * OPERATION_COMMAND_FREQUENCY) != OPERATION_STATUS_FREQUENCY)
#error OPERATION_STATUS_FREQUENCY must be a multiple of OPERATION_COMMAND_FREQUENCY
#endif /* ((OPERATION_STATUS_COUNT * OPERATION_COMMAND_FREQUENCY) != OPERATION_STATUS_FREQUENCY) */

/**
 * @brief Node event type
 */
typedef enum {
  NO_EVENT,     /* no event */
  TIME_EVENT,   /* timer expired, and it's time to send the next operation command/operation status */
  CAN_TX_EVENT, /* operation command/operation status was transmitted successfully */
  CAN_RX_EVENT, /* operation command/operation status status received */
} EventType_t;

/**
 * @brief Operation status
 */
typedef struct {
  uint8_t status;  /* device operation status: 1: ON, 0: OFF */
  uint8_t value;    /* dummy device value */
} OperationStatus_t;

/**
 * @brief Operation status
 */
typedef uint8_t OperationCommand_t;

/**
 * @brief Received CAN message
 */
typedef struct {
  uint16_t data[8]; /* message data buffer (8 bytes) */
  uint16_t StdId;   /* CAN message standard ID */
  uint8_t DLC;      /* can message data length code */
} CANRxMessage_t;

/**
 * @brief Node event
 */
typedef struct {
  EventType_t type;  /*  event type */
} Event_t;

/**
 * @brief Node state handler event processing result
 */
typedef enum {
  EVENT_IGNORED,  /* event was ignored by the current state handler */
  EVENT_REQUEUE,  /* event was not processed by the current state handler, and should be added back to the event queue */
  EVENT_HANDLED,  /* event was processed by the current state handler */
} StateResult_t;

extern const OperationCommand_t OperationCommandOFF;
extern const OperationCommand_t OperationCommandON;

void MasterNode_Initialize(void);
void SlaveNode_Initialize(void);

#endif /* _CAN2CAN_H_ */