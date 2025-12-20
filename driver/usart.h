#ifndef __USART_H__
#define __USART_H__

#include <stdbool.h>
#include "stm32f4xx.h"
void usart_write(const char str[]);
void usart_init(void);

#endif /* __USART_H__ */
