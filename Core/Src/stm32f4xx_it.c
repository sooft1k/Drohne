#include "main.h"
#include "motor.h"

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;
extern TIM_HandleTypeDef htim2;
extern I2C_HandleTypeDef hi2c1;
extern I2C_HandleTypeDef hi2c3;

/* Alle Fehler-Handler schalten zuerst die Motorpulse auf Stopp und bleiben
 * dann stehen. Nach dem Start (ab ">> Bereit.") setzt der Watchdog (IWDG)
 * den Prozessor danach zurueck; waehrend Start und Kalibrierung laeuft er
 * noch nicht, dann bleibt der Prozessor stehen. */
void NMI_Handler(void)
{
    Motor_EmergencyStop();
    while (1)
    {
    }
}
void HardFault_Handler(void)
{
    Motor_EmergencyStop();
    while (1)
    {
    }
}
void MemManage_Handler(void)
{
    Motor_EmergencyStop();
    while (1)
    {
    }
}
void BusFault_Handler(void)
{
    Motor_EmergencyStop();
    while (1)
    {
    }
}
void UsageFault_Handler(void)
{
    Motor_EmergencyStop();
    while (1)
    {
    }
}
void SVC_Handler(void) {}
void DebugMon_Handler(void) {}
void PendSV_Handler(void) {}

void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}

void USART2_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart2);
}

void USART3_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart3);
}

void TIM2_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim2);
}

/* Baro und Kompass werden per Interrupt gelesen (auxsens.c) */
void I2C1_EV_IRQHandler(void)
{
    HAL_I2C_EV_IRQHandler(&hi2c1);
}

void I2C1_ER_IRQHandler(void)
{
    HAL_I2C_ER_IRQHandler(&hi2c1);
}

/* ToF-Sensor VL53L1X wird per Interrupt gelesen (tof.c) */
void I2C3_EV_IRQHandler(void)
{
    HAL_I2C_EV_IRQHandler(&hi2c3);
}

void I2C3_ER_IRQHandler(void)
{
    HAL_I2C_ER_IRQHandler(&hi2c3);
}
