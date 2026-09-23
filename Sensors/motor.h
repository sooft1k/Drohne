#ifndef MOTOR_H
#define MOTOR_H

#include "main.h"
#include <stdint.h>

#define MOTOR_COUNT 4

/* ===== Protokoll-Umschalter =====
 * 1 = Oneshot125: Pulse 125..250 us, neuer Wert 1000x pro Sekunde
 * 0 = klassisches PWM: Pulse 1000..2000 us, neuer Wert nur 50x pro Sekunde
 *
 * BLHeli_S erkennt das Signal beim Einschalten automatisch.
 * Wichtig: STM32 zuerst laufen lassen, DANACH den Akku an den ESC. */
#define MOTOR_ONESHOT125  1

#if MOTOR_ONESHOT125
  /* 84 MHz / 2 = 42 MHz Timer-Takt -> 42 Ticks pro Mikrosekunde */
  #define MOTOR_TIM_PRESCALER   (2 - 1)
  #define MOTOR_TICKS_PER_US    42
  /* Wiederholrate 1 kHz: 42 MHz / 42000 = 1000 Hz */
  #define MOTOR_TIM_PERIOD      (42000 - 1)
  #define MOTOR_US_MIN          125   /* Stopp */
  #define MOTOR_US_MAX          250   /* Vollgas */
#else
  /* 84 MHz / 84 = 1 MHz -> 1 Tick pro Mikrosekunde */
  #define MOTOR_TIM_PRESCALER   (84 - 1)
  #define MOTOR_TICKS_PER_US    1
  /* Wiederholrate 50 Hz: 1 MHz / 20000 = 50 Hz */
  #define MOTOR_TIM_PERIOD      (20000 - 1)
  #define MOTOR_US_MIN          1000  /* Stopp */
  #define MOTOR_US_MAX          2000  /* Vollgas */
#endif

#define MOTOR_US_IDLE  MOTOR_US_MIN   /* Sicherer Startwert */

/* Motoren werden mit 1..4 angesprochen - wie auf dem ESC beschriftet.
 * M1 -> PB0 (CH3), M2 -> PB1 (CH4), M3 -> PA6 (CH1), M4 -> PA7 (CH2) */

void Motor_Init(TIM_HandleTypeDef *htim);

void Motor_SetUs(uint8_t motor, uint16_t us);
void Motor_SetPercent(uint8_t motor, float percent);
void Motor_SetAllUs(uint16_t us);
void Motor_SetAllPercent(float percent);

void Motor_Arm(void);
void Motor_Disarm(void);
uint8_t Motor_IsArmed(void);

/* Gibt die aktuelle Wiederholrate in Hz zurueck - fuer den 'status'-Befehl */
uint32_t Motor_UpdateRateHz(void);

#endif /* MOTOR_H */