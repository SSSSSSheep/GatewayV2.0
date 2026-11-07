#ifndef __INF_RGB_H__
#define __INF_RGB_H__

#include "main.h"

typedef struct 
{
    uint8_t G_VAL;
    uint8_t R_VAL;
    uint8_t B_VAL;
} My_24bits_t;


void Inf_Send_Res(void);
void Inf_Send_Color(uint8_t r, uint8_t g, uint8_t b);
#endif
