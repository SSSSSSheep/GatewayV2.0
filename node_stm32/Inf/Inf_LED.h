#ifndef __INF_LED_H__
#define __INF_LED_H__

#include "main.h"
#include "gpio.h"
#define LED uint16_t

extern LED leds[];

void LED_On(LED led);
void LED_Off(LED led);
void LED_Toggle(LED led);
void LED_OnAll(LED *leds, uint8_t size);
void LED_OffAll(LED *leds, uint8_t size);

void Inf_LED_Light(uint8_t index);

#endif
