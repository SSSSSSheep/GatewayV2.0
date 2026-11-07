#ifndef __DIR_FSMC_LCD_H__
#define __DIR_FSMC_LCD_H__
#include "stm32f1xx_hal.h"


#define RST_HIGH GPIOG->ODR |= GPIO_ODR_ODR15
#define RST_LOW  GPIOG->ODR &= ~GPIO_ODR_ODR15
#define BG_HIGH  GPIOB->ODR |= GPIO_ODR_ODR0
#define BG_LOW   GPIOB->ODR &= ~GPIO_ODR_ODR0

#define  FSMC_BCR3_WREN                      ((uint32_t)0x00001000)        /*!< Write enable bit */
#define  FSMC_BCR3_MWID_1                    ((uint32_t)0x00000020)        /*!< Bit 1 */
#define  FSMC_BCR3_MWID_0                    ((uint32_t)0x00000010)        /*!< Bit 0 */
#define  FSMC_BCR3_MTYP                      ((uint32_t)0x0000000C)        /*!< MTYP[1:0] bits (Memory type) */
#define  FSMC_BCR3_MUXEN                     ((uint32_t)0x00000002)        /*!< Address/data multiplexing enable bit */
#define  FSMC_BCR3_MBKEN                     ((uint32_t)0x00000001)        /*!< Memory bank enable bit */
#define  FSMC_BCR3_FACCEN                    ((uint32_t)0x00000040)        /*!< Flash access enable */
#define  FSMC_BTR3_BUSTURN                   ((uint32_t)0x000F0000)        /*!< BUSTURN[3:0] bits (Bus turnaround phase duration) */
#define  FSMC_BTR3_DATAST                    ((uint32_t)0x0000FF00)        /*!< DATAST [3:0] bits (Data-phase duration) */
#define  FSMC_BTR3_ADDHLD                    ((uint32_t)0x000000F0)        /*!< ADDHLD[3:0] bits (Address-hold phase duration) */
#define  FSMC_BTR3_ADDSET                    ((uint32_t)0x0000000F)        /*!< ADDSET[3:0] bits (Address setup phase duration) */
void Dir_FSMC_LCD_Init(void);

#endif

