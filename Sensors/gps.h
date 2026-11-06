#ifndef GPS_H
#define GPS_H

#include "main.h"
#include <stdint.h>

/* ATGM336H-6N-74 an USART3: PB10 TX, PB11 RX.
 * Ab Werk 115200 Baud, 8N1, NMEA, 1 Hz (Datenblatt). Die Daten fliessen nicht
 * in die Regelung ein. */
#define GPS_BAUD 115200

typedef struct
{
    uint8_t fix;      /* 1 = RMC meldet eine gueltige Position */
    uint8_t sats;     /* verwendete Satelliten, aus GGA */
    double lat_deg;   /* Nord positiv */
    double lon_deg;   /* Ost positiv */
    float alt_m;      /* Hoehe ueber dem Meeresspiegel, aus GGA */
    float hdop;
    float speed_ms;   /* Geschwindigkeit ueber Grund, aus RMC */
    float course_deg;
} GPS_Data_t;

void GPS_Init(UART_HandleTypeDef *huart);

/* Aus dem UART-Empfangs- und Fehler-Callback (command.c) */
void GPS_RxByte(void);
void GPS_RxError(void);

/* Aus dem Leerlauf der Hauptschleife aufrufen: wertet empfangene Zeichen aus */
void GPS_Process(void);

const GPS_Data_t *GPS_Get(void);

/* Ausgabe fuer den Befehl 'sens' */
void GPS_Print(void);

#endif
