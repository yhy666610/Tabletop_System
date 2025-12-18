#ifndef __IMAGE_H__
#define __IMAGE_H__

#include <stdint.h>

typedef struct
{
    uint16_t width;
    uint16_t height;
    const uint8_t *data;
} image_t;

extern const image_t image_meihua;
extern const image_t image_error;
extern const image_t image_wifi;
extern const image_t image_cloudy;
extern const image_t image_zhongxue;
extern const image_t image_zhongyu;
extern const image_t image_wenduji;
extern const image_t image_na;
extern const image_t image_leizhenyu;
extern const image_t image_qing;
extern const image_t image_icon_wifi;
extern const image_t image_moon;
#endif /* __IMAGE_H__ */
