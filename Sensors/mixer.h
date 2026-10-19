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
 * 15 % ist die gemessene Anlaufschwelle deiner Motoren. */
#define MIXER_IDLE_PERCENT   15.0f
#define MIXER_MAX_PERCENT   100.0f

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

#endif /* MIXER_H */
