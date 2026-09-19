#ifndef PID_H
#define PID_H

#include <stdint.h>

/* Ein PID-Regler fuer eine Achse.
 * Arbeitet auf Drehraten (Grad/s) - Rate-Modus / Acro.
 *
 * Eingang:  Sollrate und Istrate in Grad/s
 * Ausgang:  Stellgroesse, begrenzt auf +-out_limit
 */
typedef struct
{
    float kp;
    float ki;
    float kd;

    float integral;        /* aufsummierter Fehler */
    float prev_measured;   /* Istwert des letzten Zyklus, fuer den D-Anteil */
    float d_filtered;      /* gefilterter D-Anteil */

    float i_limit;         /* Begrenzung des I-Anteils (Anti-Windup) */
    float out_limit;       /* Begrenzung der Gesamtausgabe */
    float d_lpf_alpha;     /* Filterstaerke fuer D: 0 = aus, 0.9 = stark */
} PID_t;

/* Setzt Parameter und loescht den internen Zustand. */
void  PID_Init(PID_t *pid, float kp, float ki, float kd,
               float i_limit, float out_limit, float d_lpf_alpha);

/* Ein Regelschritt.
 * setpoint = gewuenschte Drehrate, measured = gemessene Drehrate, dt in Sekunden.
 * integrate = 0 haelt den I-Anteil an (z.B. wenn disarmed oder Gas auf Null). */
float PID_Update(PID_t *pid, float setpoint, float measured, float dt, uint8_t integrate);

/* Loescht Integral und D-Historie - beim Armen aufrufen. */
void  PID_Reset(PID_t *pid);

#endif /* PID_H */