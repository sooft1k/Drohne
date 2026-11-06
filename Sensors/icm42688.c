#include "icm42688.h"
#include <math.h>
#include <string.h>

/* Empfindlichkeit passend zu den Bereichen aus ICM42688_Init (Datenblatt) */
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

/* Erkennung eingefrorener Werte */
static uint8_t prev_buf[12];
static uint16_t frozen_run = 0;

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

void ICM42688_SetGyroLPF(float hz)
{
    if (hz >= 0.0f && hz <= 500.0f)
        gyro_lpf_hz = hz;
}
void ICM42688_SetAccelLPF(float hz)
{
    if (hz >= 0.0f && hz <= 500.0f)
        accel_lpf_hz = hz;
}
void ICM42688_SetCompTau(float s)
{
    if (s > 0.01f && s <= 10.0f)
        comp_tau_s = s;
}
float ICM42688_GetGyroLPF(void) { return gyro_lpf_hz; }
float ICM42688_GetAccelLPF(void) { return accel_lpf_hz; }
float ICM42688_GetCompTau(void) { return comp_tau_s; }

/* ===== SPI-Zugriff =====
 * Das erste Byte enthaelt die Adresse, Bit 7 = 1 heisst Lesen (Datenblatt,
 * Kapitel SPI). Der Chip-Select liegt auf PA4 und wird von Hand gesetzt. */
#define ICM_CS_PORT GPIOA
#define ICM_CS_PIN GPIO_PIN_4
#define SPI_TIMEOUT_MS 2

/* Gyro aus der Grundeinstellung: +-2000 Grad/s (000), 1 kHz (0110) */
#define GYRO_CONFIG0_VALUE 0x06
/* Accel: +-4 g (010), 1 kHz (0110) */
#define ACCEL_CONFIG0_VALUE 0x46
/* Hardware-Tiefpass Accel und Gyro: max(400 Hz, ODR)/4, bei 1 kHz also 250 Hz */
#define FILT_BW_VALUE 0x11
/* Gyro und Accel im Low-Noise-Modus */
#define PWR_MGMT0_VALUE 0x0F

static HAL_StatusTypeDef spi_xfer(SPI_HandleTypeDef *hspi, const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    HAL_GPIO_WritePin(ICM_CS_PORT, ICM_CS_PIN, GPIO_PIN_RESET);
    const HAL_StatusTypeDef st = HAL_SPI_TransmitReceive(hspi, (uint8_t *)tx, rx, len, SPI_TIMEOUT_MS);
    HAL_GPIO_WritePin(ICM_CS_PORT, ICM_CS_PIN, GPIO_PIN_SET);
    return st;
}

static HAL_StatusTypeDef reg_write(SPI_HandleTypeDef *hspi, uint8_t reg, uint8_t val)
{
    const uint8_t tx[2] = {(uint8_t)(reg & 0x7F), val};
    uint8_t rx[2];
    return spi_xfer(hspi, tx, rx, 2);
}

static HAL_StatusTypeDef reg_read(SPI_HandleTypeDef *hspi, uint8_t reg, uint8_t *val)
{
    const uint8_t tx[2] = {(uint8_t)(reg | 0x80), 0};
    uint8_t rx[2] = {0, 0};
    const HAL_StatusTypeDef st = spi_xfer(hspi, tx, rx, 2);
    *val = rx[1];
    return st;
}

/* SPI kennt kein ACK: ein Schreibfehler faellt nur beim Zuruecklesen auf */
static HAL_StatusTypeDef reg_write_check(SPI_HandleTypeDef *hspi, uint8_t reg, uint8_t val)
{
    uint8_t back = 0;
    HAL_StatusTypeDef st = reg_write(hspi, reg, val);
    if (st != HAL_OK)
        return st;
    st = reg_read(hspi, reg, &back);
    if (st != HAL_OK)
        return st;
    return (back == val) ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef ICM42688_WhoAmI(SPI_HandleTypeDef *hspi, uint8_t *id)
{
    return reg_read(hspi, ICM42688_WHO_AM_I, id);
}

/* Blockiert etwa 4 ms. Der Gyro braucht danach noch 30 ms, bis er gueltige
 * Werte liefert (Datenblatt, Start-Up Time). */
HAL_StatusTypeDef ICM42688_Init(SPI_HandleTypeDef *hspi)
{
    uint8_t check = 0;
    HAL_StatusTypeDef st;

    /* Nach dem Soft-Reset 1 ms warten (Datenblatt, DEVICE_CONFIG) */
    st = reg_write(hspi, ICM42688_DEVICE_CONFIG, 0x01);
    if (st != HAL_OK)
        return st;
    HAL_Delay(2);

    st = reg_read(hspi, ICM42688_WHO_AM_I, &check);
    if (st != HAL_OK)
        return st;
    if (check != ICM42688_WHO_AM_I_VALUE)
        return HAL_ERROR;

    st = reg_write_check(hspi, ICM42688_GYRO_CONFIG0, GYRO_CONFIG0_VALUE);
    if (st != HAL_OK)
        return st;
    st = reg_write_check(hspi, ICM42688_ACCEL_CONFIG0, ACCEL_CONFIG0_VALUE);
    if (st != HAL_OK)
        return st;
    st = reg_write_check(hspi, ICM42688_GYRO_ACCEL_CONFIG0, FILT_BW_VALUE);
    if (st != HAL_OK)
        return st;

    st = reg_write(hspi, ICM42688_PWR_MGMT0, PWR_MGMT0_VALUE);
    if (st != HAL_OK)
        return st;
    /* Nach dem Einschalten 200 us keine Schreibzugriffe (Datenblatt, PWR_MGMT0) */
    HAL_Delay(1);

    st = reg_read(hspi, ICM42688_PWR_MGMT0, &check);
    if (st != HAL_OK)
        return st;
    if (check != PWR_MGMT0_VALUE)
        return HAL_ERROR;

    filt_primed = 0;
    frozen_run = 0;
    return HAL_OK;
}

/* Liest Temperatur, Accel und Gyro am Stueck: 14 Bytes ab TEMP_DATA1, MSB zuerst.
 * Bei 10,5 MHz dauert das rund 12 us, blockiert die Regelung also praktisch
 * nicht. Deshalb laeuft es ohne Interrupt direkt in der Hauptschleife. */
static HAL_StatusTypeDef read_burst(SPI_HandleTypeDef *hspi, uint8_t *buf)
{
    uint8_t tx[15] = {0};
    uint8_t rx[15];
    tx[0] = ICM42688_TEMP_DATA1 | 0x80;
    const HAL_StatusTypeDef st = spi_xfer(hspi, tx, rx, 15);
    if (st != HAL_OK)
        return st;
    memcpy(buf, &rx[1], 14);

    /* Ohne Sensor liest MISO nur Nullen oder Einsen, und SPI meldet keinen
     * Fehler. So faellt ein abgerissener Sensor sofort auf. */
    uint8_t all_00 = 1, all_ff = 1;
    for (int i = 0; i < 14; i++)
    {
        if (buf[i] != 0x00)
            all_00 = 0;
        if (buf[i] != 0xFF)
            all_ff = 0;
    }
    return (all_00 || all_ff) ? HAL_ERROR : HAL_OK;
}

static void decode(const uint8_t *buf, ICM42688_Data_t *data)
{
    data->temp_raw = (int16_t)((buf[0] << 8) | buf[1]);
    data->accel_x = (int16_t)((buf[2] << 8) | buf[3]);
    data->accel_y = (int16_t)((buf[4] << 8) | buf[5]);
    data->accel_z = (int16_t)((buf[6] << 8) | buf[7]);
    data->gyro_x = (int16_t)((buf[8] << 8) | buf[9]);
    data->gyro_y = (int16_t)((buf[10] << 8) | buf[11]);
    data->gyro_z = (int16_t)((buf[12] << 8) | buf[13]);
}

HAL_StatusTypeDef ICM42688_ReadAll(SPI_HandleTypeDef *hspi, ICM42688_Data_t *data)
{
    uint8_t buf[14];
    const HAL_StatusTypeDef st = read_burst(hspi, buf);
    if (st == HAL_OK)
        decode(buf, data);
    return st;
}

/* Gesundheit */
static uint16_t fail_run = 0;   /* Fehler in Folge */
static uint16_t good_run = 0;   /* gute Messungen in Folge (gesaettigt) */
static uint8_t failed = 0;      /* 1 = Sensor gilt als ausgefallen */
static uint32_t err_total = 0;

int ICM42688_Read(SPI_HandleTypeDef *hspi, ICM42688_Data_t *data)
{
    uint8_t buf[14];
    if (read_burst(hspi, buf) != HAL_OK)
        return -1;
    decode(buf, data);

    /* Eingefrorene Werte erkennen: Accel und Gyro rauschen immer etwas,
     * 100 Mal exakt dieselben Bytes heisst, der Sensor liefert nichts
     * Neues mehr. Die Temperatur (Byte 0, 1) zaehlt nicht mit. */
    const uint8_t same = (memcmp(&buf[2], prev_buf, 12) == 0) ? 1 : 0;
    memcpy(prev_buf, &buf[2], 12);
    frozen_run = same ? (uint16_t)(frozen_run + 1) : 0;

    if (frozen_run >= ICM42688_FROZEN_LIMIT)
    {
        failed = 1; /* eingefroren: sofort als ausgefallen werten */
        good_run = 0;
        return -1;
    }
    return 1;
}

void ICM42688_ReportResult(uint8_t ok)
{
    if (ok)
    {
        fail_run = 0;
        if (good_run < 0xFFFF)
            good_run++;
        if (failed && good_run >= ICM42688_GOOD_TO_RECOVER)
            failed = 0;
    }
    else
    {
        err_total++;
        good_run = 0;
        if (fail_run < 0xFFFF)
            fail_run++;
        if (fail_run >= ICM42688_FAIL_LIMIT)
            failed = 1;
    }
}

uint8_t ICM42688_Failed(void) { return failed; }
uint16_t ICM42688_FailRun(void) { return fail_run; }
uint32_t ICM42688_ErrorCount(void) { return err_total; }

uint8_t ICM42688_Healthy(void)
{
    return !failed && good_run >= ICM42688_GOOD_TO_RECOVER;
}

/* --- Gyro-Kalibrierung: misst Offsets im Ruhezustand --- */
/* Misst die Offsets im Ruhezustand.
 * Geteilt wird durch die Zahl der erfolgreich gelesenen Werte. Ausserdem
 * wird geprueft, ob die Drohne still lag: Streut eine Achse staerker als
 * ICM42688_CAL_MAX_STD_DPS, wurde sie bewegt und die Offsets taugen nichts.
 * Die Offsets werden nur bei Erfolg uebernommen. */
ICM42688_CalResult_t ICM42688_Calibrate(SPI_HandleTypeDef *hspi, ICM42688_Data_t *data, uint16_t samples)
{
    /* double: bei grossem Offset waere float fuer die Varianz zu ungenau.
     * Laeuft nur beim Start, die Rechenzeit spielt keine Rolle. */
    double sx = 0, sy = 0, sz = 0; /* Summen */
    double qx = 0, qy = 0, qz = 0; /* Quadratsummen */
    uint16_t n = 0;
    for (uint16_t i = 0; i < samples; i++)
    {
        if (ICM42688_ReadAll(hspi, data) == HAL_OK)
        {
            const double x = data->gyro_x, y = data->gyro_y, z = data->gyro_z;
            sx += x;
            sy += y;
            sz += z;
            qx += x * x;
            qy += y * y;
            qz += z * z;
            n++;
        }
        HAL_Delay(2);
    }

    if (n < samples * 9 / 10) /* mehr als 10 % Lesefehler */
        return ICM42688_CAL_LESEFEHLER;

    const double mx = sx / n, my = sy / n, mz = sz / n;
    /* Varianz = Mittel der Quadrate - Quadrat des Mittels (in LSB^2) */
    const double lim = ICM42688_CAL_MAX_STD_DPS * GYRO_LSB_PER_DPS;
    const double lim2 = lim * lim;
    if (qx / n - mx * mx > lim2 || qy / n - my * my > lim2 || qz / n - mz * mz > lim2)
        return ICM42688_CAL_BEWEGT;
    /* Eine langsame, gleichmaessige Drehung streut nicht - aber ein Offset
     * ueber 40 Grad/s ist fuer einen ruhenden Sensor nicht plausibel.
     * Ganz langsame Drehungen darunter lassen sich nicht erkennen. */
    const double mlim = 40.0 * GYRO_LSB_PER_DPS;
    if (mx > mlim || mx < -mlim || my > mlim || my < -mlim || mz > mlim || mz < -mlim)
        return ICM42688_CAL_BEWEGT;

    data->gyro_x_offset = (float)mx;
    data->gyro_y_offset = (float)my;
    data->gyro_z_offset = (float)mz;
    filt_primed = 0;
    return ICM42688_CAL_OK;
}

/* Lagewinkel direkt aus dem Accel setzen - nach dem Start, sonst laeuft der
 * Komplementaerfilter erst ueber 1-2 s von 0 auf den echten Winkel. */
void ICM42688_InitAngles(ICM42688_Data_t *data)
{
    data->roll = atan2f(-data->accel_y_g, -data->accel_z_g) * 57.2958f;
    data->pitch = atan2f(data->accel_x_g,
                         sqrtf(data->accel_y_g * data->accel_y_g +
                               data->accel_z_g * data->accel_z_g)) *
                  57.2958f;
    data->yaw = 0.0f;
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
#error "IMU_ALIGN in icm42688.h ist ungueltig"
#endif
    *bz = -cz;
}

/* --- Rohwerte -> g und Grad/s, Achsen ausrichten, danach Tiefpass --- */
void ICM42688_Convert(ICM42688_Data_t *data, float dt)
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
void ICM42688_UpdateAngles(ICM42688_Data_t *data, float dt)
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