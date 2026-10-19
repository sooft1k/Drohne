#ifndef RC_H
#define RC_H

#include <stdint.h>

/* Kanalbelegung, AETR - so sendet ELRS standardmaessig.
 * Kanal 1 Roll, 2 Pitch, 3 Gas, 4 Gieren, ab 5 die Schalter. */
#define RC_CH_ROLL 1
#define RC_CH_PITCH 2
#define RC_CH_THROTTLE 3
#define RC_CH_YAW 4
#define RC_CH_ARM 5  /* AUX1: ELRS sendet ihn mit jedem Paket, die anderen
                      * Schalter nur reihum - deshalb Arm immer hier */
#define RC_CH_MODE 7 /* Schalter SB: Angle / Rate */

/* Totzone um die Mitte in Mikrosekunden.
 * Darunter gilt der Knueppel als losgelassen. */
#define RC_DEADBAND_US 20

/* Ausschlaege */
#define RC_MAX_ANGLE 35.0f     /* Grad im Angle-Modus   */
#define RC_MAX_RATE 300.0f     /* Grad/s im Rate-Modus  */
#define RC_MAX_YAW_RATE 200.0f /* Grad/s fuer Gieren    */

/* Schwelle, ab der ein Schalter als eingeschaltet gilt */
#define RC_SWITCH_HIGH 1700

/* Failsafe: Dauer der Gasrampe nach Signalverlust.
 * Waehrend der Rampe: Angle-Modus, Roll/Pitch/Yaw = 0, Gas linear auf 0.
 * Danach wird disarmt. Signalverlust gilt nach CRSF_TIMEOUT_MS (crsf.h).
 * Der Arm-Schalter (runter = aus) wirkt auch waehrend der Rampe. */
#define RC_FS_RAMP_S 1.5f

/* War das Gas beim Signalverlust schon so lange unter RC_FS_LANDED_THR,
 * gilt die Drohne als gelandet: sofort disarmen statt Rampe. */
#define RC_FS_LANDED_THR 5.0f /* %  */
#define RC_FS_LANDED_MS 1000  /* ms */

void RC_Init(void);

/* Einmal pro Regelzyklus aufrufen. Setzt die Sollwerte,
 * uebernimmt Failsafe und Arming. dt in Sekunden. */
void RC_Update(float dt);

uint8_t RC_IsFailsafe(void);         /* 1 = Failsafe: kein Signal oder Rampe laeuft noch */
uint8_t RC_FailsafeRampActive(void); /* 1 = Failsafe-Gasrampe laeuft gerade */

/* Arming ueber den Schalter sperren, bis er einmal unten war.
 * Fuer Disarm von aussen (Terminal), sonst armt der Schalter sofort wieder. */
void RC_BlockArm(void);
void RC_PrintStatus(void);

#endif /* RC_H */