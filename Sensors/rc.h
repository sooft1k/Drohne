#ifndef RC_H
#define RC_H

#include <stdint.h>

/* Kanalbelegung, AETR - so sendet ELRS standardmaessig.
 * Kanal 1 Roll, 2 Pitch, 3 Gas, 4 Gieren, ab 5 die Schalter. */
#define RC_CH_ROLL      1
#define RC_CH_PITCH     2
#define RC_CH_THROTTLE  3
#define RC_CH_YAW       4
#define RC_CH_ARM       5    /* Schalter zum Scharfschalten */
#define RC_CH_MODE      6    /* Schalter Angle / Rate */

/* Totzone um die Mitte in Mikrosekunden.
 * Darunter gilt der Knueppel als losgelassen. */
#define RC_DEADBAND_US  20

/* Ausschlaege */
#define RC_MAX_ANGLE    35.0f    /* Grad im Angle-Modus   */
#define RC_MAX_RATE     300.0f   /* Grad/s im Rate-Modus  */
#define RC_MAX_YAW_RATE 200.0f   /* Grad/s fuer Gieren    */

/* Schwelle, ab der ein Schalter als eingeschaltet gilt */
#define RC_SWITCH_HIGH  1700

void RC_Init(void);

/* Einmal pro Regelzyklus aufrufen. Setzt die Sollwerte,
 * uebernimmt Failsafe und Arming. */
void RC_Update(float dt);

uint8_t RC_IsFailsafe(void);
void    RC_PrintStatus(void);

#endif /* RC_H */