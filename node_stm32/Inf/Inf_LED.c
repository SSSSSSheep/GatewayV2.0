#include "Inf_LED.h"

LED leds[] = {LED1_Pin, LED2_Pin, LED3_Pin};

void LED_On(LED led)
{
    HAL_GPIO_WritePin(GPIOA, led, GPIO_PIN_RESET);
}
void LED_Off(LED led)
{
    HAL_GPIO_WritePin(GPIOA, led, GPIO_PIN_SET);
}

void LED_Toggle(LED led)
{
    HAL_GPIO_TogglePin(GPIOA, led);
}


void LED_OnAll(LED *leds, uint8_t size)
{
    for (uint8_t i = 0; i < size; i++)
    {
        LED_On(leds[i]);
    }
    
}
void LED_OffAll(LED *leds, uint8_t size)
{
    for (uint8_t i = 0; i < size; i++)
    {
        LED_Off(leds[i]);
    }
    
}

void Inf_LED_Light(uint8_t index)
{
    if(index <= 3 && index >= 1)
    {
        HAL_GPIO_WritePin(GPIOA, leds[index - 1], GPIO_PIN_RESET);

    }
    else if(index == 0)
    {
        LED_OffAll(leds,3);

    }
    
}
