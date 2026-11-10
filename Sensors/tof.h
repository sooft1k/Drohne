#ifndef TOF_H
#define TOF_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* Fuenf ToF-Abstandssensoren VL53L1X an I2C3 (PA8 SCL, PC9 SDA, 400 kHz).
 * Jeder Sensor hat ein eigenes XSHUT und einen eigenen Interrupt (GPIO1),
 * beide mit 10 kOhm nach 3,3 V:
 *
 *   Sensor  XSHUT  INT   Adresse
 *   1 (U8)  PC8    PC2   0x30
 *   2 (U9)  PB12   PB4   0x31
 *   3 (U10) PB13   PB5   0x32
 *   4 (U12) PB14   PB8   0x33
 *   5 (U11) PB15   PB9   0x34
 *
 * Alle VL53L1X starten mit Adresse 0x29. Die Firmware schaltet sie per XSHUT
 * nacheinander ein und gibt jedem seine eigene Adresse.
 *
 * Sensoren und Pullups laufen mit 3,3 V. Deshalb werden sie im 2V8-Modus
 * betrieben (Register 0x2E und 0x2F, Bit 0 = 1), siehe Datenblatt Tabelle 14.
 * Ab Werk stehen sie im 1V8-Modus.
 *
 * Messung: Long Distance Mode (bis etwa 4 m), 50 ms Messzeit, 20 Hz je Sensor.
 * Gelesen wird per Interrupt im Hintergrund. Die Sensoren fliessen nicht in die
 * Regelung ein, ein Ausfall beeinflusst den Flug nicht. */

#define TOF_COUNT 5
#define TOF_I2C_ADDR_DEFAULT 0x29 /* Werksadresse, 7 Bit */
#define TOF_I2C_ADDR_FIRST 0x30   /* Sensor 1 bekommt 0x30, Sensor 2 0x31 ... */
#define TOF_MODEL_ID 0xEACC

/* Bus, Pins und Sensoren einrichten. Blockiert hoechstens etwa 3,5 s
 * (alle Sensoren ausgefallen), normal etwa 0,3 s. Nur beim Start aufrufen. */
void Tof_Init(void);

/* Aus dem Leerlauf der Hauptschleife aufrufen */
void Tof_Process(void);

/* 1 = Sensor i (0..TOF_COUNT-1) misst */
uint8_t Tof_Ok(uint8_t i);

/* Anzahl der Sensoren, die messen */
uint8_t Tof_OkCount(void);

/* Letzte gueltige Entfernung von Sensor i in mm. Gibt 0 zurueck, wenn keine
 * gueltige Messung vorliegt, die juenger als max_age_ms ist. */
uint16_t Tof_DistanceMm(uint8_t i, uint32_t max_age_ms);

/* Ausgabe fuer die Befehle 'tof' und 'sens' */
void Tof_Print(void);

/* Aus den HAL-I2C-Callbacks fuer I2C3 aufrufen (ok = 0 bei Fehler) */
void Tof_I2CDone(uint8_t ok);

#endif
