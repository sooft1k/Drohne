#include "command.h"
#include "motor.h"
#include "control.h"
#include "mixer.h"
#include "mpu6050.h"
#include "battery.h"
#include "crsf.h"
#include "rc.h"
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
static volatile uint8_t kipp_on = 0;

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

uint8_t Command_KippOn(void)
{
    return kipp_on;
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
    if (strcmp(line, "arm") == 0 && CRSF_LinkUp())
    {
        /* Mit Funk entscheidet der Arm-Schalter - ein Terminal-arm wuerde
         * im naechsten Zyklus ohnehin wieder aufgehoben. */
        printf(">> Funk ist verbunden - bitte mit dem Arm-Schalter armen.\r\n");
    }
    else if (strcmp(line, "arm") == 0)
    {
        setpoint.throttle = 0.0f; /* ein vorher gesetztes 't' nicht sofort anlegen */
        Control_Reset();          /* Integratoren loeschen vor dem Scharfschalten */
        Motor_Arm();
        printf(">> ARMED\r\n");
    }
    else if (strcmp(line, "disarm") == 0 || strcmp(line, "stop") == 0)
    {
        Motor_Disarm();
        setpoint.throttle = 0.0f;
        RC_BlockArm(); /* sonst armt der noch oben stehende Arm-Schalter sofort wieder */
        printf(">> DISARMED - alle Motoren gestoppt.\r\n");
    }

    /* ---------- Status ---------- */
    else if (strcmp(line, "status") == 0)
    {
        printf(">> Status: %s | Modus: %s | Gas: %.0f%% | Funk: %s\r\n",
               Motor_IsArmed() ? "ARMED" : "DISARMED",
               (flight_mode == MODE_ANGLE) ? "ANGLE" : "RATE",
               setpoint.throttle,
               CRSF_LinkUp() ? "OK" : "KEIN SIGNAL");
        printf("   Stream: %s | Loop: %lu Hz | Motoren: %lu Hz\r\n",
               stream_on ? "AN" : "AUS", loop_hz, Motor_UpdateRateHz());
    }
    else if (strcmp(line, "loop") == 0)
    {
        printf(">> Regeltakt: %lu Hz | verpasste Zyklen: %lu\r\n",
               loop_hz, loop_missed);
    }

    /* ---------- Funkempfang ---------- */
    else if (strcmp(line, "rc") == 0)
    {
        RC_PrintStatus();
    }

    /* ---------- Akku ---------- */
    else if (strcmp(line, "bat") == 0)
    {
        if (Battery_Cells() == 0)
        {
            printf(">> Akku: kein Akku erkannt (%.2f V am Teiler)\r\n", Battery_Volt());
        }
        else
        {
            printf(">> Akku: %.2f V | %uS | %.2f V pro Zelle | %s\r\n",
                   Battery_Volt(), Battery_Cells(),
                   Battery_CellVolt(), Battery_StateText());
        }
    }
    else if (strncmp(line, "batcal ", 7) == 0)
    {
        float v = strtof(line + 7, 0);
        Battery_Calibrate(v);
        printf(">> Kalibriert auf %.2f V (Faktor %.4f)\r\n", Battery_Volt(), Battery_GetScale());
    }

    /* ---------- Filter ----------
     * Muss VOR dem PID-Block stehen, sonst wird 'alpf' als Angle-Befehl gelesen. */
    else if (strcmp(line, "filt") == 0)
    {
        printf(">> Gyro-Tiefpass : %.0f Hz\r\n", MPU6050_GetGyroLPF());
        printf(">> Accel-Tiefpass: %.0f Hz\r\n", MPU6050_GetAccelLPF());
        printf(">> Komplementaer : tau %.2f s\r\n", MPU6050_GetCompTau());
    }
    else if (strncmp(line, "glpf ", 5) == 0)
    {
        float v = strtof(line + 5, 0);
        MPU6050_SetGyroLPF(v);
        printf(">> Gyro-Tiefpass: %.0f Hz%s\r\n", MPU6050_GetGyroLPF(),
               (v <= 0.0f) ? " (aus)" : "");
    }
    else if (strncmp(line, "alpf ", 5) == 0)
    {
        float v = strtof(line + 5, 0);
        MPU6050_SetAccelLPF(v);
        printf(">> Accel-Tiefpass: %.0f Hz\r\n", MPU6050_GetAccelLPF());
    }
    else if (strncmp(line, "tau ", 4) == 0)
    {
        float v = strtof(line + 4, 0);
        MPU6050_SetCompTau(v);
        printf(">> Komplementaerfilter tau: %.2f s\r\n", MPU6050_GetCompTau());
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
        kipp_on = 0;
        printf(">> Stream AN (50 Hz Ausgabe)\r\n");
    }
    else if (strcmp(line, "stream off") == 0)
    {
        stream_on = 0;
        printf(">> Stream AUS\r\n");
    }

    /* ---------- Kipptest ohne Propeller ----------
     * Zeigt 5x pro Sekunde, was der Mixer bei 50 % Gas aus der aktuellen
     * Lage machen wuerde. Nur disarmed, die Motoren bleiben aus. */
    else if (strcmp(line, "kipp") == 0 && Motor_IsArmed())
    {
        printf(">> Kipptest nur disarmed - erst 'disarm'.\r\n");
    }
    else if (strcmp(line, "kipp") == 0)
    {
        kipp_on = !kipp_on;
        if (kipp_on)
        {
            stream_on = 0;
            printf(">> Kipptest AN (Vorschau bei 50 %% Gas, Motoren bleiben aus).\r\n");
            printf(">> Nochmal 'kipp' zum Beenden.\r\n");
        }
        else
        {
            printf(">> Kipptest AUS\r\n");
        }
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
            if (Control_SetGain(line[0], line[1], line[2], v))
                printf(">> %c%c%c = %.4f\r\n", line[0], line[1], line[2], v);
            else
                printf(">> Ungueltig (Yaw hat keinen Winkelregler, Wert 0..1000)\r\n");
        }
        else
        {
            printf(">> Format: rrp 1.2  (r/a Kreis, r/p/y Achse, p/i/d Term)\r\n");
        }
    }

    /* ---------- Gas setzen: t 20 ----------
     * Nur zum Testen ohne Funke. Sobald die Funke verbunden ist,
     * ueberschreibt RC_Update den Wert im naechsten Zyklus. */
    else if (line[0] == 't' && line[1] == ' ')
    {
        char *end;
        float v = strtof(line + 2, &end);

        if (end != line + 2)
        {
            if (!(v >= 0.0f)) /* auch NaN */
                v = 0.0f;
            if (v > 100.0f)
                v = 100.0f;
            setpoint.throttle = v;
            printf(">> Gas: %.0f%%\r\n", v);
            if (CRSF_LinkUp())
                printf(">> Achtung: Funk ist verbunden, der Knueppel ueberschreibt das sofort.\r\n");
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

    /* Die frueheren Motortests 'm1 20' und nackte Zahlen sind entfallen:
     * Solange gearmt ist, schreibt der Mixer jede Millisekunde alle vier
     * Motoren neu, ein Einzelwert hielt nicht einmal eine Millisekunde. */
    else
    {
        printf(">> Unbekannt: '%s'\r\n", line);
        printf(">> arm | disarm | status | loop | pid | mix | filt | bat | rc |\r\n");
        printf(">> angle | rate | stream on/off | kipp | t <gas> |\r\n");
        printf(">> rrp/arp <wert> | glpf <hz> | alpf <hz> | tau <sek> | batcal <volt>\r\n");
    }

    line_len = 0;
    line_ready = 0;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
        Command_RxByte();
    else if (huart->Instance == USART1)
        CRSF_RxByte();
}