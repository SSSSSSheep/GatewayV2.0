#include "18B20.h"

void Common_Delay_Us(uint32_t us) {
    uint32_t Delay = us * 72 / 9; /* 72M时钟频率 */
    do {
        __NOP(); /* 空指令（NOP）来占用 CPU 时间 */
    } while (Delay--);
}

// 复位DS18B20
void DS18B20_Reset(void)
{
    // 拉低总线至少480us
    HAL_GPIO_WritePin(B20_DATA_GPIO_Port, B20_DATA_Pin, GPIO_PIN_RESET);
    Common_Delay_Us(500);

    // 释放总线，等待DS18B20回应（15 - 60us内DS18B20会拉低总线）
    HAL_GPIO_WritePin(B20_DATA_GPIO_Port, B20_DATA_Pin, GPIO_PIN_SET);
    Common_Delay_Us(50);
}

// 等待DS18B20的回应
uint8_t DS18B20_CheckEcho(void)
{
    uint32_t timeout = 0;

    // 设置超时时间，防止一直等待
    timeout = HAL_GetTick();

    // 等待DS18B20拉低总线作为回应
    while (HAL_GPIO_ReadPin(B20_DATA_GPIO_Port, B20_DATA_Pin) == GPIO_PIN_SET)
    {
        if ((uint32_t)(HAL_GetTick() - timeout) >= 100U)
        {
            return 0;
        }
    }

    // 等待DS18B20释放总线（拉高）
    timeout = HAL_GetTick();
    while (HAL_GPIO_ReadPin(B20_DATA_GPIO_Port, B20_DATA_Pin) == GPIO_PIN_RESET)
    {
        if ((uint32_t)(HAL_GetTick() - timeout) >= 100U)
        {
            return 0;
        }
    }

    return 1;
}

// 向DS18B20写一位数据
void DS18B20_WriteBit(uint8_t bit)
{
    HAL_GPIO_WritePin(B20_DATA_GPIO_Port, B20_DATA_Pin, GPIO_PIN_RESET);
    Common_Delay_Us(10);
    if (bit)
    {
        HAL_GPIO_WritePin(B20_DATA_GPIO_Port, B20_DATA_Pin, GPIO_PIN_SET);
    }
    else
    {
        HAL_GPIO_WritePin(B20_DATA_GPIO_Port, B20_DATA_Pin, GPIO_PIN_RESET);
    }
    
    Common_Delay_Us(50);
    HAL_GPIO_WritePin(B20_DATA_GPIO_Port, B20_DATA_Pin, GPIO_PIN_SET);
}

// 从DS18B20读一位数据
uint8_t DS18B20_ReadBit(void)
{
    uint8_t bit = 0;

    // 拉低总线至少1us
    HAL_GPIO_WritePin(B20_DATA_GPIO_Port, B20_DATA_Pin, GPIO_PIN_RESET);
    Common_Delay_Us(2);
    HAL_GPIO_WritePin(B20_DATA_GPIO_Port, B20_DATA_Pin, GPIO_PIN_SET);
    // 延时等待数据稳定，然后读取总线电平
    Common_Delay_Us(10);
    if (HAL_GPIO_ReadPin(B20_DATA_GPIO_Port, B20_DATA_Pin) == GPIO_PIN_SET)
    {
        bit = 1;
    }
    Common_Delay_Us(50);
    return bit;
}

// 向DS18B20写一个字节数据
void DS18B20_WriteByte(uint8_t data)
{
    uint8_t i;

    for (i = 0; i < 8; i++)
    {
        DS18B20_WriteBit((data >> i) & 0x01);
    }
}

// 从DS18B20读一个字节数据
uint8_t DS18B20_ReadByte(void)
{
    uint8_t i, data = 0;

    for (i = 0; i < 8; i++)
    {
        data |= (DS18B20_ReadBit() << i);
    }

    return data;
}

// 启动DS18B20温度转换
uint8_t DS18B20_StartConversion(void)
{
    DS18B20_Reset();
    // 传感器未响应就返回，让主循环继续处理控制指令。
    if (!DS18B20_CheckEcho())
    {
        return 0;
    }

    DS18B20_WriteByte(0xCC); // 跳过ROM操作
    DS18B20_WriteByte(0x44); // 启动温度转换
    return 1;
}

// 读取DS18B20温度值
uint16_t DS18B20_ReadTemperature(void)
{
    uint8_t tempL, tempH;
    int16_t temp;
    float temperature;

    DS18B20_Reset();
    DS18B20_CheckEcho();
    DS18B20_WriteByte(0xCC); // 跳过ROM操作
    DS18B20_WriteByte(0xBE); // 读取温度寄存器

    tempL = DS18B20_ReadByte();
    tempH = DS18B20_ReadByte();

    temp = (tempH << 8) | tempL;

    temperature = (int)temp / 16;

    return temperature;
}
