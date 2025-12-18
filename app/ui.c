#include <stdint.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "st7789.h"
#include "ui.h"
#include "font.h"
#include "image.h"

typedef enum
{
    UI_EVENT_FILL_COLOR,
    UI_EVENT_WRITE_STRING,
    UI_EVENT_DRAW_IMAGE,
} ui_event_t;

typedef struct
{
    ui_event_t event;
    union
    {
        struct
        {
            uint16_t x;
            uint16_t y;
            uint16_t width;
            uint16_t height;
            uint16_t color;
        } fill_color;
        struct
        {
            uint16_t x;
            uint16_t y;
            const char *str;
            uint16_t color;
            uint16_t bg_color;
            const font_t *font;
        } write_string;
        struct
        {
            uint16_t x;
            uint16_t y;
            const image_t *image;
        } draw_image;
    };
} ui_message_t;

static QueueHandle_t ui_queue;

static void ui_func(void *param)
{
    ui_message_t ui_msg;

    st7789_init();

    while (1)
    {
        xQueueReceive(ui_queue, &ui_msg, portMAX_DELAY);    // 无期限等待接收UI消息

        switch (ui_msg.event)
        {
            case UI_EVENT_FILL_COLOR:
                st7789_fill_color(ui_msg.fill_color.x, ui_msg.fill_color.y,
                                  ui_msg.fill_color.width, ui_msg.fill_color.height,
                                  ui_msg.fill_color.color);
                break;
            case UI_EVENT_WRITE_STRING:
                st7789_write_string(ui_msg.write_string.x, ui_msg.write_string.y,
                                    ui_msg.write_string.str,
                                    ui_msg.write_string.color,
                                    ui_msg.write_string.bg_color,
                                    ui_msg.write_string.font);
                vPortFree((void *)ui_msg.write_string.str); // 释放字符串内存,避免内存泄漏
                break;
            case UI_EVENT_DRAW_IMAGE:
                st7789_draw_image(ui_msg.draw_image.x, ui_msg.draw_image.y,
                                  ui_msg.draw_image.image);
                break;
            default:
                printf("Unknown UI event: %d\n", ui_msg.event);
                break;
        }
    }
}

void ui_init(void)
{
    ui_queue = xQueueCreate(16, sizeof(ui_message_t));  //创建一个队列,最多容纳16个UI消息
    configASSERT(ui_queue);
    xTaskCreate(ui_func, "UI Task", 1024, NULL, tskIDLE_PRIORITY + 8, NULL);
}
void ui_fill_color(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    ui_message_t ui_msg;
    ui_msg.event = UI_EVENT_FILL_COLOR;
    ui_msg.fill_color.x = x1;
    ui_msg.fill_color.y = y1;
    ui_msg.fill_color.width = x2;
    ui_msg.fill_color.height = y2;
    ui_msg.fill_color.color = color;

    xQueueSend(ui_queue, &ui_msg, portMAX_DELAY); // 无期限等待发送UI消息
}

void ui_write_string(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bg_color, const font_t *font)
{
    char *str_copy = pvPortMalloc(strlen(str) + 1);
    if (str_copy == NULL)
    {
        printf("UI Write String: Memory allocation failed: %s\n", str_copy);
        return; // 内存分配失败，直接返回,不发送消息,避免UI任务崩溃,会丢失这次字符串显示请求,但整个系统会继续运行
    }
    strcpy(str_copy, str);

    ui_message_t ui_msg;
    ui_msg.event = UI_EVENT_WRITE_STRING;
    ui_msg.write_string.x = x;
    ui_msg.write_string.y = y;
    ui_msg.write_string.str = str_copy;
    ui_msg.write_string.color = color;
    ui_msg.write_string.bg_color = bg_color;
    ui_msg.write_string.font = font;

    xQueueSend(ui_queue, &ui_msg, portMAX_DELAY); // 无期限等待发送UI消息
}

void ui_draw_image(uint16_t x, uint16_t y, const image_t *image)
{
    ui_message_t ui_msg;
    ui_msg.event = UI_EVENT_DRAW_IMAGE;
    ui_msg.draw_image.x = x;
    ui_msg.draw_image.y = y;
    ui_msg.draw_image.image = image;

    xQueueSend(ui_queue, &ui_msg, portMAX_DELAY); // 无期限等待发送UI消息
}
