#include "pid.h"

void PID_Init(PID_t *pid, float kp, float ki, float kd,
              float i_limit, float out_limit, float d_lpf_alpha)
{
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;

    pid->i_limit = i_limit;
    pid->out_limit = out_limit;
    pid->d_lpf_alpha = d_lpf_alpha;

    PID_Reset(pid);
}

void PID_Reset(PID_t *pid)
{
    pid->integral = 0.0f;
    pid->prev_measured = 0.0f;
    pid->d_filtered = 0.0f;
}

float PID_Update(PID_t *pid, float setpoint, float measured, float dt, uint8_t integrate)
{
    float error = setpoint - measured;

    /* --- P --- */
    float p_term = pid->kp * error;

    /* --- I ---
     * Nur integrieren wenn erlaubt. Sonst laeuft der Integrator am Boden voll
     * und die Drohne springt beim Armen weg (Integral-Windup). */
    if (integrate)
    {
        pid->integral += error * dt;

        if (pid->integral > pid->i_limit)
            pid->integral = pid->i_limit;
        if (pid->integral < -pid->i_limit)
            pid->integral = -pid->i_limit;
    }
    float i_term = pid->ki * pid->integral;

    /* --- D ---
     * Ableitung des ISTWERTS, nicht des Fehlers. Sonst gibt es bei jeder
     * Knueppelbewegung einen Ausschlag (derivative kick).
     * Vorzeichen deshalb negativ. */
    float d_raw = -(measured - pid->prev_measured) / dt;
    pid->prev_measured = measured;

    /* Tiefpass auf den D-Anteil - Gyro-Rauschen wird durch das Ableiten
     * stark verstaerkt, ohne Filter ist D unbrauchbar. */
    pid->d_filtered = pid->d_lpf_alpha * pid->d_filtered + (1.0f - pid->d_lpf_alpha) * d_raw;

    float d_term = pid->kd * pid->d_filtered;

    /* --- Summe und Begrenzung --- */
    float out = p_term + i_term + d_term;

    if (out > pid->out_limit)
        out = pid->out_limit;
    if (out < -pid->out_limit)
        out = -pid->out_limit;

    return out;
}