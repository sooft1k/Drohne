#ifndef MOTOR_H
#define MOTOR_H

#include "main.h"
#include <stdint.h>

#define MOTOR_COUNT 4
#define MOTOR_US_MIN 1000  /* Stopp */
#define MOTOR_US_MAX 2000  /* Vollgas */
#define MOTOR_US_IDLE 1000 /* Sicherer Startwert */

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

#endif /* MOTOR_H */