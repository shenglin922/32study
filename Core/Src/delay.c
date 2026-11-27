#include "delay.h"
#include "stdio.h"
#include "string.h"
#include "stm32f1xx_hal.h"

extern TIM_HandleTypeDef htim2;

/**
 * @brief  基于 TIM2（72 MHz 源，预分频 71，计数频率 1 MHz）实现微秒级忙等延时
 * @note   最大延时约 65535 µs；不可在关中断后超过 TIM2 一圈（65535 µs）使用
 * @param  us  延时微秒数
 */
void delay_us(uint16_t us)
{
    uint16_t start = __HAL_TIM_GET_COUNTER(&htim2);
    while((uint16_t)(__HAL_TIM_GET_COUNTER(&htim2) - start) < us);
}