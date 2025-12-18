#include "string.h"
#include "ui.h"
#include "app.h"
#include "wifi.h"

void wifi_page_display(void)
{
    static const char *ssid = WIFI_SSID;
    uint16_t ssid_start_x = 0;

    int ssid_len = strlen(ssid) * font24_maple_bold.size / 2;
    if (ssid_len < UI_WIDTH)
        {
            ssid_start_x = (UI_WIDTH - ssid_len + 1) / 2;
        }

    const uint16_t color_bg = mkcolor(0, 0, 0); // Black background
    ui_fill_color(0, 0, UI_WIDTH - 1, UI_HEIGHT - 1, color_bg);
    ui_draw_image(30, 15, &image_wifi);
    ui_write_string(88, 191, "WiFi", mkcolor(0, 255, 234), color_bg, &font32_maple_bold); // (240-32*2)/2=88
    ui_write_string(ssid_start_x, 231, ssid, mkcolor(153, 38, 157), color_bg, &font24_maple_bold);
    ui_write_string(84, 263, "Á¬½ÓÖÐ", mkcolor(148, 198, 255), color_bg, &font24_maple_bold); // (240-24*3)/2=84
}
