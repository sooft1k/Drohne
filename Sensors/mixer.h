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
 * weniger bekommt.
 */

/* Leerlauf im Flug: Motoren duerfen nie ganz stehen, sonst braucht der
 * ESC beim Wiederanlaufen zu lange und die Regelung verliert die Achse.
 * 15 % ist die gemessene Anlaufschwelle deiner Motoren. */
#define MIXER_IDLE_PERCENT   15.0f

/* Rechnet Gas und die drei Reglerausgaben auf vier Motoren um
 * und schreibt sie direkt an die Motoren.
 *
 * throttle: 0..100 %
 * roll/pitch/yaw: Stellgroessen aus den PIDs
 *
 * Wirkt nur wenn armed - Motor_SetPercent prueft das selbst. */
void Mixer_Update(float throttle, float roll, float pitch, float yaw);

/* Letzte berechnete Motorwerte in Prozent - fuer Debug-Ausgaben. */
void Mixer_GetOutputs(float *m1, float *m2, float *m3, float *m4);

#endif /* MIXER_H */