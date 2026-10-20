#include "rc.h"
#include "crsf.h"
#include "control.h"
#include "motor.h"
#include "arming.h"
#include <stdio.h>

static uint8_t failsafe = 1;
static uint8_t arm_latch = 0; /* verhindert Armen, solange der Schalter
                               * beim Einschalten schon oben steht */

/* Failsafe-Gasrampe */
static uint8_t fs_ramp = 0;       /* 1 = Rampe laeuft */
static uint32_t fs_t0 = 0;        /* Beginn der Rampe, HAL_GetTick() */
static float fs_thr_start = 0.0f; /* Gas beim Signalverlust, % */
static uint32_t thr_high_ms = 0;  /* zuletzt Gas >= RC_FS_LANDED_THR, HAL_GetTick() */

void RC_Init(void)
{
    failsafe = 1;
    arm_latch = 0;
    fs_ramp = 0;
}

/* Ein Schritt der Failsafe-Rampe: Angle-Modus, waagrecht, kein Gieren,
 * Gas linear in RC_FS_RAMP_S auf 0, danach disarmen.
 * Laeuft bis zum Ende durch, auch wenn das Signal zwischendurch
 * zurueckkommt - Knueppel und Modus-Schalter bleiben bis dahin wirkungslos.
 * Nur der Arm-Schalter wirkt: runter = sofort aus.
 * Die Zeit kommt aus HAL_GetTick(), nicht aus der Zahl der Regelzyklen -
 * so bleibt es bei 1,5 s, auch wenn die Schleife einmal langsamer laeuft. */
static void failsafe_ramp(uint8_t link)
{
    if (!Motor_IsArmed())
    {
        /* z.B. per Terminal disarmt - Rampe beenden */
        fs_ramp = 0;
        setpoint.throttle = 0.0f;
        return;
    }

    if (link && CRSF_Channel(RC_CH_ARM) <= RC_SWITCH_HIGH)
    {
        /* Signal ist zurueck und der Pilot schaltet ab */
        Motor_Disarm();
        setpoint.throttle = 0.0f;
        Control_Reset();
        fs_ramp = 0;
        return;
    }

    Control_SetMode(MODE_ANGLE);
    setpoint.roll = 0.0f;
    setpoint.pitch = 0.0f;
    setpoint.yaw = 0.0f;

    const float t = (float)(HAL_GetTick() - fs_t0) * 0.001f;
    if (t >= RC_FS_RAMP_S)
    {
        Motor_Disarm();
        setpoint.throttle = 0.0f;
        Control_Reset();
        fs_ramp = 0;
        return;
    }

    setpoint.throttle = fs_thr_start * (1.0f - t / RC_FS_RAMP_S);
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
    const uint8_t link = CRSF_LinkUp();

    /* ---------- Failsafe ---------- */
    if (!link && !failsafe)
    {
        /* Verbindung gerade verloren */
        failsafe = 1;
        arm_latch = 0; /* nach Failsafe muss der Schalter erst wieder runter */

        if (Motor_IsArmed() && HAL_GetTick() - thr_high_ms >= RC_FS_LANDED_MS)
        {
            /* Gas war schon laenger ganz unten: die Drohne steht. Eine
             * Rampe von 0 auf 0 braucht es nicht, sofort aus. */
            Motor_Disarm();
            setpoint.throttle = 0.0f;
            Control_Reset();
        }
        else if (Motor_IsArmed())
        {
            /* im Flug: Gas langsam herunterfahren statt abschalten */
            fs_ramp = 1;
            fs_t0 = HAL_GetTick();
            fs_thr_start = setpoint.throttle;
        }
        else
        {
            setpoint.throttle = 0.0f;
            setpoint.roll = setpoint.pitch = setpoint.yaw = 0.0f;
        }
    }

    if (fs_ramp)
    {
        failsafe_ramp(link);
        return;
    }

    /* Kein Signal: nichts vom Funk uebernehmen. Ohne Funk bleibt der
     * Betrieb ueber das Terminal moeglich (arm, t). */
    if (!link)
        return;
    failsafe = 0;

    /* ---------- Gas ---------- */
    int32_t thr_us = (int32_t)CRSF_Channel(RC_CH_THROTTLE);
    float thr = ((float)thr_us - 1000.0f) / 10.0f; /* 1000..2000 -> 0..100 */
    if (thr < 0.0f)
        thr = 0.0f;
    if (thr > 100.0f)
        thr = 100.0f;
    if (thr >= RC_FS_LANDED_THR || !Motor_IsArmed())
        thr_high_ms = HAL_GetTick(); /* fuer die Landeerkennung beim Failsafe */

    /* ---------- Flugmodus ueber Schalter ---------- */
    if (CRSF_Channel(RC_CH_MODE) > RC_SWITCH_HIGH)
        Control_SetMode(MODE_RATE);
    else
        Control_SetMode(MODE_ANGLE);

    /* ---------- Arming ----------
     * Bedingungen: Schalter oben UND Gas unter 5 % UND der Schalter
     * war seit dem Einschalten mindestens einmal unten UND die
     * Sicherheitsabfragen in arming.c (Sensor, Neigung, Akku) sind ok.
     * Scheitert eine davon, muss der Schalter erst wieder runter - sonst
     * wuerde die Drohne von selbst armen, sobald z.B. die Neigung passt. */
    uint8_t sw_on = (CRSF_Channel(RC_CH_ARM) > RC_SWITCH_HIGH) ? 1 : 0;

    if (!sw_on)
        arm_latch = 1; /* Schalter war unten, Armen ist jetzt erlaubt */

    if (sw_on && arm_latch && !Motor_IsArmed())
    {
        if (thr >= 5.0f)
        {
            Arming_Reject(ARM_GAS);
            arm_latch = 0;
        }
        else if (Arming_Arm() != ARM_OK)
        {
            arm_latch = 0; /* Grund merkt sich arming.c fuers Terminal */
        }
    }
    else if (!sw_on && Motor_IsArmed())
    {
        Arming_Disarm();
    }
    else if (sw_on && !arm_latch && Motor_IsArmed())
    {
        /* Gearmt, aber nicht vom Funk (per Terminal ohne Funk). Sobald die
         * Funke da ist, entscheidet sie - und der Schalter stand schon oben,
         * ohne dass Gas-unten geprueft wurde. Also aus. */
        Arming_Disarm();
    }

    /* ---------- Sollwerte ----------
     * Vorzeichen nach der Konvention in mixer.c:
     * Roll rechts = +, Pitch nach vorne = Nase runter = -, Yaw rechts = + */
    float r = stick_norm(RC_CH_ROLL);
    float p = -stick_norm(RC_CH_PITCH);
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

void RC_BlockArm(void)
{
    arm_latch = 0;
}

uint8_t RC_IsFailsafe(void)
{
    return failsafe;
}

uint8_t RC_FailsafeRampActive(void)
{
    return fs_ramp;
}

void RC_PrintStatus(void)
{
    printf(">> Funk: %s%s | Pakete: %lu | Fehler: %lu | letztes vor %lu ms\r\n",
           CRSF_LinkUp() ? "VERBUNDEN" : "KEIN SIGNAL",
           fs_ramp ? " (FAILSAFE-RAMPE)" : "",
           CRSF_FrameCount(), CRSF_ErrorCount(),
           CRSF_LinkUp() ? CRSF_AgeMs() : 0);
    printf("   Roll %4u  Pitch %4u  Gas %4u  Yaw %4u\r\n",
           CRSF_Channel(RC_CH_ROLL), CRSF_Channel(RC_CH_PITCH),
           CRSF_Channel(RC_CH_THROTTLE), CRSF_Channel(RC_CH_YAW));
    printf("   Arm-Schalter (Kanal %u) %4u  Modus-Schalter (Kanal %u) %4u\r\n",
           RC_CH_ARM, CRSF_Channel(RC_CH_ARM), RC_CH_MODE, CRSF_Channel(RC_CH_MODE));
}