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
    work_msg_queue = xQueueCreate(32, sizeof(work_message_t));  //创建一个队列,最多容纳32个工作项
    configASSERT(work_msg_queue);
    xTaskCreate(work_func, "Workqueue Task", 512, NULL, tskIDLE_PRIORITY + 5, NULL);
}

bool workqueue_run(work_t work, void *arg)
{
    configASSERT(work_msg_queue);
    work_message_t work_msg = {work, arg};
    if (xQueueSend(work_msg_queue, &work_msg, 0) != pdTRUE)
    {
        return false; // 队列满了,丢弃这次任务(工作项)
    }
    return true;
}
