#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "stm32f4xx.h"
#include "st7789.h"
// CLK —— PB13
// MOSI —— PC3
// MISO —— PC2
// CS —— PE2
// RESET —— PE3
// DC —— PE4
// BL —— PE5

#define CS_PORT     GPIOE
#define CS_PIN      GPIO_Pin_2
#define RESET_PORT  GPIOE
#define RESET_PIN   GPIO_Pin_3
#define DC_PORT     GPIOE
#define DC_PIN      GPIO_Pin_4
#define BL_PORT     GPIOE
#define BL_PIN      GPIO_Pin_5

static SemaphoreHandle_t write_gram_semaphore;

void ST7789_io_init(void)
{
    GPIO_SetBits(GPIOE, GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5);
	GPIO_ResetBits(BL_PORT, BL_PIN);

    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_StructInit(&GPIO_InitStruct);
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5;
    GPIO_Init(GPIOE, &GPIO_InitStruct);

	GPIO_PinAFConfig(GPIOC, GPIO_PinSource2, GPIO_AF_SPI2); // MISO
	GPIO_PinAFConfig(GPIOC, GPIO_PinSource3, GPIO_AF_SPI2); // MOSI
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource13, GPIO_AF_SPI2); // SCK

    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_13;
	GPIO_Init(GPIOB, &GPIO_InitStruct);
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3;
	GPIO_Init(GPIOC, &GPIO_InitStruct);
}

void ST7789_spi_init(void)
{
	SPI_InitTypeDef SPI_InitStruct;
	SPI_StructInit(&SPI_InitStruct);
	SPI_InitStruct.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
	SPI_InitStruct.SPI_Mode = SPI_Mode_Master;
	SPI_InitStruct.SPI_DataSize = SPI_DataSize_8b;
	SPI_InitStruct.SPI_CPOL = SPI_CPOL_Low;// 时钟空闲低电平
	SPI_InitStruct.SPI_CPHA = SPI_CPHA_1Edge;// 数据在第一个时钟沿采样
	SPI_InitStruct.SPI_NSS = SPI_NSS_Soft;
	SPI_InitStruct.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4;
	SPI_InitStruct.SPI_FirstBit = SPI_FirstBit_MSB;
	SPI_Init(SPI2, &SPI_InitStruct);
	SPI_DMACmd(SPI2, SPI_DMAReq_Tx, ENABLE);
	SPI_Cmd(SPI2, ENABLE);
}

static void st7789_dma_init(void)
{
    DMA_InitTypeDef DMA_InitStruct;
    DMA_StructInit(&DMA_InitStruct);
    DMA_InitStruct.DMA_Channel = DMA_Channel_0; // 查参考手册得知SPI2_TX对应DMA1的通道0
    DMA_InitStruct.DMA_PeripheralBaseAddr = (uint32_t)&(SPI2->DR); //SPI数据寄存器地址
    DMA_InitStruct.DMA_DIR = DMA_DIR_MemoryToPeripheral; //因为数据是从内存传输到外设SPI的数据寄存器，所以传输方向是内存到外设
    DMA_InitStruct.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStruct.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStruct.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStruct.DMA_Priority = DMA_Priority_High;
    DMA_InitStruct.DMA_FIFOMode = DMA_FIFOMode_Enable; // FIFO模式启用
    DMA_InitStruct.DMA_FIFOThreshold = DMA_FIFOThreshold_Full; // FIFO满才传输
    DMA_InitStruct.DMA_MemoryBurst = DMA_MemoryBurst_INC8; // 内存突发传输8个数据单元
    DMA_InitStruct.DMA_PeripheralBurst = DMA_PeripheralBurst_Single; // 外设突发传输单个数据单元
    DMA_ITConfig(DMA1_Stream4, DMA_IT_TC, ENABLE); // 传输完成中断使能
    DMA_Init(DMA1_Stream4, &DMA_InitStruct); // SPI2_TX对应DMA1的Stream4
}

void st7789_interrupt_init(void)
{
    // DMA TC中断设置
    NVIC_InitTypeDef NVIC_InitStructure;
    memset(&NVIC_InitStructure, 0, sizeof(NVIC_InitTypeDef));
    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Stream4_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    NVIC_SetPriority(DMA1_Stream4_IRQn, 5);
}

static void st7789_write_register(uint8_t reg, uint8_t data[], uint16_t len)
{
    SPI_DataSizeConfig(SPI2, SPI_DataSize_8b);// 设置为8位数据模式

    GPIO_ResetBits(CS_PORT, CS_PIN);

    GPIO_ResetBits(DC_PORT, DC_PIN);
    SPI_SendData(SPI2, reg);
    while (SPI_GetFlagStatus(SPI2, SPI_FLAG_TXE) == RESET);
    while (SPI_GetFlagStatus(SPI2, SPI_FLAG_BSY) == SET);

    GPIO_SetBits(DC_PORT, DC_PIN);

    for (uint16_t i = 0; i < len; i++)
    {
        SPI_SendData(SPI2, data[i]);
        while (SPI_GetFlagStatus(SPI2, SPI_FLAG_TXE) == RESET);
    }
    while (SPI_GetFlagStatus(SPI2, SPI_FLAG_BSY) == SET);

    GPIO_SetBits(CS_PORT, CS_PIN);
}

static void st7789_write_gram(uint8_t data[], uint32_t len, bool single_color)
{
    SPI_DataSizeConfig(SPI2, SPI_DataSize_16b); // 设置为16位数据模式

    GPIO_ResetBits(CS_PORT, CS_PIN);
    GPIO_SetBits(DC_PORT, DC_PIN);

    len >>= 1; // 转换为16位数据单元数量

    do
    {
        uint32_t chunk_size = (len < 65535) ? len : 65535; // 因为DMA的缓冲区大小限制，每次传输不超过65535个数据单元
        len -= chunk_size; // 更新剩余数据单元数量

        if (single_color) DMA1_Stream4->CR &= ~DMA_SxCR_MINC; // 禁止内存地址递增
        else              DMA1_Stream4->CR |= DMA_SxCR_MINC; // 允许内存地址递增
        DMA1_Stream4->NDTR = chunk_size; // 设置传输的数据单元数量
        DMA1_Stream4->M0AR = (uint32_t)data; // 设置内存地址

        DMA_Cmd(DMA1_Stream4, ENABLE); // 启动DMA传输
        xSemaphoreTake(write_gram_semaphore, portMAX_DELAY); // 等待传输完成信号
        // while (DMA_GetFlagStatus(DMA1_Stream4, DMA_FLAG_TCIF4) == RESET); //等待DMA传输完成
        // DMA_ClearFlag(DMA1_Stream4, DMA_FLAG_TCIF4); //清除传输完成标志

        if (single_color == false) // 如果不是单色填充模式，更新数据指针
        data += chunk_size * 2; //每个数据单元2字节,执行这句的目的是更新data指针，指向下一个待传输的数据位置
    } while (len > 0);

    while (SPI_GetFlagStatus(SPI2, SPI_FLAG_BSY) == SET); //等待SPI传输完成

    GPIO_SetBits(CS_PORT, CS_PIN);
}

static void st7789_reset(void)
{
    GPIO_ResetBits(RESET_PORT, RESET_PIN);
    vTaskDelay(pdMS_TO_TICKS(2)); //20us以上
    GPIO_SetBits(RESET_PORT, RESET_PIN);
    vTaskDelay(pdMS_TO_TICKS(120));
}

static void st7789_set_backlight(bool on)
{
    GPIO_WriteBit(BL_PORT, BL_PIN, on ? Bit_SET : Bit_RESET);
}

void st7789_init_display(void)
{
    st7789_reset();

    st7789_write_register(0x11, NULL, 0); // Sleep OUT
    vTaskDelay(pdMS_TO_TICKS(5));

    st7789_write_register(0x36, (uint8_t[]){0x00}, 1); // MADCTL
    st7789_write_register(0x3A, (uint8_t[]){0x55}, 1); // COLMOD
    st7789_write_register(0xB7, (uint8_t[]){0x46}, 1);
    st7789_write_register(0xBB, (uint8_t[]){0x1B}, 1);
    st7789_write_register(0xC0, (uint8_t[]){0x2C}, 1);
    st7789_write_register(0xC2, (uint8_t[]){0x01}, 1);
    st7789_write_register(0xC4, (uint8_t[]){0x20}, 1);
    st7789_write_register(0xC6, (uint8_t[]){0x0F}, 1);
    st7789_write_register(0xD0, (uint8_t[]){0xA4,0xA1}, 2);
    st7789_write_register(0xD6, (uint8_t[]){0xA1}, 1);
    st7789_write_register(0xE0, (uint8_t[]){0xF0,0x00,0x06,0x04,0x05,0x05,0x31,0x44,0x48,0x36,0x12,0x12,0x2B,0x34}, 14);
    st7789_write_register(0xE0, (uint8_t[]){0xF0,0x0B,0x0F,0x0F,0x0D,0x26,0x31,0x43,0x47,0x38,0x14,0x14,0x2C,0x32}, 14);
    st7789_write_register(0x20, NULL, 0);
    st7789_write_register(0x29, NULL, 0); // Display ON

    st7789_fill_color(0, 0, ST7789_WIDTH - 1, ST7789_HEIGHT - 1, 0x0000); // Fill black
    st7789_set_backlight(true);
}

static bool in_screen_bound(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    if (x1 >= ST7789_WIDTH || y1 >= ST7789_HEIGHT || x2 >= ST7789_WIDTH || y2 >= ST7789_HEIGHT)
        return false;

    if (x1 > x2 || y1 > y2)
        return false;

    return true;
}

static void st7789_set_range(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    st7789_write_register(0x2A, (uint8_t[]){(x1 >> 8) & 0xff, x1 & 0xff, (x2 >> 8) & 0xff, x2 & 0xff}, 4);// Column Address Set
    st7789_write_register(0x2B, (uint8_t[]){(y1 >> 8) & 0xff, y1 & 0xff, (y2 >> 8) & 0xff, y2 & 0xff}, 4);// Row Address Set
}

static void st7789_set_gram_mode(void)
{
    st7789_write_register(0x2C, NULL, 0); // Memory Write
}

void st7789_fill_color(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    if (!in_screen_bound(x1, y1, x2, y2))
        return;

    st7789_set_range(x1, y1, x2, y2);
    st7789_set_gram_mode();

    uint32_t total_pixels = (x2 - x1 + 1) * (y2 - y1 + 1);
    st7789_write_gram((uint8_t *)&color, total_pixels * 2, true);
}

static void st7789_draw_font(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t *model, uint16_t color, uint16_t bg_color)
{
    uint16_t bytes_per_row = (width + 7) / 8;

    static uint8_t buf[72 * 72 * 2]; //最大字体72x72
    uint8_t *pbuf = buf;

    for (uint16_t row = 0; row < height; row++)
	{
		const uint8_t *row_data = model + row * bytes_per_row;
		for (uint16_t col = 0; col < width; col++)
		{
			uint16_t pixel = row_data[col / 8] & (1 << (7 - col % 8)); //获取像素点
            uint16_t pixel_color = pixel ? color : bg_color;
            *pbuf++ = pixel_color & 0xff; //低字节先发，地址自增，&0xff：取低8位
            *pbuf++ = (pixel_color >> 8) & 0xff; //高字节后发,.右移的目的是为了取高8位
		}
	}

    st7789_set_range(x, y, x + width - 1, y + height - 1);
    st7789_set_gram_mode();
    st7789_write_gram(buf, pbuf - buf, false); // pbuf - buf计算出总字节数


    // GPIO_ResetBits(CS_PORT, CS_PIN);
    // GPIO_SetBits(DC_PORT, DC_PIN);
	// for (uint16_t row = 0; row < height; row++)
	// {
	// 	const uint8_t *row_data = model
	// 		+ row * bytes_per_row;
	// 	for (uint16_t col = 0; col < width; col++)
	// 	{
	// 		uint8_t pixel = row_data[col / 8] & (1 << (7 - col % 8));
	// 		if (pixel)
    //         {
    //             SPI_SendData(SPI2, color_data[0]);
    //             while (SPI_GetFlagStatus(SPI2, SPI_FLAG_TXE) == RESET);
    //             SPI_SendData(SPI2, color_data[1]);
    //             while (SPI_GetFlagStatus(SPI2, SPI_FLAG_TXE) == RESET);
    //         }
    //         else
    //         {
    //             SPI_SendData(SPI2, bg_color_data[0]);
    //             while (SPI_GetFlagStatus(SPI2, SPI_FLAG_TXE) == RESET);
    //             SPI_SendData(SPI2, bg_color_data[1]);
    //             while (SPI_GetFlagStatus(SPI2, SPI_FLAG_TXE) == RESET);
    //         }
	// 	}
	// }
    // while (SPI_GetFlagStatus(SPI2, SPI_FLAG_BSY) != RESET);
    // GPIO_SetBits(CS_PORT, CS_PIN);

    /* copilot */
    // uint16_t char_index = ch - 32;
    // uint16_t char_width = font->height / 2; // Fixed width for font16
    // uint16_t char_height = font->height;
    // const uint8_t *char_model = font->ascii_model + char_index * char_height * (char_width / 8);

    // st7789_set_range(x, y, x + char_width - 1, y + char_height - 1);
    // st7789_set_gram_mode();

    // GPIO_ResetBits(CS_PORT, CS_PIN);
    // GPIO_SetBits(DC_PORT, DC_PIN);

    // for (uint16_t row = 0; row < char_height; row++)
    // {
    //     for (uint16_t col = 0; col < char_width; col++)
    //     {
    //         uint8_t byte = char_model[row * (char_width / 8) + (col / 8)];
    //         uint8_t bit = 0x80 >> (col % 8);
    //         uint16_t pixel_color = (byte & bit) ? color : bg_color;

    //         SPI_SendData(SPI2, (pixel_color >> 8) & 0xff);
    //         while (SPI_GetFlagStatus(SPI2, SPI_FLAG_TXE) == RESET);
    //         SPI_SendData(SPI2, pixel_color & 0xff);
    //         while (SPI_GetFlagStatus(SPI2, SPI_FLAG_TXE) == RESET);
    //     }
    // }
}

static const uint8_t *ascii_get_model(const char ch, const font_t *font)
{
    if (font == NULL)
        return NULL;

    uint16_t bytes_per_row = (font->size / 2 + 7) / 8;
    uint16_t bytes_per_char = font->size * bytes_per_row;

    if (font->ascii_map)
    {
        const char *map = font->ascii_map;
        do
        {
            if (*map == ch)
            {
                uint16_t index = map - font->ascii_map;
                return font->ascii_model + index * bytes_per_char;
            }
        } while (*(++map) != '\0');
    }
    else
    {
        if (ch >= 32 && ch <= 126)
        {
            uint16_t index = ch - 32; //或者写成ch-' '(空格)
            return font->ascii_model + index * bytes_per_char;
        }
    }

    return NULL;
}

static void st7789_write_ascii(uint16_t x, uint16_t y, char ch, uint16_t color, uint16_t bg_color, const font_t *font)
{
    if (font == NULL)
        return;

    if (ch < 32 || ch > 126)
        return;

    uint16_t fheight = font->size, fwidth = font->size / 2;
    if (!in_screen_bound(x, y, x + fwidth - 1, y + fheight - 1))
        return;

	const uint8_t *model = ascii_get_model(ch, font);
    if (model)
	    st7789_draw_font(x, y, fwidth, fheight, model, color, bg_color);
}

static void st7789_write_chinese(uint16_t x, uint16_t y, const char *ch, uint16_t color, uint16_t bg_color, const font_t *font)
{
    if (ch == NULL || font == NULL)
        return;
    uint16_t fheight = font->size, fwidth = font->size;
    if (!in_screen_bound(x, y, x + fwidth - 1, y + fheight - 1))
        return;

    const font_chinese_t *chinese_font = font->chinese;
    while (chinese_font->name != NULL)
    {
        if (strcmp(chinese_font->name, ch) == 0)
        {
            st7789_draw_font(x, y, fwidth, fheight, chinese_font->model, color, bg_color);
            return;
        }
        chinese_font++;
    }

    /* 鱼佬写法 */
    // const font_chinese_t *c = font->chinese;
    // for (; c->name != NULL; c++)
    // {
    //     if (strcmp(c->name, ch) == 0)
    //         break;
    // }
    // if (c->name == NULL)
    //     return;

    // st7789_draw_font(x, y, fwidth, fheight, c->model, color, bg_color);
}

/* 判断是否是GB2312字符 */
static bool is_gb2312(char ch)
{
    return ((unsigned char)ch >= 0xA1 && (unsigned char)ch <= 0xF7);
}

/* 判断是否是UTF-8字符 */
//static int utf8_char_length(const char *str)
//{
//    if ((*str & 0x80) == 0) return 1; // 1 byte
//    if ((*str & 0xE0) == 0xC0) return 2; // 2 bytes
//    if ((*str & 0xF0) == 0xE0) return 3; // 3 bytes
//    if ((*str & 0xF8) == 0xF0) return 4; // 4 bytes
//    return -1; // Invalid UTF-8
//}

void st7789_write_string(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bg_color, const font_t *font)
{
    while (*str)
    {
        // int len = utf8_char_length(*str);
        int len = is_gb2312(*str) ? 2 : 1;
        if (len <= 0)
        {
            str++;
            continue;
        }
        else if (len == 1)
        {
            st7789_write_ascii(x, y, *str, color, bg_color, font);
            str++;
            x += font->size / 2;
        }
        else
        {
            char ch[5];
            strncpy(ch, str, len);
            st7789_write_chinese(x, y, ch, color, bg_color, font);
            str += len;
            x += font->size;
        }
    }
}

void st7789_draw_image(uint16_t x, uint16_t y, const image_t *image)
{
    if (image == NULL)
        return;

    uint16_t img_width = image->width;
    uint16_t img_height = image->height;

    if (!in_screen_bound(x, y, x + img_width - 1, y + img_height - 1))
        return;

    st7789_set_range(x, y, x + img_width - 1, y + img_height - 1);
    st7789_set_gram_mode();
    st7789_write_gram((uint8_t *)image->data, img_width * img_height * 2, false);
}

void st7789_init(void)
{
    write_gram_semaphore = xSemaphoreCreateBinary();
    configASSERT(write_gram_semaphore);
    ST7789_spi_init();
    st7789_dma_init();
    st7789_interrupt_init();
    ST7789_io_init();

    st7789_init_display();
}

void DMA1_Stream4_IRQHandler(void)
{
    if (DMA_GetITStatus(DMA1_Stream4, DMA_IT_TCIF4) != RESET)
    {
        BaseType_t pxHigherPriorityTaskWoken;
        xSemaphoreGiveFromISR(write_gram_semaphore, &pxHigherPriorityTaskWoken);   // 释放信号量，通知写入完成
        portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);  // 如果需要切换任务，则进行切换

        DMA_ClearITPendingBit(DMA1_Stream4, DMA_IT_TCIF4);
    }
}
