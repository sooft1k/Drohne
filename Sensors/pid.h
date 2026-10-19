#ifndef PID_H
#define PID_H

#include <stdint.h>

/* Ein PID-Regler fuer eine Achse.
 *
 * Eingang:  Sollwert und Istwert (Grad/s im inneren, Grad im aeusseren Kreis)
 * Ausgang:  Stellgroesse, begrenzt auf +-out_limit
 */
typedef struct
{
    float kp;
    float ki;
    float kd;

    float i_term;        /* aufsummierter I-Anteil, schon mit ki multipliziert */
    float prev_measured; /* Istwert des letzten Zyklus, fuer den D-Anteil */
    float d_filtered;    /* gefilterter D-Anteil (vor kd) */
    uint8_t primed;      /* 0 = nach Reset, prev_measured noch nicht gueltig */

    float i_limit;   /* Begrenzung des I-Anteils, in Einheiten des Ausgangs */
    float out_limit; /* Begrenzung der Gesamtausgabe */
    float d_lpf_hz;  /* Grenzfrequenz des D-Tiefpasses, 0 = aus */
} PID_t;

/* Setzt Parameter und loescht den internen Zustand. */
void PID_Init(PID_t *pid, float kp, float ki, float kd,
              float i_limit, float out_limit, float d_lpf_hz);

/* Ein Regelschritt. dt in Sekunden (gemessen, nicht fest).
 * integrate = 0 haelt den I-Anteil an (z.B. wenn disarmed oder Gas niedrig).
 * saturated = 1: die Stellgroesse kommt gerade nicht voll an (Mixer am
 *             Anschlag) - der I-Anteil darf dann nur kleiner werden. */
float PID_Update(PID_t *pid, float setpoint, float measured, float dt,
                 uint8_t integrate, uint8_t saturated);

/* Loescht I-Anteil und D-Filter. Der naechste PID_Update nimmt seinen
 * Istwert als Startwert fuer die Ableitung - kein D-Ausschlag. */
void PID_Reset(PID_t *pid);

#endif /* PID_H */
