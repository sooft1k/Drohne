#ifndef CRSF_H
#define CRSF_H

#include "main.h"
#include <stdint.h>

/* CRSF ist das Protokoll, das ELRS-Empfaenger sprechen.
 * 420000 Baud, 8N1, binaere Pakete.
 *
 * Paketaufbau:
 *   [Adresse][Laenge][Typ][Daten ...][CRC]
 *   Laenge zaehlt Typ + Daten + CRC.
 *
 * Uns interessiert nur Typ 0x16: 16 Kanaele zu je 11 Bit in 22 Bytes. */

#define CRSF_BAUD             420000
#define CRSF_CHANNELS         16
#define CRSF_FRAME_MAX        64

/* Rohwerte des Protokolls und die zugehoerigen Mikrosekunden */
#define CRSF_VAL_MIN          172     /* entspricht 988 us  */
#define CRSF_VAL_MID          992     /* entspricht 1500 us */
#define CRSF_VAL_MAX          1811    /* entspricht 2012 us */

/* Kein Paket laenger als das -> Verbindung gilt als verloren */
#define CRSF_TIMEOUT_MS       500

void CRSF_Init(UART_HandleTypeDef *huart);

/* Wird aus dem USART1-Interrupt gerufen, nicht selbst aufrufen. */
void CRSF_RxByte(void);

/* Kanal 1..16, Rueckgabe in Mikrosekunden (988..2012). */
uint16_t CRSF_Channel(uint8_t ch);

/* 1 = Verbindung steht, 0 = seit CRSF_TIMEOUT_MS nichts empfangen */
uint8_t  CRSF_LinkUp(void);

/* Zaehler fuer die Diagnose */
uint32_t CRSF_FrameCount(void);
uint32_t CRSF_ErrorCount(void);
uint32_t CRSF_AgeMs(void);

#endif /* CRSF_H */