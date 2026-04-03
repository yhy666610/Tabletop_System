#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "shell.h"

#define OS_INFO_BUF_SIZE 1024

/**
 * @brief Show FreeRTOS task list and runtime statistics.
 *
 * Requires the following macros to be enabled in FreeRTOSConfig.h:
 *   configUSE_TRACE_FACILITY            1
 *   configUSE_STATS_FORMATTING_FUNCTIONS 1
 *   configGENERATE_RUN_TIME_STATS       1
 */
int cmd_os_info(int argc, char *argv[])
{
    static char buf[OS_INFO_BUF_SIZE];
    Shell *shell = shellGetCurrent();

    shellPrint(shell, "=== Task List ===\r\n");
    shellPrint(shell, "Name          State    Prio     HWM   Num\r\n"); // HWM代表最高水位（字节）
    vTaskList(buf);
    shellWriteString(shell, buf);
    shellWriteString(shell, "\r\n");

    shellPrint(shell, "\r\n=== Runtime Stats ===\r\n");
    shellPrint(shell, "Name            Abs Time    %% Time\r\n");
    vTaskGetRunTimeStats(buf);
    shellWriteString(shell, buf);
    shellWriteString(shell, "\r\n");

    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC),
                 os_info, cmd_os_info, Show FreeRTOS task list and runtime stats);
