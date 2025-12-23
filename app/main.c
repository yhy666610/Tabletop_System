#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "tim_delay.h"
#include "app.h"
#include "wifi.h"
#include "page.h"
#include "ui.h"
#include "elog.h"
#include "workqueue.h"

extern void board_init(void);
extern void board_lowlevel_init(void);

static void main_init(void *param)
{
    board_init();

    ui_init();

    elog_init();
    elog_set_fmt(ELOG_LVL_ASSERT, ELOG_FMT_ALL);
    elog_set_fmt(ELOG_LVL_ERROR, ELOG_FMT_ALL);
    elog_set_fmt(ELOG_LVL_WARN, ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME | ELOG_FMT_T_INFO);
    elog_set_fmt(ELOG_LVL_INFO, ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME);
    elog_set_fmt(ELOG_LVL_DEBUG, ELOG_FMT_LVL | ELOG_FMT_TAG);
    elog_set_fmt(ELOG_LVL_VERBOSE, ELOG_FMT_LVL | ELOG_FMT_TAG);
    elog_async_enabled(true);
    elog_start();

    welcome_page_display();

	printf("current app version: V1.0\r\n");

    wifi_init();
    wifi_page_display();
    wifi_wait_connect();

    main_page_display();

    app_init();

    vTaskDelete(NULL);
}

int main(void)
{
	board_lowlevel_init();

    workqueue_init();

    xTaskCreate(main_init, "main_init", 1024, NULL, tskIDLE_PRIORITY + 9, NULL);

    vTaskStartScheduler();

    while (1);// Should never reach here
}
