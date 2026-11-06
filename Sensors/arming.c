#include "arming.h"
#include "motor.h"
#include "control.h"
#include "icm42688.h"
#include "battery.h"
#include "rc.h"
#include <stdio.h>
#include <math.h>

/* aus main.c */
extern ICM42688_Data_t sensor;
extern uint8_t imu_ok;
extern UART_HandleTypeDef huart2;

static volatile ArmCheck_t pending = ARM_OK; /* noch nicht ausgegebener Ablehnungsgrund */
static uint32_t arm_count = 0;                /* erfolgreiche Arm-Vorgaenge seit dem Start */
static uint8_t bat_bypass = 0;                /* 'batbypass': nur RAM, nach Reset aus */
static volatile uint8_t bypass_warn = 0;      /* Warnung nach dem Armen noch offen */

ArmCheck_t Arming_Check(void)
{
    if (Motor_IsArmed())
        return ARM_SCHON_ARMED;

    if (!imu_ok || !ICM42688_Healthy())
        return ARM_KEIN_SENSOR;

    if (sensor.roll > ARM_MAX_TILT_DEG || sensor.roll < -ARM_MAX_TILT_DEG ||
        sensor.pitch > ARM_MAX_TILT_DEG || sensor.pitch < -ARM_MAX_TILT_DEG)
        return ARM_NEIGUNG;

    /* Zusaetzlich direkt aus dem Accel: der gefilterte Winkel braucht nach
     * dem Start oder nach einem Sensorausfall eine Weile, bis er stimmt. */
    const float acc_roll = atan2f(-sensor.accel_y_g, -sensor.accel_z_g) * 57.2958f;
    const float acc_pitch = atan2f(sensor.accel_x_g,
                                   sqrtf(sensor.accel_y_g * sensor.accel_y_g +
                                         sensor.accel_z_g * sensor.accel_z_g)) * 57.2958f;
    if (fabsf(acc_roll) > ARM_MAX_TILT_DEG || fabsf(acc_pitch) > ARM_MAX_TILT_DEG)
        return ARM_NEIGUNG;

    if (!bat_bypass)
    {
        if (Battery_Cells() == 0)
            return ARM_KEIN_AKKU;
        if (Battery_State() != BAT_OK)
            return ARM_AKKU_LEER;
    }

    return ARM_OK;
}

ArmCheck_t Arming_Arm(void)
{
    ArmCheck_t r = Arming_Check();
    if (r != ARM_OK)
    {
        pending = r;
        return r;
    }

    setpoint.throttle = 0.0f;
    Control_Reset();
    Motor_Arm();
    arm_count++;
    if (bat_bypass)
        bypass_warn = 1;
    return ARM_OK;
}

uint32_t Arming_ArmCount(void)
{
    return arm_count;
}

void Arming_Reject(ArmCheck_t r)
{
    pending = r;
}

void Arming_Disarm(void)
{
    Motor_Disarm();
    setpoint.throttle = 0.0f;
    /* Jedes Disarm sperrt das Armen ueber den Schalter, bis er einmal unten
     * war - sonst armt ein noch oben stehender Schalter sofort wieder
     * (z.B. nach 'disarm' im Terminal oder nach einem Sensorausfall). */
    RC_BlockArm();
}

const char *Arming_Text(ArmCheck_t r)
{
    switch (r)
    {
    case ARM_OK:
        return "OK";
    case ARM_KEIN_SENSOR:
        return "Sensor fehlt oder liefert keine stabilen Werte";
    case ARM_NEIGUNG:
        return "zu schief - flach hinstellen";
    case ARM_KEIN_AKKU:
        return "keine Akkuspannung erkannt";
    case ARM_AKKU_LEER:
        return "Akku zu leer";
    case ARM_GAS:
        return "Gasknueppel nicht unten";
    case ARM_SCHON_ARMED:
        return "schon gearmt";
    default:
        return "?";
    }
}

void Arming_PrintPending(void)
{
    ArmCheck_t r = pending;
    if (r == ARM_OK)
        return;
    pending = ARM_OK;
    printf(">> Arming abgelehnt: %s\r\n", Arming_Text(r));
}

void Arming_SetBatBypass(uint8_t on)
{
    bat_bypass = on ? 1 : 0;
    if (!bat_bypass)
        bypass_warn = 0;
}

uint8_t Arming_BatBypass(void)
{
    return bat_bypass;
}

float Arming_LimitThrottle(float throttle)
{
    /* Ohne Akkuteiler ist die Spannung unbekannt - ein offener Pin koennte
     * sonst einen leeren Akku vortaeuschen und das Gas drosseln */
    return bat_bypass ? throttle : Battery_LimitThrottle(throttle);
}

void Arming_PrintBypassWarning(void)
{
    static const char msg[] = ">> " ARM_BAT_BYPASS_WARNUNG "\r\n";
    if (!bypass_warn)
        return;
    /* Laeuft gerade noch eine Ausgabe, beim naechsten Aufruf nochmal */
    if (HAL_UART_Transmit_IT(&huart2, (uint8_t *)msg, sizeof(msg) - 1) == HAL_OK)
        bypass_warn = 0;
}
