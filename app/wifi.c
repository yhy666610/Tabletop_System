#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "esp_at.h"
#include "page.h"
#include "app.h"
#include "wifi.h"

#define LOG_TAG "WIFI"
#define LOG_LVL ELOG_LVL_INFO
#include "elog.h"

void wifi_init(void)
{
    if (!esp_at_init())
	{
		log_e("[AT] Init failed");
		goto err;
	}
    log_i("[AT] Inited");

	if (!esp_at_wifi_init())
	{
		log_e("[WIFI] Init failed");
		goto err;
	}
    log_i("[WIFI] Inited");

    if (!esp_at_init_sntp())
    {
        log_e("[SNTP] Init failed");
        goto err;
    }
    log_i("[SNTP] Inited");

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
    log_i("[WIFI] Connecting");

    esp_at_connect_wifi(WIFI_SSID, WIFI_PASSWORD, NULL);

    for (uint32_t t = 0; t < 10000; t += 100)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
        esp_wifi_info_t wifi_info = { 0 };
        if (esp_at_get_wifi_info(&wifi_info) && wifi_info.connected)
        {
            log_i("[WIFI] Connected");
            log_i("[WIFI] SSID: %s, BSSID: %s, Channel: %d, RSSI: %d",
                   wifi_info.ssid, wifi_info.bssid, wifi_info.channel, wifi_info.rssi);
            return;
        }
    }

    log_w("[WIFI] Connect timeout");
    error_page_display("WIFI init failed");
	while (1)
	{
		;
	}
}
