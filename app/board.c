#include <stdint.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "stm32f4xx.h"
#include "usart.h"
#include "rtc.h"
#include "st7789.h"
#include "aht20.h"
#include "tim_delay.h"

void board_lowlevel_init(void)
{
	SCB->VTOR = 0x8010000;
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);   //备用，实际没用到
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM6, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C2, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);  // 备用
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);     // 使能电源接口时钟
    PWR_BackupAccessCmd(ENABLE);                            // 允许访问备份寄存器
    RCC_LSEConfig(RCC_LSE_ON);                              // 打开外部低速晶振
    while (RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET);    // 等待外部低速晶振就绪
    RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);                 // 选择LSE作为RTC时钟源
}

void board_init(void)
{
    tim_delay_init();
	usart_init();
    printf("[SYS] Build Date: %s %s\r\n", __DATE__, __TIME__);

    rtc_init();
    aht20_init();
}

int fputc(int ch, FILE *f)
{
	USART_ClearFlag(USART1, USART_FLAG_TC);
	USART_SendData(USART1, (uint8_t)ch);
	while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
	return ch;
}

void vAssertCalled(const char *file, int line)
{
    portDISABLE_INTERRUPTS();
	printf("Assert Called : %s(%d)\n", file, line);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName )
{
	printf("Stack Overflow : %s\n", pcTaskName);
	configASSERT(0);
}

void vApplicationMallocFailedHook(void)
{
	printf("Malloc Failed\n");
	configASSERT(0);
}
