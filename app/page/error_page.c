#include <string.h>
#include "ui.h"

void error_page_display(const char *error_msg)
{
    const uint16_t color_bg = mkcolor(255, 255, 255); // Black background
    ui_fill_color(0, 0, UI_WIDTH - 1, UI_HEIGHT - 1, color_bg);
    ui_draw_image(40, 37, &image_error);

    uint16_t start_x = 0;
    int len = strlen(error_msg) * (font24_maple_bold.size) / 2;
    if (len < UI_WIDTH)
    {
        start_x = (UI_WIDTH - len + 1) / 2;
    }
    ui_write_string(start_x, 245, error_msg, mkcolor(255, 0, 0), color_bg, &font24_maple_bold);
}
