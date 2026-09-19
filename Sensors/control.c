#include "control.h"
#include <stdio.h>

PID_t pid_rate_roll;
PID_t pid_rate_pitch;
PID_t pid_rate_yaw;

PID_t pid_angle_roll;
PID_t pid_angle_pitch;

Setpoint_t setpoint = {0};
FlightMode_t flight_mode = MODE_ANGLE; /* Start im gutmuetigen Modus */

/* ---- Startwerte innerer Kreis (Drehrate) ----
 * Bewusst niedrig. Beim Abstimmen wird hochgetastet, nicht heruntergedreht. */
#define RATE_KP 0.8f
#define RATE_KI 0.0f /* zuletzt dazunehmen */
#define RATE_KD 0.0f /* nach Kp */
#define RATE_ILIM 50.0f
#define RATE_OUTLIM 400.0f
#define RATE_DLPF 0.85f

/* ---- Startwerte aeusserer Kreis (Winkel) ----
 * Braucht praktisch nur P. I fuehrt zu traegem Nachlaufen,
 * D ist ueberfluessig weil der innere Kreis schon daempft. */
#define ANGLE_KP 4.0f
#define ANGLE_KI 0.0f
#define ANGLE_KD 0.0f
#define ANGLE_ILIM 20.0f
#define ANGLE_OUTLIM 250.0f /* max. Soll-Drehrate in Grad/s */
#define ANGLE_DLPF 0.0f

/* Begrenzung der Neigung im Angle-Modus */
#define MAX_ANGLE 35.0f /* Grad */

void Control_Init(void)
{
    PID_Init(&pid_rate_roll, RATE_KP, RATE_KI, RATE_KD,
             RATE_ILIM, RATE_OUTLIM, RATE_DLPF);
    PID_Init(&pid_rate_pitch, RATE_KP, RATE_KI, RATE_KD,
             RATE_ILIM, RATE_OUTLIM, RATE_DLPF);
    /* Yaw braucht mehr P und kein D */
    PID_Init(&pid_rate_yaw, 1.2f, RATE_KI, 0.0f,
             RATE_ILIM, RATE_OUTLIM, RATE_DLPF);

    PID_Init(&pid_angle_roll, ANGLE_KP, ANGLE_KI, ANGLE_KD,
             ANGLE_ILIM, ANGLE_OUTLIM, ANGLE_DLPF);
    PID_Init(&pid_angle_pitch, ANGLE_KP, ANGLE_KI, ANGLE_KD,
             ANGLE_ILIM, ANGLE_OUTLIM, ANGLE_DLPF);

    setpoint.roll = setpoint.pitch = setpoint.yaw = 0.0f;
    setpoint.throttle = 0.0f;

    flight_mode = MODE_ANGLE;
}

void Control_Reset(void)
{
    PID_Reset(&pid_rate_roll);
    PID_Reset(&pid_rate_pitch);
    PID_Reset(&pid_rate_yaw);
    PID_Reset(&pid_angle_roll);
    PID_Reset(&pid_angle_pitch);
}

void Control_SetMode(FlightMode_t mode)
{
    if (mode != flight_mode)
    {
        flight_mode = mode;
        Control_Reset(); /* Integratoren loeschen beim Umschalten */
    }
}

void Control_Update(MPU6050_Data_t *s, float dt,
                    float *out_roll, float *out_pitch, float *out_yaw)
{
    /* I-Anteil nur bei anliegendem Gas - sonst laeuft der Integrator
     * am Boden voll und die Drohne springt beim Armen weg. */
    uint8_t integrate = (setpoint.throttle > 10.0f) ? 1 : 0;

    float rate_sp_roll;
    float rate_sp_pitch;

    if (flight_mode == MODE_ANGLE)
    {
        /* --- Aeusserer Kreis: Winkelfehler -> Soll-Drehrate --- */
        float sp_roll = setpoint.roll;
        float sp_pitch = setpoint.pitch;

        if (sp_roll > MAX_ANGLE)
            sp_roll = MAX_ANGLE;
        if (sp_roll < -MAX_ANGLE)
            sp_roll = -MAX_ANGLE;
        if (sp_pitch > MAX_ANGLE)
            sp_pitch = MAX_ANGLE;
        if (sp_pitch < -MAX_ANGLE)
            sp_pitch = -MAX_ANGLE;

        /* Istwert ist der gefilterte Winkel aus dem Komplementaerfilter */
        rate_sp_roll = PID_Update(&pid_angle_roll, sp_roll, s->roll, dt, integrate);
        rate_sp_pitch = PID_Update(&pid_angle_pitch, sp_pitch, s->pitch, dt, integrate);
    }
    else
    {
        /* Rate-Modus: Knueppel ist direkt die Soll-Drehrate */
        rate_sp_roll = setpoint.roll;
        rate_sp_pitch = setpoint.pitch;
    }

    /* --- Innerer Kreis: Drehratenfehler -> Stellgroesse ---
     * Istwert ist immer der rohe Gyro. */
    *out_roll = PID_Update(&pid_rate_roll, rate_sp_roll, s->gyro_x_dps, dt, integrate);
    *out_pitch = PID_Update(&pid_rate_pitch, rate_sp_pitch, s->gyro_y_dps, dt, integrate);

    /* Yaw laeuft immer im Rate-Modus */
    *out_yaw = PID_Update(&pid_rate_yaw, setpoint.yaw, s->gyro_z_dps, dt, integrate);
}

void Control_SetGain(char loop, char axis, char term, float value)
{
    PID_t *pid = 0;

    if (loop == 'r') /* innerer Kreis */
    {
        if (axis == 'r')
            pid = &pid_rate_roll;
        else if (axis == 'p')
            pid = &pid_rate_pitch;
        else if (axis == 'y')
            pid = &pid_rate_yaw;
    }
    else if (loop == 'a') /* aeusserer Kreis */
    {
        if (axis == 'r')
            pid = &pid_angle_roll;
        else if (axis == 'p')
            pid = &pid_angle_pitch;
        /* Yaw hat keinen Winkelregler */
    }

    if (!pid)
        return;

    if (term == 'p')
        pid->kp = value;
    else if (term == 'i')
        pid->ki = value;
    else if (term == 'd')
        pid->kd = value;
    else
        return;

    Control_Reset();
}

void Control_PrintGains(void)
{
    printf(">> Modus: %s\r\n", (flight_mode == MODE_ANGLE) ? "ANGLE" : "RATE");
    printf(">> Rate (innen)\r\n");
    printf("   roll : P=%.3f I=%.3f D=%.4f\r\n",
           pid_rate_roll.kp, pid_rate_roll.ki, pid_rate_roll.kd);
    printf("   pitch: P=%.3f I=%.3f D=%.4f\r\n",
           pid_rate_pitch.kp, pid_rate_pitch.ki, pid_rate_pitch.kd);
    printf("   yaw  : P=%.3f I=%.3f D=%.4f\r\n",
           pid_rate_yaw.kp, pid_rate_yaw.ki, pid_rate_yaw.kd);
    printf(">> Angle (aussen)\r\n");
    printf("   roll : P=%.3f I=%.3f D=%.4f\r\n",
           pid_angle_roll.kp, pid_angle_roll.ki, pid_angle_roll.kd);
    printf("   pitch: P=%.3f I=%.3f D=%.4f\r\n",
           pid_angle_pitch.kp, pid_angle_pitch.ki, pid_angle_pitch.kd);
}