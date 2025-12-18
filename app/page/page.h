#ifndef __PAGE_H__
#define __PAGE_H__

#include "rtc.h"

void main_page_display(void);
void error_page_display(const char *error_msg);
void wifi_page_display(void);
void welcome_page_display(void);
void main_redraw_wifi_ssid(const char *ssid);
void main_redraw_time(rtc_time_t *time);
void main_redraw_date(rtc_time_t *date);
void main_redraw_inner_temperature(float temperature);
void main_redraw_inner_humidity(float humidity);
void main_redraw_outdoor_city(const char *city);
void main_redraw_outdoor_temperature(float temperature);
void main_redraw_outdoor_weather_icon(int icon_id);

#endif // __PAGE_H__
