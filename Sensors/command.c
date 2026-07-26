#include "command.h"
#include "motor.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define CMD_BUF_SIZE 32

static UART_HandleTypeDef *cmd_uart = 0;
static uint8_t rx_char;
static char line[CMD_BUF_SIZE];
static uint8_t line_len = 0;
static volatile uint8_t line_ready = 0;
static volatile uint8_t stream_on = 0;

void Command_Init(UART_HandleTypeDef *huart)
{
    cmd_uart = huart;
    line_len = 0;
    line_ready = 0;
    HAL_UART_Receive_IT(cmd_uart, &rx_char, 1);
}

uint8_t Command_StreamOn(void)
{
    return stream_on;
}

void Command_RxByte(void)
{
    char c = (char)rx_char;

    if (c == '\r' || c == '\n')
    {
        if (line_len > 0)
        {
            line[line_len] = '\0';
            line_ready = 1;
        }
    }
    else if (line_len < (CMD_BUF_SIZE - 1))
    {
        line[line_len++] = c;
    }

    HAL_UART_Receive_IT(cmd_uart, &rx_char, 1);
}

void Command_Process(void)
{
    if (!line_ready)
        return;

    if (strcmp(line, "arm") == 0)
    {
        Motor_Arm();
        printf(">> ARMED - Speed mit 0-100 (alle) oder m1..m4 <wert>\r\n");
    }
    else if (strcmp(line, "disarm") == 0 || strcmp(line, "stop") == 0)
    {
        Motor_Disarm();
        printf(">> DISARMED - alle Motoren gestoppt.\r\n");
    }
    else if (strcmp(line, "status") == 0)
    {
        printf(">> Status: %s | Stream: %s\r\n",
               Motor_IsArmed() ? "ARMED" : "DISARMED",
               stream_on ? "AN" : "AUS");
    }
    else if (strcmp(line, "stream on") == 0)
    {
        stream_on = 1;
        printf(">> Stream AN\r\n");
    }
    else if (strcmp(line, "stream off") == 0)
    {
        stream_on = 0;
        printf(">> Stream AUS\r\n");
    }
    /* Einzelmotor:  m1 20  /  m3 15 */
    else if (line[0] == 'm' && line[1] >= '1' && line[1] <= '4')
    {
        uint8_t motor = (uint8_t)(line[1] - '0');
        char *end;
        long val = strtol(line + 2, &end, 10);

        if (end != line + 2 && *end == '\0')
        {
            if (val < 0)
                val = 0;
            if (val > 100)
                val = 100;

            if (Motor_IsArmed())
            {
                Motor_SetPercent(motor, (float)val);
                printf(">> Motor %u: %ld%%\r\n", motor, val);
            }
            else
            {
                printf(">> Erst 'arm' senden!\r\n");
            }
        }
        else
        {
            printf(">> Format: m1 20\r\n");
        }
    }
    else
    {
        /* Nackte Zahl -> alle vier Motoren */
        char *end;
        long val = strtol(line, &end, 10);
        if (end != line && *end == '\0')
        {
            if (val < 0)
                val = 0;
            if (val > 100)
                val = 100;

            if (Motor_IsArmed())
            {
                Motor_SetAllPercent((float)val);
                printf(">> Alle Motoren: %ld%%\r\n", val);
            }
            else
            {
                printf(">> Erst 'arm' senden!\r\n");
            }
        }
        else
        {
            printf(">> Unbekannt: '%s'\r\n", line);
            printf(">> arm | disarm | status | stream on/off | 0-100 | m1..m4 <wert>\r\n");
        }
    }

    line_len = 0;
    line_ready = 0;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
        Command_RxByte();
}