#include "Inf_RGB.h"

#define LED_NUM  1  // WS2812 LED的数量
#define BIT_PER_LED 24  // 每个LED的比特数

// WS2812 的 PWM 协议
#define WS2812_HIGH 60   // 占空比 0.8us 高电平 (基于72MHz的时钟)
#define WS2812_LOW  30   // 占空比 0.4us 低电平

uint8_t led_data[LED_NUM][3];  // 存储 LED 的 RGB 数据
uint16_t pwm_data[LED_NUM * BIT_PER_LED * 3];  // 存储 PWM 信号



void Inf_Send_Bit(int8_t val)
{ 
  if(val != 1)
  {
      GPIOG->ODR |= GPIO_PIN_11;
      __NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();
      GPIOG->ODR &= ~GPIO_PIN_11;
      __NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();
      __NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();

  }
  else
  {
      GPIOG->ODR |= GPIO_PIN_11;
      __NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();
      __NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();
      __NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();
      __NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();
      GPIOG->ODR &= ~GPIO_PIN_11;
      __NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();
      __NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();
      __NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();__NOP();
  }

}
void Inf_Send_Res()
{
  HAL_GPIO_WritePin(GPIOG,GPIO_PIN_11,GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOG,GPIO_PIN_11,GPIO_PIN_RESET);
  HAL_Delay(1);
}



My_24bits_t rgb_val;

void Inf_Send_Color(uint8_t r, uint8_t g, uint8_t b)
{

  rgb_val.R_VAL = r;
  rgb_val.G_VAL = g;
  rgb_val.B_VAL = b;
  uint8_t i = 0;
  for (i = 0; i < 8; i++)
  {
    Inf_Send_Bit(rgb_val.G_VAL >> (7 - i) & 0x01);
  }
  for (i = 8; i < 16; i++)
  {
    Inf_Send_Bit(rgb_val.R_VAL >> (15 - i) & 0x01);
  }
  for (i = 16; i < 24; i++)
  {
    Inf_Send_Bit(rgb_val.B_VAL >> (23 - i) & 0x01);
  }
  
  
}
