#ifndef TOF_H
#define TOF_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* ToF-Abstandssensor VL53L1X an I2C3 (PA8 SCL, PC9 SDA, 400 kHz).
 * XSHUT an PC8, Interrupt (GPIO1) an PC2, beide mit 10 kOhm nach 3,3 V.
 *
 * Der Sensor laeuft mit 3,3 V und die Pullups haengen ebenfalls an 3,3 V.
 * Deshalb wird er im 2V8-Modus betrieben (Register 0x2E und 0x2F, Bit 0 = 1),
 * siehe Datenblatt Tabelle 14. Ab Werk steht er im 1V8-Modus.
 *
 * Messung: Long Distance Mode (bis etwa 4 m), 50 ms Messzeit, 20 Hz.
 * Gelesen wird per Interrupt im Hintergrund. Der Sensor fliesst nicht in die
 * Regelung ein, ein Ausfall beeinflusst den Flug nicht. */

#define TOF_I2C_ADDR (0x29 << 1) /* Werksadresse, 8-Bit-Schreibweise der HAL */
#define TOF_MODEL_ID 0xEACC

/* Bus, Pins und Sensor einrichten. Blockiert hoechstens etwa 0,7 s,
 * nur beim Start aufrufen. */
void Tof_Init(void);

/* Aus dem Leerlauf der Hauptschleife aufrufen */
void Tof_Process(void);

/* 1 = Sensor misst */
uint8_t Tof_Ok(void);

/* Letzte gueltige Entfernung in mm. Gibt 0 zurueck, wenn keine gueltige
 * Messung vorliegt, die juenger als max_age_ms ist. */
uint16_t Tof_DistanceMm(uint32_t max_age_ms);

/* Ausgabe fuer die Befehle 'tof' und 'sens' */
void Tof_Print(void);

/* Aus den HAL-I2C-Callbacks fuer I2C3 aufrufen (ok = 0 bei Fehler) */
void Tof_I2CDone(uint8_t ok);

#endif
