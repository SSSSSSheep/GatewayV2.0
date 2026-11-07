
#ifndef _18B20_H
#define _18B20_H

#include "main.h"
#include "tim.h"

#define DS18_L  GPIOC->ODR &= ~GPIO_ODR_ODR3
#define DS18_H  GPIOC->ODR |= GPIO_ODR_ODR3
// void Common_Delay_Ms(uint32_t ms);
// void Common_Delay_Us(uint32_t us);
// 启动成功返回1，无响应返回0，不在主循环内反复等待传感器。
uint8_t DS18B20_StartConversion(void);
// void TemperatureConvert(void);
// float GetTemperature(void);
uint16_t DS18B20_ReadTemperature(void);
// char Int_DS18B20_GetTemperature(void);
void DelayUS(uint16_t us);
#endif






