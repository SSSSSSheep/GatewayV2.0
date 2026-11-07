#include "Inf_LCD.h"
#include "Inf_LCD_Font.h"

/**
 * @brief 向LCD写命令
 *
 * @param cmd
 */
void Inf_LCD_WriteCmd(uint16_t cmd)
{
    *WRITE_CMD = cmd;
}
/**
 * @brief 向LCD写数据
 *
 * @param _data
 */
void Inf_LCD_WriteData(uint16_t _data)
{
    *WRITE_DATA = _data;
}
/**
 * @brief 读取LCD信息
 *
 * @return uint16_t
 */
uint16_t Inf_LCD_ReadData()
{
    return *WRITE_DATA;
}
/**
 * @brief LCD复位
 *
 */
void Inf_LCD_Reset()
{
    RST_LOW;
    HAL_Delay(800);
    RST_HIGH;
    HAL_Delay(100);
}

void Inf_LCD_RegConfig()
{
    /* 1. 设置灰阶电压以调整TFT面板的伽马特性， 正校准。一般出厂就设置好了 */
    Inf_LCD_WriteCmd(0xE0);
    Inf_LCD_WriteData(0x00);
    Inf_LCD_WriteData(0x07);
    Inf_LCD_WriteData(0x10);
    Inf_LCD_WriteData(0x09);
    Inf_LCD_WriteData(0x17);
    Inf_LCD_WriteData(0x0B);
    Inf_LCD_WriteData(0x41);
    Inf_LCD_WriteData(0x89);
    Inf_LCD_WriteData(0x4B);
    Inf_LCD_WriteData(0x0A);
    Inf_LCD_WriteData(0x0C);
    Inf_LCD_WriteData(0x0E);
    Inf_LCD_WriteData(0x18);
    Inf_LCD_WriteData(0x1B);
    Inf_LCD_WriteData(0x0F);

    /* 2. 设置灰阶电压以调整TFT面板的伽马特性，负校准 */
    Inf_LCD_WriteCmd(0XE1);
    Inf_LCD_WriteData(0x00);
    Inf_LCD_WriteData(0x17);
    Inf_LCD_WriteData(0x1A);
    Inf_LCD_WriteData(0x04);
    Inf_LCD_WriteData(0x0E);
    Inf_LCD_WriteData(0x06);
    Inf_LCD_WriteData(0x2F);
    Inf_LCD_WriteData(0x45);
    Inf_LCD_WriteData(0x43);
    Inf_LCD_WriteData(0x02);
    Inf_LCD_WriteData(0x0A);
    Inf_LCD_WriteData(0x09);
    Inf_LCD_WriteData(0x32);
    Inf_LCD_WriteData(0x36);
    Inf_LCD_WriteData(0x0F);

    /* 3.  Adjust Control 3 (F7h)  */
    /*LCD_WriteCmd(0XF7);
   Inf_LCD_WriteData(0xA9);
   Inf_LCD_WriteData(0x51);
   Inf_LCD_WriteData(0x2C);
   Inf_LCD_WriteData(0x82);*/
    /* DSI write DCS command, use loose packet RGB 666 */

    /* 4. 电源控制1*/
    Inf_LCD_WriteCmd(0xC0);
    Inf_LCD_WriteData(0x11); /* 正伽马电压 */
    Inf_LCD_WriteData(0x09); /* 负伽马电压 */

    /* 5. 电源控制2 */
    Inf_LCD_WriteCmd(0xC1);
    Inf_LCD_WriteData(0x02);
    Inf_LCD_WriteData(0x03);

    /* 6. VCOM控制 */
    Inf_LCD_WriteCmd(0XC5);
    Inf_LCD_WriteData(0x00);
    Inf_LCD_WriteData(0x0A);
    Inf_LCD_WriteData(0x80);

    /* 7. Frame Rate Control (In Normal Mode/Full Colors) (B1h) */
    Inf_LCD_WriteCmd(0xB1);
    Inf_LCD_WriteData(0xB0);
    Inf_LCD_WriteData(0x11);

    /* 8.  Display Inversion Control (B4h) （正负电压反转，减少电磁干扰）*/
    Inf_LCD_WriteCmd(0xB4);
    Inf_LCD_WriteData(0x02);

    /* 9.  Display Function Control (B6h)  */
    Inf_LCD_WriteCmd(0xB6);
    Inf_LCD_WriteData(0x0A);
    Inf_LCD_WriteData(0xA2);

    /* 10. Entry Mode Set (B7h)  */
    Inf_LCD_WriteCmd(0xB7);
    Inf_LCD_WriteData(0xc6);

    /* 11. HS Lanes Control (BEh) */
    Inf_LCD_WriteCmd(0xBE);
    Inf_LCD_WriteData(0x00);
    Inf_LCD_WriteData(0x04);

    /* 12.  Interface Pixel Format (3Ah) */
    Inf_LCD_WriteCmd(0x3A);
    Inf_LCD_WriteData(0x55); /* 0x55 : 16 bits/pixel  */

    /* 13. Sleep Out (11h) 关闭休眠模式 */
    Inf_LCD_WriteCmd(0x11);

    /* 14. 设置屏幕方向和RGB */
    Inf_LCD_WriteCmd(0x36);
    Inf_LCD_WriteData(0x08);

    HAL_Delay(120);

    /* 14. display on */
    Inf_LCD_WriteCmd(0x29);
}
void Inf_LCD_Init()
{
    // 先初始化FSMC
    Dir_FSMC_LCD_Init();
    // 重启LCD
    Inf_LCD_Reset();
    // 打开背光灯
    BG_HIGH;

    // 其他初始化
    Inf_LCD_RegConfig();
}
/**
 * @brief 获取LCD的ID
 *
 * @return uint32_t
 */
uint32_t Inf_LCD_GetID()
{
    uint32_t id = 0;
    Inf_LCD_WriteCmd(0x04);
    Inf_LCD_ReadData();
    id |= Inf_LCD_ReadData() << 16;
    id |= Inf_LCD_ReadData() << 8;
    id |= Inf_LCD_ReadData() << 0;
    return id;
}
/**
 * @brief 设置字符在哪个位置
 *
 * @param x
 * @param y
 * @param w
 * @param h
 */
void Inf_LCD_SetSize(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    Inf_LCD_WriteCmd(0x2A);             // 发送设置列位置命令
    Inf_LCD_WriteData((x >> 8) & 0xff); // 发送列起始位置
    Inf_LCD_WriteData((x) & 0xff);

    Inf_LCD_WriteData(((x + w - 1) >> 8) & 0xff); // 发送列结束位置
    Inf_LCD_WriteData((x + w - 1) & 0xff);        // 发送列结束位置

    Inf_LCD_WriteCmd(0x2B);             // 发送设置行位置命令
    Inf_LCD_WriteData((y >> 8) & 0xff); // 发送行起始位置
    Inf_LCD_WriteData((y) & 0xff);

    Inf_LCD_WriteData(((y + h - 1) >> 8) & 0xff); // 发送行结束位置
    Inf_LCD_WriteData((y + h - 1) & 0xff);        // 发送行结束位置
}
/**
 * @brief 在LCD上显示一个字符
 *
 * @param x 为显示字符的位置
 * @param y 为显示字符的位置
 * @param height 显示字符的大小 16 ，24 ，32
 * @param c 要显示的字符
 * @param fColor 字符的颜色
 * @param bColor 字符的背景颜色
 */
void Inf_LCD_ShowChar(uint16_t x, uint16_t y, uint16_t height, uint8_t c, uint16_t fColor, uint16_t bColor)
{
    // 先设置字符尺寸
    Inf_LCD_SetSize(x, y, height / 2, height);
    // 得到字符集下标
    uint8_t index = c - ' ';

    Inf_LCD_WriteCmd(0x2C);
    if (height == 16)
    {
        for (uint8_t i = 0; i < height; i++)
        {
            // 取出字符
            uint8_t value = ascii_1608[index][i];
            for (uint8_t j = 0; j < 8; j++)
            {
                if (value & 0x01)
                {
                    Inf_LCD_WriteData(fColor);
                }
                else
                {
                    Inf_LCD_WriteData(bColor);
                }
                value >>= 1;
            }
        }
    }
    else if (height == 24)
    {
        for (uint8_t i = 0; i < height * 2; i++)
        {
            // 取出字符
            uint8_t value = ascii_2412[index][i];
            uint8_t max = ((i % 2) == 0) ? 8 : 4;
            for (uint8_t j = 0; j < max; j++)
            {
                if (value & 0x01)
                {
                    Inf_LCD_WriteData(fColor);
                }
                else
                {
                    Inf_LCD_WriteData(bColor);
                }
                value >>= 1;
            }
        }
    }
    else if (height == 32)
    {
        for (uint8_t i = 0; i < height * 2; i++)
        {
            // 取出字符
            uint8_t value = ascii_3216[index][i];
            for (uint8_t j = 0; j < 8; j++)
            {
                if (value & 0x01)
                {
                    Inf_LCD_WriteData(fColor);
                }
                else
                {
                    Inf_LCD_WriteData(bColor);
                }
                value >>= 1;
            }
        }
    }
}
/**
 * @brief 显示字符串
 *
 * @param x
 * @param y
 * @param height
 * @param str
 * @param fColor
 * @param bColor
 */
void Inf_LCD_ShowString(uint16_t x, uint16_t y, uint16_t height, uint8_t *str, uint16_t fColor, uint16_t bColor)
{

    uint8_t index = 0;
    while (str[index] != '\0')
    {
        if (str[index] == '\n') //判断是不是换行
        {
            y += height;
            x = 0;
            index++;
            continue;
        }
        if (x < (SCREEN_W - height / 2)) // 判断要不要换行
        {

            Inf_LCD_ShowChar(x, y, height, str[index], fColor, bColor);
            x += height / 2; // 设置下一个要显示字符的宽度
            index++;
        }
        else
        {
            y += height;
            x = 0;
        }
    }
}
/**
 * @brief 把屏幕颜色全部设置为一个颜色
 *
 * @param sColor
 */
void Inf_LCD_SetScreenColor(uint16_t sColor)
{
    // 先设置尺寸
    Inf_LCD_SetSize(0, 0, SCREEN_W, SCREEN_H);
    // 再设置给每个值设置颜色
    Inf_LCD_WriteCmd(0x2C);
    for (uint32_t i = 0; i < SCREEN_H * SCREEN_W; i++)
    {
        Inf_LCD_WriteData(sColor);
    }
}

/**
 * @brief 显示一个汉字
 *
 * @param x 再哪个位置显示
 * @param y 再哪个位置显示
 * @param height 要显示多大
 * @param fColor 字体颜色
 * @param bColor 字体背景颜色
 */
void Inf_LCD_ShowChinese(uint16_t x, uint16_t y, uint16_t height, uint16_t fColor, uint16_t bColor)
{
    Inf_LCD_SetSize(x, y, height, height); // 设置字符的大小

    Inf_LCD_WriteCmd(0x2C); // 开始填充颜色命令

    for (uint32_t i = 0; i < height * (height / 8); i++)
    {
        uint8_t value = handsome[i];
        for (uint8_t j = 0; j < 8; j++)
        {
            if (value & 0x01)
            {
                Inf_LCD_WriteData(fColor);
            }
            else
            {
                Inf_LCD_WriteData(bColor);
            }
            value >>= 1;
        }
    }
}

/**
 * @brief 显示一个图片
 * 
 * @param x 
 * @param y 
 * @param image 
 * @param len 
 * @param w 
 * @param h 
 */
void Inf_LCD_ShowImage(uint16_t x, uint16_t y, uint8_t *image, uint32_t len, uint16_t w, uint16_t h)
{
    Inf_LCD_SetSize(x, y, w, h); // 设置字符的大小
    Inf_LCD_WriteCmd(0x2C); // 开始填充颜色命令
    for (uint32_t i = 0; i < len; i+=2)
    {
        uint16_t color = (image[i]) + (image[i + 1] << 8);
        Inf_LCD_WriteData(color);
    }
    
}

/**
 * @brief 显示一个点
 * 
 * @param x 
 * @param y 
 * @param w 
 * @param fColor 
 */
void Inf_LCD_DrawPoint(uint16_t x, uint16_t y, uint8_t w,uint16_t fColor)
{
    Inf_LCD_SetSize(x,y,w,w);
    Inf_LCD_WriteCmd(0x2C);

    for (uint8_t i = 0; i < w * w; i++)
    {
        Inf_LCD_WriteData(fColor);
    }
}
//y1 = kx1 + b
//y2 = kx2 + b
// y2 = kx2 + (y1 - kx1)
// y2 - y1 = kx2 - kx1
// y2 - y1 = k(x2 - x1)
// k = (y2 - y1) / (x2 - x1)
/**
 * @brief 画一条直线
 * 
 * @param x1 
 * @param y1 
 * @param x2 
 * @param y2 
 * @param fColor 
 */
void Inf_LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t w, uint16_t fColor)
{

    if(x1 == x2)
    {
        for (uint16_t i = y1; i <= y2 ; i+= w)
        {
            Inf_LCD_DrawPoint(x1,i,w,fColor);
        }
        return;
    }

    double k = 1.0 * (y2 - y1) / (x2 - x1);
    double b = y1 - k * x1;
    uint16_t y = 0;
    for (uint16_t x = x1; x <= x2; x++)
    {
        y = (k * x + b + 0.5);
        Inf_LCD_DrawPoint(x,y,w,fColor);
    }
    
}

#include <math.h>
/**
 * @brief 画一个圆
 * 
 * @param x 
 * @param y 
 * @param r 
 * @param fColor 
 */
void Inf_LCD_DrawCircle(uint16_t x1, uint16_t y1, uint16_t r, uint8_t w, uint16_t fColor)
{
    for (double i = 0; i < 2 * 3.14; i+= 0.01)
    {
        uint16_t x = r * sin(i) + x1;
        uint16_t y = r * cos(i) + y1;

        Inf_LCD_DrawPoint(x,y,w,fColor);
    }
    
}
