#include <stdint.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "rtc.h"
#include "aht20.h"
#include "esp_at.h"
#include "weather.h"
#include "page.h"
#include "app.h"
#include "workqueue.h"

#define LOG_TAG "APP"
#define LOG_LVL ELOG_LVL_INFO
#include "elog.h"

#define WEATHER_URL "https://api.seniverse.com/v3/weather/now.json?key=S5uYc8UoqdGOUI1eq&location=WTW3SJ5ZBJUY&language=en&unit=c"
//static const char *weather_url = "https://api.seniverse.com/v3/weather/now.json?key=S5uYc8UoqdGOUI1eq&location=WTW3SJ5ZBJUY&language=en&unit=c";

#define MILLISECONDS(x)   (x)
#define SECONDS(x)        MILLISECONDS((x) * 1000)
#define MINUTES(x)        SECONDS((x) * 60)
#define HOURS(x)          MINUTES((x) * 60)
#define DAYS(x)           HOURS((x) * 24)

#define TIME_SYNC_INTERVAL      HOURS(1)
#define WIFI_UPDATE_INTERVAL    SECONDS(5)
#define TIME_UPDATE_INTERVAL    SECONDS(1)
#define INNER_UPDATE_INTERVAL   SECONDS(3)
#define OUTDOOR_UPDATE_INTERVAL MINUTES(1)

#define MLOOP_EVT_TIME_SYNC        (1 << 0)
#define MLOOP_EVT_WIFI_UPDATE      (1 << 1)
#define MLOOP_EVT_INNER_UPDATE     (1 << 2)
#define MLOOP_EVT_OUTDOOR_UPDATE   (1 << 3)
#define MLOOP_EVT_ALL              (MLOOP_EVT_TIME_SYNC     | \
                                    MLOOP_EVT_WIFI_UPDATE   | \
                                    MLOOP_EVT_INNER_UPDATE  | \
                                    MLOOP_EVT_OUTDOOR_UPDATE)

// static uint32_t time_sync_delay;
// static uint32_t wifi_update_delay;
// static uint32_t time_update_delay;
// static uint32_t inner_update_delay;
// static uint32_t outdoor_update_delay;

static TimerHandle_t time_sync_timer;
static TimerHandle_t wifi_update_timer;
static TimerHandle_t time_update_timer;
static TimerHandle_t inner_update_timer;
static TimerHandle_t outdoor_update_timer;

// 1s钟周期回调函数
// static void cpu_periodic_callback(void)
// {
//     if (time_sync_delay > 0)
//         time_sync_delay--;
//     if (wifi_update_delay > 0)
//         wifi_update_delay--;
//     if (time_update_delay > 0)
//         time_update_delay--;
//     if (inner_update_delay > 0)
//         inner_update_delay--;
//     if (outdoor_update_delay > 0)
//         outdoor_update_delay--;
// }

// void main_loop_init(void)
// {
     //cpu_register_periodic_callback(cpu_periodic_callback);
// }

// 时间同步,从ESP-AT获取时间并设置到RTC,每24小时同步一次
static void time_sync(void)
{
    uint32_t restart_sync_delay = TIME_SYNC_INTERVAL;
    rtc_time_t rtc_time = {0};
    esp_time_t esp_time = {0};

    if (!esp_at_get_time(&esp_time))
    {
        log_e("[SNTP] Get time failed");
        restart_sync_delay = SECONDS(1); //延时1秒后重试
        goto err;
    }

    if (esp_time.year < 2000)
    {
        log_e("[SNTP] Invalid date received");
        restart_sync_delay = SECONDS(1);
        goto err;
    }
    log_i("[SNTP] Sync time: %04u-%02u-%02u %02u:%02u:%02u (%d)",
           esp_time.year, esp_time.month, esp_time.day,
           esp_time.hour, esp_time.minute, esp_time.second, esp_time.weekday);

    rtc_time.year = esp_time.year;
    rtc_time.month = esp_time.month;
    rtc_time.day = esp_time.day;
    rtc_time.hour = esp_time.hour;
    rtc_time.minute = esp_time.minute;
    rtc_time.second = esp_time.second;
    rtc_time.weekday = esp_time.weekday;
    rtc_set_time(&rtc_time);
err:
    xTimerChangePeriod(time_sync_timer, pdMS_TO_TICKS(restart_sync_delay), 0);
    // xTaskNotify(main_loop_task, MLOOP_EVT_TIME_UPDATE, eSetBits); //同步时间后立即更新时间显示
}
// WiFi状态更新,每5秒更新一次
static void wifi_update(void)
{
    static esp_wifi_info_t last_info = {0}; //这里一定要加上static，否则每次调用都会被重置，导致无法正确比较，亲测不加static会导致wifi状态无法更新

    //xTimerChangePeriod(wifi_update_timer, pdMS_TO_TICKS(WIFI_UPDATE_INTERVAL), 0);
    esp_wifi_info_t info = {0};
    if (!esp_at_get_wifi_info(&info))
    {
        log_e("[WIFI] WiFi info get failed");
        return;
    }

    if (memcmp(&info, &last_info, sizeof(info)) == 0)
    {
        return;
    }

    if (last_info.connected == info.connected)
    {
        return;
    }

    if (info.connected)
    {
        log_i("[WIFI] Connected to SSID: %s, BSSID: %s, Channel: %d, RSSI: %d",
               info.ssid, info.bssid, info.channel, info.rssi);
        main_redraw_wifi_ssid(info.ssid);
    }
    else
    {
        log_w("[WIFI] Disconnected from %s", last_info.ssid);
        main_redraw_wifi_ssid("Wifi lost");
    }

    memcpy(&last_info, &info, sizeof(info));
}
//更新时间显示,每秒更新一次
static void time_update(void)
{
    static rtc_time_t last_time = {0};

    //xTimerChangePeriod(time_update_timer, pdMS_TO_TICKS(TIME_UPDATE_INTERVAL), 0);
    // 获取当前RTC时间
    rtc_time_t rtc_time;
    rtc_get_time(&rtc_time);

    if (rtc_time.year < 2020)
    {
        return;
    }

    if (rtc_time.second == last_time.second)
    {
        return;
    }

    if (memcmp(&rtc_time, &last_time, sizeof(rtc_time_t)) == 0)
    {
        return;
    }

    main_redraw_time(&rtc_time);
    main_redraw_date(&rtc_time);
    memcpy(&last_time, &rtc_time, sizeof(rtc_time));
}
//室内环境更新,每3秒更新一次
static void inner_update(void)
{
    static float last_temperature = 0.0f;
    static float last_humidity = 0.0f;

    //xTimerChangePeriod(inner_update_timer, pdMS_TO_TICKS(INNER_UPDATE_INTERVAL), 0);

    if (!aht20_start_measurement())
    {
        log_e("[AHT20] Start measurement failed");
        return;
    }

    if (!aht20_wait_for_measurement())
    {
        log_e("[AHT20] Wait for measurement failed");
        return;
    }

    float temperature = 0.0f;
    float humidity = 0.0f;
    if (aht20_read_measurement(&temperature, &humidity))
    {
        if (last_temperature != temperature || last_humidity != humidity)
        {
            main_redraw_inner_temperature(temperature);
            main_redraw_inner_humidity(humidity);
            last_temperature = temperature;
            last_humidity = humidity;
        }
        log_i("[AHT20] Temperature: %.2f C, Humidity: %.2f %%", temperature, humidity);
    }
    else
    {
        log_e("[AHT20] Read measurement failed");
    }
}
//室外天气更新,每1分钟更新一次
static void outdoor_update(void)
{
    static weather_info_t last_weather = {0};

    //xTimerChangePeriod(outdoor_update_timer, pdMS_TO_TICKS(OUTDOOR_UPDATE_INTERVAL), 0);

    weather_info_t weather_info = { 0 };
    const char *weather_url = WEATHER_URL;
    const char *weather_http_response = esp_at_http_get(weather_url);
    if (weather_http_response == NULL)
    {
        log_e("[HTTP] GET request failed");
        return;
    }
    if (!parse_seniverse_response(weather_http_response, &weather_info))
    {
        log_e("[WEATHER] Parse response failed");
        return;
	}

    log_i("[WEATHER] %s, %s, %.1f C", weather_info.city, weather_info.weather, weather_info.temperature);
    if (memcmp(&last_weather, &weather_info, sizeof(weather_info_t)) != 0)
    {
        last_weather = weather_info;
    }

    main_redraw_outdoor_temperature(weather_info.temperature);
    main_redraw_outdoor_weather_icon(weather_info.weather_code);

}

typedef void (*app_job_t)(void);    // 定义应用任务函数指针类型,需用它声明变量后才能使用
//static void (*app_job_t)(void);   // 定义应用任务函数指针变量

static void app_work(void *arg)
{
    app_job_t job = (app_job_t)arg;
    job();
}
static void work_timer_callback(TimerHandle_t xTimer)
{
    app_job_t job = (app_job_t)pvTimerGetTimerID(xTimer); //获取定时器ID作为事件标志
    workqueue_run(app_work, job); //将任务提交到工作队列执行
}

static void app_timer_callback(TimerHandle_t xTimer)
{
    app_job_t job= (app_job_t)pvTimerGetTimerID(xTimer);
    job();

    // app_job_t = (app_job_t)pvTimerGetTimerID(xTimer);
    // app_job_t();
}

void app_init(void)
{
    time_update_timer = xTimerCreate("time_update_timer",
                                    pdMS_TO_TICKS(TIME_UPDATE_INTERVAL),
                                    pdTRUE,
                                    time_update,
                                    (TimerCallbackFunction_t)app_timer_callback);

    time_sync_timer = xTimerCreate("time_sync_timer",
                                  pdMS_TO_TICKS(200),
                                  pdTRUE,
                                  time_sync,
                                  (TimerCallbackFunction_t)work_timer_callback);

    wifi_update_timer = xTimerCreate("wifi_update_timer",
                                    pdMS_TO_TICKS(WIFI_UPDATE_INTERVAL),
                                    pdTRUE,
                                    wifi_update,
                                    (TimerCallbackFunction_t)work_timer_callback);

    inner_update_timer = xTimerCreate("inner_update_timer",
                                     pdMS_TO_TICKS(INNER_UPDATE_INTERVAL),
                                     pdTRUE,
                                     inner_update,
                                     (TimerCallbackFunction_t)work_timer_callback);

    outdoor_update_timer = xTimerCreate("outdoor_update_timer",
                                       pdMS_TO_TICKS(OUTDOOR_UPDATE_INTERVAL),
                                       pdTRUE,
                                       outdoor_update,
                                       (TimerCallbackFunction_t)work_timer_callback);

    workqueue_run(app_work, time_sync); //启动时立即同步时间
    workqueue_run(app_work, wifi_update); //启动时立即更新WiFi状态
    workqueue_run(app_work, inner_update); //启动时立即更新内部传感器
    workqueue_run(app_work, outdoor_update); //启动时立即更新室外天气

    //以下代码可选，因为定时器已经设置为自动重载，xTimerCreate时传入pdTRUE即可
    xTimerStart(time_update_timer, 0);
    xTimerStart(time_sync_timer, 0);
    xTimerStart(wifi_update_timer, 0);
    xTimerStart(inner_update_timer, 0);
    xTimerStart(outdoor_update_timer, 0);
}
