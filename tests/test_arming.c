/* Host-Test fuer Sensors/arming.c (batbypass), laeuft auf dem PC:
 *   make -C tests
 * Motor, Sensor, Akku und UART sind hier durch Attrappen ersetzt. */

#include "arming.h"
#include "motor.h"
#include "control.h"
#include "mpu6050.h"
#include "battery.h"
#include "rc.h"
#include <stdio.h>
#include <string.h>

/* ---------- Attrappen ---------- */
TIM_TypeDef stub_tim3;
UART_HandleTypeDef huart2;
MPU6050_Data_t sensor;
uint8_t mpu_ok;
Setpoint_t setpoint;

static uint8_t armed;
static uint8_t healthy;
static uint8_t cells;
static BatteryState_t bat_state;
static float thr_cap;
static HAL_StatusTypeDef uart_result;
static char uart_sent[128];
static int uart_calls;

uint8_t Motor_IsArmed(void) { return armed; }
void Motor_Arm(void) { armed = 1; }
void Motor_Disarm(void) { armed = 0; }
void Control_Reset(void) {}
uint8_t MPU6050_Healthy(void) { return healthy; }
uint8_t Battery_Cells(void) { return cells; }
BatteryState_t Battery_State(void) { return bat_state; }
float Battery_LimitThrottle(float t) { return (t > thr_cap) ? thr_cap : t; }
void RC_BlockArm(void) {}

HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *huart, const uint8_t *pData, uint16_t Size)
{
    (void)huart;
    uart_calls++;
    if (uart_result == HAL_OK)
    {
        memcpy(uart_sent, pData, Size);
        uart_sent[Size] = '\0';
    }
    return uart_result;
}

/* ---------- Testrahmen ---------- */
static int failed = 0;

#define CHECK(cond)                                                   \
    do                                                                \
    {                                                                 \
        if (!(cond))                                                  \
        {                                                             \
            printf("FEHLER %s:%d: %s\n", __FILE__, __LINE__, #cond);  \
            failed++;                                                 \
        }                                                             \
    } while (0)

/* Pruefstand: Sensor ok, flach, kein Akkuteiler (keine Zellen erkannt) */
static void bench(void)
{
    Arming_Disarm();
    Arming_SetBatBypass(0);
    memset(&sensor, 0, sizeof(sensor));
    sensor.accel_z_g = -1.0f;
    mpu_ok = 1;
    healthy = 1;
    cells = 0;
    bat_state = BAT_NONE;
    thr_cap = 100.0f;
    uart_result = HAL_OK;
    uart_sent[0] = '\0';
    uart_calls = 0;
}

static void test_nach_reset_aus(void)
{
    /* Erster Test, noch nichts gesetzt: Startwert wie nach einem Reset */
    CHECK(Arming_BatBypass() == 0);
    bench();
    CHECK(Arming_Check() == ARM_KEIN_AKKU);
    CHECK(Arming_Arm() == ARM_KEIN_AKKU);
    CHECK(!armed);
}

static void test_bypass_armt_ohne_akku(void)
{
    bench();
    Arming_SetBatBypass(1);
    CHECK(Arming_BatBypass() == 1);
    CHECK(Arming_Check() == ARM_OK);
    CHECK(Arming_Arm() == ARM_OK);
    CHECK(armed);
}

static void test_bypass_ignoriert_leeren_akku(void)
{
    bench();
    cells = 6;
    bat_state = BAT_CRITICAL;
    CHECK(Arming_Check() == ARM_AKKU_LEER);
    Arming_SetBatBypass(1);
    CHECK(Arming_Check() == ARM_OK);
}

static void test_andere_pruefungen_bleiben(void)
{
    bench();
    Arming_SetBatBypass(1);

    mpu_ok = 0;
    CHECK(Arming_Check() == ARM_KEIN_SENSOR);
    mpu_ok = 1;
    healthy = 0;
    CHECK(Arming_Check() == ARM_KEIN_SENSOR);
    healthy = 1;

    sensor.roll = 30.0f;
    CHECK(Arming_Check() == ARM_NEIGUNG);
    sensor.roll = 0.0f;
    sensor.accel_y_g = -0.7f; /* schief laut Accel, Filterwinkel noch flach */
    sensor.accel_z_g = -0.7f;
    CHECK(Arming_Check() == ARM_NEIGUNG);
    sensor.accel_y_g = 0.0f;
    sensor.accel_z_g = -1.0f;

    CHECK(Arming_Arm() == ARM_OK);
    CHECK(Arming_Check() == ARM_SCHON_ARMED);
}

static void test_warnung_bei_jedem_armen(void)
{
    const char *expected = ">> " ARM_BAT_BYPASS_WARNUNG "\r\n";
    CHECK(strcmp(ARM_BAT_BYPASS_WARNUNG, "AKKUPRUEFUNG AUS - NUR OHNE PROPELLER") == 0);

    bench();
    Arming_SetBatBypass(1);
    Arming_PrintBypassWarning();
    CHECK(uart_calls == 0); /* noch nicht gearmt */

    for (int i = 0; i < 3; i++)
    {
        uart_sent[0] = '\0';
        uart_calls = 0;
        CHECK(Arming_Arm() == ARM_OK);
        Arming_PrintBypassWarning();
        CHECK(strcmp(uart_sent, expected) == 0);
        Arming_PrintBypassWarning();
        CHECK(uart_calls == 1); /* genau einmal pro Armen */
        Arming_Disarm();
    }
}

static void test_warnung_wartet_auf_freien_uart(void)
{
    bench();
    Arming_SetBatBypass(1);
    CHECK(Arming_Arm() == ARM_OK);
    uart_result = HAL_BUSY;
    Arming_PrintBypassWarning();
    CHECK(uart_sent[0] == '\0');
    uart_result = HAL_OK;
    Arming_PrintBypassWarning();
    CHECK(strstr(uart_sent, ARM_BAT_BYPASS_WARNUNG) != NULL);
}

static void test_ohne_bypass_keine_warnung(void)
{
    bench();
    cells = 6;
    bat_state = BAT_OK;
    CHECK(Arming_Arm() == ARM_OK);
    Arming_PrintBypassWarning();
    CHECK(uart_calls == 0);
}

static void test_gasbegrenzung(void)
{
    bench();
    thr_cap = 60.0f; /* Akku "kritisch" laut Messung */
    CHECK(Arming_LimitThrottle(90.0f) == 60.0f);
    Arming_SetBatBypass(1);
    CHECK(Arming_LimitThrottle(90.0f) == 90.0f);
    CHECK(Arming_LimitThrottle(100.0f) == 100.0f);
    Arming_SetBatBypass(0);
    CHECK(Arming_LimitThrottle(90.0f) == 60.0f);
}

static void test_ausschalten(void)
{
    bench();
    Arming_SetBatBypass(1);
    CHECK(Arming_Check() == ARM_OK);
    Arming_SetBatBypass(0);
    CHECK(Arming_BatBypass() == 0);
    CHECK(Arming_Check() == ARM_KEIN_AKKU);
}

int main(void)
{
    test_nach_reset_aus(); /* muss als erster laufen */
    test_bypass_armt_ohne_akku();
    test_bypass_ignoriert_leeren_akku();
    test_andere_pruefungen_bleiben();
    test_warnung_bei_jedem_armen();
    test_warnung_wartet_auf_freien_uart();
    test_ohne_bypass_keine_warnung();
    test_gasbegrenzung();
    test_ausschalten();

    if (failed)
    {
        printf("%d Pruefung(en) fehlgeschlagen\n", failed);
        return 1;
    }
    printf("Alle Tests bestanden\n");
    return 0;
}
