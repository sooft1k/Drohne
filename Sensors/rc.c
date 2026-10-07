#include "rc.h"
#include "crsf.h"
#include "control.h"
#include "motor.h"
#include <stdio.h>

static uint8_t failsafe = 1;
static uint8_t arm_latch = 0; /* verhindert Armen, solange der Schalter
                               * beim Einschalten schon oben steht */

void RC_Init(void)
{
    failsafe = 1;
    arm_latch = 0;
}

/* Knueppelwert in -1.0 bis +1.0 umrechnen, mit Totzone um die Mitte */
static float stick_norm(uint8_t ch)
{
    int32_t us = (int32_t)CRSF_Channel(ch) - 1500;

    if (us > -RC_DEADBAND_US && us < RC_DEADBAND_US)
        return 0.0f;

    /* Totzone herausrechnen, damit es direkt hinter der Zone
     * bei null anfaengt und nicht springt. */
    if (us > 0)
        us -= RC_DEADBAND_US;
    else
        us += RC_DEADBAND_US;

    float f = (float)us / (500.0f - RC_DEADBAND_US);
    if (f > 1.0f)
        f = 1.0f;
    if (f < -1.0f)
        f = -1.0f;
    return f;
}

void RC_Update(float dt)
{
    (void)dt;

    /* ---------- Failsafe ---------- */
    if (!CRSF_LinkUp())
    {
        if (!failsafe)
        {
            /* Verbindung gerade verloren - sofort alles aus */
            Motor_Disarm();
            setpoint.throttle = 0.0f;
            setpoint.roll = setpoint.pitch = setpoint.yaw = 0.0f;
            Control_Reset();
        }
        failsafe = 1;
        arm_latch = 0; /* nach Failsafe muss der Schalter erst wieder runter */
        return;
    }
    failsafe = 0;

    /* ---------- Gas ---------- */
    int32_t thr_us = (int32_t)CRSF_Channel(RC_CH_THROTTLE);
    float thr = ((float)thr_us - 1000.0f) / 10.0f; /* 1000..2000 -> 0..100 */
    if (thr < 0.0f)
        thr = 0.0f;
    if (thr > 100.0f)
        thr = 100.0f;

    /* ---------- Flugmodus ueber Schalter ---------- */
    if (CRSF_Channel(RC_CH_MODE) > RC_SWITCH_HIGH)
        Control_SetMode(MODE_RATE);
    else
        Control_SetMode(MODE_ANGLE);

    /* ---------- Arming ----------
     * Bedingungen: Schalter oben UND Gas unter 5 % UND der Schalter
     * war seit dem Einschalten mindestens einmal unten. */
    uint8_t sw_on = (CRSF_Channel(RC_CH_ARM) > RC_SWITCH_HIGH) ? 1 : 0;

    if (!sw_on)
        arm_latch = 1; /* Schalter war unten, Armen ist jetzt erlaubt */

    if (sw_on && arm_latch && !Motor_IsArmed())
    {
        if (thr < 5.0f)
        {
            Control_Reset();
            Motor_Arm();
        }
        else
        {
            /* Gas stand nicht unten - sperren bis der Schalter neu kommt */
            arm_latch = 0;
        }
    }
    else if (!sw_on && Motor_IsArmed())
    {
        Motor_Disarm();
        setpoint.throttle = 0.0f;
    }

    /* ---------- Sollwerte ---------- */
    float r = stick_norm(RC_CH_ROLL);
    float p = stick_norm(RC_CH_PITCH);
    float y = stick_norm(RC_CH_YAW);

    if (flight_mode == MODE_ANGLE)
    {
        setpoint.roll = r * RC_MAX_ANGLE;
        setpoint.pitch = p * RC_MAX_ANGLE;
    }
    else
    {
        setpoint.roll = r * RC_MAX_RATE;
        setpoint.pitch = p * RC_MAX_RATE;
    }
    setpoint.yaw = y * RC_MAX_YAW_RATE;
    setpoint.throttle = Motor_IsArmed() ? thr : 0.0f;
}

uint8_t RC_IsFailsafe(void)
{
    return failsafe;
}

void RC_PrintStatus(void)
{
    printf(">> Funk: %s | Pakete: %lu | Fehler: %lu | letztes vor %lu ms\r\n",
           CRSF_LinkUp() ? "VERBUNDEN" : "KEIN SIGNAL",
           CRSF_FrameCount(), CRSF_ErrorCount(),
           CRSF_LinkUp() ? CRSF_AgeMs() : 0);
    printf("   Roll %4u  Pitch %4u  Gas %4u  Yaw %4u\r\n",
           CRSF_Channel(RC_CH_ROLL), CRSF_Channel(RC_CH_PITCH),
           CRSF_Channel(RC_CH_THROTTLE), CRSF_Channel(RC_CH_YAW));
    printf("   Arm-Schalter %4u  Modus-Schalter %4u\r\n",
           CRSF_Channel(RC_CH_ARM), CRSF_Channel(RC_CH_MODE));
}