#include <string.h>
#include "main.h"
#include "gpio.h"
#include "can.h"
#include "cmsis_os.h"
#include "can2can.h"

/**
 * @brief Master node state
 */
typedef enum {
  MASTER_NODE_STATE_IDLE,  /* master node is idle, waiting for time event to send next operation command */
  MASTER_NODE_STATE_TX,  /* master node waits for operation command transmission */
  MASTER_NODE_STATE_RX,  /* master node receives operation status from slave node */
} MasterNode_State_t;


/* operation commands */
const OperationCommand_t OperationCommandOFF = OPERATION_COMMAND_OFF;
const OperationCommand_t OperationCommandON  = OPERATION_COMMAND_ON;

/* master node current state */
static MasterNode_State_t MasterNode_CurrentState = MASTER_NODE_STATE_IDLE;

/* master node current operation status */
static OperationStatus_t MasterNode_CurrentOperationStatus = {0};

static uint32_t MasterNode_ReceivedMessages = 0;

/* master node task */
static TaskHandle_t MasterNode_TaskHandle = NULL;
static StaticTask_t MasterNode_TaskBuffer = {0};
static StackType_t MasterNode_TaskStack[MASTER_TASK_STACK_DEPTH] = {0};

/* master node CAN RX message queue */
static QueueHandle_t MasterNode_RxQueueHandle = NULL;
static uint8_t MasterNode_RxQueueStorage[MASTER_NODE_RX_QUEUE_SIZE * OPERATION_STATUS_MSG_SIZE] = {0};
static StaticQueue_t MasterNode_RxQueue = {0};

/* master node event queue */
static QueueHandle_t MasterNode_EventQueueHandle = NULL;
static StaticQueue_t MasterNode_EventQueue = {0};
static Event_t MasterNode_EventQueueStorage[MASTER_NODE_MAX_EVENTS] = {0};

/* master node timer */
static TimerHandle_t MasterNode_TimerHandle = NULL;
static StaticTimer_t MasterNode_Timer = {0};
static uint32_t MasterNode_TimerID = 0xF0;

static const bxCAN_Filter_t MasterNode_CANRxFilter = {
  .as_struct = {
    .RTR = 0, /* data */
    .IDE = 0, /* standard ID */
    .StdId = OPERATION_STATUS_STD_ID
    // .StdId = OPERATION_COMMAND_STD_ID
  }
};

static const bxCAN_Filter_t MasterNode_CANRxMask = {
  .as_struct = {
    .RTR = 0, /* don't care */
    .IDE = 0, /* don't care */
    .StdId = ~0 /* exact match */
  }
};

/**
 * @brief Master node timer callback function, sends TIME_EVENT
 * to MasterTask_EventQueue
 * 
 * @param timer_handle 
 */
static void MasterNode_TimerCallback(TimerHandle_t timer_handle) {
  Event_t time_event = {
      .type = TIME_EVENT,
  };

  /* send timer event to MasterTask_EventQueue */
  configASSERT(xQueueSend(MasterNode_EventQueueHandle, (const void *const)&time_event, 0) == pdTRUE);
}

/**
 * @brief CAN FIFO 0 message pending callback
 * 
 * @param hcan [in] pointer to CAN handle that triggered the callback
 */
void MASTER_NODE_RX_FIFO_CALLBACK(CAN_HandleTypeDef *hcan) {
  BaseType_t xTaskWoken = pdFALSE;
  Event_t rx_event = {
    .type = CAN_RX_EVENT,
  };

  configASSERT(HAL_CAN_DeactivateNotification(hcan, MASTER_NODE_RX_FIFO_NOTIFICATION) == HAL_OK);

  xQueueSendFromISR(MasterNode_EventQueueHandle, &rx_event, &xTaskWoken);
  portYIELD_FROM_ISR(xTaskWoken);
}

/**
 * @brief CAN TX complete callback
 * 
 * @param hcan [in] pointer to CAN handle that triggered the callback
 * @param mailbox [in] mailbox used to transmit the message
 */
static void MasterNode_BxCANTxCompleteCallback(void) {
  BaseType_t xTaskWoken = pdFALSE;
  Event_t rx_event = {
    .type = CAN_TX_EVENT,
  };

  xQueueSendFromISR(MasterNode_EventQueueHandle, &rx_event, &xTaskWoken);
  portYIELD_FROM_ISR(xTaskWoken);
}

/**
 * @brief Master node idle state handler
 * 
 * @param pEvent [in] pointer to the current event
 */
static StateResult_t MasterNode_Idle_StateHandler(const Event_t * const pEvent) {
  const uint8_t * command = NULL;

  if(pEvent->type != TIME_EVENT) {
    /* pass event */
    return EVENT_IGNORED;
  }

  /* select command based on current operation status */
  if(MasterNode_CurrentOperationStatus.status == 0x00) {
    command = (uint8_t *)&OperationCommandON;
  } else {
    command = (uint8_t *)&OperationCommandOFF;
  }

  /* send command */
  configASSERT(
    bxCAN_Transmit(
      command, 
      OPERATION_COMMAND_MSG_SIZE, 
      OPERATION_COMMAND_STD_ID, 
      MasterNode_BxCANTxCompleteCallback) 
    == HAL_OK
  );

  /* event was processed */
  return EVENT_HANDLED;
}

/**
 * @brief Master node transmit state handler
 * 
 * @param pEvent [in] pointer to the current event
 */
static StateResult_t MasterNode_Transmit_StateHandler(const Event_t * const pEvent) {
  if(pEvent->type != CAN_TX_EVENT) {
    return EVENT_IGNORED;
  }

  /* reset received message count */
  MasterNode_ReceivedMessages = 0;

  /* event was processed */
  return EVENT_HANDLED;
}

/**
 * @brief Master node receive status state handler
 * 
 * @param pEvent [in] pointer to the current event
 */
static StateResult_t MasterNode_ReceiveStatus_StateHandler(const Event_t * const pEvent) {
  uint8_t rx_message [BXCAN_MAX_DATA_SIZE] = {0};
  uint16_t StdId = 0;
  uint8_t len = 0;

  if(pEvent->type != CAN_RX_EVENT) {
    /* pass event */
    return EVENT_IGNORED;
  }

  /* receive message from CAN RX message queue */
  configASSERT(bxCAN_Receive(MASTER_NODE_RX_FIFO, rx_message, &len, &StdId) == HAL_OK);
  configASSERT(HAL_CAN_ActivateNotification(&hcan, MASTER_NODE_RX_FIFO_NOTIFICATION) == HAL_OK);

  /* process received message */
  MasterNode_CurrentOperationStatus.status = rx_message[0];
  MasterNode_CurrentOperationStatus.value  = rx_message[1];
  
  /* update received message count */
  MasterNode_ReceivedMessages++;
  if(MasterNode_ReceivedMessages < OPERATION_STATUS_COUNT) {
    /* event processed */
    return EVENT_IGNORED;
  }
  
  /* event processed */
  return EVENT_HANDLED;
}


/**
 * @brief Master node task, handles events generated by MasterNode_Timer and HAL_CAN_RxFifo0MsgPendingCallback
 * 
 * @param pvParam 
 */
static void MasterNode_TaskFunction(void *const pvParam) {
  Event_t current_event = {0};

  xTimerStart(MasterNode_TimerHandle, portMAX_DELAY);

  while (1) {
    /* reset current event */
    memset(&current_event, 0x00, sizeof(Event_t));

    /* get event */
    xQueueReceive(MasterNode_EventQueueHandle, (void * const)&current_event, portMAX_DELAY);

    switch(MasterNode_CurrentState) {
      case MASTER_NODE_STATE_IDLE: {
        if(MasterNode_Idle_StateHandler(&current_event) == EVENT_HANDLED) {
          MasterNode_CurrentState = MASTER_NODE_STATE_TX;
        }
      } break;

      case MASTER_NODE_STATE_TX: {
        if(MasterNode_Transmit_StateHandler(&current_event) == EVENT_HANDLED) {
          MasterNode_CurrentState = MASTER_NODE_STATE_RX;
        }
      } break;

      case MASTER_NODE_STATE_RX: {
        if(MasterNode_ReceiveStatus_StateHandler(&current_event) == EVENT_HANDLED) {
          MasterNode_CurrentState = MASTER_NODE_STATE_IDLE;
        }
      } break;
      
      default:
      break;
    }
  }

  (void)pvParam;
}

void MasterNode_Initialize(void) {
  MasterNode_CurrentState = MASTER_NODE_STATE_IDLE;
  MasterNode_CurrentOperationStatus.status = 0;
  MasterNode_CurrentOperationStatus.value = 0;
  MasterNode_ReceivedMessages = 0;

  /* initialize CAN RX filters for operation status STD ID */
  configASSERT(bxCAN_SetFilterPolicy(MASTER_NODE_POLICY_NUMBER, 
    MASTER_NODE_RX_FIFO,
    MasterNode_CANRxFilter, 
    MasterNode_CANRxMask) == HAL_OK
  );

  /* start CAN */
  if (hcan.State == HAL_CAN_STATE_READY) {
    configASSERT(bxCAN_Initialize() == HAL_OK);
  }

  /* initialize timer */
  MasterNode_TimerHandle = xTimerCreateStatic(
    "MasterNodeTimer", 
    pdMS_TO_TICKS(1000u / OPERATION_COMMAND_FREQUENCY),
    pdTRUE,
    (const void* const)&MasterNode_TimerID,
    MasterNode_TimerCallback, 
    &MasterNode_Timer
  );

  /* initialize rx queue */
  MasterNode_RxQueueHandle = xQueueCreateStatic(
    MASTER_NODE_RX_QUEUE_SIZE,
    OPERATION_STATUS_MSG_SIZE,
    MasterNode_RxQueueStorage,
    &MasterNode_RxQueue
  );

  /* initialize event queue */
  MasterNode_EventQueueHandle = xQueueCreateStatic(
    MASTER_NODE_MAX_EVENTS, 
    sizeof(Event_t),
    (uint8_t *)&MasterNode_EventQueueStorage, 
    &MasterNode_EventQueue
  );

  // initialize master node task
  MasterNode_TaskHandle = xTaskCreateStatic(
    &MasterNode_TaskFunction, 
    "MasterNodeTask", 
    MASTER_TASK_STACK_DEPTH, 
    NULL,
    MASTER_TASK_TASK_PRIORITY,
    MasterNode_TaskStack,  /* stack buffer (StackType_t *)  */
    &MasterNode_TaskBuffer /* task buffer (StaticTask_t *) */
  );
}
