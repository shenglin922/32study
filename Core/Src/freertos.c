/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "modbus.h"
#include "temp_task.h"
#include "cmsis_os.h"
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
/* USER CODE BEGIN Variables */
SemaphoreHandle_t g_reg_mutex = NULL;
extern UART_HandleTypeDef huart1;
extern osThreadId ModbusTaskHandle;
extern osThreadId TempTaskHandle;
extern osThreadId CliTaskHandle;
/* USER CODE END Variables */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

/* GetIdleTaskMemory prototype (linked to static allocation support) */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );

/* USER CODE BEGIN GET_IDLE_TASK_MEMORY */
static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
  *ppxIdleTaskStackBuffer = &xIdleStack[0];
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
  /* place for user code */
}
/* USER CODE END GET_IDLE_TASK_MEMORY */

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
void StartModbusTask(void const * argument)
{
  for(;;)
  {
    modbus_poll();
    osDelay(2);   /* 关键：让出 CPU 给低优先级任务 */
  }
}

void StartTempTask(void const * argument)
{
  for(;;)
  {
    temp_task_rtos_step();
  }
}

void StartCliTask(void const * argument)
{
  char buf[128];
  for(;;)
  {
    /* 查询三个任务的剩余最小栈空间（单位：Word，乘 4 换算成字节） */
    UBaseType_t wm_mb   = uxTaskGetStackHighWaterMark(ModbusTaskHandle) * 4;
    UBaseType_t wm_temp = uxTaskGetStackHighWaterMark(TempTaskHandle) * 4;
    UBaseType_t wm_cli  = uxTaskGetStackHighWaterMark(CliTaskHandle) * 4;
    size_t free_heap    = xPortGetFreeHeapSize();

    int len = snprintf(buf, sizeof(buf),
                       "[RTOS] T=%d.%02dC ST=%u TH=%d | StackFree(B): MB=%lu TMP=%lu CLI=%lu | HeapFree=%u\r\n",
                       g_temp_x100 / 100, (g_temp_x100 >= 0 ? g_temp_x100 % 100 : -g_temp_x100 % 100),
                       g_dev_status, g_alarm_th_x100,
                       (unsigned long)wm_mb, (unsigned long)wm_temp, (unsigned long)wm_cli,
                       (unsigned int)free_heap);
    HAL_UART_Transmit(&huart1, (uint8_t *)buf, len, 100);

    osDelay(3000);
  }
}
/* USER CODE END Application */

