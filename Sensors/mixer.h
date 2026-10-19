#ifndef MIXER_H
#define MIXER_H

#include <stdint.h>

/* Motorbelegung (Betaflight-Standard, Blick von oben):
 *
 *     M4        M2        vorne
 *        \     /
 *         \   /
 *          \ /
 *          / \
 *         /   \
 *        /     \
 *     M3        M1        hinten
 *
 *   M1 = hinten rechts, CW
 *   M2 = vorne  rechts, CCW
 *   M3 = hinten links,  CCW
 *   M4 = vorne  links,  CW
 *
 * Diagonal gegenueberliegende Motoren drehen gleich - das hebt das
 * Drehmoment auf. Yaw entsteht, indem ein Paar mehr und das andere
 * weniger bekommt. Die Vorzeichenkonvention steht oben in mixer.c.
 */

/* Leerlauf: Solange gearmt ist, laeuft jeder Motor mindestens mit diesem
 * Wert - auch bei Gas ganz unten. Sonst braucht der ESC beim Wiederanlaufen
 * zu lange und die Regelung verliert die Achse.
 * Bezogen auf den Gasbereich in motor.h: 0 % = MOTOR_US_MIN (143,5 us, dort
 * steht der Motor gerade noch). Die frueher gemessene Anlaufschwelle
 * (15 % von 125..250 us = 143,75 us) liegt genau an diesem Minimum. 5 % =
 * 147,8 us laesst Abstand, damit kein Motor im Leerlauf stockt. Drehen die
 * Motoren im Leerlauf ungleichmaessig an, erhoehen; zu schnell, senken. */
#define MIXER_IDLE_PERCENT    5.0f
#define MIXER_MAX_PERCENT   100.0f

/* Airmode: Erst wenn das Gas seit dem Armen einmal diesen Wert erreicht hat
 * (die Drohne also fliegt), darf der Mixer das Grundgas anheben, damit die
 * Korrekturen auch bei Gas ganz unten voll wirken. Vorher - am Boden -
 * bleiben die Motoren bei Gas 0 im reinen Leerlauf, auch wenn die Drohne
 * schief steht. Gilt bis zum Disarm.
 * Das Schwebegas muss ueber diesem Wert liegen, sonst fehlt beim Sinken
 * mit wenig Gas die Lageregelung - dann den Wert senken. */
#define MIXER_AIRMODE_START_PERCENT 25.0f

/* Rechnet Gas und die drei Reglerausgaben auf vier Motoren um
 * und schreibt sie direkt an die Motoren.
 *
 * throttle: 0..100 % Knueppelweg. 0 % = Leerlauf, 100 % = Vollgas.
 * roll/pitch/yaw: Stellgroessen aus den PIDs (+-400 = +-100 % Stellbereich)
 *
 * Disarmed bleiben alle Motoren aus. */
void Mixer_Update(float throttle, float roll, float pitch, float yaw);

/* Rechnet dieselbe Mischung, schreibt aber nichts an die Motoren.
 * Fuer den Kipptest ohne Propeller ('kipp'-Befehl). */
void Mixer_Preview(float throttle, float roll, float pitch, float yaw, float out[4]);

/* Letzte berechnete Motorwerte in Prozent - fuer Debug-Ausgaben. */
void Mixer_GetOutputs(float *m1, float *m2, float *m3, float *m4);

/* 1 = die Korrekturen passten im letzten Zyklus nicht in den Stellbereich
 * und wurden gestaucht. */
uint8_t Mixer_IsSaturated(void);

/* 1 = Airmode an, die Drohne hat seit dem Armen schon Gas gegeben (fliegt) */
uint8_t Mixer_AirmodeActive(void);

#endif /* MIXER_H */
