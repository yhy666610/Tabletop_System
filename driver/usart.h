#ifndef __USART_H__
#define __USART_H__

#include <stdbool.h>
#include "stm32f4xx.h"
void usart_write(const char str[], uint32_t size);
signed short usart_read(char *data, unsigned short len);
signed short usart_try_read(char *data);
void usart_init(void);

#endif /* __USART_H__ */
