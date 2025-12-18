#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "rtc.h"
#include "st7789.h"
#include "font.h"
#include "image.h"
#include "wifi.h"
#include "app.h"
#include "page.h"

static const uint16_t color_bg_time = mkcolor(248, 248, 248);
static const uint16_t color_bg_inner = mkcolor(136, 217, 234);
static const uint16_t color_bg_outdoor = mkcolor(254, 135, 75);

void main_page_display(void)
{
    st7789_fill_color(0, 0, ST7789_WIDTH - 1, ST7789_HEIGHT - 1, mkcolor(0, 0, 0));

    do
    {
        st7789_fill_color(15, 15, 224, 154, color_bg_time);
        // wifi图标
        st7789_draw_image(23, 20, &image_icon_wifi);
        // wifi ssid
        main_redraw_wifi_ssid(WIFI_SSID);
        // 时间
        st7789_write_string(25, 42, "--:--", mkcolor(0, 0, 0), color_bg_time, &font76_maple_extrabold);
        // 日期
        st7789_write_string(25, 130, "----/--/-- ---", mkcolor(143, 143, 143), color_bg_time, &font20_maple_bold);
    } while(0);

    do
    {
        st7789_fill_color(15, 165, 114, 304, color_bg_inner);
        st7789_write_string(19, 170, "室内环境", mkcolor(0, 0, 0), color_bg_inner, &font20_maple_bold);
        st7789_write_string(86, 191, "C", mkcolor(0, 0, 0), color_bg_inner, &font32_maple_bold);
        st7789_write_string(91, 262, "%", mkcolor(0, 0, 0), color_bg_inner, &font32_maple_bold);
        main_redraw_inner_temperature(999.9f);
        main_redraw_inner_humidity(999.9f);
    } while(0);

    do
    {
        st7789_fill_color(125, 165, 224, 304, color_bg_outdoor);
        st7789_write_string(192, 189, "C", mkcolor(0, 0, 0), color_bg_outdoor, &font32_maple_bold);
        st7789_draw_image(139, 239, &image_wenduji);
        main_redraw_outdoor_city("上海");
        main_redraw_outdoor_temperature(999.9f);
        main_redraw_outdoor_weather_icon(-1);
    } while (0);
}

void main_redraw_wifi_ssid(const char *ssid)
{
    char str[21];
    snprintf(str, sizeof(str), "%20s", ssid); // 保证字符串长度为20个字符,不足部分用空格填充,右对齐
    st7789_write_string(50, 23, str, mkcolor(143, 143, 143), color_bg_time, &font16_maple);
}

void main_redraw_time(rtc_time_t *time)
{
    char str[6];
    char comma = (time->second % 2 == 0) ? ':' : ' '; // 每秒闪烁一次
    snprintf(str, sizeof(str), "%02u%c%02u", time->hour, comma, time->minute);
    st7789_write_string(25, 42, str, mkcolor(0, 0, 0), color_bg_time, &font76_maple_extrabold);
}

void main_redraw_date(rtc_time_t *date)
{
    char str[18];
    snprintf(str, sizeof(str), "%04u/%02u/%02u 星期%s", date->year, date->month, date->day,
            date->weekday == 1 ? "一" :
            date->weekday == 2 ? "二" :
            date->weekday == 3 ? "三" :
            date->weekday == 4 ? "四" :
            date->weekday == 5 ? "五" :
            date->weekday == 6 ? "六" :
            date->weekday == 7 ? "日" : "X");
    st7789_write_string(25, 130, str, mkcolor(143, 143, 143), color_bg_time, &font20_maple_bold);
}

void main_redraw_inner_temperature(float temperature)
{
    char temp_str[3] = {'-', '-'};
    if (temperature >10.0f && temperature < 100.0f)
        snprintf(temp_str, sizeof(temp_str), "%2.0f", temperature);
    st7789_write_string(30, 192, temp_str, mkcolor(0, 0, 0), color_bg_inner, &font54_maple_semibold);
}

void main_redraw_inner_humidity(float humidity)
{
    char humi_str[3];
    if (humidity > 0.0f && humidity <= 99.99f)
        snprintf(humi_str, sizeof(humi_str), "%2.0f", humidity);
    st7789_write_string(25, 239, humi_str, mkcolor(0, 0, 0), color_bg_inner, &font64_maple_extrabold);
}

void main_redraw_outdoor_city(const char *city)
{
    char city_str[9];
    if (city != NULL)
        snprintf(city_str, sizeof(city_str), "%s", city);
    st7789_write_string(127, 170, city_str, mkcolor(0, 0, 0), color_bg_outdoor, &font20_maple_bold);
}

void main_redraw_outdoor_temperature(float temperature)
{
    char temp_str[3] = {'-', '-'};
    if (temperature > -10.0f && temperature < 100.0f)
        snprintf(temp_str, sizeof(temp_str), "%2.0f", temperature);
    st7789_write_string(135, 190, temp_str, mkcolor(0, 0, 0), color_bg_outdoor, &font54_maple_semibold);
}

void main_redraw_outdoor_weather_icon(const int code)
{
    const image_t *icon;
    if (code == 0 || code == 2 || code ==38)
    icon = &image_qing;
    else if (code ==1 || code ==3)
    icon = &image_moon;
    else if (code == 4 || code == 9)
        icon = &image_cloudy; //阴天
    else if (code == 5 || code == 6 || code == 7 || code == 8)
        icon = &image_cloudy; //多云
    else if (code == 10 || code == 13 || code == 14 || code == 15 || code == 16 || code == 17 || code == 18 || code == 19)
        icon = &image_zhongyu;
    else if (code == 11 || code == 12)
        icon = &image_leizhenyu;
    else if (code == 20 || code == 21 || code == 22 || code == 23 || code == 24 || code == 25)
        icon = &image_zhongxue;
    else // 扬沙、龙卷风等
        icon = &image_na;
    st7789_draw_image(166, 240, icon);
}
