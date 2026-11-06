#ifndef AUXSENS_H
#define AUXSENS_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* Barometer und Kompass an I2C1 (PB6 SCL, PB7 SDA, 400 kHz).
 * Beide werden im Hintergrund per Interrupt gelesen und fliessen nicht in die
 * Regelung ein. Ein Ausfall hier beeinflusst den Flug deshalb nicht. */

/* Bus und Sensoren einrichten. Blockiert, nur beim Start aufrufen. */
void AuxSens_Init(void);

/* Aus dem Leerlauf der Hauptschleife aufrufen */
void AuxSens_Process(void);

uint8_t AuxSens_BaroOk(void);
uint8_t AuxSens_MagOk(void);

/* Ausgabe fuer den Befehl 'sens' */
void AuxSens_Print(void);

#endif
