#ifndef CONTROL_H
#define CONTROL_H

#include "pid.h"
#include "mpu6050.h"

/* Flugmodus */
typedef enum
{
    MODE_RATE  = 0,   /* Knueppel = Drehrate, Lage bleibt stehen (Acro) */
    MODE_ANGLE = 1    /* Knueppel = Winkel, richtet sich selbst auf */
} FlightMode_t;

/* Sollwerte.
 * In MODE_ANGLE sind roll/pitch Winkel in Grad.
 * In MODE_RATE sind roll/pitch Drehraten in Grad/s.
 * yaw ist immer eine Drehrate - ohne Kompass gibt es keinen absoluten Gierwinkel. */
typedef struct
{
    float roll;
    float pitch;
    float yaw;
    float throttle;    /* 0..100 % */
} Setpoint_t;

/* Innerer Kreis - Drehraten */
extern PID_t pid_rate_roll;
extern PID_t pid_rate_pitch;
extern PID_t pid_rate_yaw;

/* Aeusserer Kreis - Winkel, nur Roll und Pitch */
extern PID_t pid_angle_roll;
extern PID_t pid_angle_pitch;

extern Setpoint_t   setpoint;
extern FlightMode_t flight_mode;

void Control_Init(void);
void Control_Reset(void);

void Control_Update(MPU6050_Data_t *s, float dt,
                    float *out_roll, float *out_pitch, float *out_yaw);

void Control_SetMode(FlightMode_t mode);

/* loop: 'r' = Rate (innen), 'a' = Angle (aussen)
 * axis: 'r' / 'p' / 'y'
 * term: 'p' / 'i' / 'd' */
void Control_SetGain(char loop, char axis, char term, float value);
void Control_PrintGains(void);

#endif /* CONTROL_H */