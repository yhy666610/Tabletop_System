#include <stdio.h>
#include "elog.h"
#include "usart.h"
#include "rtc.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

static bool elog_inited;
static SemaphoreHandle_t elog_mutex;
static SemaphoreHandle_t elog_semphr;

static void async_output(void *arg);

ElogErrCode elog_port_init(void)
{
    BaseType_t result;

    if (elog_inited)
    {
        return ELOG_NO_ERR;
    }

    elog_mutex = xSemaphoreCreateMutex();
    if (elog_mutex == NULL)
    {
        printf("Error: EasyLogger mutex create failed!\r\n");
        goto err1;
    }
    elog_semphr = xSemaphoreCreateBinary();
    if (elog_semphr == NULL)
    {
        printf("Error: EasyLogger semaphore create failed!\r\n");
        goto err2;
    }

    result = xTaskCreate(async_output, "elog", 1024, NULL, tskIDLE_PRIORITY + 1, NULL);
    if (result != pdPASS)
    {
        printf("Error: EasyLogger async output task create failed!\r\n");
        goto err3;
    }

    elog_inited = true;
    return ELOG_NO_ERR;

err3:
    vSemaphoreDelete(elog_semphr);
    elog_semphr = NULL;
err2:
    vSemaphoreDelete(elog_mutex);
    elog_mutex = NULL;
err1:
    return ELOG_HAS_ERR;
}

void elog_port_deinit(void)
{
    if (elog_semphr != NULL)
    {
        vSemaphoreDelete(elog_semphr);
        elog_semphr = NULL;
    }
    if (elog_mutex != NULL)
    {
        vSemaphoreDelete(elog_mutex);
        elog_mutex = NULL;
    }
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

void elog_async_output_notice(void)
{
    if (elog_semphr != NULL)
    xSemaphoreGive(elog_semphr);
}

static void async_output(void *arg)
{
    size_t log_size = 0;
    char log_buf[ELOG_LINE_BUF_SIZE];

    while(true)
    {
        xSemaphoreTake(elog_semphr, portMAX_DELAY);

        do
        {
#ifdef ELOG_ASYNC_LINE_OUTPUT
            log_size = elog_async_get_line_log(log_buf, ELOG_LINE_BUF_SIZE);
#else
            log_size = elog_async_get_log(log_buf, ELOG_LINE_BUF_SIZE);
#endif

            if (log_size > 0)
            {
                elog_port_output(log_buf, log_size);
            }
        } while (log_size > 0);
    }
}
