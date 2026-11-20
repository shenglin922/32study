#include "delay.h"
#include "stdio.h"
#include "string.h"
#include "stm32f1xx_hal.h"

extern TIM_HandleTypeDef htim2;

void delay_us(uint16_t us)
{
    uint16_t start = __HAL_TIM_GET_COUNTER(&htim2);
    while((uint16_t)(__HAL_TIM_GET_COUNTER(&htim2) - start) < us);
}