/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "gateway/jdy_message.h"
#include <stdio.h>
#include "Inf_LED.h"
#include "Inf_RGB.h"
#include "18B20.h"
#include "Inf_LCD.h"
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
// 微秒延时

// 读取温度

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#define NODE_RX_CHUNK_SIZE 128U
#define NODE_RX_CHUNK_COUNT 8U

static uint8_t usart3_data[NODE_RX_CHUNK_SIZE];
typedef struct
{
    uint8_t data[NODE_RX_CHUNK_SIZE];
    uint16_t len;
    uint32_t received_ms;
} node_rx_chunk_t;

// 接收中断是唯一生产者，主循环是唯一消费者；发布索引前先完成数据复制。
static node_rx_chunk_t node_rx_chunks[NODE_RX_CHUNK_COUNT];
static volatile uint8_t node_rx_head;
static volatile uint8_t node_rx_tail;
static volatile uint8_t node_rx_discard;
static volatile uint8_t node_rx_rearm_needed;
static volatile uint32_t node_rx_dropped;
static volatile uint32_t node_uart_errors;
static jdy_parser_t node_parser;
static uint8_t data_status = 5;
static volatile uint8_t data_count;

// 消息格式保持不变：节点0002向网关0101发送两字节温度消息体。
static uint8_t mesh_send[17] = {
    'A', 'T', '+', 'M', 'E', 'S', 'H', 0x00,
    0x01, 0x01, 0x00, 0x02, 0x02, 0x00, 0x00, 0x0D, 0x0A};
static uint16_t temp;

static void Node_DiscardRx(void)
{
    uint32_t irq_state = __get_PRIMASK();
    __disable_irq();
    node_rx_tail = node_rx_head;
    node_rx_discard = 0;
    __DMB();
    __set_PRIMASK(irq_state);
    parser_init(&node_parser);
}

// 配置应答可能留下溢出标志；先停止旧接收，再清理硬件状态并重新接收。
static int Node_StartReceive(void)
{
    if (HAL_UART_AbortReceive(&huart3) != HAL_OK)
    {
        return -1;
    }
    // STM32F1通过先读SR再读DR清除错误，同时丢弃旧的未读字节。
    __HAL_UART_CLEAR_OREFLAG(&huart3);
    return HAL_UARTEx_ReceiveToIdle_IT(&huart3, usart3_data,
                                     sizeof(usart3_data)) == HAL_OK ? 0 : -1;
}

static void Node_ProcessRx(void)
{
    unsigned int budget;
    static const uint8_t node_address[2] = {0x00, 0x02};

    if (node_rx_rearm_needed)
    {
        node_rx_rearm_needed = 0;
        if (Node_StartReceive() != 0)
        {
            node_rx_rearm_needed = 1;
        }
        node_rx_discard = 1;
    }
    if (node_rx_discard)
    {
        // 丢过接收块后不能把缺口两侧字节拼成指令，先丢弃积压并重新找帧头。
        Node_DiscardRx();
    }

    for (budget = 0; budget < NODE_RX_CHUNK_COUNT; ++budget)
    {
        node_rx_chunk_t chunk;
        jdy_frame_t frame;
        uint8_t tail = node_rx_tail;
        if (node_rx_discard)
        {
            Node_DiscardRx();
            break;
        }
        if (tail == node_rx_head)
        {
            break;
        }
        __DMB();
        chunk = node_rx_chunks[tail];
        __DMB();
        node_rx_tail = (uint8_t)((tail + 1U) % NODE_RX_CHUNK_COUNT);

        // 用接收时刻检查半帧期限，不能用延后的主循环处理时刻代替。
        parser_tick(&node_parser, chunk.received_ms);
        if (parser_feed(&node_parser, chunk.data, chunk.len) != JDY_PARSER_OK)
        {
            parser_init(&node_parser);
            node_rx_discard = 1;
            break;
        }
        while (parser_next(&node_parser, &frame) == JDY_PARSER_OK)
        {
            gateway_message_t message;
            if (jdy_frame_to_message(&frame, node_address, &message) != 0 ||
                message.id[0] != JDY_GATEWAY_ADDRESS_HIGH ||
                message.id[1] != JDY_GATEWAY_ADDRESS_LOW || message.msg_len != 1U)
            {
                continue;
            }
            if (message.msg[0] == '1')
            {
                data_status = 1;
            }
            else if (message.msg[0] == '0')
            {
                data_status = 2;
            }
        }
    }
    if (node_rx_tail == node_rx_head)
    {
        parser_tick(&node_parser, HAL_GetTick());
    }
}
/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{

    /* USER CODE BEGIN 1 */
    uint32_t rgb_changed_ms = 0;
    uint8_t rgb_step = 0;
    uint8_t temperature_pending = 0;
    uint32_t temperature_started_ms = 0;
/* USER CODE END 1 */

    /* MCU Configuration--------------------------------------------------------*/

    /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
    HAL_Init();

    /* USER CODE BEGIN Init */

    /* USER CODE END Init */

    /* Configure the system clock */
    SystemClock_Config();

    /* USER CODE BEGIN SysInit */

    /* USER CODE END SysInit */

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_USART3_UART_Init();
    MX_TIM1_Init();
    MX_ADC1_Init();
    MX_USART2_UART_Init();
    MX_TIM6_Init();
    MX_TIM2_Init();
    /* USER CODE BEGIN 2 */
    Inf_LCD_Init();
    Inf_LCD_SetScreenColor(WHITE);
    Inf_LCD_ShowString(150, 0, 32, "start", BLACK, WHITE);
    HAL_Delay(1000);
    HAL_UART_Transmit(&huart3, (const uint8_t *)"AT+NETID1111\r\n", 14, 1000);
    Inf_LCD_ShowString(0, 32, 32, "NETID:1111", BLACK, WHITE);
    HAL_Delay(1000);
    HAL_UART_Transmit(&huart3, (const uint8_t *)"AT+MADDR0002\r\n", 14, 1000);
    Inf_LCD_ShowString(0, 64, 32, "MADDR:0002", BLACK, WHITE);
    HAL_Delay(1000);
    HAL_UART_Transmit(&huart3, (const uint8_t *)"AT+RESET\r\n", 10, 1000);
    HAL_Delay(1000);
    HAL_TIM_Base_Start_IT(&htim6);
    Inf_Send_Res();
    parser_init(&node_parser);
    if (Node_StartReceive() != 0)
    {
        // 首次接收失败也进入主循环，由恢复分支重试，不能停住整个节点。
        node_uart_errors++;
        node_rx_discard = 1;
        node_rx_rearm_needed = 1;
    }
    Inf_LCD_ShowString(0, 96, 32, (uint8_t *)"RUN:2A-R1", BLACK, WHITE);
/* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    while(1)
    {
        Node_ProcessRx();
        if (data_status == 1 && (uint32_t)(HAL_GetTick() - rgb_changed_ms) >= 50U)
        {
            // 每次只推进一档灯效，期间继续处理串口，不等待50毫秒。
            if (rgb_step == 0) Inf_Send_Color(0, 255, 255);
            else if (rgb_step == 1) Inf_Send_Color(255, 0, 255);
            else Inf_Send_Color(255, 255, 0);
            rgb_step = (uint8_t)((rgb_step + 1U) % 3U);
            rgb_changed_ms = HAL_GetTick();
        }
        else if (data_status == 2)
        {
            Inf_Send_Color(0, 0, 0);
            Inf_Send_Res();
            data_status = 0;
            rgb_step = 0;
        }
/* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */
        if (!temperature_pending && data_count >= 2U)
        {
            uint32_t irq_state = __get_PRIMASK();
            __disable_irq();
            data_count = 0;
            __set_PRIMASK(irq_state);
            if (DS18B20_StartConversion())
            {
                temperature_started_ms = HAL_GetTick();
                temperature_pending = 1;
                Inf_LCD_ShowString(0, 128, 32, (uint8_t *)"TEMP:CONVERT ", BLACK, WHITE);
            }
            else
            {
                // 应答失败要能看见，避免误以为MQTT或JDY没有转发。
                Inf_LCD_ShowString(0, 128, 32, (uint8_t *)"TEMP:NO ECHO ", BLACK, WHITE);
            }
        }
        // 转换等待期间继续运行while(1)，不阻塞750毫秒。
        if (temperature_pending &&
            (uint32_t)(HAL_GetTick() - temperature_started_ms) >= 750U)
        {
            temp = DS18B20_ReadTemperature();
            mesh_send[13] = (uint8_t)(temp / 10U + '0');
            mesh_send[14] = (uint8_t)(temp % 10U + '0');
            if (HAL_UART_Transmit(&huart3, mesh_send, sizeof(mesh_send), 1000) == HAL_OK)
            {
                // 这里只确认本机串口发送完成，不代表网关已经收到。
                Inf_LCD_ShowString(0, 160, 32, (uint8_t *)"UART:TX OK  ", BLACK, WHITE);
            }
            else
            {
                Inf_LCD_ShowString(0, 160, 32, (uint8_t *)"UART:TX FAIL", BLACK, WHITE);
            }
            temperature_pending = 0;
        }
    }
/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    /** Initializes the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
     */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
        Error_Handler();
    }
}

/* USER CODE BEGIN 4 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART3)
    {
        uint8_t head = node_rx_head;
        uint8_t next = (uint8_t)((head + 1U) % NODE_RX_CHUNK_COUNT);
        if (node_rx_discard || Size > sizeof(usart3_data) || next == node_rx_tail)
        {
            node_rx_dropped++;
            node_rx_discard = 1;
        }
        else if (Size > 0U)
        {
            node_rx_chunks[head].len = Size;
            node_rx_chunks[head].received_ms = HAL_GetTick();
            memcpy(node_rx_chunks[head].data, usart3_data, Size);
            __DMB();
            node_rx_head = next;
        }
        // 中断只复制接收块并重新启动接收，不打印、不绘屏、不执行指令。
        if (HAL_UARTEx_ReceiveToIdle_IT(&huart3, usart3_data, sizeof(usart3_data)) != HAL_OK)
        {
            node_uart_errors++;
            node_rx_rearm_needed = 1;
        }
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3)
    {
        node_uart_errors++;
        node_rx_discard = 1;
        node_rx_rearm_needed = 1;
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6 && data_count < 2U)
    {
        data_count++;
    }
}
/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
    /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */
    __disable_irq();
    while (1)
    {
    }
    /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{
    /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line number,
       ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
