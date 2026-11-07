#include "Dir_FSMC_LCD.h"




void Dri_FSMC_LCD_GPIO_Init(void)
{
    /* 1 配置 A10 = G0 地址端口的输出模式 复用推挽输出CNF:10 50MHz速度 MODE:11*/
    /* =============MODE=============== */

    GPIOG->CRL |= (GPIO_CRL_MODE0 | GPIO_CRL_CNF0_1);
    GPIOG->CRL &= ~(GPIO_CRL_CNF0_0);

    /*
        2 数据端口 复用推挽输出
            在实际应用中，即使数据线被配置为输出模式，FSMC控制器仍然能够管理数据线的方向，使其在需要时成为输入线。
            这种自动切换是由FSMC控制器硬件管理的，不需要软件干预。
            因此，即使GPIO配置为复用推挽输出，FSMC依然可以实现读取操作。
    */
    /* =============MODE=============== */
    GPIOD->CRL |= (GPIO_CRL_MODE0 |
                   GPIO_CRL_MODE1);
    GPIOD->CRH |= (GPIO_CRH_MODE8 |
                   GPIO_CRH_MODE9 |
                   GPIO_CRH_MODE10 |
                   GPIO_CRH_MODE14 |
                   GPIO_CRH_MODE15);

    GPIOE->CRL |= (GPIO_CRL_MODE7);
    GPIOE->CRH |= (GPIO_CRH_MODE8 |
                   GPIO_CRH_MODE9 |
                   GPIO_CRH_MODE10 |
                   GPIO_CRH_MODE11 |
                   GPIO_CRH_MODE12 |
                   GPIO_CRH_MODE13 |
                   GPIO_CRH_MODE14 |
                   GPIO_CRH_MODE15);

    /* =============CNF=============== */
    GPIOD->CRL |= (GPIO_CRL_CNF0_1 |
                   GPIO_CRL_CNF1_1);
    GPIOD->CRL &= ~(GPIO_CRL_CNF0_0 |
                    GPIO_CRL_CNF1_0);

    GPIOD->CRH |= (GPIO_CRH_CNF8_1 |
                   GPIO_CRH_CNF9_1 |
                   GPIO_CRH_CNF10_1 |
                   GPIO_CRH_CNF14_1 |
                   GPIO_CRH_CNF15_1);
    GPIOD->CRH &= ~(GPIO_CRH_CNF8_0 |
                    GPIO_CRH_CNF9_0 |
                    GPIO_CRH_CNF10_0 |
                    GPIO_CRH_CNF14_0 |
                    GPIO_CRH_CNF15_0);

    GPIOE->CRL |= (GPIO_CRL_CNF7_1);
    GPIOE->CRL &= ~(GPIO_CRL_CNF7_0);

    GPIOE->CRH |= (GPIO_CRH_CNF8_1 |
                   GPIO_CRH_CNF9_1 |
                   GPIO_CRH_CNF10_1 |
                   GPIO_CRH_CNF11_1 |
                   GPIO_CRH_CNF12_1 |
                   GPIO_CRH_CNF13_1 |
                   GPIO_CRH_CNF14_1 |
                   GPIO_CRH_CNF15_1);
    GPIOE->CRH &= ~(GPIO_CRH_CNF8_0 |
                    GPIO_CRH_CNF9_0 |
                    GPIO_CRH_CNF10_0 |
                    GPIO_CRH_CNF11_0 |
                    GPIO_CRH_CNF12_0 |
                    GPIO_CRH_CNF13_0 |
                    GPIO_CRH_CNF14_0 |
                    GPIO_CRH_CNF15_0);

    /* 3 其他控制端口  复用推挽输出  OE和WE*/
    GPIOD->CRL |= (GPIO_CRL_MODE4 |
                   GPIO_CRL_MODE5);
    GPIOD->CRL |= (GPIO_CRL_CNF4_1 |
                   GPIO_CRL_CNF5_1);
    GPIOD->CRL &= ~(GPIO_CRL_CNF4_0 |
                    GPIO_CRL_CNF5_0);


    //配置为4扇区 G12
    GPIOG->CRH |= (GPIO_CRH_MODE12);
    GPIOG->CRH |= (GPIO_CRH_CNF12_1);
    GPIOG->CRH &= ~(GPIO_CRH_CNF12_0);


    //PG15为RST 设置为通用推挽输出 PB0为BG背光 , mdoe = 11, cnf = 00
    GPIOG->CRH |= GPIO_CRH_MODE15;
    GPIOG->CRH &= ~GPIO_CRH_CNF15;

    GPIOB->CRL |= GPIO_CRL_MODE0;
    GPIOB->CRL &= ~GPIO_CRL_CNF0;


}

void Dir_FSMC_LCD_Init()
{
    RCC->APB2ENR |= (RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPDEN | RCC_APB2ENR_IOPEEN | RCC_APB2ENR_IOPGEN | RCC_APB2ENR_AFIOEN);
    RCC->AHBENR |= RCC_AHBENR_FSMCEN;
    
    Dri_FSMC_LCD_GPIO_Init();

    //对FSMC的BCR4进行初始化配置
    //设置写使能
    FSMC_Bank1->BTCR[6] |= FSMC_BCR3_WREN; 

    //配置数据为16位，高八位，和低八位
    FSMC_Bank1->BTCR[6] &= ~FSMC_BCR3_MWID_1;
    FSMC_Bank1->BTCR[6] |= FSMC_BCR3_MWID_0;
    //设置存储器类型为SRAM
    FSMC_Bank1->BTCR[6] &= ~FSMC_BCR3_MTYP;
    //关闭数据\地址线复用
    FSMC_Bank1->BTCR[6] &= ~FSMC_BCR3_MUXEN;
    //设置块使能
    FSMC_Bank1->BTCR[6] |= FSMC_BCR3_MBKEN;
    //禁止NOR对存储器进行操作
    FSMC_Bank1->BTCR[6] &= ~FSMC_BCR3_FACCEN;

    //对FSMC的BTR3进行初始化配置
    //设置总线恢复时间
    FSMC_Bank1->BTCR[7] &= ~FSMC_BTR3_BUSTURN;
   
    //设置数据的保持时间为72个时钟
    FSMC_Bank1->BTCR[7] &= ~FSMC_BTR3_DATAST;
    FSMC_Bank1->BTCR[7] |= (71 << 8);
    //地址保持时间
    FSMC_Bank1->BTCR[7] &= ~FSMC_BTR3_ADDHLD;
    //地址建立时间
    FSMC_Bank1->BTCR[7] &= ~FSMC_BTR3_ADDSET;
}
