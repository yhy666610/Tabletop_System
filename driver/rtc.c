#include "string.h"
#include "stm32f4xx.h"
#include "rtc.h"

void rtc_init(void)
{
    RTC_InitTypeDef RTC_InitStructure;
    RTC_StructInit(&RTC_InitStructure);
    RTC_Init(&RTC_InitStructure);

    RCC_RTCCLKCmd(ENABLE);
    RTC_WaitForSynchro();
}

static void _rtc_set_time_once(const rtc_time_t *time)
{
    RTC_TimeTypeDef RTC_TimeStruct;
    RTC_DateTypeDef RTC_DateStruct;

    RTC_TimeStruct.RTC_Hours = time->hour;
    RTC_TimeStruct.RTC_Minutes = time->minute;
    RTC_TimeStruct.RTC_Seconds = time->second;
    RTC_TimeStruct.RTC_H12 = 0; // 24小时制

    RTC_DateStruct.RTC_Year = time->year - 2000;
    RTC_DateStruct.RTC_Month = time->month;
    RTC_DateStruct.RTC_Date = time->day;
    RTC_DateStruct.RTC_WeekDay = time->weekday;

    RTC_SetTime(RTC_Format_BIN, &RTC_TimeStruct);
    RTC_SetDate(RTC_Format_BIN, &RTC_DateStruct);
}

static void _rtc_get_time_once(rtc_time_t *time)
{
    RTC_TimeTypeDef RTC_TimeStruct;
    RTC_DateTypeDef RTC_DateStruct;
    RTC_TimeStructInit(&RTC_TimeStruct);
    RTC_DateStructInit(&RTC_DateStruct);

    RTC_GetTime(RTC_Format_BIN, &RTC_TimeStruct);
    RTC_GetDate(RTC_Format_BIN, &RTC_DateStruct);

    time->year = 2000 + RTC_DateStruct.RTC_Year;
    time->month = RTC_DateStruct.RTC_Month;
    time->day = RTC_DateStruct.RTC_Date;
    time->hour = RTC_TimeStruct.RTC_Hours;
    time->minute = RTC_TimeStruct.RTC_Minutes;
    time->second = RTC_TimeStruct.RTC_Seconds;
    time->weekday = RTC_DateStruct.RTC_WeekDay;
}

/* 获取准确的RTC时间 */
void rtc_get_time(rtc_time_t *time)
{
    rtc_time_t t1, t2;
    do
    {
        _rtc_get_time_once(&t1);
        _rtc_get_time_once(&t2);
    } while (memcmp(&t1, &t2, sizeof(rtc_time_t)) != 0);
    memcpy(time, &t1, sizeof(rtc_time_t)); //此时t1和t2相等，复制哪个都行
}

/* 设置准确的RTC时间 */
void rtc_set_time(const rtc_time_t *time)
{
    rtc_time_t rtime; //read back time
    do
    {
        _rtc_set_time_once(time);
        _rtc_get_time_once(&rtime);
    } while (time->second != rtime.second);
}
