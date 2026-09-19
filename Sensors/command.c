#include "command.h"
#include "motor.h"
#include "control.h"
#include "mixer.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define CMD_BUF_SIZE 32

/* aus main.c */
extern uint32_t loop_hz;
extern volatile uint32_t loop_missed;

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

    /* ---------- Motorsteuerung ---------- */
    if (strcmp(line, "arm") == 0)
    {
        Control_Reset(); /* Integratoren loeschen vor dem Scharfschalten */
        Motor_Arm();
        printf(">> ARMED\r\n");
    }
    else if (strcmp(line, "disarm") == 0 || strcmp(line, "stop") == 0)
    {
        Motor_Disarm();
        setpoint.throttle = 0.0f;
        printf(">> DISARMED - alle Motoren gestoppt.\r\n");
    }

    /* ---------- Status ---------- */
    else if (strcmp(line, "status") == 0)
    {
        printf(">> Status: %s | Modus: %s | Gas: %.0f%% | Stream: %s | Loop: %lu Hz\r\n",
               Motor_IsArmed() ? "ARMED" : "DISARMED",
               (flight_mode == MODE_ANGLE) ? "ANGLE" : "RATE",
               setpoint.throttle,
               stream_on ? "AN" : "AUS",
               loop_hz);
    }
    else if (strcmp(line, "loop") == 0)
    {
        printf(">> Regeltakt: %lu Hz | verpasste Zyklen: %lu\r\n",
               loop_hz, loop_missed);
    }

    /* ---------- Flugmodus ---------- */
    else if (strcmp(line, "angle") == 0)
    {
        Control_SetMode(MODE_ANGLE);
        printf(">> Modus: ANGLE (richtet sich selbst auf)\r\n");
    }
    else if (strcmp(line, "rate") == 0)
    {
        Control_SetMode(MODE_RATE);
        printf(">> Modus: RATE (Acro)\r\n");
    }

    /* ---------- PID ---------- */
    else if (strcmp(line, "pid") == 0)
    {
        Control_PrintGains();
    }

    /* ---------- Stream ---------- */
    else if (strcmp(line, "stream on") == 0)
    {
        stream_on = 1;
        printf(">> Stream AN (50 Hz Ausgabe)\r\n");
    }
    else if (strcmp(line, "stream off") == 0)
    {
        stream_on = 0;
        printf(">> Stream AUS\r\n");
    }

    /* ---------- PID-Werte setzen: rrp 1.2 / arp 4.0 ----------
     * Zeichen 1: r = Rate (innen), a = Angle (aussen)
     * Zeichen 2: r/p/y = Achse
     * Zeichen 3: p/i/d = Term */
    else if ((line[0] == 'r' || line[0] == 'a') &&
             (line[1] == 'r' || line[1] == 'p' || line[1] == 'y') &&
             (line[2] == 'p' || line[2] == 'i' || line[2] == 'd'))
    {
        char *end;
        float v = strtof(line + 3, &end);

        if (end != line + 3)
        {
            Control_SetGain(line[0], line[1], line[2], v);
            printf(">> %c%c%c = %.4f\r\n", line[0], line[1], line[2], v);
        }
        else
        {
            printf(">> Format: rrp 1.2  (r/a Kreis, r/p/y Achse, p/i/d Term)\r\n");
        }
    }

    /* ---------- Gas setzen: t 20 ----------
     * Speist den Mixer, nicht die Motoren direkt. */
    else if (line[0] == 't' && line[1] == ' ')
    {
        char *end;
        float v = strtof(line + 2, &end);

        if (end != line + 2)
        {
            if (v < 0.0f)
                v = 0.0f;
            if (v > 100.0f)
                v = 100.0f;
            setpoint.throttle = v;
            printf(">> Gas: %.0f%%\r\n", v);
        }
        else
        {
            printf(">> Format: t 20\r\n");
        }
    }

    /* ---------- Mixer-Ausgaben anzeigen ---------- */
    else if (strcmp(line, "mix") == 0)
    {
        float m1, m2, m3, m4;
        Mixer_GetOutputs(&m1, &m2, &m3, &m4);
        printf(">> M1=%.1f  M2=%.1f  M3=%.1f  M4=%.1f  (Gas %.0f%%)\r\n",
               m1, m2, m3, m4, setpoint.throttle);
    }

    /* ---------- Einzelmotor: m1 20 ----------
     * Wird vom Mixer im naechsten Zyklus ueberschrieben, solange Gas > 0.
     * Fuer Einzeltests vorher 't 0' setzen. */
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

    /* ---------- Nackte Zahl: alle vier Motoren ---------- */
    else
    {
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
            printf(">> arm | disarm | status | loop | pid | mix | angle | rate |\r\n");
            printf(">> stream on/off | t <gas> | 0-100 | m1..m4 <wert> | rrp/arp <wert>\r\n");
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