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

/* Zellenzahl. 0 = automatisch beim Anstecken erkennen.
 * Die Erkennung ist bei teilentladenen 5S/6S-Akkus nicht eindeutig
 * (ein 6S mit 3,5 V/Zelle sieht aus wie ein voller 5S) - wer immer
 * denselben Akkutyp fliegt, traegt die Zellenzahl hier fest ein. */
#define BAT_CELLS_DEFAULT 0

/* Warnschwellen pro Zelle */
#define BAT_WARN_V   3.50f   /* landen */
#define BAT_CRIT_V   3.30f   /* sofort landen, darunter nimmt der Akku Schaden */

/* Im Flug faellt die Spannung bei Vollgas kurz ab. Erst wenn sie so lange
 * unter einer Schwelle bleibt, gilt der Zustand - und er wird bis zum
 * Disarm nicht wieder besser (sonst wechselt er mit jedem Gasstoss). */
#define BAT_DEBOUNCE_S 2.0f

/* Gasbegrenzung bei niedrigem Akku, in % Knueppelweg. Die Grenze sinkt
 * langsam (BAT_THR_SLEW pro Sekunde), damit die Drohne nicht absackt. */
#define BAT_THR_LIMIT_WARN 80.0f
#define BAT_THR_LIMIT_CRIT 60.0f
#define BAT_THR_SLEW       20.0f

/* Strommessung: Stromausgang "C" des ESC an PA4 (ADC2_IN4).
 * Skalierung in mV pro Ampere. Fuer den B-Cube-4in1 gibt es keinen
 * gesicherten Wert - 12,75 mV/A nennt B-Cube fuer seinen AM32-4in1, ueblich
 * sind 12..40 mV/A. Mit 'curcal <ampere>' kalibrieren (siehe README).
 * Der Nullpunkt wird beim Anstecken des Akkus automatisch gemessen
 * (disarmed, Motoren stehen) oder mit 'curzero'.
 * Achtung: PA4 vertraegt hoechstens 3,3 V - am Stromausgang vorher messen. */
#define CURR_MV_PER_A_DEFAULT 12.75f

typedef enum
{
    BAT_OK = 0,
    BAT_WARN,
    BAT_CRITICAL,
    BAT_NONE          /* kein Akku angesteckt (Betrieb nur ueber USB) */
} BatteryState_t;

void  Battery_Init(void);

/* Einmal pro Regelzyklus aufrufen. Misst nicht jedes Mal,
 * sondern intern gedrosselt (100 Hz) - kostet fast keine Zeit. */
void  Battery_Update(float dt);

float Battery_Volt(void);         /* Gesamtspannung */
float Battery_CellVolt(void);     /* Spannung pro Zelle */
uint8_t Battery_Cells(void);      /* erkannte Zellenzahl, 0 = keiner */
BatteryState_t Battery_State(void);
const char *Battery_StateText(void);

float Battery_Current(void);      /* Strom in A, gefiltert */
float Battery_MahUsed(void);      /* verbrauchte Kapazitaet seit dem Anstecken */
uint8_t Battery_Percent(void);    /* grobe Restladung aus der Zellspannung */

/* Begrenzt den Gas-Sollwert je nach Akkuzustand (siehe BAT_THR_LIMIT_*) */
float Battery_LimitThrottle(float throttle);
float Battery_ThrottleCap(void);

/* Kalibrieren, falls die Widerstaende nicht exakt sind:
 * echte Spannung am Akku messen und hier eintragen. */
void  Battery_Calibrate(float real_volt);
float Battery_GetScale(void);

/* Strom: Nullpunkt jetzt setzen (Motoren aus) bzw. mit bekanntem Strom
 * die Skalierung bestimmen. Rueckgabe 1 = uebernommen. */
uint8_t Battery_CurrentZero(void);
uint8_t Battery_CurrentCalibrate(float real_amps);
float Battery_CurrentMvPerA(void);
float Battery_CurrentZeroMv(void);

#endif /* BATTERY_H */
