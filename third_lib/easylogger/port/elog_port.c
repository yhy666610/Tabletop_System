#include <stdio.h>
#include "elog.h"
#include "usart.h"
#include "rtc.h"
#include "FreeRTOS.h"
#include "semphr.h"

static SemaphoreHandle_t elog_mutex;
ElogErrCode elog_port_init(void)
{
    elog_mutex = xSemaphoreCreateMutex();
    return ELOG_NO_ERR;
}

void elog_port_deinit(void)
{
    vSemaphoreDelete(elog_mutex);
    elog_mutex = NULL;
}

void elog_port_output(const char *log, size_t size)
{
    usart_write(log, size);
}

void elog_port_output_lock(void)
{
    if (elog_mutex != NULL)
    xSemaphoreTake(elog_mutex, portMAX_DELAY);
}

void elog_port_output_unlock(void)
{
    if (elog_mutex != NULL)
    xSemaphoreGive(elog_mutex);
}

const char *elog_port_get_time(void)
{
    static char time_str[32];
    rtc_time_t time;
    rtc_get_time(&time);
    if (time.year >= 2000 && time.year <= 2099)
    {
        snprintf(time_str, sizeof(time_str), "%02d-%02d-%02d %02d:%02d:%02d",
                 time.year % 100, time.month, time.day,
                 time.hour, time.minute, time.second);
        return time_str;
    }
    else
    {
        snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d",
                 time.hour, time.minute, time.second);
        return time_str;
    }
}

const char *elog_port_get_p_info(void)
{
    return " ";
}

/**
 * get current thread name interface
 *
 * @return current thread name
 */
const char *elog_port_get_t_info(void)
{
    if (xTaskGetSchedulerState() != NULL)
    {
        return pcTaskGetName(NULL); // NULL也可以使用xTaskGetCurrentTaskHandle()代替,表示获取当前任务句柄
    }
    else
    {
        return "none";
    }
}
