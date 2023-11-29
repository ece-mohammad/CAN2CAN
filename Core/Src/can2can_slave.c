#include <string.h>
#include "main.h"
#include "gpio.h"
#include "can.h"
#include "cmsis_os.h"
#include "can2can.h"

/**
 * @brief Slave node state
 */
typedef enum {
  SLAVE_NODE_STATE_IDLE,  /* slave node is idle, waiting for CAN RX event */
  SLAVE_NODE_STATE_TX,  /* slave node waits for CAN tx complete event */
  SLAVE_NODE_WAIT_TIMER,  /* slave node waits for time event to send out operation status */
} SlaveNode_State_t;


/* slave node current state */
static SlaveNode_State_t SlaveNode_CurrentState = SLAVE_NODE_STATE_IDLE;

/* slave node current operation status */
static OperationStatus_t SlaveNode_CurrentOperationStatus = {0};
static OperationCommand_t SlaveNode_CurrentOperationCommand = {0};
static uint32_t SlaveNode_TransmitCount = 0;

/* slave node task */
static TaskHandle_t SlaveNode_TaskHandle = NULL;
static StaticTask_t SlaveNode_TaskBuffer = {0};
static StackType_t SlaveNode_TaskStack[SLAVE_TASK_STACK_DEPTH] = {0};

/* slave node CAN RX message queue */
static QueueHandle_t SlaveNode_RxQueueHandle = NULL;
static uint8_t SlaveNode_RxQueueStorage[SLAVE_NODE_RX_QUEUE_SIZE * OPERATION_COMMAND_MSG_SIZE] = {0};
static StaticQueue_t SlaveNode_RxQueue = {0};

/* slave node event queue */
static QueueHandle_t SlaveNode_EventQueueHandle = NULL;
static StaticQueue_t SlaveNode_EventQueue = {0};
static Event_t SlaveNode_EventQueueStorage[SLAVE_NODE_MAX_EVENTS] = {0};

/* slave node timer */
static TimerHandle_t SlaveNode_TimerHandle = NULL;
static StaticTimer_t SlaveNode_Timer = {0};
static uint32_t SlaveNode_TimerID = 0xF0;

static const bxCAN_Filter_t SlaveNode_CANRxFilter = {
  .as_struct = {
    .RTR = 0, /* data */
    .IDE = 0, /* standard ID */
    .StdId = OPERATION_COMMAND_STD_ID
  }
};

static const bxCAN_Filter_t SlaveNode_CANRxMask = {
  .as_struct = {
    .RTR = 0, /* don't care */
    .IDE = 0, /* don't care */
    .StdId = ~0 /* exact match */
  }
};

/**
 * @brief Slave node timer callback function, sends TIME_EVENT
 * to SlaveTask_EventQueue
 * 
 * @param timer_handle 
 */
static void SlaveNode_TimerCallback(TimerHandle_t timer_handle) {
  Event_t time_event = {
      .type = TIME_EVENT,
  };

  /* send timer event to SlaveTask_EventQueue */
  configASSERT(xQueueSend(SlaveNode_EventQueueHandle, (const void *const)&time_event, 0) == pdTRUE);
}

/**
 * @brief CAN FIFO 0 message pending callback
 * 
 * @param hcan [in] pointer to CAN handle that triggered the callback
 */
void SLAVE_NODE_RX_FIFO_CALLBACK(CAN_HandleTypeDef *hcan) {
  BaseType_t xTaskWoken = pdFALSE;
  Event_t rx_event = {
    .type = CAN_RX_EVENT,
  };

  configASSERT(HAL_CAN_DeactivateNotification(hcan, SLAVE_NODE_RX_FIFO_NOTIFICATION) == HAL_OK);

  xQueueSendFromISR(SlaveNode_EventQueueHandle, &rx_event, &xTaskWoken);
  portYIELD_FROM_ISR(xTaskWoken);
}

/**
 * @brief CAN TX complete callback
 * 
 * @param hcan [in] pointer to CAN handle that triggered the callback
 * @param mailbox [in] mailbox used to transmit the message
 */
static void SlaveNode_BxCANTxCompleteCallback(void) {
  BaseType_t xTaskWoken = pdFALSE;
  Event_t rx_event = {
    .type = CAN_TX_EVENT,
  };

  xQueueSendFromISR(SlaveNode_EventQueueHandle, &rx_event, &xTaskWoken);
  portYIELD_FROM_ISR(xTaskWoken);
}

static inline void SlaveNode_UpdateOperationStatus(void) {
  if(SlaveNode_CurrentOperationCommand == OperationCommandON) {
    SlaveNode_CurrentOperationStatus.status = OPERATION_STATUS_ON;
    SlaveNode_CurrentOperationStatus.value += OPERATION_STATUS_VALUE_MODIFIER;
  } else {
    SlaveNode_CurrentOperationStatus.status = OPERATION_STATUS_OFF;
    SlaveNode_CurrentOperationStatus.value -= OPERATION_STATUS_VALUE_MODIFIER;
  }
}

static inline void SlaveNode_TransmitOperationStatus(void) {
  configASSERT(
    bxCAN_Transmit(
      (uint8_t *)&SlaveNode_CurrentOperationStatus, 
      OPERATION_STATUS_MSG_SIZE, 
      OPERATION_STATUS_STD_ID, 
      SlaveNode_BxCANTxCompleteCallback
    ) == HAL_OK
  );

  configASSERT(xTimerStart(SlaveNode_TimerHandle, portMAX_DELAY) == pdTRUE);
}

/**
 * @brief Slave node idle state handler
 * 
 * @param pEvent [in] pointer to the current event
 */
static StateResult_t SlaveNode_Idle_StateHandler(const Event_t * const pEvent) {
  uint8_t data_buffer [BXCAN_MAX_DATA_SIZE] = {0};
  uint16_t std_id = 0x00;
  uint8_t data_len = 0;

  if(pEvent->type != CAN_RX_EVENT) {
    /* pass event */
    return EVENT_IGNORED;
  }

  /* receive operation command */
  configASSERT(bxCAN_Receive(SLAVE_NODE_RX_FIFO, data_buffer, &data_len, &std_id) == HAL_OK);
  configASSERT(HAL_CAN_ActivateNotification(&hcan, SLAVE_NODE_RX_FIFO_NOTIFICATION) == HAL_OK);

  /* save operation command */
  SlaveNode_CurrentOperationCommand = data_buffer[OPERATION_COMMAND_POS];

  /* update & send operation status */
  SlaveNode_UpdateOperationStatus();
  SlaveNode_TransmitOperationStatus();

  /* event was processed */
  return EVENT_HANDLED;
}

/**
 * @brief Slave node receive status state handler
 * 
 * @param pEvent [in] pointer to the current event
 */
static StateResult_t SlaveNode_WaitTimer_StateHandler(const Event_t * const pEvent) {
  if(pEvent->type != TIME_EVENT) {
    /* pass event */
    return EVENT_IGNORED;
  }

  /* update & send operation status */
  SlaveNode_UpdateOperationStatus();
  SlaveNode_TransmitOperationStatus();

  /* event processed */
  return EVENT_HANDLED;
}

/**
 * @brief Slave node transmit state handler
 * 
 * @param pEvent [in] pointer to the current event
 */
static StateResult_t SlaveNode_Transmit_StateHandler(const Event_t * const pEvent) {
  if(pEvent->type != CAN_TX_EVENT) {
    return EVENT_IGNORED;
  }

  /* increment transmit count */
  SlaveNode_TransmitCount++;

  /* event was processed */
  return EVENT_HANDLED;
}


/**
 * @brief Slave node task, handles events generated by SlaveNode_Timer and HAL_CAN_RxFifo0MsgPendingCallback
 * 
 * @param pvParam 
 */
static void SlaveNode_TaskFunction(void *const pvParam) {
  Event_t current_event = {0};

  while (1) {
    /* reset current event */
    memset(&current_event, 0x00, sizeof(Event_t));

    /* get event */
    xQueueReceive(SlaveNode_EventQueueHandle, (void * const)&current_event, portMAX_DELAY);

    switch(SlaveNode_CurrentState) {
      case SLAVE_NODE_STATE_IDLE: {
        if(SlaveNode_Idle_StateHandler(&current_event) == EVENT_HANDLED) {
          SlaveNode_CurrentState = SLAVE_NODE_STATE_TX;
        }
      } break;

      case SLAVE_NODE_WAIT_TIMER: {
        if(SlaveNode_WaitTimer_StateHandler(&current_event) == EVENT_HANDLED) {
          SlaveNode_CurrentState = SLAVE_NODE_STATE_TX;
        }
      } break;

      case SLAVE_NODE_STATE_TX: {
        if(SlaveNode_Transmit_StateHandler(&current_event) == EVENT_HANDLED) {
          if(SlaveNode_TransmitCount == OPERATION_STATUS_COUNT) {
            SlaveNode_TransmitCount = 0;
            SlaveNode_CurrentState = SLAVE_NODE_STATE_IDLE;
          } else {
            configASSERT(xTimerStart(SlaveNode_TimerHandle, portMAX_DELAY) == pdTRUE);
            SlaveNode_CurrentState = SLAVE_NODE_WAIT_TIMER;
          }
        }
      } break;
      
      default:
      break;
    }
  }

  (void)pvParam;
}

void SlaveNode_Initialize(void) {
  SlaveNode_CurrentState = SLAVE_NODE_STATE_IDLE;
  SlaveNode_CurrentOperationStatus.status = 0;
  SlaveNode_CurrentOperationStatus.value = 0;
  SlaveNode_TransmitCount = 0;

  /* initialize CAN RX filters for operation status STD ID */
  configASSERT(bxCAN_SetFilterPolicy(SLAVE_NODE_POLICY_NUMBER, 
    SLAVE_NODE_RX_FIFO,
    SlaveNode_CANRxFilter, 
    SlaveNode_CANRxMask) == HAL_OK
  );

  /* start CAN */
  if (hcan.State == HAL_CAN_STATE_READY) {
    configASSERT(bxCAN_Initialize() == HAL_OK);
  }

  /* initialize timer */
  SlaveNode_TimerHandle = xTimerCreateStatic(
    "SlaveNodeTimer", 
    pdMS_TO_TICKS(1000 / OPERATION_STATUS_FREQUENCY),
    pdFALSE,
    (const void* const)&SlaveNode_TimerID,
    SlaveNode_TimerCallback, 
    &SlaveNode_Timer
  );

  /* initialize rx queue */
  SlaveNode_RxQueueHandle = xQueueCreateStatic(
    SLAVE_NODE_RX_QUEUE_SIZE,
    OPERATION_COMMAND_MSG_SIZE,
    SlaveNode_RxQueueStorage,
    &SlaveNode_RxQueue
  );

  /* initialize event queue */
  SlaveNode_EventQueueHandle = xQueueCreateStatic(
    SLAVE_NODE_MAX_EVENTS, 
    sizeof(Event_t),
    (uint8_t *)&SlaveNode_EventQueueStorage, 
    &SlaveNode_EventQueue
  );

  // initialize slave node task
  SlaveNode_TaskHandle = xTaskCreateStatic(
    &SlaveNode_TaskFunction, 
    "SlaveNodeTask", 
    SLAVE_TASK_STACK_DEPTH, 
    NULL,
    SLAVE_TASK_TASK_PRIORITY,
    SlaveNode_TaskStack,  /* stack buffer (StackType_t *)  */
    &SlaveNode_TaskBuffer /* task buffer (StaticTask_t *) */
  );
}
