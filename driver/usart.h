#ifndef __USART_H__
#define __USART_H__

#include <stdbool.h>
#include "stm32f4xx.h"
void usart_write(USART_TypeDef* USARTx, char str[]);
void usart_init(void);

#endif /* __USART_H__ */
