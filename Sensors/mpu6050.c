#include "mpu6050.h"
#include <math.h>

#define ACCEL_SCALE 16384.0f /* LSB pro g  (±2g)      */
#define GYRO_SCALE 131.0f    /* LSB pro °/s (±250°/s) */
#define ALPHA 0.98f          /* Komplementärfilter-Gewicht */

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

    /* Aus Sleep-Mode wecken */
    uint8_t data = 0x00;
    status = HAL_I2C_Mem_Write(hi2c, MPU6050_ADDR, MPU6050_PWR_MGMT_1,
                               I2C_MEMADD_SIZE_8BIT, &data, 1, 100);
    if (status != HAL_OK)
        return status;

    /* Sample Rate = 1kHz / (1+7) = 125 Hz */
    data = 0x07;
    status = HAL_I2C_Mem_Write(hi2c, MPU6050_ADDR, MPU6050_SMPLRT_DIV,
                               I2C_MEMADD_SIZE_8BIT, &data, 1, 100);
    if (status != HAL_OK)
        return status;

    /* Accel ±2g */
    data = 0x00;
    status = HAL_I2C_Mem_Write(hi2c, MPU6050_ADDR, MPU6050_ACCEL_CONFIG,
                               I2C_MEMADD_SIZE_8BIT, &data, 1, 100);
    if (status != HAL_OK)
        return status;

    /* Gyro ±250°/s */
    data = 0x00;
    status = HAL_I2C_Mem_Write(hi2c, MPU6050_ADDR, MPU6050_GYRO_CONFIG,
                               I2C_MEMADD_SIZE_8BIT, &data, 1, 100);
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
}

/* --- Rohwerte -> g und °/s umrechnen (mit Offset-Korrektur) --- */
void MPU6050_Convert(MPU6050_Data_t *data)
{
    data->accel_x_g = data->accel_x / ACCEL_SCALE;
    data->accel_y_g = data->accel_y / ACCEL_SCALE;
    data->accel_z_g = data->accel_z / ACCEL_SCALE;

    data->gyro_x_dps = (data->gyro_x - data->gyro_x_offset) / GYRO_SCALE;
    data->gyro_y_dps = (data->gyro_y - data->gyro_y_offset) / GYRO_SCALE;
    data->gyro_z_dps = (data->gyro_z - data->gyro_z_offset) / GYRO_SCALE;
}

/* --- Komplementärfilter: Accel + Gyro -> Roll/Pitch --- */
void MPU6050_UpdateAngles(MPU6050_Data_t *data, float dt)
{
    float accel_roll = atan2f(data->accel_y_g, data->accel_z_g) * 57.2958f;
    float accel_pitch = atan2f(-data->accel_x_g,
                               sqrtf(data->accel_y_g * data->accel_y_g +
                                     data->accel_z_g * data->accel_z_g)) *
                        57.2958f;

    data->roll = ALPHA * (data->roll + data->gyro_x_dps * dt) + (1.0f - ALPHA) * accel_roll;
    data->pitch = ALPHA * (data->pitch + data->gyro_y_dps * dt) + (1.0f - ALPHA) * accel_pitch;

    /* Yaw: nur Gyro-Integration (kein Accel-Bezug -> driftet langsam) */
    data->yaw += data->gyro_z_dps * dt;
}