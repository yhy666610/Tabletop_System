#include "stm32f4xx.h" // Device header
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "esp_at.h"

#define ESP_AT_DEBUG 1
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

static bool esp_at_write_cmd(const char *cmd, uint32_t timeout);
static bool esp_at_wait_boot(uint32_t timeout);
static bool esp_at_wait_ready(uint32_t timeout);

typedef enum
{
	AT_ACK_NONE,
	AT_ACK_OK,
	AT_ACK_ERROR,
	AT_ACK_BUSY,
	AT_ACK_READY,
} at_ack_t;

typedef struct
{
	at_ack_t ack;
	const char *string;
} at_ack_match_t;

static const at_ack_match_t at_ack_matches[] =
{
	{AT_ACK_OK, "OK\r\n"},
	{AT_ACK_ERROR, "ERROR\r\n"},
	{AT_ACK_BUSY, "busy p...\r\n"},
	{AT_ACK_READY, "ready\r\n"},
	{AT_ACK_NONE, NULL},
};

static char rxbuf[1024];
static uint32_t rxlen;
static const char *rxline;// 指向当前处理的行
static at_ack_t rxack;
static SemaphoreHandle_t at_ack_semaphore;

static void esp_at_io_init(void)
{
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_USART2);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_USART2);

	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_StructInit(&GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
}

static void esp_at_usart_init(void)
{
    USART_InitTypeDef USART_InitStructure;
	USART_StructInit(&USART_InitStructure);
	USART_InitStructure.USART_BaudRate = 115200;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART2, &USART_InitStructure);
    USART_DMACmd(USART2, USART_DMAReq_Tx, ENABLE);  // 使能DMA发送
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);  // 使能接收中断
	USART_Cmd(USART2, ENABLE);
}

static void esp_at_dma_init(void)
{
    DMA_InitTypeDef DMA_InitStruct;
    DMA_StructInit(&DMA_InitStruct);
    DMA_InitStruct.DMA_Channel = DMA_Channel_4; // 查参考手册得知USART2_TX对应DMA1的通道4
    DMA_InitStruct.DMA_PeripheralBaseAddr = (uint32_t)&(USART2->DR); // 外设地址
    DMA_InitStruct.DMA_DIR = DMA_DIR_MemoryToPeripheral; // 内存到外设
    DMA_InitStruct.DMA_PeripheralInc = DMA_PeripheralInc_Disable; // 外设地址不变
    DMA_InitStruct.DMA_MemoryInc = DMA_MemoryInc_Enable; // 内存地址递增
    DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte; // 外设数据大小:字节
    DMA_InitStruct.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte; // 内存数据大小:字节
    DMA_InitStruct.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStruct.DMA_Priority = DMA_Priority_Medium;
    DMA_InitStruct.DMA_FIFOMode = DMA_FIFOMode_Enable; // FIFO模式启用
    DMA_InitStruct.DMA_FIFOThreshold = DMA_FIFOThreshold_Full; // FIFO满
    DMA_InitStruct.DMA_MemoryBurst = DMA_MemoryBurst_INC8; // 内存突发传输:8个数据单元
    DMA_InitStruct.DMA_PeripheralBurst = DMA_PeripheralBurst_Single; // 外设突发传输:单次
    DMA_Init(DMA1_Stream6, &DMA_InitStruct); // USART2_TX对应DMA1的Stream6
}

static void esp_at_interrupt_init(void)
{
    // USART RX中断设置
    NVIC_InitTypeDef NVIC_InitStructure;
    memset(&NVIC_InitStructure, 0, sizeof(NVIC_InitTypeDef));
    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    NVIC_SetPriority(USART2_IRQn, 5);
}

static void esp_at_lowlevel_init(void)
{
    esp_at_usart_init();
    esp_at_dma_init();
    esp_at_interrupt_init();
    esp_at_io_init();
}

bool esp_at_init(void)
{
    at_ack_semaphore = xSemaphoreCreateBinary();
    configASSERT(at_ack_semaphore);

	esp_at_lowlevel_init();

    if (!esp_at_wait_boot(3000))
        return false;
	if (!esp_at_write_cmd("AT+RESTORE\r\n", 2000))
		return false;
	if (!esp_at_wait_ready(5000))
		return false;

	return true;
}

static void esp_at_usart_write(const char *data)
{
    uint32_t len = strlen(data);

    DMA1_Stream6->NDTR = len; // 设置传输的数据单元数量
    DMA1_Stream6->M0AR = (uint32_t)data; // 设置内存地址
    DMA_ClearFlag(DMA1_Stream6, DMA_FLAG_TCIF6); // 清除传输完成标志
    DMA_Cmd(DMA1_Stream6, ENABLE); // 启动DMA传输

	// while (data && *data) // 发送字符串直到结束符
    // {
    //     while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
	//     USART_SendData(USART2, *data++);
    // }

	// while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
	// USART_SendData(USART2, '\r');
	// while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
	// USART_SendData(USART2, '\n');
}

static at_ack_t match_internal_ack(const char *str)
{
	for (uint32_t i = 0; i < ARRAY_SIZE(at_ack_matches); i++)
	{
		if (at_ack_matches[i].string == NULL)
			break;
		if (strcmp(str, at_ack_matches[i].string) == 0)
			return at_ack_matches[i].ack;
	}
	return AT_ACK_NONE;
}
/* 等待接收指定长度的数据，直到匹配到已知的应答或者超时 */
static at_ack_t esp_at_usart_wait_receive(uint32_t timeout)
{
	rxlen = 0;
    rxline = rxbuf;

    bool ack = xSemaphoreTake(at_ack_semaphore, pdMS_TO_TICKS(timeout)) == pdPASS;  // 等待应答信号
    return ack ? rxack : AT_ACK_NONE;
	// const char *line = rxbuf;
	// uint32_t start = xTaskGetTickCount();
	// rxbuf[0] = '\0';

	// while (rxlen < sizeof(rxbuf) - 1)
	// {
	// 	while (USART_GetFlagStatus(USART2, USART_FLAG_RXNE) == RESET)
	// 	{
	// 		if (xTaskGetTickCount() - start >= timeout)
	// 			return AT_ACK_NONE;
	// 	}
	// 	rxbuf[rxlen++] = USART_ReceiveData(USART2);
	// 	rxbuf[rxlen] = '\0';
	// 	if (rxbuf[rxlen - 1] == '\n')
	// 	{
	// 		at_ack_t ack = match_internal_ack(line);
	// 		if (ack != AT_ACK_NONE)
	// 			return ack;
	// 		line = rxbuf + rxlen;
	// 	}
	// }
	// return AT_ACK_NONE;
}

static bool esp_at_wait_ready(uint32_t timeout)
{
	return esp_at_usart_wait_receive(timeout) == AT_ACK_READY; // 等待ready应答
}
/* 发送AT命令并等待应答，返回是否成功 */
static bool esp_at_write_cmd(const char *cmd, uint32_t timeout)
{
#if ESP_AT_DEBUG
    printf("[DEBUG] Send: %s\n", cmd);
#endif

	esp_at_usart_write(cmd);
	at_ack_t ack = esp_at_usart_wait_receive(timeout);

#if ESP_AT_DEBUG
	printf("[DEBUG] Response: \n%s\n", rxbuf);
#endif

	if (ack == AT_ACK_OK)
		return true;

	return false;
}

static const char *esp_at_get_response(void)
{
	return rxbuf;
}

static bool esp_at_wait_boot(uint32_t timeout)
{
    for (int t = 0; t < timeout; t += 100)
    {
        if (esp_at_write_cmd("AT\r\n", 100))
            return true;
    }
    return false;
}

bool esp_at_wifi_init(void)
{
	return esp_at_write_cmd("AT+CWMODE=1\r\n", 2000); // 设置为STATION模式
}

bool esp_at_connect_wifi(const char *ssid, const char *pwd, const char *mac)
{
	if (ssid == NULL || pwd == NULL)
		return false;

	char *cmd = rxbuf;
	int len = snprintf(cmd, sizeof(rxbuf), "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, pwd); //这里sizeof(cmd)是错误的，应该是sizeof(rxbuf),原因：cmd是指针，指针大小固定，一般为4或8字节
	if (mac)
		snprintf(cmd + len, sizeof(rxbuf) - len, ",\"%s\"", mac);

	return esp_at_write_cmd(cmd, 5000);
}

static bool parse_cwstate_response(const char* resp, esp_wifi_info_t *info)
{
	// AT+CWSTATE?
	// +CWSTATE:2,"iQOO 11"

	// OK
	resp = strstr(resp, "+CWSTATE:"); // Find the start of the CWSTATE response
	if (resp == NULL)
        return false;

	int wifi_state = 0;
    if (sscanf(resp, "+CWSTATE:%d,\"%63[^\"]", &wifi_state, info->ssid) != 2) // Parse CWSTATE response
		return false;

	info->connected = (wifi_state == 2); // 2 means connected
	return true;
}

static bool parse_cwjap_response(const char* resp, esp_wifi_info_t* info)
{
	// AT+CWJAP?
	// +CWJAP:"iQOO 11","26:eb:0b:a9:ef:5a",11,-66,0,1,3,0,1

	// OK
	resp = strstr(resp, "+CWJAP:"); // Find the start of the CWJAP response
    if (resp == NULL)
        return false;

    if (sscanf(resp, "+CWJAP:\"%63[^\"]\",\"%17[^\"]\",%d,%d", info->ssid, info->bssid, &info->channel, &info->rssi) != 4)
        return false;

    return true;
}

bool esp_at_get_wifi_info(esp_wifi_info_t *info)
{
	if (info == NULL)
		return false;

	if (!esp_at_write_cmd("AT+CWSTATE?\r\n", 2000))// 获取WiFi连接状态
		return false;

	if (!parse_cwstate_response(esp_at_get_response(), info)) // 解析连接状态
		return false;

	if (info->connected == true)
	{
		if (!esp_at_write_cmd("AT+CWJAP?\r\n", 2000))// 获取已连接的WiFi信息
			return false;

		if (!parse_cwjap_response(esp_at_get_response(), info)) // 解析WiFi信息
			return false;
	}

	return true;
}

bool wifi_connected(void)
{
	esp_wifi_info_t info;
	// 解析info结构体中的WiFi信息
	if (esp_at_get_wifi_info(&info))
		return info.connected;

	return false;
}

bool esp_at_init_sntp(void)
{
    return esp_at_write_cmd("AT+CIPSNTPCFG=1,8\r\n", 2000);
}

static uint8_t month_str_to_num(const char* month_str)
{
	const char* months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	for (uint8_t i = 0; i < 12; i++)
	{
		if (strcmp(month_str, months[i]) == 0)
		{
			return i + 1; // Months are 1-12
		}
	}
	return 0; // Invalid month
}

static uint8_t weekday_str_to_num(const char* weekday_str)
{
    const char* weekdays[] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
	for (uint8_t i = 0; i < 7; i++)
	{
		if (strcmp(weekday_str, weekdays[i]) == 0)
		{
			return i + 1; // Weekdays are 1-7
		}
	}
	return 0; // Invalid weekday
}

static bool parse_cipsntptime_response(const char* response, esp_time_t *date)
{
    //AT+CIPSNTPTIME?
    //+CIPSNTPTIME:Tue Nov 11 18:04:27 2025
    //OK
	char weekday_str[8] = { 0 };
	char month_str[4] = { 0 };
    response = strstr(response, "+CIPSNTPTIME:");
	if (response == NULL)
		return false;
    if (sscanf(response, "+CIPSNTPTIME:%3s %3s %hhu %hhu:%hhu:%hhu %hu",
                    weekday_str,
                    month_str,
                    &date->day,
                    &date->hour,
                    &date->minute,
                    &date->second,
                    &date->year) != 7)
       {
           return false;
       }

	date->weekday = weekday_str_to_num(weekday_str);
	date->month = month_str_to_num(month_str);

	return true;
}

bool esp_at_get_time(esp_time_t *time)
{
    if (time == NULL)
        return false;

    if (!esp_at_write_cmd("AT+CIPSNTPTIME?\r\n", 2000))
        return false;

    if (!parse_cipsntptime_response(esp_at_get_response(), time))
        return false;

    return true;
}
const char *esp_at_http_get(const char *url)
{
	if (url == NULL)
		return NULL;

	char *txbuf = rxbuf;
	snprintf(txbuf, sizeof(rxbuf), "AT+HTTPCLIENT=2,1,\"%s\",,,2\r\n", url);
	bool ret = esp_at_write_cmd(txbuf, 5000);
	return ret ? esp_at_get_response() : NULL;
}

void USART2_IRQHandler(void)
{
    if (USART_GetITStatus(USART2, USART_IT_RXNE) == SET)
    {

        if (rxlen < sizeof(rxbuf) - 1)
        {
            rxbuf[rxlen++] = USART_ReceiveData(USART2);

            if (rxbuf[rxlen - 1] == '\n')
            {
                rxbuf[rxlen] = '\0';
                at_ack_t ack = match_internal_ack(rxline);

                if (ack != AT_ACK_NONE)
                {
                    rxack = ack;
                    BaseType_t pxHigherPriorityTaskWoken;
                    xSemaphoreGiveFromISR(at_ack_semaphore, &pxHigherPriorityTaskWoken);
                    portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
                }
                rxline = rxbuf + rxlen;
            }
        }

        USART_ClearITPendingBit(USART2, USART_IT_RXNE);
    }
}
