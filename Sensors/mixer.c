/* ===== Vorzeichenkonvention - gilt fuer die ganze Firmware =====
 *
 * Koerperachsen wie in der Luftfahrt, rechtshaendig:
 *   X nach vorne, Y nach rechts, Z nach unten
 *
 * Positive Drehrichtungen (Rechte-Hand-Regel um die jeweilige Achse):
 *   Roll  +  = rechte Seite geht nach unten
 *   Pitch +  = Nase geht nach oben
 *   Yaw   +  = Nase dreht nach rechts (von oben gesehen im Uhrzeigersinn)
 *
 * Das gilt ueberall gleich: Lagewinkel, Gyro-Drehraten, Sollwerte und
 * Reglerausgaben. Ein positiver Reglerausgang erzeugt eine Drehung in
 * positiver Richtung.
 *
 * Knueppel (rc.c):
 *   Roll  rechts        -> Roll  +
 *   Pitch nach vorne    -> Pitch -  (Nase runter, Drohne fliegt vorwaerts)
 *   Yaw   rechts        -> Yaw   +
 *
 * Sensor (mpu6050.c): die Chip-Achsen werden je nach Einbaulage
 * (IMU_ALIGN in mpu6050.h) auf diese Koerperachsen umgerechnet.
 *
 * Mixer (unten):
 *   Roll  +  -> rechte Motoren (M1, M2) weniger, linke (M3, M4) mehr
 *   Pitch +  -> vordere Motoren (M2, M4) mehr, hintere (M1, M3) weniger
 *   Yaw   +  -> CCW-Motoren (M2, M3) mehr, CW-Motoren (M1, M4) weniger.
 *              Ein CCW-Propeller dreht den Rahmen im Uhrzeigersinn.
 */

#include "mixer.h"
#include "motor.h"
#include <math.h>

static float out[4] = {0}; /* zuletzt berechnete Werte in Prozent */
static uint8_t saturated = 0;
static uint8_t airmode = 0;   /* 1 = Gas war seit dem Armen schon ab MIXER_AIRMODE_START_PERCENT */
static uint8_t was_armed = 0; /* fuer die Erkennung des Arm-Zeitpunkts */

/* Gemeinsame Rechnung fuer Mixer_Update und Mixer_Preview.
 * Ergebnis liegt immer zwischen Leerlauf und 100 %.
 * lift = 1: Grundgas darf angehoben werden, damit die Korrekturen auch bei
 *           wenig Gas voll wirken (Airmode, nur in der Luft sinnvoll).
 * lift = 0: Grundgas bleibt beim Knueppelwert, die Korrekturen werden so
 *           weit gestaucht, dass sie hineinpassen. Bei Gas 0 bleibt damit
 *           reiner Leerlauf - eine schief stehende Drohne am Boden kippt
 *           nicht um. */
static uint8_t mix(float throttle, float roll, float pitch, float yaw, uint8_t lift, float o[4])
{
    /* Reglerausgaben kommen in einer eigenen Skala (out_limit 400).
     * Umrechnen auf Prozent: 400 entspricht 100 % Stellbereich. */
    const float SCALE = 100.0f / 400.0f;

    /* Ungueltige Werte (NaN, unendlich) wie 0 behandeln - jeder Vergleich
     * mit NaN ist falsch, die Begrenzungen unten wuerden sie durchlassen
     * und die Motoren bekaemen 0 statt Leerlauf. */
    if (!isfinite(roll))
        roll = 0.0f;
    if (!isfinite(pitch))
        pitch = 0.0f;
    if (!isfinite(yaw))
        yaw = 0.0f;
    if (!(throttle >= 0.0f)) /* auch NaN */
        throttle = 0.0f;

    const float r = roll * SCALE;
    const float p = pitch * SCALE;
    const float y = yaw * SCALE;

    /* Anteil jedes Motors, Vorzeichen siehe Konvention oben */
    float m[4];
    m[0] = -r - p - y; /* M1 hinten rechts, CW  */
    m[1] = -r + p + y; /* M2 vorne  rechts, CCW */
    m[2] = +r - p + y; /* M3 hinten links,  CCW */
    m[3] = +r + p - y; /* M4 vorne  links,  CW  */

    float lo = m[0];
    float hi = m[0];
    for (int i = 1; i < 4; i++)
    {
        if (m[i] < lo)
            lo = m[i];
        if (m[i] > hi)
            hi = m[i];
    }

    /* Verfuegbarer Bereich zwischen Leerlauf und Vollgas */
    const float span = MIXER_MAX_PERCENT - MIXER_IDLE_PERCENT;

    /* Passen die Korrekturen nicht hinein, alle gleichmaessig stauchen.
     * Die Richtung bleibt erhalten, nur die Staerke sinkt. */
    float k = 1.0f;
    uint8_t sat = 0;
    if (hi - lo > span)
    {
        k = span / (hi - lo);
        sat = 1;
    }

    /* Gas 0..100 % linear auf Leerlauf..Vollgas abbilden.
     * Damit gibt es keinen Sprung: 0 % Knueppel = Leerlauf. */
    if (throttle > 100.0f)
        throttle = 100.0f;
    float base = MIXER_IDLE_PERCENT + throttle * (span / 100.0f);

    if (lift)
    {
        /* Grundgas so verschieben, dass kein Motor unter den Leerlauf oder
         * ueber 100 % muss. Die Differenzen zwischen den Motoren bleiben. */
        const float base_min = MIXER_IDLE_PERCENT - lo * k;
        const float base_max = MIXER_MAX_PERCENT - hi * k;
        if (base < base_min)
            base = base_min;
        if (base > base_max)
            base = base_max;
    }
    else
    {
        /* Grundgas fest, Korrekturen in den Platz darunter und darueber stauchen */
        if (lo < 0.0f && (base - MIXER_IDLE_PERCENT) < -lo * k)
        {
            k = (base - MIXER_IDLE_PERCENT) / -lo;
            sat = 1;
        }
        if (hi > 0.0f && (MIXER_MAX_PERCENT - base) < hi * k)
        {
            k = (MIXER_MAX_PERCENT - base) / hi;
            sat = 1;
        }
    }

    for (int i = 0; i < 4; i++)
    {
        float v = base + m[i] * k;
        /* nur gegen Rundungsfehler */
        if (v < MIXER_IDLE_PERCENT)
            v = MIXER_IDLE_PERCENT;
        if (v > MIXER_MAX_PERCENT)
            v = MIXER_MAX_PERCENT;
        o[i] = v;
    }
    return sat;
}

void Mixer_Update(float throttle, float roll, float pitch, float yaw)
{
    if (!Motor_IsArmed())
    {
        /* Disarmed: alles aus, auch kein Leerlauf */
        for (int i = 0; i < 4; i++)
            out[i] = 0.0f;
        saturated = 0;
        airmode = 0;
        was_armed = 0;
        Motor_SetAllPercent(0.0f);
        return;
    }

    /* Frisch gearmt: Airmode immer aus. Auch wenn zwischen Disarm und
     * neuem Arm kein Zyklus disarmed lief (z.B. Terminal-disarm, Funk armt
     * sofort wieder). */
    if (!was_armed)
    {
        airmode = 0;
        was_armed = 1;
    }

    /* Airmode erst, wenn die Drohne einmal richtig Gas bekommen hat.
     * Bleibt bis zum Disarm an - auch waehrend der Failsafe-Rampe. */
    if (throttle >= MIXER_AIRMODE_START_PERCENT)
        airmode = 1;

    saturated = mix(throttle, roll, pitch, yaw, airmode, out);

    Motor_SetPercent(1, out[0]);
    Motor_SetPercent(2, out[1]);
    Motor_SetPercent(3, out[2]);
    Motor_SetPercent(4, out[3]);
}

void Mixer_Preview(float throttle, float roll, float pitch, float yaw, float o[4])
{
    (void)mix(throttle, roll, pitch, yaw, 1, o);
}

void Mixer_GetOutputs(float *m1, float *m2, float *m3, float *m4)
{
    *m1 = out[0];
    *m2 = out[1];
    *m3 = out[2];
    *m4 = out[3];
}

uint8_t Mixer_IsSaturated(void)
{
    return saturated;
}
