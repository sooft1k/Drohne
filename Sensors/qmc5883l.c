#include "qmc5883l.h"

/* Register, Datenblatt Kapitel 9 */
#define REG_CTRL1 0x09
#define REG_CTRL2 0x0A
#define REG_PERIOD 0x0B
#define REG_CHIP_ID 0x0D

#define CHIP_ID 0xFF
#define CTRL2_SOFT_RST 0x80

/* OSR 512, 8 Gauss, 200 Hz, Continuous (Datenblatt 7.1) */
#define CTRL1_VALUE 0x1D

/* Empfindlichkeit bei 8 Gauss */
#define LSB_PER_GAUSS 3000.0f

/* Status 06H, Bit 1 */
#define STATUS_OVL 0x02

static float x_g = 0.0f, y_g = 0.0f, z_g = 0.0f;
static uint8_t overflow = 0;

static HAL_StatusTypeDef wr(I2C_HandleTypeDef *hi2c, uint8_t reg, uint8_t val, uint32_t timeout)
{
    return HAL_I2C_Mem_Write(hi2c, QMC5883L_ADDR, reg, I2C_MEMADD_SIZE_8BIT, &val, 1, timeout);
}

static HAL_StatusTypeDef rd(I2C_HandleTypeDef *hi2c, uint8_t reg, uint8_t *val, uint32_t timeout)
{
    return HAL_I2C_Mem_Read(hi2c, QMC5883L_ADDR, reg, I2C_MEMADD_SIZE_8BIT, val, 1, timeout);
}

HAL_StatusTypeDef QMC5883L_Init(I2C_HandleTypeDef *hi2c, uint32_t timeout)
{
    uint8_t v = 0;
    HAL_StatusTypeDef st;

    st = rd(hi2c, REG_CHIP_ID, &v, timeout);
    if (st != HAL_OK)
        return st;
    if (v != CHIP_ID)
        return HAL_ERROR;

    st = wr(hi2c, REG_CTRL2, CTRL2_SOFT_RST, timeout);
    if (st != HAL_OK)
        return st;
    HAL_Delay(2);

    st = wr(hi2c, REG_PERIOD, 0x01, timeout);
    if (st != HAL_OK)
        return st;
    st = wr(hi2c, REG_CTRL1, CTRL1_VALUE, timeout);
    if (st != HAL_OK)
        return st;

    st = rd(hi2c, REG_CTRL1, &v, timeout);
    if (st != HAL_OK)
        return st;
    return (v == CTRL1_VALUE) ? HAL_OK : HAL_ERROR;
}

void QMC5883L_Parse(const uint8_t *buf)
{
    /* LSB zuerst, Reihenfolge X, Y, Z */
    x_g = (float)(int16_t)(buf[0] | (buf[1] << 8)) / LSB_PER_GAUSS;
    y_g = (float)(int16_t)(buf[2] | (buf[3] << 8)) / LSB_PER_GAUSS;
    z_g = (float)(int16_t)(buf[4] | (buf[5] << 8)) / LSB_PER_GAUSS;
    overflow = (buf[6] & STATUS_OVL) ? 1 : 0;
}

float QMC5883L_X(void) { return x_g; }
float QMC5883L_Y(void) { return y_g; }
float QMC5883L_Z(void) { return z_g; }
uint8_t QMC5883L_Overflow(void) { return overflow; }
