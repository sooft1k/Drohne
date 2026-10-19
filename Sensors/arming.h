#ifndef ARMING_H
#define ARMING_H

#include <stdint.h>

/* Sicherheitsabfragen vor dem Scharfschalten.
 * Gilt fuer beide Wege: Arm-Schalter an der Funke und 'arm' im Terminal. */

/* Groesste erlaubte Neigung beim Armen, Grad (Roll und Pitch einzeln) */
#define ARM_MAX_TILT_DEG 25.0f

typedef enum
{
    ARM_OK = 0,
    ARM_KEIN_SENSOR, /* MPU6050 fehlt, ausgefallen oder liefert noch nicht stabil */
    ARM_NEIGUNG,     /* Drohne steht zu schief */
    ARM_KEIN_AKKU,   /* keine Akkuspannung erkannt */
    ARM_AKKU_LEER,   /* Akku unter der Warnschwelle */
    ARM_GAS,         /* Gasknueppel nicht unten (nur Funk) */
    ARM_SCHON_ARMED
} ArmCheck_t;

/* Prueft alle Bedingungen, armt aber nicht. */
ArmCheck_t Arming_Check(void);

/* Prueft und armt bei Erfolg (PIDs loeschen, Gas auf 0, Motor_Arm).
 * Bei Misserfolg wird der Grund fuer die naechste Terminal-Ausgabe gemerkt. */
ArmCheck_t Arming_Arm(void);

/* Motoren aus, Gas-Sollwert auf 0, Arm-Schalter muss erst wieder runter. */
void Arming_Disarm(void);

/* Ablehnungsgrund von aussen melden (z.B. Gas nicht unten), wird wie bei
 * Arming_Arm einmal im Terminal ausgegeben. */
void Arming_Reject(ArmCheck_t r);

const char *Arming_Text(ArmCheck_t r);

/* Gibt einen gemerkten Ablehnungsgrund einmal aus. Aus der Leerlaufzeit
 * aufrufen, nicht aus dem Regelzyklus. */
void Arming_PrintPending(void);

#endif /* ARMING_H */
