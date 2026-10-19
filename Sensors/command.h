#ifndef COMMAND_H
#define COMMAND_H

#include "main.h"
#include <stdint.h>

void Command_Init(UART_HandleTypeDef *huart);
void Command_RxByte(void);
void Command_RxError(void);
void Command_Process(void);
uint8_t Command_StreamOn(void); /* 1 = Sensor-Ausgabe an, 0 = aus */
uint8_t Command_KippOn(void);   /* 1 = Kipptest-Vorschau an */

/* Abschaltung fuer den Terminal-Arm: Kommt laenger als
 * COMMAND_KEEPALIVE_MS kein Zeichen ueber das Terminal, wird disarmt.
 * Jedes Zeichen zaehlt, als Keepalive reicht ein Enter (leere Zeile).
 * Einmal pro Regelzyklus aufrufen. */
#define COMMAND_KEEPALIVE_MS 1000
void Command_ArmWatchdog(void);

#endif /* COMMAND_H */