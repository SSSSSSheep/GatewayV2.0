#ifndef __INF_LCD_H__
#define __INF_LCD_H__
#include "stm32f103xe.h"
#include "Dir_FSMC_LCD.h"
#include "main.h"

#define WRITE_CMD (uint16_t *)0x6C000000
#define WRITE_DATA (uint16_t *)(0x6C000000 + (1 << 11))


#define SCREEN_W 320
#define SCREEN_H 480

/* 常见颜色 */
#define WHITE 0xFFFF
#define BLACK 0x0000
#define BLUE 0x001F
#define BRED 0XF81F
#define GRED 0XFFE0
#define GBLUE 0X07FF
#define RED 0xF800
#define MAGENTA 0xF81F
#define GREEN 0x07E0
#define CYAN 0x7FFF
#define YELLOW 0xFFE0
#define BROWN 0XBC40 // 棕色
#define BRRED 0XFC07 // 棕红色
#define GRAY 0X8430  // 灰色
#define PINK 0XF81F

void Inf_LCD_Init(void);
uint32_t Inf_LCD_GetID(void);
void Inf_LCD_ShowChar(uint16_t x, uint16_t y, uint16_t height, uint8_t c, uint16_t fColor, uint16_t bColor);
void Inf_LCD_ShowString(uint16_t x, uint16_t y, uint16_t height, uint8_t *str, uint16_t fColor, uint16_t bColor);
void Inf_LCD_SetScreenColor(uint16_t sColor);
void Inf_LCD_ShowChinese(uint16_t x, uint16_t y, uint16_t height, uint16_t fColor, uint16_t bColor);
void Inf_LCD_ShowImage(uint16_t x, uint16_t y, uint8_t *image, uint32_t len, uint16_t w, uint16_t h);

void Inf_LCD_DrawPoint(uint16_t x, uint16_t y, uint8_t w, uint16_t fColor);

void Inf_LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t w, uint16_t fColor);

void Inf_LCD_DrawCircle(uint16_t x1, uint16_t y1, uint16_t r, uint8_t w, uint16_t fColor);





#endif
