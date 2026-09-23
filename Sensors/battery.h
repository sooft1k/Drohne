#ifndef BATTERY_H
#define BATTERY_H

#include "main.h"
#include <stdint.h>

/* Spannungsteiler an PC0 (ADC1_IN10):
 *   Akku+ --[R_OBEN]--+--[R_UNTEN]-- GND
 *                     |
 *                    PC0
 * Bei 10k/1k kommt 1/11 der Akkuspannung am Pin an:
 * 25,2 V -> 2,29 V. Sicher unter den 3,3 V des Pins. */
#define BAT_R_OBEN   10000.0f
#define BAT_R_UNTEN   1000.0f
#define BAT_VREF         3.30f   /* Versorgungsspannung des STM32 */

/* Zellenzahl. 0 = automatisch beim Anstecken erkennen. */
#define BAT_CELLS_DEFAULT 0

/* Warnschwellen pro Zelle */
#define BAT_WARN_V   3.50f   /* landen */
#define BAT_CRIT_V   3.30f   /* sofort landen, darunter nimmt der Akku Schaden */

typedef enum
{
    BAT_OK = 0,
    BAT_WARN,
    BAT_CRITICAL,
    BAT_NONE          /* kein Akku angesteckt (Betrieb nur ueber USB) */
} BatteryState_t;

void  Battery_Init(void);

/* Einmal pro Regelzyklus aufrufen. Misst nicht jedes Mal,
 * sondern intern gedrosselt - kostet fast keine Zeit. */
void  Battery_Update(float dt);

float Battery_Volt(void);         /* Gesamtspannung */
float Battery_CellVolt(void);     /* Spannung pro Zelle */
uint8_t Battery_Cells(void);      /* erkannte Zellenzahl, 0 = keiner */
BatteryState_t Battery_State(void);
const char *Battery_StateText(void);

/* Kalibrieren, falls die Widerstaende nicht exakt sind:
 * echte Spannung am Akku messen und hier eintragen. */
void  Battery_Calibrate(float real_volt);
float Battery_GetScale(void);

#endif /* BATTERY_H */