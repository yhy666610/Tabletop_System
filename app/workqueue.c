#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "workqueue.h"

typedef struct
{
    work_t work;
    void *arg;
} work_message_t;

static QueueHandle_t work_msg_queue;

static void work_func(void *arg)
{
    work_message_t msg;
    while (1)
    {
        xQueueReceive(work_msg_queue, &msg, portMAX_DELAY); // 无期限等待接收工作项
        msg.work(msg.arg); // 执行工作项
    }
}

void workqueue_init(void)
{
    work_msg_queue = xQueueCreate(16, sizeof(work_message_t));  //创建一个队列,最多容纳16个工作项
    configASSERT(work_msg_queue);
    xTaskCreate(work_func, "Workqueue Task", 1024, NULL, tskIDLE_PRIORITY + 5, NULL);
}

void workqueue_run(work_t work, void *arg)
{
    configASSERT(work_msg_queue);
    work_message_t work_msg = {work, arg};
    xQueueSend(work_msg_queue, &work_msg, portMAX_DELAY); // 无期限等待发送工作项到队列
}
