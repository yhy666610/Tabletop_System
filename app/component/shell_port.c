#include "FreeRTOS.h"
#include "task.h"
#include "shell.h"
#include "usart.h"

static Shell shell;
static char shell_buffer[512];

static signed short shell_write(char *buffer, unsigned short size)
{
    usart_write(buffer, size);
    return size;
}

static signed short shell_read(char *data, unsigned short len)
{
    return usart_read(data, len);
}

void shell_init(void)
{
    shell.read = shell_read;
    shell.write = shell_write;
    shellInit(&shell, shell_buffer, 512);

    xTaskCreate(shellTask, "Shell", 512, &shell, tskIDLE_PRIORITY + 1, NULL);
}
