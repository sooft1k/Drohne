#include "gps.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Bei 115200 Baud kommen 11,5 Zeichen pro Millisekunde. Der Puffer reicht also
 * fuer gut 20 ms, falls der Leerlauf solange nicht drankommt. */
#define RX_BUF_SIZE 256
/* NMEA-Zeilen sind hoechstens 82 Zeichen lang */
#define LINE_MAX 96
/* Zeichen pro Aufruf, damit der Leerlauf den Regeltakt nicht aufhaelt */
#define MAX_BYTES_PER_CALL 64
/* Felder einer Zeile, RMC und GGA brauchen weniger */
#define MAX_FIELDS 16

#define KNOTS_TO_MS 0.514444f

static UART_HandleTypeDef *gps_uart = 0;
static uint8_t rx_byte;
static uint8_t rx_buf[RX_BUF_SIZE];
static volatile uint16_t rx_head = 0; /* schreibt der Interrupt */
static uint16_t rx_tail = 0;          /* liest GPS_Process */
static volatile uint32_t rx_lost = 0;

static char line[LINE_MAX];
static uint8_t line_len = 0;

static GPS_Data_t data;
static uint32_t lines_ok = 0;
static uint32_t lines_bad = 0;
static uint32_t last_ok_ms = 0;

void GPS_Init(UART_HandleTypeDef *huart)
{
    gps_uart = huart;
    HAL_UART_Receive_IT(gps_uart, &rx_byte, 1);
}

void GPS_RxByte(void)
{
    const uint16_t next = (uint16_t)((rx_head + 1U) % RX_BUF_SIZE);
    if (next != rx_tail)
    {
        rx_buf[rx_head] = rx_byte;
        rx_head = next;
    }
    else
    {
        rx_lost++;
    }
    HAL_UART_Receive_IT(gps_uart, &rx_byte, 1);
}

/* Bei Ueberlauf (ORE) beendet die HAL den Empfang - neu starten */
void GPS_RxError(void)
{
    if (!gps_uart)
        return;
    __HAL_UART_CLEAR_OREFLAG(gps_uart);
    __HAL_UART_CLEAR_NEFLAG(gps_uart);
    __HAL_UART_CLEAR_FEFLAG(gps_uart);
    HAL_UART_Receive_IT(gps_uart, &rx_byte, 1);
}

static int hex_val(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

/* Prueft die XOR-Pruefsumme zwischen '$' und '*' und schneidet sie ab */
static uint8_t checksum_ok(char *s)
{
    if (s[0] != '$')
        return 0;
    uint8_t sum = 0;
    char *p = s + 1;
    while (*p && *p != '*')
        sum ^= (uint8_t)*p++;
    if (*p != '*')
        return 0;
    const int hi = hex_val(p[1]);
    const int lo = (hi >= 0) ? hex_val(p[2]) : -1;
    if (hi < 0 || lo < 0 || sum != (uint8_t)((hi << 4) | lo))
        return 0;
    *p = '\0';
    return 1;
}

/* Trennt an den Kommas. strtok ginge nicht, das ueberspringt leere Felder. */
static int split(char *s, char **f)
{
    int n = 0;
    f[n++] = s;
    while (*s && n < MAX_FIELDS)
    {
        if (*s == ',')
        {
            *s = '\0';
            f[n++] = s + 1;
        }
        s++;
    }
    return n;
}

/* NMEA: (d)ddmm.mmmm -> Grad. hemi ist N/E (positiv) oder S/W (negativ). */
static double to_degrees(const char *v, const char *hemi)
{
    const double raw = strtod(v, 0);
    const int deg = (int)(raw / 100.0);
    const double res = (double)deg + (raw - (double)deg * 100.0) / 60.0;
    return (hemi[0] == 'S' || hemi[0] == 'W') ? -res : res;
}

/* $xxRMC,zeit,status,lat,N,lon,E,knoten,kurs,datum,... */
static void parse_rmc(char **f, int n)
{
    if (n < 9)
        return;
    data.fix = (f[2][0] == 'A') ? 1 : 0;
    if (!data.fix || f[3][0] == '\0' || f[5][0] == '\0')
        return;
    data.lat_deg = to_degrees(f[3], f[4]);
    data.lon_deg = to_degrees(f[5], f[6]);
    data.speed_ms = strtof(f[7], 0) * KNOTS_TO_MS;
    data.course_deg = strtof(f[8], 0);
}

/* $xxGGA,zeit,lat,N,lon,E,qualitaet,sats,hdop,hoehe,M,... */
static void parse_gga(char **f, int n)
{
    if (n < 10)
        return;
    data.sats = (uint8_t)atoi(f[7]);
    if (f[6][0] == '0' || f[6][0] == '\0')
        return;
    data.hdop = strtof(f[8], 0);
    data.alt_m = strtof(f[9], 0);
}

static void handle_line(void)
{
    char *f[MAX_FIELDS];

    if (!checksum_ok(line))
    {
        lines_bad++;
        return;
    }
    lines_ok++;
    last_ok_ms = HAL_GetTick();

    const int n = split(line, f);
    /* $ und zwei Zeichen Kennung (GN, GP, BD, ...), danach der Satztyp */
    if (strlen(f[0]) != 6)
        return;
    if (strcmp(f[0] + 3, "RMC") == 0)
        parse_rmc(f, n);
    else if (strcmp(f[0] + 3, "GGA") == 0)
        parse_gga(f, n);
}

void GPS_Process(void)
{
    for (int i = 0; i < MAX_BYTES_PER_CALL && rx_tail != rx_head; i++)
    {
        const char c = (char)rx_buf[rx_tail];
        rx_tail = (uint16_t)((rx_tail + 1U) % RX_BUF_SIZE);

        if (c == '$')
            line_len = 0; /* neue Zeile, auch wenn die alte abgerissen ist */

        if (c == '\r' || c == '\n')
        {
            if (line_len > 0)
            {
                line[line_len] = '\0';
                handle_line();
                line_len = 0;
            }
        }
        else if (line_len < LINE_MAX - 1)
        {
            line[line_len++] = c;
        }
        else
        {
            line_len = 0; /* zu lang, kann kein NMEA sein */
            lines_bad++;
        }
    }
}

const GPS_Data_t *GPS_Get(void)
{
    return &data;
}

void GPS_Print(void)
{
    if (lines_ok == 0)
    {
        printf(">> GPS: keine Daten | fehlerhafte Zeilen %lu\r\n", lines_bad);
        return;
    }
    printf(">> GPS: %s | Sats %u | letzte Zeile vor %lu ms | fehlerhaft %lu | verloren %lu\r\n",
           data.fix ? "FIX" : "kein Fix", data.sats, HAL_GetTick() - last_ok_ms,
           lines_bad, rx_lost);
    if (data.fix)
        printf("   %.6f %.6f | %.1f m | HDOP %.1f | %.1f m/s | Kurs %.0f\r\n",
               data.lat_deg, data.lon_deg, data.alt_m, data.hdop,
               data.speed_ms, data.course_deg);
}
