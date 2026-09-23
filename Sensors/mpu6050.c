#include "mpu6050.h"
#include <math.h>

#define ACCEL_SCALE 16384.0f /* LSB pro g   (+-2g)      */
#define GYRO_SCALE 131.0f    /* LSB pro Grad/s (+-250)  */

/* Einstellbare Filterparameter */
static float gyro_lpf_hz = GYRO_LPF_DEFAULT_HZ;
static float accel_lpf_hz = ACCEL_LPF_DEFAULT_HZ;
static float comp_tau_s = COMP_TAU_DEFAULT_S;

/* Zustand der Tiefpaesse */
static float gx_f = 0, gy_f = 0, gz_f = 0;
static float ax_f = 0, ay_f = 0, az_f = 1.0f;
static uint8_t filt_primed = 0;

/* Tiefpass erster Ordnung (PT1).
 * Grenzfrequenz f, Zeitschritt dt:
 *   a = dt / (1/(2*pi*f) + dt)
 * a nahe 1 = kaum Filterung, a klein = starke Filterung. */
static inline float pt1(float prev, float in, float hz, float dt)
{
    if (hz <= 0.0f)
        return in; /* Filter aus */
    const float rc = 1.0f / (2.0f * 3.14159265f * hz);
    const float a = dt / (rc + dt);
    return prev + a * (in - prev);
}

void MPU6050_SetGyroLPF(float hz)
{
    if (hz >= 0.0f && hz <= 500.0f)
        gyro_lpf_hz = hz;
}
void MPU6050_SetAccelLPF(float hz)
{
    if (hz >= 0.0f && hz <= 500.0f)
        accel_lpf_hz = hz;
}
void MPU6050_SetCompTau(float s)
{
    if (s > 0.01f && s <= 10.0f)
        comp_tau_s = s;
}
float MPU6050_GetGyroLPF(void) { return gyro_lpf_hz; }
float MPU6050_GetAccelLPF(void) { return accel_lpf_hz; }
float MPU6050_GetCompTau(void) { return comp_tau_s; }

HAL_StatusTypeDef MPU6050_WhoAmI(I2C_HandleTypeDef *hi2c, uint8_t *id)
{
    return HAL_I2C_Mem_Read(hi2c, MPU6050_ADDR, MPU6050_WHO_AM_I,
                            I2C_MEMADD_SIZE_8BIT, id, 1, 100);
}

HAL_StatusTypeDef MPU6050_Init(I2C_HandleTypeDef *hi2c)
{
    uint8_t check;
    HAL_StatusTypeDef status;

    status = MPU6050_WhoAmI(hi2c, &check);
    if (status != HAL_OK)
        return status;
    if (check != 0x68)
        return HAL_ERROR;

    /* Aus Sleep-Mode wecken, Takt vom Gyro statt vom internen Oszillator
     * (stabiler, empfiehlt InvenSense selbst) */
    uint8_t data = 0x01;
    status = HAL_I2C_Mem_Write(hi2c, MPU6050_ADDR, MPU6050_PWR_MGMT_1,
                               I2C_MEMADD_SIZE_8BIT, &data, 1, 100);
    if (status != HAL_OK)
        return status;
    HAL_Delay(10);

    /* Interner Hardware-Tiefpass: DLPF_CFG = 1 -> Gyro 188 Hz, Accel 184 Hz.
     * Erste Stufe gegen Motorvibrationen. Gleichzeitig sinkt die interne
     * Gyro-Rate von 8 kHz auf 1 kHz - passend zur Regelschleife. */
    data = 0x01;
    status = HAL_I2C_Mem_Write(hi2c, MPU6050_ADDR, MPU6050_CONFIG,
                               I2C_MEMADD_SIZE_8BIT, &data, 1, 100);
    if (status != HAL_OK)
        return status;

    /* Sample Rate = 1 kHz / (1+0) = 1000 Hz - ein frischer Wert pro Regelzyklus */
    data = 0x00;
    status = HAL_I2C_Mem_Write(hi2c, MPU6050_ADDR, MPU6050_SMPLRT_DIV,
                               I2C_MEMADD_SIZE_8BIT, &data, 1, 100);
    if (status != HAL_OK)
        return status;

    /* Accel +-4g. Ein 5-Zoll-Quad sieht im Flug regelmaessig mehr als 2g,
     * und bei Anschlag liefert der Sensor sonst Unsinn. */
    data = 0x08;
    status = HAL_I2C_Mem_Write(hi2c, MPU6050_ADDR, MPU6050_ACCEL_CONFIG,
                               I2C_MEMADD_SIZE_8BIT, &data, 1, 100);
    if (status != HAL_OK)
        return status;

    /* Gyro +-500 Grad/s. +-250 reicht fuer Angle, aber im Rate-Modus
     * drehst du schneller, und dann wuerde der Sensor abschneiden. */
    data = 0x08;
    status = HAL_I2C_Mem_Write(hi2c, MPU6050_ADDR, MPU6050_GYRO_CONFIG,
                               I2C_MEMADD_SIZE_8BIT, &data, 1, 100);

    filt_primed = 0;
    return status;
}

HAL_StatusTypeDef MPU6050_ReadAll(I2C_HandleTypeDef *hi2c, MPU6050_Data_t *data)
{
    uint8_t buf[14];
    HAL_StatusTypeDef status;

    status = HAL_I2C_Mem_Read(hi2c, MPU6050_ADDR, MPU6050_ACCEL_XOUT_H,
                              I2C_MEMADD_SIZE_8BIT, buf, 14, 100);
    if (status != HAL_OK)
        return status;

    data->accel_x = (int16_t)((buf[0] << 8) | buf[1]);
    data->accel_y = (int16_t)((buf[2] << 8) | buf[3]);
    data->accel_z = (int16_t)((buf[4] << 8) | buf[5]);
    data->temp_raw = (int16_t)((buf[6] << 8) | buf[7]);
    data->gyro_x = (int16_t)((buf[8] << 8) | buf[9]);
    data->gyro_y = (int16_t)((buf[10] << 8) | buf[11]);
    data->gyro_z = (int16_t)((buf[12] << 8) | buf[13]);

    return HAL_OK;
}

/* --- Gyro-Kalibrierung: misst Offsets im Ruhezustand --- */
void MPU6050_Calibrate(I2C_HandleTypeDef *hi2c, MPU6050_Data_t *data, uint16_t samples)
{
    float sx = 0, sy = 0, sz = 0;
    for (uint16_t i = 0; i < samples; i++)
    {
        if (MPU6050_ReadAll(hi2c, data) == HAL_OK)
        {
            sx += data->gyro_x;
            sy += data->gyro_y;
            sz += data->gyro_z;
        }
        HAL_Delay(2);
    }
    data->gyro_x_offset = sx / samples;
    data->gyro_y_offset = sy / samples;
    data->gyro_z_offset = sz / samples;

    filt_primed = 0;
}

/* --- Rohwerte -> g und Grad/s, danach Tiefpass --- */
void MPU6050_Convert(MPU6050_Data_t *data, float dt)
{
    /* Skalierung passend zu den Bereichen aus MPU6050_Init:
     * Accel +-4g  -> halbe Empfindlichkeit
     * Gyro  +-500 -> halbe Empfindlichkeit */
    const float acc_s = ACCEL_SCALE / 2.0f; /* 8192 LSB pro g     */
    const float gyr_s = GYRO_SCALE / 2.0f;  /* 65.5 LSB pro Grad/s */

    const float ax = data->accel_x / acc_s;
    const float ay = data->accel_y / acc_s;
    const float az = data->accel_z / acc_s;

    const float gx = (data->gyro_x - data->gyro_x_offset) / gyr_s;
    const float gy = (data->gyro_y - data->gyro_y_offset) / gyr_s;
    const float gz = (data->gyro_z - data->gyro_z_offset) / gyr_s;

    /* ungefiltert mitfuehren, damit man im Stream vergleichen kann */
    data->gyro_x_raw_dps = gx;
    data->gyro_y_raw_dps = gy;
    data->gyro_z_raw_dps = gz;

    /* Beim ersten Durchlauf die Filter auf den Istwert setzen,
     * sonst laufen sie aus der Null hoch und erzeugen einen Einschwinger. */
    if (!filt_primed)
    {
        gx_f = gx;
        gy_f = gy;
        gz_f = gz;
        ax_f = ax;
        ay_f = ay;
        az_f = az;
        filt_primed = 1;
    }

    gx_f = pt1(gx_f, gx, gyro_lpf_hz, dt);
    gy_f = pt1(gy_f, gy, gyro_lpf_hz, dt);
    gz_f = pt1(gz_f, gz, gyro_lpf_hz, dt);

    ax_f = pt1(ax_f, ax, accel_lpf_hz, dt);
    ay_f = pt1(ay_f, ay, accel_lpf_hz, dt);
    az_f = pt1(az_f, az, accel_lpf_hz, dt);

    data->gyro_x_dps = gx_f;
    data->gyro_y_dps = gy_f;
    data->gyro_z_dps = gz_f;

    data->accel_x_g = ax_f;
    data->accel_y_g = ay_f;
    data->accel_z_g = az_f;
}

/* --- Komplementaerfilter: Accel + Gyro -> Roll/Pitch --- */
void MPU6050_UpdateAngles(MPU6050_Data_t *data, float dt)
{
    float accel_roll = atan2f(data->accel_y_g, data->accel_z_g) * 57.2958f;
    float accel_pitch = atan2f(-data->accel_x_g,
                               sqrtf(data->accel_y_g * data->accel_y_g +
                                     data->accel_z_g * data->accel_z_g)) *
                        57.2958f;

    /* Gewicht aus der Zeitkonstante rechnen statt fest 0.98.
     * Fest waere an die Schleifenfrequenz gekoppelt: bei 1000 Hz wuerde
     * 0.98 nur 49 ms bedeuten, der Winkel haenge fast nur am Accel. */
    const float alpha = comp_tau_s / (comp_tau_s + dt);

    data->roll = alpha * (data->roll + data->gyro_x_dps * dt) + (1.0f - alpha) * accel_roll;
    data->pitch = alpha * (data->pitch + data->gyro_y_dps * dt) + (1.0f - alpha) * accel_pitch;

    /* Yaw: nur Gyro-Integration (kein Accel-Bezug -> driftet langsam) */
    data->yaw += data->gyro_z_dps * dt;
}