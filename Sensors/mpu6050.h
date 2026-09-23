#ifndef MPU6050_H
#define MPU6050_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

#define MPU6050_ADDR (0x68 << 1)
#define MPU6050_WHO_AM_I 0x75
#define MPU6050_PWR_MGMT_1 0x6B
#define MPU6050_SMPLRT_DIV 0x19
#define MPU6050_CONFIG 0x1A
#define MPU6050_GYRO_CONFIG 0x1B
#define MPU6050_ACCEL_CONFIG 0x1C
#define MPU6050_ACCEL_XOUT_H 0x3B
#define MPU6050_GYRO_XOUT_H 0x43

/* Startwerte der Filter */
#define GYRO_LPF_DEFAULT_HZ 90.0f  /* Software-Tiefpass auf die Drehraten */
#define ACCEL_LPF_DEFAULT_HZ 20.0f /* Software-Tiefpass auf die Beschleunigung */
#define COMP_TAU_DEFAULT_S 0.50f   /* Zeitkonstante des Komplementaerfilters */

typedef struct
{
    /* Rohwerte */
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
    int16_t temp_raw;

    /* Umgerechnet und gefiltert - das benutzt die Regelung */
    float accel_x_g;
    float accel_y_g;
    float accel_z_g;
    float gyro_x_dps;
    float gyro_y_dps;
    float gyro_z_dps;

    /* Ungefiltert - nur zum Vergleichen im Stream */
    float gyro_x_raw_dps;
    float gyro_y_raw_dps;
    float gyro_z_raw_dps;

    /* Gyro-Offsets (aus Kalibrierung) */
    float gyro_x_offset;
    float gyro_y_offset;
    float gyro_z_offset;

    /* Gefilterte Lagewinkel */
    float roll;
    float pitch;
    float yaw;
} MPU6050_Data_t;

HAL_StatusTypeDef MPU6050_Init(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef MPU6050_WhoAmI(I2C_HandleTypeDef *hi2c, uint8_t *id);
HAL_StatusTypeDef MPU6050_ReadAll(I2C_HandleTypeDef *hi2c, MPU6050_Data_t *data);

void MPU6050_Calibrate(I2C_HandleTypeDef *hi2c, MPU6050_Data_t *data, uint16_t samples);

/* Braucht jetzt dt, weil die Filter darauf rechnen */
void MPU6050_Convert(MPU6050_Data_t *data, float dt);
void MPU6050_UpdateAngles(MPU6050_Data_t *data, float dt);

/* Filter zur Laufzeit einstellen - 0 schaltet den jeweiligen Filter ab */
void MPU6050_SetGyroLPF(float hz);
void MPU6050_SetAccelLPF(float hz);
void MPU6050_SetCompTau(float seconds);
float MPU6050_GetGyroLPF(void);
float MPU6050_GetAccelLPF(void);
float MPU6050_GetCompTau(void);

#endif