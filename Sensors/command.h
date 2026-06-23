#ifndef COMMAND_H
#define COMMAND_H

#include "main.h"
#include <stdint.h>

void Command_Init(UART_HandleTypeDef *huart);
void Command_RxByte(void);
void Command_Process(void);
uint8_t Command_StreamOn(void); /* 1 = Sensor-Ausgabe an, 0 = aus */

#endif /* COMMAND_H */