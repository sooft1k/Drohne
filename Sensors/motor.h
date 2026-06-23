#ifndef MOTOR_H
#define MOTOR_H

#include "main.h"
#include <stdint.h>

/* ESC-PWM Grenzen (Standard-PWM Protokoll) */
#define MOTOR_US_MIN   1000   /* Stopp / Leerlauf */
#define MOTOR_US_MAX   2000   /* Vollgas */
#define MOTOR_US_IDLE  1000   /* Sicherer Startwert (Motor steht) */

/* Initialisiert TIM3 PWM fuer Motor 1 (PB0 / TIM3_CH3).
 * Setzt direkt MOTOR_US_IDLE (1000us = Stopp) aus Sicherheit. */
void Motor_Init(TIM_HandleTypeDef *htim);

/* Setzt die Pulsbreite direkt in Mikrosekunden (wird auf 1000-2000 begrenzt).
 * Wirkt nur wenn armed - sonst wird immer 1000us (Stopp) ausgegeben. */
void Motor_SetUs(uint16_t us);

/* Setzt die Motorleistung in Prozent (0-100).
 * 0%   -> 1000us (Stopp)
 * 100% -> 2000us (Vollgas)
 * Wirkt nur wenn armed. */
void Motor_SetPercent(float percent);

/* Scharfschalten: ab jetzt reagiert der Motor auf SetUs/SetPercent. */
void Motor_Arm(void);

/* Entschaerfen: Motor sofort auf Stopp (1000us), reagiert nicht mehr. */
void Motor_Disarm(void);

/* Gibt zurueck ob der Motor scharf ist (1) oder nicht (0). */
uint8_t Motor_IsArmed(void);

/* ESC-Kalibrierung: bringt dem ESC den Bereich bei (Vollgas -> Stopp).
 * NUR EINMAL pro ESC noetig, OHNE Propeller!
 * Ablauf: sendet 2000us, wartet, dann 1000us.
 * Muss VOR dem normalen Betrieb laufen (mit Anleitung am UART). */
void Motor_CalibrateESC(void);

#endif /* MOTOR_H */