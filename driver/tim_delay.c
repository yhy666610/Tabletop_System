#include "stm32f4xx.h"
#include "tim_delay.h"

static volatile uint64_t tim_tick_count;
static tim_periodic_callback_t periodic_callback;

void tim_delay_init(void)
{
    RCC_ClocksTypeDef RCC_ClocksStruct;
    RCC_GetClocksFreq(&RCC_ClocksStruct);   // 获取时钟频率信息
    uint32_t apb1_tim_freq_mhz = RCC_ClocksStruct.PCLK1_Frequency / 1000 / 1000 * 2; // APB1定时器时钟频率，单位MHz

    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
    TIM_TimeBaseStructure.TIM_Prescaler = apb1_tim_freq_mhz - 1;
    TIM_TimeBaseStructure.TIM_Period = 999;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM6, &TIM_TimeBaseStructure);
    TIM_ITConfig(TIM6, TIM_IT_Update, ENABLE);
    TIM_Cmd(TIM6, ENABLE);

    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = TIM6_DAC_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

uint64_t tim_now(void)
{
    /* 获取当前的滴答数 */
    uint64_t now, last_count;
    do {
        last_count = tim_tick_count;
        now = tim_tick_count + TIM_GetCounter(TIM6);
    } while (last_count != tim_tick_count);
    return now;
}
// 获取当前时间，单位为微秒
uint64_t tim_get_us(void)
{
    return tim_now();
}
// 获取当前时间，单位为毫秒
uint64_t tim_get_ms(void)
{
    return tim_now() / 1000;
}
// 延时函数，单位为微秒
void tim_delay_us(uint32_t us)
{
    uint64_t now = tim_now();
    while (tim_now() - now < (uint64_t)us);
}

void tim_delay_ms(uint32_t ms)
{
    uint64_t now = tim_now();
    while (tim_now() - now < (uint64_t)ms * 1000);
}

void tim_register_periodic_callback(tim_periodic_callback_t callback)
{
    periodic_callback = callback;
}

void TIM6_DAC_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM6, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(TIM6, TIM_IT_Update);
        tim_tick_count += 1000; // 每次中断增加1000微秒

        if (periodic_callback)
        {
            periodic_callback();
        }
    }
}

// void cpu_delay(uint32_t us)
// {
//    while (us >= 1000)
//    {
//        SysTick->LOAD = SystemCoreClock / 1000;
//        SysTick->VAL = 0;
//        SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
//        while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0);
//        SysTick->CTRL = ~SysTick_CTRL_ENABLE_Msk;
//        us -= 1000;
//    }

//    if (us > 0)
//    {
//        SysTick->LOAD = us * SystemCoreClock / 1000 / 1000;
//        SysTick->VAL = 0;
//        SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
//        while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0);
//        SysTick->CTRL = ~SysTick_CTRL_ENABLE_Msk;
//    }
// }

// #include "stm32f10x.h"

// /**
//   * @brief  微秒级延时
//   * @param  xus 延时时长，范围：0~233015
//   * @retval 无
//   */
// void Delay_us(uint32_t xus)
// {
// 	SysTick->LOAD = 72 * xus;				//设置定时器重装值
// 	SysTick->VAL = 0x00;					//清空当前计数值
// 	SysTick->CTRL = 0x00000005;				//设置时钟源为HCLK，启动定时器
// 	while(!(SysTick->CTRL & 0x00010000));	//等待计数到0
// 	SysTick->CTRL = 0x00000004;				//关闭定时器
// }

// /**
//   * @brief  毫秒级延时
//   * @param  xms 延时时长，范围：0~4294967295
//   * @retval 无
//   */
// void Delay_ms(uint32_t xms)
// {
// 	while(xms--)
// 	{
// 		Delay_us(1000);
// 	}
// }

// /**
//   * @brief  秒级延时
//   * @param  xs 延时时长，范围：0~4294967295
//   * @retval 无
//   */
// void Delay_s(uint32_t xs)
// {
// 	while(xs--)
// 	{
// 		Delay_ms(1000);
// 	}
// }
