#ifndef QMC5883L_H
#define QMC5883L_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* Kompass an I2C1, feste Adresse 0x0D (Datenblatt 5.4) */
#define QMC5883L_ADDR (0x0D << 1)

/* Ein Messwert: X, Y, Z ab 00H und der Status 06H */
#define QMC5883L_READ_REG 0x00
#define QMC5883L_READ_LEN 7

/* Blockiert, nur beim Start oder disarmed aufrufen */
HAL_StatusTypeDef QMC5883L_Init(I2C_HandleTypeDef *hi2c, uint32_t timeout);

/* Wertet QMC5883L_READ_LEN gelesene Bytes aus */
void QMC5883L_Parse(const uint8_t *buf);

/* Feld in Gauss, in Chip-Achsen und unkalibriert */
float QMC5883L_X(void);
float QMC5883L_Y(void);
float QMC5883L_Z(void);
uint8_t QMC5883L_Overflow(void); /* 1 = eine Achse ausserhalb des Messbereichs */

#endif
