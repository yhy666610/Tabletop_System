#include <string.h>

#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "stm32f4xx.h"
#include "usart.h"

static SemaphoreHandle_t write_async_semaphore;
static SemaphoreHandle_t write_busy_locker;
static QueueHandle_t     rx_queue;

void usart_io_init(void)
{
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_USART1);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_StructInit(&GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

void usart_serial_init(void)
{
    USART_InitTypeDef USART_InitStructure;
	USART_StructInit(&USART_InitStructure);
	USART_InitStructure.USART_BaudRate = 115200;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART1, &USART_InitStructure);
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
    USART_DMACmd(USART1, USART_DMAReq_Tx, ENABLE);
	USART_Cmd(USART1, ENABLE);
}

void usart_dma_init(void)
{
    DMA_InitTypeDef DMA_InitStruct;
    DMA_StructInit(&DMA_InitStruct);
    DMA_InitStruct.DMA_Channel = DMA_Channel_4;
    DMA_InitStruct.DMA_PeripheralBaseAddr = (uint32_t)&(USART1->DR);
    DMA_InitStruct.DMA_DIR = DMA_DIR_MemoryToPeripheral;
    DMA_InitStruct.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStruct.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStruct.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStruct.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStruct.DMA_Priority = DMA_Priority_Low;
    DMA_InitStruct.DMA_FIFOMode = DMA_FIFOMode_Enable;
    DMA_InitStruct.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;
    DMA_InitStruct.DMA_MemoryBurst = DMA_MemoryBurst_INC8;
    DMA_InitStruct.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
    DMA_ITConfig(DMA2_Stream7, DMA_IT_TC, ENABLE);
    DMA_Init(DMA2_Stream7, &DMA_InitStruct);
}

void usart_interrupt_init(void)
{
    NVIC_InitTypeDef NVIC_InitStructure;
    memset(&NVIC_InitStructure, 0, sizeof(NVIC_InitTypeDef));
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    NVIC_SetPriority(USART1_IRQn, 5);
    NVIC_InitStructure.NVIC_IRQChannel = DMA2_Stream7_IRQn;
    NVIC_Init(&NVIC_InitStructure);
    NVIC_SetPriority(DMA2_Stream7_IRQn, 5);
}

void usart_init(void)
{
    write_async_semaphore = xSemaphoreCreateBinary();
    configASSERT(write_async_semaphore);

    write_busy_locker = xSemaphoreCreateMutex();
    configASSERT(write_busy_locker);

    rx_queue = xQueueCreate(64, sizeof(char));
    configASSERT(rx_queue);

    usart_io_init();
    usart_serial_init();
    usart_dma_init();
    usart_interrupt_init();
}

void usart_write(const char str[], uint32_t size)
{
    xSemaphoreTake(write_busy_locker, portMAX_DELAY);

	uint32_t len = size;
    do
    {
        uint32_t chunk_size = (len < 65535) ? len : 65535;
        len -= chunk_size;

        DMA_Cmd(DMA2_Stream7, DISABLE);
        while (DMA_GetCmdStatus(DMA2_Stream7) != DISABLE);

        DMA_ClearFlag(DMA2_Stream7, DMA_FLAG_TCIF7 | DMA_FLAG_HTIF7 | DMA_FLAG_TEIF7 | DMA_FLAG_DMEIF7 | DMA_FLAG_FEIF7);

        DMA2_Stream7->NDTR = chunk_size;
        DMA2_Stream7->M0AR = (uint32_t)str;

        DMA_Cmd(DMA2_Stream7, ENABLE);

        xSemaphoreTake(write_async_semaphore, portMAX_DELAY);

        str += chunk_size;
    } while (len > 0);

    xSemaphoreGive(write_busy_locker);
}

void DMA2_Stream7_IRQHandler(void)
{
    if (DMA_GetITStatus(DMA2_Stream7, DMA_IT_TCIF7) != RESET)
    {
        DMA_ClearITPendingBit(DMA2_Stream7, DMA_IT_TCIF7);

        BaseType_t pxHigherPriorityTaskWoken;
        xSemaphoreGiveFromISR(write_async_semaphore, &pxHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
    }
}

void USART1_IRQHandler(void)
{
    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
    {
        char ch = (char)USART_ReceiveData(USART1);
        BaseType_t pxHigherPriorityTaskWoken = pdFALSE;
        /* If the queue is full the incoming byte is discarded.  This is
         * acceptable for interactive keyboard input because the shell
         * processes each character before the next keystroke arrives. */
        xQueueSendFromISR(rx_queue, &ch, &pxHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
    }
}

signed short usart_read(char *data, unsigned short len)
{
    unsigned short received = 0;
    while (received < len)
    {
        /* portMAX_DELAY: blocks forever until a byte is available;
         * xQueueReceive always returns pdTRUE in this case. */
        xQueueReceive(rx_queue, &data[received], portMAX_DELAY);
        received++;
    }
    return (signed short)received;
}

signed short usart_try_read(char *data)
{
    return (xQueueReceive(rx_queue, data, 0) == pdTRUE) ? 1 : 0;
}
