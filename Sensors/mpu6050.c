#include "mpu6050.h"
#include <math.h>

/* Empfindlichkeit passend zu den Bereichen aus MPU6050_Init (Datenblatt) */
#define ACCEL_LSB_PER_G 8192.0f /* +-4 g          */
#define GYRO_LSB_PER_DPS 16.4f  /* +-2000 Grad/s  */

/* Einstellbare Filterparameter */
static float gyro_lpf_hz = GYRO_LPF_DEFAULT_HZ;
static float accel_lpf_hz = ACCEL_LPF_DEFAULT_HZ;
static float comp_tau_s = COMP_TAU_DEFAULT_S;

/* Zustand der Tiefpaesse */
static float gx_f = 0, gy_f = 0, gz_f = 0;
static float ax_f = 0, ay_f = 0, az_f = -1.0f;
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
    return MPU6050_InitTimeout(hi2c, 100);
}

/* Wie MPU6050_Init, aber mit waehlbarem Timeout pro I2C-Transfer (ms).
 * Die Bus-Recovery nutzt kurze Timeouts, damit sie den Watchdog nicht
 * ausloest. */
HAL_StatusTypeDef MPU6050_InitTimeout(I2C_HandleTypeDef *hi2c, uint32_t timeout)
{
    uint8_t check;
    HAL_StatusTypeDef status;

    status = HAL_I2C_Mem_Read(hi2c, MPU6050_ADDR, MPU6050_WHO_AM_I,
                              I2C_MEMADD_SIZE_8BIT, &check, 1, timeout);
    if (status != HAL_OK)
        return status;
    if (check != 0x68)
        return HAL_ERROR;

    /* Aus Sleep-Mode wecken, Takt vom Gyro statt vom internen Oszillator
     * (stabiler, empfiehlt InvenSense selbst) */
    uint8_t data = 0x01;
    status = HAL_I2C_Mem_Write(hi2c, MPU6050_ADDR, MPU6050_PWR_MGMT_1,
                               I2C_MEMADD_SIZE_8BIT, &data, 1, timeout);
    if (status != HAL_OK)
        return status;
    HAL_Delay(10);

    /* Interner Hardware-Tiefpass: DLPF_CFG = 1 -> Gyro 188 Hz, Accel 184 Hz.
     * Erste Stufe gegen Motorvibrationen. Gleichzeitig sinkt die interne
     * Gyro-Rate von 8 kHz auf 1 kHz - passend zur Regelschleife. */
    data = 0x01;
    status = HAL_I2C_Mem_Write(hi2c, MPU6050_ADDR, MPU6050_CONFIG,
                               I2C_MEMADD_SIZE_8BIT, &data, 1, timeout);
    if (status != HAL_OK)
        return status;

    /* Sample Rate = 1 kHz / (1+0) = 1000 Hz - ein frischer Wert pro Regelzyklus */
    data = 0x00;
    status = HAL_I2C_Mem_Write(hi2c, MPU6050_ADDR, MPU6050_SMPLRT_DIV,
                               I2C_MEMADD_SIZE_8BIT, &data, 1, timeout);
    if (status != HAL_OK)
        return status;

    /* Accel +-4g. Ein 5-Zoll-Quad sieht im Flug regelmaessig mehr als 2g,
     * und bei Anschlag liefert der Sensor sonst Unsinn. */
    data = 0x08;
    status = HAL_I2C_Mem_Write(hi2c, MPU6050_ADDR, MPU6050_ACCEL_CONFIG,
                               I2C_MEMADD_SIZE_8BIT, &data, 1, timeout);
    if (status != HAL_OK)
        return status;

    /* Gyro +-2000 Grad/s (FS_SEL = 3). Bei Flips und Stoessen dreht ein
     * Quad schnell ueber 500 Grad/s - der Sensor wuerde abschneiden und der
     * Lagewinkel danach nicht mehr stimmen. */
    data = 0x18;
    status = HAL_I2C_Mem_Write(hi2c, MPU6050_ADDR, MPU6050_GYRO_CONFIG,
                               I2C_MEMADD_SIZE_8BIT, &data, 1, timeout);

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

/* ===== Nicht blockierendes Lesen per Interrupt =====
 * Der TIM2-Interrupt startet jede Millisekunde einen Lesevorgang, die
 * I2C-Interrupts der HAL arbeiten ihn ab, die Hauptschleife holt das
 * Ergebnis mit MPU6050_ReadPoll ab.
 * Die HAL wartet in HAL_I2C_Mem_Read_IT bis zu 25 ms auf ein freies
 * BUSY-Flag - deshalb wird das vorher selbst geprueft. */
static I2C_HandleTypeDef *rd_i2c = 0;
static uint8_t rd_buf[14];
static uint8_t prev_buf[14];
static volatile uint8_t rd_active = 0;  /* Transfer laeuft */
static volatile uint8_t rd_done = 0;    /* Ergebnis liegt vor, noch nicht abgeholt */
static volatile uint8_t rd_ok = 0;      /* Ergebnis ist gueltig */
static volatile uint8_t rd_blocked = 0; /* 1 = nichts starten (Bus-Recovery) */

/* Gesundheit */
static uint16_t fail_run = 0;   /* Fehler in Folge */
static uint16_t good_run = 0;   /* gute Messungen in Folge (gesaettigt) */
static uint16_t frozen_run = 0; /* identische Messungen in Folge */
static uint8_t failed = 0;      /* 1 = Sensor gilt als ausgefallen */
static uint32_t err_total = 0;

HAL_StatusTypeDef MPU6050_StartRead(I2C_HandleTypeDef *hi2c)
{
    /* Altes Ergebnis noch nicht abgeholt: nicht ueberschreiben */
    if (rd_blocked || rd_active || rd_done)
        return HAL_BUSY;

    rd_i2c = hi2c;
    if (__HAL_I2C_GET_FLAG(hi2c, I2C_FLAG_BUSY) != RESET ||
        HAL_I2C_GetState(hi2c) != HAL_I2C_STATE_READY)
    {
        /* Bus haengt oder Peripherie ist noch beschaeftigt */
        rd_ok = 0;
        rd_done = 1;
        return HAL_BUSY;
    }

    rd_active = 1;
    if (HAL_I2C_Mem_Read_IT(hi2c, MPU6050_ADDR, MPU6050_ACCEL_XOUT_H,
                            I2C_MEMADD_SIZE_8BIT, rd_buf, 14) != HAL_OK)
    {
        rd_active = 0;
        rd_ok = 0;
        rd_done = 1;
        return HAL_ERROR;
    }
    return HAL_OK;
}

void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c != rd_i2c)
        return;
    rd_ok = 1;
    rd_active = 0;
    rd_done = 1;
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c != rd_i2c)
        return;
    rd_ok = 0;
    rd_active = 0;
    rd_done = 1;
}

int MPU6050_ReadPoll(MPU6050_Data_t *data)
{
    /* rd_active VOR rd_done lesen: der I2C-Interrupt setzt erst rd_active = 0
     * und dann rd_done = 1. Andersherum gelesen koennte ein gerade fertig
     * gewordener Transfer als "nichts gestartet" = Fehler zaehlen. */
    const uint8_t act = rd_active;
    if (!rd_done)
        return act ? 0 : -1;

    const uint8_t ok = rd_ok;
    if (ok)
    {
        data->accel_x = (int16_t)((rd_buf[0] << 8) | rd_buf[1]);
        data->accel_y = (int16_t)((rd_buf[2] << 8) | rd_buf[3]);
        data->accel_z = (int16_t)((rd_buf[4] << 8) | rd_buf[5]);
        data->temp_raw = (int16_t)((rd_buf[6] << 8) | rd_buf[7]);
        data->gyro_x = (int16_t)((rd_buf[8] << 8) | rd_buf[9]);
        data->gyro_y = (int16_t)((rd_buf[10] << 8) | rd_buf[11]);
        data->gyro_z = (int16_t)((rd_buf[12] << 8) | rd_buf[13]);

        /* Eingefrorene Werte erkennen: Accel und Gyro rauschen immer etwas,
         * 100 Mal exakt dieselben Bytes heisst, der Sensor liefert nichts
         * Neues mehr. Die Temperatur (Byte 6, 7) zaehlt nicht mit. */
        uint8_t same = 1;
        for (int i = 0; i < 14; i++)
        {
            if (i == 6 || i == 7)
                continue;
            if (rd_buf[i] != prev_buf[i])
                same = 0;
            prev_buf[i] = rd_buf[i];
        }
        frozen_run = same ? (uint16_t)(frozen_run + 1) : 0;
    }

    /* Erst nach dem Kopieren freigeben, sonst koennte der naechste
     * Lesevorgang rd_buf schon wieder beschreiben. */
    rd_done = 0;

    if (frozen_run >= MPU6050_FROZEN_LIMIT)
    {
        failed = 1; /* eingefroren: sofort als ausgefallen werten */
        good_run = 0;
        return -1;
    }
    if (!ok)
        return -1;
    return 1;
}

uint8_t MPU6050_ReadBusy(void)
{
    return rd_active;
}

void MPU6050_BlockReads(uint8_t block)
{
    /* Beim Sperren und beim Freigeben einen evtl. halb fertigen Transfer
     * verwerfen - die Recovery setzt die I2C-Peripherie dazwischen zurueck. */
    if (block)
        rd_blocked = 1;
    rd_active = 0;
    rd_done = 0;
    __COMPILER_BARRIER();
    if (!block)
        rd_blocked = 0; /* erst freigeben, wenn alles zurueckgesetzt ist */
}

void MPU6050_ReportResult(uint8_t ok)
{
    if (ok)
    {
        fail_run = 0;
        if (good_run < 0xFFFF)
            good_run++;
        if (failed && good_run >= MPU6050_GOOD_TO_RECOVER)
            failed = 0;
    }
    else
    {
        err_total++;
        good_run = 0;
        if (fail_run < 0xFFFF)
            fail_run++;
        if (fail_run >= MPU6050_FAIL_LIMIT)
            failed = 1;
    }
}

uint8_t MPU6050_Failed(void) { return failed; }
uint16_t MPU6050_FailRun(void) { return fail_run; }
uint32_t MPU6050_ErrorCount(void) { return err_total; }

uint8_t MPU6050_Healthy(void)
{
    return !failed && good_run >= MPU6050_GOOD_TO_RECOVER;
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

/* Chip-Achsen -> Koerperachsen (X vorne, Y rechts, Z unten).
 * Der Chip liegt mit der Bauteilseite oben, also zeigt Chip-Z nach oben
 * und Koerper-Z = -Chip-Z. Chip-Y liegt von oben gesehen 90 Grad gegen
 * den Uhrzeigersinn neben Chip-X. Gilt fuer Drehrate und Beschleunigung. */
static void align(float cx, float cy, float cz, float *bx, float *by, float *bz)
{
#if IMU_ALIGN == IMU_X_VORNE
    *bx = cx;  /* Chip-X vorne, Chip-Y links  */
    *by = -cy;
#elif IMU_ALIGN == IMU_X_RECHTS
    *bx = cy;  /* Chip-X rechts, Chip-Y vorne */
    *by = cx;
#elif IMU_ALIGN == IMU_X_HINTEN
    *bx = -cx; /* Chip-X hinten, Chip-Y rechts */
    *by = cy;
#elif IMU_ALIGN == IMU_X_LINKS
    *bx = -cy; /* Chip-X links, Chip-Y hinten */
    *by = -cx;
#else
#error "IMU_ALIGN in mpu6050.h ist ungueltig"
#endif
    *bz = -cz;
}

/* --- Rohwerte -> g und Grad/s, Achsen ausrichten, danach Tiefpass --- */
void MPU6050_Convert(MPU6050_Data_t *data, float dt)
{
    const float acc_s = ACCEL_LSB_PER_G;
    const float gyr_s = GYRO_LSB_PER_DPS;

    float ax, ay, az, gx, gy, gz;
    align(data->accel_x / acc_s, data->accel_y / acc_s, data->accel_z / acc_s,
          &ax, &ay, &az);
    align((data->gyro_x - data->gyro_x_offset) / gyr_s,
          (data->gyro_y - data->gyro_y_offset) / gyr_s,
          (data->gyro_z - data->gyro_z_offset) / gyr_s,
          &gx, &gy, &gz);

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

/* --- Komplementaerfilter: Accel + Gyro -> Roll/Pitch ---
 * Der Accel misst im Stand die Gegenkraft zur Schwerkraft, sie zeigt nach
 * oben, also in Koerperachsen (0, 0, -1 g). Rechte Seite runter -> die
 * Kraft bekommt einen Anteil nach links (-Y), Nase hoch -> nach vorne (+X). */
void MPU6050_UpdateAngles(MPU6050_Data_t *data, float dt)
{
    float accel_roll = atan2f(-data->accel_y_g, -data->accel_z_g) * 57.2958f;
    float accel_pitch = atan2f(data->accel_x_g,
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