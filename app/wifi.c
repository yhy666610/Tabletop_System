#include <stdio.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "esp_at.h"
#include "page.h"
#include "app.h"
#include "wifi.h"

void wifi_init(void)
{
    if (!esp_at_init())
	{
		printf("[AT] Init failed\r\n");
		goto err;
	}
    printf("[AT] Inited\r\n");

	if (!esp_at_wifi_init())
	{
		printf("[WIFI] Init failed\r\n");
		goto err;
	}
    printf("[WIFI] Inited\r\n");

    if (!esp_at_init_sntp())
    {
        printf("[SNTP] Init failed\r\n");
        goto err;
    }
    printf("[SNTP] Inited\r\n");

    return;

err:
    error_page_display("WIFI init failed");
	while (1)
	{
		;
	}
}

void wifi_wait_connect(void)
{
    printf("[WIFI] Connecting\r\n");

    esp_at_connect_wifi(WIFI_SSID, WIFI_PASSWORD, NULL);

    for (uint32_t t = 0; t < 10000; t += 100)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
        esp_wifi_info_t wifi_info = { 0 };
        if (esp_at_get_wifi_info(&wifi_info) && wifi_info.connected)
        {
            printf("[WIFI] Connected\r\n");
            printf("[WIFI] SSID: %s, BSSID: %s, Channel: %d, RSSI: %d\r\n",
                   wifi_info.ssid, wifi_info.bssid, wifi_info.channel, wifi_info.rssi);
            return;
        }
    }

    printf("[WIFI] Connect timeout\r\n");
    error_page_display("WIFI init failed");
	while (1)
	{
		;
	}
}
