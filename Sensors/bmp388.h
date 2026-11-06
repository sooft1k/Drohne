#ifndef BMP388_H
#define BMP388_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* Barometer an I2C1. SDO liegt auf GND, also Adresse 0x76 (Datenblatt 5.2). */
#define BMP388_ADDR (0x76 << 1)

/* Ein Messwert: Status 0x03 und die Daten 0x04..0x09 */
#define BMP388_READ_REG 0x03
#define BMP388_READ_LEN 7

/* Blockiert, nur beim Start oder disarmed aufrufen */
HAL_StatusTypeDef BMP388_Init(I2C_HandleTypeDef *hi2c, uint32_t timeout);

/* Wertet BMP388_READ_LEN gelesene Bytes aus. 1 = neue Werte. */
uint8_t BMP388_Parse(const uint8_t *buf);

float BMP388_PressurePa(void);
float BMP388_TempC(void);
float BMP388_AltitudeM(void); /* relativ zum Druck in der ersten Sekunde nach dem Start */

#endif
