#include "bmp388.h"
#include <math.h>

/* Register, Datenblatt Kapitel 4 */
#define REG_CHIP_ID 0x00
#define REG_ERR 0x02
#define REG_PWR_CTRL 0x1B
#define REG_OSR 0x1C
#define REG_ODR 0x1D
#define REG_CONFIG 0x1F
#define REG_CALIB 0x31
#define REG_CMD 0x7E

#define CHIP_ID 0x50
#define CMD_SOFTRESET 0xB6

#define CALIB_LEN 21 /* 0x31..0x45 */

/* Status 0x03 */
#define STATUS_DRDY_PRESS 0x20
#define STATUS_DRDY_TEMP 0x40

/* So viele Messungen ergeben den Nullpunkt der Hoehe (1 s bei 50 Hz) */
#define REF_SAMPLES 50

/* Kalibrierwerte, umgerechnet nach Datenblatt Kapitel 9.1 */
static float par_t1, par_t2, par_t3;
static float par_p1, par_p2, par_p3, par_p4, par_p5, par_p6;
static float par_p7, par_p8, par_p9, par_p10, par_p11;

static float press_pa = 0.0f;
static float temp_c = 0.0f;
static float alt_m = 0.0f;
static float ref_sum = 0.0f;
static float ref_pa = 0.0f;
static uint8_t ref_n = 0;

static HAL_StatusTypeDef wr(I2C_HandleTypeDef *hi2c, uint8_t reg, uint8_t val, uint32_t timeout)
{
    return HAL_I2C_Mem_Write(hi2c, BMP388_ADDR, reg, I2C_MEMADD_SIZE_8BIT, &val, 1, timeout);
}

static HAL_StatusTypeDef rd(I2C_HandleTypeDef *hi2c, uint8_t reg, uint8_t *buf, uint16_t len,
                            uint32_t timeout)
{
    return HAL_I2C_Mem_Read(hi2c, BMP388_ADDR, reg, I2C_MEMADD_SIZE_8BIT, buf, len, timeout);
}

static uint16_t u16le(const uint8_t *b) { return (uint16_t)(b[0] | (b[1] << 8)); }
static int16_t s16le(const uint8_t *b) { return (int16_t)u16le(b); }

static void calib_convert(const uint8_t *c)
{
    par_t1 = ldexpf((float)u16le(&c[0]), 8);
    par_t2 = ldexpf((float)u16le(&c[2]), -30);
    par_t3 = ldexpf((float)(int8_t)c[4], -48);
    par_p1 = ldexpf((float)s16le(&c[5]) - 16384.0f, -20);
    par_p2 = ldexpf((float)s16le(&c[7]) - 16384.0f, -29);
    par_p3 = ldexpf((float)(int8_t)c[9], -32);
    par_p4 = ldexpf((float)(int8_t)c[10], -37);
    par_p5 = ldexpf((float)u16le(&c[11]), 3);
    par_p6 = ldexpf((float)u16le(&c[13]), -6);
    par_p7 = ldexpf((float)(int8_t)c[15], -8);
    par_p8 = ldexpf((float)(int8_t)c[16], -15);
    par_p9 = ldexpf((float)s16le(&c[17]), -48);
    par_p10 = ldexpf((float)(int8_t)c[19], -48);
    par_p11 = ldexpf((float)(int8_t)c[20], -65);
}

HAL_StatusTypeDef BMP388_Init(I2C_HandleTypeDef *hi2c, uint32_t timeout)
{
    uint8_t id = 0;
    uint8_t calib[CALIB_LEN];
    HAL_StatusTypeDef st;

    st = rd(hi2c, REG_CHIP_ID, &id, 1, timeout);
    if (st != HAL_OK)
        return st;
    if (id != CHIP_ID)
        return HAL_ERROR;

    st = wr(hi2c, REG_CMD, CMD_SOFTRESET, timeout);
    if (st != HAL_OK)
        return st;
    HAL_Delay(5);

    st = rd(hi2c, REG_CALIB, calib, CALIB_LEN, timeout);
    if (st != HAL_OK)
        return st;
    calib_convert(calib);

    /* Druck x8, Temperatur x1, 50 Hz. Die Wandlung dauert dann laut der
     * Formel im Datenblatt rund 19 ms, passt also in die 20 ms. */
    st = wr(hi2c, REG_OSR, 0x03, timeout);
    if (st != HAL_OK)
        return st;
    st = wr(hi2c, REG_ODR, 0x02, timeout);
    if (st != HAL_OK)
        return st;
    /* IIR-Filter Koeffizient 3 (Bits 3..1 = 010) */
    st = wr(hi2c, REG_CONFIG, 0x04, timeout);
    if (st != HAL_OK)
        return st;
    /* Normal Mode (Bits 5..4 = 11), Temperatur und Druck an */
    st = wr(hi2c, REG_PWR_CTRL, 0x33, timeout);
    if (st != HAL_OK)
        return st;
    HAL_Delay(2);

    /* Bit 2 = conf_err: der Sensor lehnt die Einstellung ab, Bit 0 = fatal_err */
    uint8_t err = 0;
    st = rd(hi2c, REG_ERR, &err, 1, timeout);
    if (st != HAL_OK)
        return st;
    if (err & 0x05)
        return HAL_ERROR;

    return HAL_OK;
}

/* Datenblatt Kapitel 9.2 */
static float compensate_temp(uint32_t raw)
{
    const float d1 = (float)raw - par_t1;
    const float d2 = d1 * par_t2;
    return d2 + (d1 * d1) * par_t3;
}

/* Datenblatt Kapitel 9.3 */
static float compensate_press(uint32_t raw, float t_lin)
{
    const float t2 = t_lin * t_lin;
    const float t3 = t2 * t_lin;
    const float u = (float)raw;

    const float out1 = par_p5 + par_p6 * t_lin + par_p7 * t2 + par_p8 * t3;
    const float out2 = u * (par_p1 + par_p2 * t_lin + par_p3 * t2 + par_p4 * t3);
    const float d4 = u * u * (par_p9 + par_p10 * t_lin) + u * u * u * par_p11;

    return out1 + out2 + d4;
}

uint8_t BMP388_Parse(const uint8_t *buf)
{
    if ((buf[0] & (STATUS_DRDY_PRESS | STATUS_DRDY_TEMP)) !=
        (STATUS_DRDY_PRESS | STATUS_DRDY_TEMP))
        return 0;

    const uint32_t raw_p = (uint32_t)buf[1] | ((uint32_t)buf[2] << 8) | ((uint32_t)buf[3] << 16);
    const uint32_t raw_t = (uint32_t)buf[4] | ((uint32_t)buf[5] << 8) | ((uint32_t)buf[6] << 16);

    temp_c = compensate_temp(raw_t);
    press_pa = compensate_press(raw_p, temp_c);

    if (ref_n < REF_SAMPLES)
    {
        ref_sum += press_pa;
        if (++ref_n == REF_SAMPLES)
            ref_pa = ref_sum / (float)REF_SAMPLES;
    }
    else if (press_pa > 0.0f)
    {
        alt_m = 44330.0f * (1.0f - powf(press_pa / ref_pa, 0.1903f));
    }
    return 1;
}

float BMP388_PressurePa(void) { return press_pa; }
float BMP388_TempC(void) { return temp_c; }
float BMP388_AltitudeM(void) { return alt_m; }
