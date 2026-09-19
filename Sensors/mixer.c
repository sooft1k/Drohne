#include "mixer.h"
#include "motor.h"

static float out[4] = {0}; /* zuletzt berechnete Werte in Prozent */

void Mixer_Update(float throttle, float roll, float pitch, float yaw)
{
    /* Reglerausgaben kommen in einer eigenen Skala (out_limit 400).
     * Umrechnen auf Prozent: 400 entspricht 100 % Stellbereich. */
    const float SCALE = 100.0f / 400.0f;

    float r = roll * SCALE;
    float p = pitch * SCALE;
    float y = yaw * SCALE;

    /* --- Mischung ---
     * Vorzeichen ergeben sich aus Position und Drehrichtung.
     * Roll positiv  = nach rechts kippen -> rechte Motoren runter, linke hoch
     * Pitch positiv = Nase hoch          -> vordere runter, hintere hoch
     * Yaw positiv   = nach rechts drehen -> CW-Paar runter, CCW-Paar hoch */
    out[0] = throttle - r - p + y; /* M1 hinten rechts, CW  */
    out[1] = throttle - r + p - y; /* M2 vorne  rechts, CCW */
    out[2] = throttle + r - p - y; /* M3 hinten links,  CCW */
    out[3] = throttle + r + p + y; /* M4 vorne  links,  CW  */

    /* --- Sättigung behandeln ---
     * Wenn ein Motor ueber 100 % laeuft, gehen die Korrekturen verloren
     * und die Drohne kippt. Deshalb alle vier gemeinsam absenken,
     * statt einzeln abzuschneiden. Das erhaelt die Differenzen. */
    float max = out[0];
    float min = out[0];
    for (int i = 1; i < 4; i++)
    {
        if (out[i] > max)
            max = out[i];
        if (out[i] < min)
            min = out[i];
    }

    if (max > 100.0f)
    {
        float ueber = max - 100.0f;
        for (int i = 0; i < 4; i++)
            out[i] -= ueber;
        min -= ueber;
    }

    /* Nach unten: Leerlauf halten, damit kein Motor stehenbleibt.
     * Nur anheben wenn ueberhaupt Gas anliegt. */
    if (throttle > 1.0f && min < MIXER_IDLE_PERCENT)
    {
        float unter = MIXER_IDLE_PERCENT - min;
        for (int i = 0; i < 4; i++)
            out[i] += unter;
    }

    /* Harte Begrenzung als letzte Absicherung */
    for (int i = 0; i < 4; i++)
    {
        if (out[i] > 100.0f)
            out[i] = 100.0f;
        if (out[i] < 0.0f)
            out[i] = 0.0f;
    }

    /* Bei Gas auf Null alles aus - sonst drehen die Motoren am Boden mit */
    if (throttle < 1.0f)
    {
        for (int i = 0; i < 4; i++)
            out[i] = 0.0f;
    }

    Motor_SetPercent(1, out[0]);
    Motor_SetPercent(2, out[1]);
    Motor_SetPercent(3, out[2]);
    Motor_SetPercent(4, out[3]);
}

void Mixer_GetOutputs(float *m1, float *m2, float *m3, float *m4)
{
    *m1 = out[0];
    *m2 = out[1];
    *m3 = out[2];
    *m4 = out[3];
}