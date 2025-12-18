#include <string.h>

#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "stm32f4xx.h" // Device header
#include "usart.h"

static SemaphoreHandle_t write_async_semaphore;

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
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);// 接收中断使能
    USART_DMACmd(USART1, USART_DMAReq_Tx, ENABLE);
	USART_Cmd(USART1, ENABLE);
}

void usart_dma_init(void)
{
    DMA_InitTypeDef DMA_InitStruct;
    DMA_StructInit(&DMA_InitStruct);
    DMA_InitStruct.DMA_Channel = DMA_Channel_4; // 查参考手册得知USART1_TX对应DMA2的通道4
    DMA_InitStruct.DMA_PeripheralBaseAddr = (uint32_t)&(USART1->DR); // 外设地址
    DMA_InitStruct.DMA_DIR = DMA_DIR_MemoryToPeripheral; // 内存到外设
    DMA_InitStruct.DMA_PeripheralInc = DMA_PeripheralInc_Disable; // 外设地址不变
    DMA_InitStruct.DMA_MemoryInc = DMA_MemoryInc_Enable; // 内存地址递增
    DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte; // 外设数据大小:字节
    DMA_InitStruct.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte; // 内存数据大小:字节
    DMA_InitStruct.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStruct.DMA_Priority = DMA_Priority_Low;
    DMA_InitStruct.DMA_FIFOMode = DMA_FIFOMode_Enable; // FIFO模式启用
    DMA_InitStruct.DMA_FIFOThreshold = DMA_FIFOThreshold_Full; // FIFO满
    DMA_InitStruct.DMA_MemoryBurst = DMA_MemoryBurst_INC8; // 内存突发传输:8个数据单元
    DMA_InitStruct.DMA_PeripheralBurst = DMA_PeripheralBurst_Single; // 外设突发传输:单次
    DMA_ITConfig(DMA2_Stream7, DMA_IT_TC, ENABLE); // 传输完成中断使能
    DMA_Init(DMA2_Stream7, &DMA_InitStruct); // USART1_TX对应DMA2的Stream7
}

void usart_interrupt_init(void)
{
    // USART RX中断设置
    NVIC_InitTypeDef NVIC_InitStructure;
    memset(&NVIC_InitStructure, 0, sizeof(NVIC_InitTypeDef));
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    NVIC_SetPriority(USART1_IRQn, 5);
    // DMA TC中断设置
    NVIC_InitStructure.NVIC_IRQChannel = DMA2_Stream7_IRQn;
    NVIC_Init(&NVIC_InitStructure);
    NVIC_SetPriority(DMA2_Stream7_IRQn, 5);
}

void usart_init(void)
{
    write_async_semaphore = xSemaphoreCreateBinary();
    configASSERT(write_async_semaphore);

    usart_io_init();
    usart_serial_init();
    usart_dma_init();
    usart_interrupt_init();
}

void usart_write(USART_TypeDef *USARTx, char str[])
{
	uint8_t len = strlen(str);

    do
    {
        uint32_t chunk_size = (len < 65535) ? len : 65535; // 因为DMA的缓冲区大小限制，每次传输不超过65535个数据单元
        len -= chunk_size; // 更新剩余数据单元数量

        DMA2_Stream7->NDTR = chunk_size; // 设置传输的数据单元数量
        DMA2_Stream7->M0AR = (uint32_t)str; // 设置内存地址

        DMA_Cmd(DMA2_Stream7, ENABLE); // 启动DMA传输

        xSemaphoreTake(write_async_semaphore, portMAX_DELAY); // 等待传输完成信号
        // while (DMA_GetFlagStatus(DMA2_Stream7, DMA_FLAG_TCIF7) == RESET);   //等待DMA传输完成
        // DMA_ClearFlag(DMA2_Stream7, DMA_FLAG_TCIF7); //清除传输完成标志

        str += chunk_size; //每个数据单元1字节,执行这句的目的是更新data指针，指向下一个待传输的数据位置
    } while (len > 0);

    USART_ClearFlag(USARTx, USART_FLAG_TC);
    while (USART_GetFlagStatus(USARTx, USART_FLAG_TC) == RESET);
}

void DMA2_Stream7_IRQHandler(void)
{
    if (DMA_GetITStatus(DMA2_Stream7, DMA_IT_TCIF7) != RESET)
    {
        BaseType_t pxHigherPriorityTaskWoken;
        xSemaphoreGiveFromISR(write_async_semaphore, &pxHigherPriorityTaskWoken);   // 释放信号量，通知写入完成
        portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);  // 如果需要切换任务，则进行切换

        DMA_ClearITPendingBit(DMA2_Stream7, DMA_IT_TCIF7);
    }
}
