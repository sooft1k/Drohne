#include "motor.h"

/* Interner Zustand */
static TIM_HandleTypeDef *motor_tim = 0;
static uint8_t armed = 0;

/* Hilfsfunktion: schreibt einen us-Wert direkt ins Timer-Register.
 * Keine Sicherheitspruefung - nur intern verwenden! */
static void motor_write_raw(uint16_t us)
{
    __HAL_TIM_SET_COMPARE(motor_tim, TIM_CHANNEL_3, us);
}

void Motor_Init(TIM_HandleTypeDef *htim)
{
    motor_tim = htim;

    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* PB0 als TIM3_CH3 Alternate Function */
    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_0;
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = GPIO_AF2_TIM3;
    HAL_GPIO_Init(GPIOB, &g);

    /* Timer-Basis: 1 MHz Tick (1 us), 20 ms Periode (50 Hz) */
    motor_tim->Instance = TIM3;
    motor_tim->Init.Prescaler = 84 - 1; /* 84 MHz / 84 = 1 MHz */
    motor_tim->Init.CounterMode = TIM_COUNTERMODE_UP;
    motor_tim->Init.Period = 20000 - 1; /* 20 ms = 50 Hz */
    motor_tim->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    motor_tim->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_PWM_Init(motor_tim) != HAL_OK)
        Error_Handler();

    TIM_OC_InitTypeDef oc = {0};
    oc.OCMode = TIM_OCMODE_PWM1;
    oc.Pulse = MOTOR_US_IDLE; /* SICHER: 1000us = Stopp */
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(motor_tim, &oc, TIM_CHANNEL_3) != HAL_OK)
        Error_Handler();

    HAL_TIM_PWM_Start(motor_tim, TIM_CHANNEL_3);

    armed = 0; /* Start immer disarmed! */
    motor_write_raw(MOTOR_US_IDLE);
}

void Motor_SetUs(uint16_t us)
{
    /* Sicherheit: nur wenn scharf, sonst Stopp */
    if (!armed)
    {
        motor_write_raw(MOTOR_US_IDLE);
        return;
    }

    /* Begrenzen auf gueltigen Bereich */
    if (us < MOTOR_US_MIN)
        us = MOTOR_US_MIN;
    if (us > MOTOR_US_MAX)
        us = MOTOR_US_MAX;

    motor_write_raw(us);
}

void Motor_SetPercent(float percent)
{
    if (percent < 0.0f)
        percent = 0.0f;
    if (percent > 100.0f)
        percent = 100.0f;

    /* 0% -> 1000us, 100% -> 2000us */
    uint16_t us = (uint16_t)(MOTOR_US_MIN + (percent / 100.0f) * (MOTOR_US_MAX - MOTOR_US_MIN));
    Motor_SetUs(us);
}

void Motor_Arm(void)
{
    armed = 1;
}

void Motor_Disarm(void)
{
    armed = 0;
    motor_write_raw(MOTOR_US_IDLE); /* Sofort Stopp */
}

uint8_t Motor_IsArmed(void)
{
    return armed;
}

void Motor_CalibrateESC(void)
{
    /* WICHTIG: Nur OHNE Propeller, ESC noch stromlos starten!
     * Ablauf (typisch fuer BLHeli_S):
     * 1. Max-Signal senden (2000us)
     * 2. ESC mit Strom verbinden -> ESC hoert Max, merkt sich "oben"
     * 3. Nach Toenen: Min-Signal (1000us) -> ESC merkt sich "unten"
     * Diese Funktion sendet die Signale; das Strom-Timing machst du manuell
     * nach Anleitung am UART. */

    motor_write_raw(MOTOR_US_MAX); /* 2000us = Max */
    HAL_Delay(4000);               /* 4 Sek warten (Strom anstecken) */
    motor_write_raw(MOTOR_US_MIN); /* 1000us = Min */
    HAL_Delay(3000);               /* 3 Sek warten (ESC bestaetigt) */
    /* Danach ist der ESC kalibriert und bereit. */
}