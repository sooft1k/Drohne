#include "pid.h"

void PID_Init(PID_t *pid, float kp, float ki, float kd,
              float i_limit, float out_limit, float d_lpf_hz)
{
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;

    pid->i_limit = i_limit;
    pid->out_limit = out_limit;
    pid->d_lpf_hz = d_lpf_hz;

    PID_Reset(pid);
}

void PID_Reset(PID_t *pid)
{
    pid->i_term = 0.0f;
    pid->d_filtered = 0.0f;
    pid->primed = 0; /* prev_measured setzt der naechste PID_Update */
}

float PID_Update(PID_t *pid, float setpoint, float measured, float dt,
                 uint8_t integrate, uint8_t saturated)
{
    if (dt <= 0.0f)
        dt = 0.001f; /* nur gegen Division durch Null, kommt nicht vor */

    float error = setpoint - measured;

    /* --- P --- */
    float p_term = pid->kp * error;

    /* --- D ---
     * Ableitung des ISTWERTS, nicht des Fehlers. Sonst gibt es bei jeder
     * Knueppelbewegung einen Ausschlag (derivative kick).
     * Vorzeichen deshalb negativ.
     * Nach einem Reset gilt der aktuelle Istwert als Vorwert - sonst waere
     * die erste Ableitung (Istwert - 0) / dt, ein grosser Ausschlag. */
    if (!pid->primed)
    {
        pid->prev_measured = measured;
        pid->primed = 1;
    }
    float d_raw = -(measured - pid->prev_measured) / dt;
    pid->prev_measured = measured;

    /* Tiefpass erster Ordnung auf den D-Anteil - Gyro-Rauschen wird durch
     * das Ableiten stark verstaerkt. Faktor aus Grenzfrequenz und dt, damit
     * die Filterwirkung nicht von der Schleifenfrequenz abhaengt. */
    if (pid->d_lpf_hz > 0.0f)
    {
        const float rc = 1.0f / (2.0f * 3.14159265f * pid->d_lpf_hz);
        const float a = dt / (rc + dt);
        pid->d_filtered += a * (d_raw - pid->d_filtered);
    }
    else
    {
        pid->d_filtered = d_raw;
    }
    float d_term = pid->kd * pid->d_filtered;

    /* --- I ---
     * Nur integrieren wenn erlaubt. Sonst laeuft der Integrator am Boden voll
     * und die Drohne springt beim Armen weg.
     * Aufsummiert wird ki * Fehler * dt, begrenzt wird der I-Anteil selbst:
     * die Grenze bleibt gleich, auch wenn ki beim Abstimmen geaendert wird.
     * Anti-Windup: kein weiteres Aufintegrieren, solange der Ausgang am
     * Limit steht oder der Mixer die Korrektur nicht voll umsetzen kann -
     * nur in die Richtung, die den I-Anteil wieder kleiner macht. */
    if (integrate)
    {
        float i_new = pid->i_term + pid->ki * error * dt;
        if (i_new > pid->i_limit)
            i_new = pid->i_limit;
        if (i_new < -pid->i_limit)
            i_new = -pid->i_limit;

        const float out_try = p_term + i_new + d_term;
        const uint8_t grows = (i_new * i_new) > (pid->i_term * pid->i_term);
        const uint8_t at_limit = (out_try > pid->out_limit && i_new > pid->i_term) ||
                                 (out_try < -pid->out_limit && i_new < pid->i_term);

        if (!at_limit && !(saturated && grows))
            pid->i_term = i_new;
    }

    /* --- Summe und Begrenzung --- */
    float out = p_term + pid->i_term + d_term;

    if (out > pid->out_limit)
        out = pid->out_limit;
    if (out < -pid->out_limit)
        out = -pid->out_limit;

    return out;
}
