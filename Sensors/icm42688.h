#ifndef ICM42688_H
#define ICM42688_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* Register in Bank 0, Datenblatt Kapitel 14 */
#define ICM42688_DEVICE_CONFIG 0x11
#define ICM42688_TEMP_DATA1 0x1D
#define ICM42688_PWR_MGMT0 0x4E
#define ICM42688_GYRO_CONFIG0 0x4F
#define ICM42688_ACCEL_CONFIG0 0x50
#define ICM42688_GYRO_ACCEL_CONFIG0 0x52
#define ICM42688_WHO_AM_I 0x75
#define ICM42688_WHO_AM_I_VALUE 0x47

/* Startwerte der Filter */
#define GYRO_LPF_DEFAULT_HZ 90.0f  /* Software-Tiefpass auf die Drehraten */
#define ACCEL_LPF_DEFAULT_HZ 20.0f /* Software-Tiefpass auf die Beschleunigung */
#define COMP_TAU_DEFAULT_S 0.50f   /* Zeitkonstante des Komplementaerfilters */

/* ===== Einbaulage des Sensors =====
 * Der Chip liegt mit der Bauteilseite nach OBEN. Eingestellt wird, wohin
 * die X-Achse des Chips zeigt (Blick von oben, Nase = vorne). Sie haengt von
 * der Lage auf der Platine ab und muss nach dem Layout geprueft werden.
 * Daraus rechnet ICM42688_Convert die Chip-Achsen in die Koerperachsen um
 * (X vorne, Y rechts, Z unten - Konvention siehe mixer.c).
 *
 * Pruefen mit dem Kipptest aus dem README: Stimmt ein Vorzeichen nicht,
 * ist hier die falsche Lage eingestellt. */
#define IMU_X_VORNE 0
#define IMU_X_RECHTS 1
#define IMU_X_HINTEN 2
#define IMU_X_LINKS 3

#define IMU_ALIGN IMU_X_VORNE

typedef struct
{
    /* Rohwerte in Chip-Achsen */
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
    int16_t temp_raw;

    /* Umgerechnet und gefiltert, in KOERPERACHSEN - das benutzt die Regelung.
     * gyro_x = Rolldrehrate, gyro_y = Nickdrehrate, gyro_z = Gierdrehrate.
     * Im Stand liegt accel_z bei -1 g (Z zeigt nach unten). */
    float accel_x_g;
    float accel_y_g;
    float accel_z_g;
    float gyro_x_dps;
    float gyro_y_dps;
    float gyro_z_dps;

    /* Ungefiltert, Koerperachsen - nur zum Vergleichen im Stream */
    float gyro_x_raw_dps;
    float gyro_y_raw_dps;
    float gyro_z_raw_dps;

    /* Gyro-Offsets in Chip-Achsen (aus Kalibrierung) */
    float gyro_x_offset;
    float gyro_y_offset;
    float gyro_z_offset;

    /* Gefilterte Lagewinkel in Grad, Vorzeichen siehe mixer.c */
    float roll;
    float pitch;
    float yaw;
} ICM42688_Data_t;

HAL_StatusTypeDef ICM42688_Init(SPI_HandleTypeDef *hspi);
HAL_StatusTypeDef ICM42688_WhoAmI(SPI_HandleTypeDef *hspi, uint8_t *id);
HAL_StatusTypeDef ICM42688_ReadAll(SPI_HandleTypeDef *hspi, ICM42688_Data_t *data);

/* Gyro-Kalibrierung. Gilt als bewegt, wenn eine Achse staerker streut */
#define ICM42688_CAL_MAX_STD_DPS 1.0f
typedef enum
{
    ICM42688_CAL_OK = 0,
    ICM42688_CAL_BEWEGT,     /* Drohne wurde bewegt */
    ICM42688_CAL_LESEFEHLER  /* mehr als 10 % der Messungen nicht lesbar */
} ICM42688_CalResult_t;
ICM42688_CalResult_t ICM42688_Calibrate(SPI_HandleTypeDef *hspi, ICM42688_Data_t *data, uint16_t samples);

/* Roll/Pitch aus dem Accel setzen (braucht umgerechnete Werte) */
void ICM42688_InitAngles(ICM42688_Data_t *data);

/* ===== Lesen im Regelbetrieb ===== */

/* Fehler in Folge, ab denen der Sensor als ausgefallen gilt (1 pro ms) */
#define ICM42688_FAIL_LIMIT 20
/* Gute Messungen in Folge, ab denen er wieder als gesund gilt (1 pro ms) */
#define ICM42688_GOOD_TO_RECOVER 500
/* Identische Messungen in Folge, ab denen die Werte als eingefroren gelten
 * (der Sensor gilt dann sofort als ausgefallen) */
#define ICM42688_FROZEN_LIMIT 100

/* Liest einen Messwert, einmal pro Regelzyklus. 1 = neue Rohwerte in data,
 * -1 = Fehler (Sensor antwortet nicht oder Werte eingefroren). */
int ICM42688_Read(SPI_HandleTypeDef *hspi, ICM42688_Data_t *data);

/* Ergebnis jedes Regelzyklus melden, fuehrt den Fehlerzaehler */
void ICM42688_ReportResult(uint8_t ok);
uint8_t ICM42688_Failed(void);      /* 1 = ICM42688_FAIL_LIMIT Fehler in Folge */
uint8_t ICM42688_Healthy(void);     /* 1 = nicht ausgefallen und seit 0,5 s stabil */
uint16_t ICM42688_FailRun(void);    /* aktuelle Fehler in Folge */
uint32_t ICM42688_ErrorCount(void); /* Fehler seit dem Start */

/* Braucht jetzt dt, weil die Filter darauf rechnen */
void ICM42688_Convert(ICM42688_Data_t *data, float dt);
void ICM42688_UpdateAngles(ICM42688_Data_t *data, float dt);

/* Filter zur Laufzeit einstellen - 0 schaltet den jeweiligen Filter ab */
void ICM42688_SetGyroLPF(float hz);
void ICM42688_SetAccelLPF(float hz);
void ICM42688_SetCompTau(float seconds);
float ICM42688_GetGyroLPF(void);
float ICM42688_GetAccelLPF(void);
float ICM42688_GetCompTau(void);

#endif