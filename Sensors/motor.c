#include "motor.h"

static TIM_HandleTypeDef *motor_tim = 0;
static uint8_t armed = 0;

/* Motor 1..4 -> TIM3-Kanal (Index 0..3) */
static const uint32_t motor_ch[MOTOR_COUNT] = {
    TIM_CHANNEL_3, /* Motor 1 -> PB0 */
    TIM_CHANNEL_4, /* Motor 2 -> PB1 */
    TIM_CHANNEL_1, /* Motor 3 -> PA6 */
    TIM_CHANNEL_2  /* Motor 4 -> PA7 */
};

/* Schreibt direkt ins Timer-Register, ohne Pruefung. Nur intern!
 * Erwartet Timer-Ticks, nicht Mikrosekunden. */
static void motor_write_raw(uint8_t idx, uint32_t ticks)
{
    __HAL_TIM_SET_COMPARE(motor_tim, motor_ch[idx], ticks);
}

void Motor_Init(TIM_HandleTypeDef *htim)
{
    motor_tim = htim;

    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = GPIO_AF2_TIM3;

    /* PB0 = M1, PB1 = M2 */
    g.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    HAL_GPIO_Init(GPIOB, &g);

    /* PA6 = M3, PA7 = M4 */
    g.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    HAL_GPIO_Init(GPIOA, &g);

    /* Timer-Basis: Werte kommen aus motor.h, je nach Protokoll.
     * Oneshot125: 42 MHz Tick, 1 kHz Wiederholrate, Pulse 125..250 us
     * PWM:         1 MHz Tick, 50 Hz Wiederholrate, Pulse 1000..2000 us */
    motor_tim->Instance = TIM3;
    motor_tim->Init.Prescaler = MOTOR_TIM_PRESCALER;
    motor_tim->Init.CounterMode = TIM_COUNTERMODE_UP;
    motor_tim->Init.Period = MOTOR_TIM_PERIOD;
    motor_tim->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    motor_tim->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_PWM_Init(motor_tim) != HAL_OK)
        Error_Handler();

    /* Alle vier Kanaele gleich konfigurieren */
    TIM_OC_InitTypeDef oc = {0};
    oc.OCMode = TIM_OCMODE_PWM1;
    oc.Pulse = (uint32_t)MOTOR_US_IDLE * MOTOR_TICKS_PER_US; /* SICHER: Stopp */
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;

    for (uint8_t i = 0; i < MOTOR_COUNT; i++)
    {
        if (HAL_TIM_PWM_ConfigChannel(motor_tim, &oc, motor_ch[i]) != HAL_OK)
            Error_Handler();
        HAL_TIM_PWM_Start(motor_tim, motor_ch[i]);
    }

    armed = 0; /* Start immer disarmed */
    Motor_SetAllUs(MOTOR_US_IDLE);
}

void Motor_SetUs(uint8_t motor, uint16_t us)
{
    if (motor < 1 || motor > MOTOR_COUNT)
        return;

    uint8_t idx = motor - 1;

    /* Sicherheit: ohne arm immer Stopp */
    if (!armed)
    {
        motor_write_raw(idx, (uint32_t)MOTOR_US_IDLE * MOTOR_TICKS_PER_US);
        return;
    }

    if (us < MOTOR_US_MIN)
        us = MOTOR_US_MIN;
    if (us > MOTOR_US_MAX)
        us = MOTOR_US_MAX;

    motor_write_raw(idx, (uint32_t)us * MOTOR_TICKS_PER_US);
}

void Motor_SetPercent(uint8_t motor, float percent)
{
    if (motor < 1 || motor > MOTOR_COUNT)
        return;

    if (percent < 0.0f)
        percent = 0.0f;
    if (percent > 100.0f)
        percent = 100.0f;

    uint8_t idx = motor - 1;

    if (!armed)
    {
        motor_write_raw(idx, (uint32_t)MOTOR_US_IDLE * MOTOR_TICKS_PER_US);
        return;
    }

    /* Direkt in Ticks rechnen statt ueber ganze Mikrosekunden.
     * Bei Oneshot125 sind 125 us Stellbereich nur 125 ganze Schritte -
     * in Ticks sind es 5250, also fein genug fuer die Regelung. */
    const uint32_t t_min = (uint32_t)MOTOR_US_MIN * MOTOR_TICKS_PER_US;
    const uint32_t t_max = (uint32_t)MOTOR_US_MAX * MOTOR_TICKS_PER_US;

    uint32_t ticks = t_min + (uint32_t)((percent / 100.0f) * (float)(t_max - t_min));

    motor_write_raw(idx, ticks);
}

void Motor_SetAllUs(uint16_t us)
{
    for (uint8_t m = 1; m <= MOTOR_COUNT; m++)
        Motor_SetUs(m, us);
}

void Motor_SetAllPercent(float percent)
{
    for (uint8_t m = 1; m <= MOTOR_COUNT; m++)
        Motor_SetPercent(m, percent);
}

void Motor_Arm(void)
{
    armed = 1;
}

void Motor_Disarm(void)
{
    armed = 0;
    for (uint8_t i = 0; i < MOTOR_COUNT; i++)
        motor_write_raw(i, (uint32_t)MOTOR_US_IDLE * MOTOR_TICKS_PER_US); /* sofort Stopp */
}

uint8_t Motor_IsArmed(void)
{
    return armed;
}

uint32_t Motor_UpdateRateHz(void)
{
    /* Timer-Takt / (Periode + 1) */
    return (84000000UL / (MOTOR_TIM_PRESCALER + 1)) / (MOTOR_TIM_PERIOD + 1);
}