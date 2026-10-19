#ifndef MOTOR_H
#define MOTOR_H

#include "main.h"
#include <stdint.h>

#define MOTOR_COUNT 4

/* ===== Protokoll-Umschalter =====
 * 1 = Oneshot125: neuer Wert 1000x pro Sekunde, Pulse = PWM-Werte / 8
 * 0 = klassisches PWM: neuer Wert nur 50x pro Sekunde
 *
 * BLHeli_S erkennt das Signal beim Einschalten automatisch.
 * Wichtig: STM32 zuerst laufen lassen, DANACH den Akku an den ESC. */
#define MOTOR_ONESHOT125 1

#if MOTOR_ONESHOT125
/* 84 MHz / 2 = 42 MHz Timer-Takt -> 42 Ticks pro Mikrosekunde */
#define MOTOR_TIM_PRESCALER (2 - 1)
#define MOTOR_TICKS_PER_US 42
/* Wiederholrate 1 kHz: 42 MHz / 42000 = 1000 Hz */
#define MOTOR_TIM_PERIOD (42000 - 1)
/* Gasbereich = Werkseinstellung BLHeli_S (PPM Min/Max Throttle 1148/1832 us,
 * bei Oneshot125 durch 8). Wird der ESC umprogrammiert, hier mitaendern
 * (z.B. Min 1040 / Max 2000 -> 130.0f / 250.0f). */
#define MOTOR_US_MIN 143.5f /* 0 % Gas = ESC-Minimum, Motor steht gerade noch */
#define MOTOR_US_MAX 229.0f /* 100 % Gas = ESC-Maximum */
/* Disarmed, beim Start und im Notaus: deutlich unter MOTOR_US_MIN, damit der
 * ESC sicher stillsteht und armt. Nie in die Naehe des Maximums - ein hoher
 * Wert beim Einschalten startet den Programmiermodus des ESC. */
#define MOTOR_US_STOP 125
#else
  /* 84 MHz / 84 = 1 MHz -> 1 Tick pro Mikrosekunde */
#define MOTOR_TIM_PRESCALER (84 - 1)
#define MOTOR_TICKS_PER_US 1
  /* Wiederholrate 50 Hz: 1 MHz / 20000 = 50 Hz */
#define MOTOR_TIM_PERIOD (20000 - 1)
#define MOTOR_US_MIN 1148.0f /* 0 % Gas, Werkseinstellung BLHeli_S */
#define MOTOR_US_MAX 1832.0f /* 100 % Gas */
#define MOTOR_US_STOP 1000   /* disarmed/Start */
#endif

/* Mikrosekunden (auch mit Nachkomma) in Timer-Ticks, gerundet */
#define MOTOR_US_TO_TICKS(us) ((uint32_t)((float)(us) * (float)MOTOR_TICKS_PER_US + 0.5f))

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

/* Notaus fuer Fault-Handler, Error_Handler und assert_failed:
 * schreibt die Stopp-Pulsbreite direkt in alle vier Vergleichsregister von
 * TIM3, ohne HAL, ohne Funktionsaufruf und ohne Stack. TIM3 laeuft in
 * Hardware weiter und erzeugt sonst endlos den letzten Gaswert, auch wenn
 * die CPU haengt.
 * Vorher wird die Vorladung (OCxPE) abgeschaltet, damit der neue Wert sofort
 * gilt: ein laufender laengerer Puls endet beim Vergleich mit 125 us, nichts
 * wird verlaengert. Ein Update-Event (UG) waere falsch - es setzt den Zaehler
 * zurueck und verlaengert einen laufenden Puls auf bis zu 375 us = Vollgas. */
static inline void Motor_EmergencyStop(void)
{
    const uint32_t stop = (uint32_t)MOTOR_US_STOP * MOTOR_TICKS_PER_US;
    TIM3->CCMR1 &= ~(TIM_CCMR1_OC1PE | TIM_CCMR1_OC2PE);
    TIM3->CCMR2 &= ~(TIM_CCMR2_OC3PE | TIM_CCMR2_OC4PE);
    TIM3->CCR1 = stop;
    TIM3->CCR2 = stop;
    TIM3->CCR3 = stop;
    TIM3->CCR4 = stop;
}

/* Gibt die aktuelle Wiederholrate in Hz zurueck - fuer den 'status'-Befehl */
uint32_t Motor_UpdateRateHz(void);

#endif /* MOTOR_H */