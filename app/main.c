#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "tim_delay.h"
#include "app.h"
#include "wifi.h"
#include "page.h"
#include "ui.h"
#include "workqueue.h"

extern void board_init(void);
extern void board_lowlevel_init(void);
extern void logger_init();

static void main_init(void *param)
{
    board_init();

    logger_init();

    ui_init();

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
