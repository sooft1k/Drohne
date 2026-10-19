#include "main.h"
#include "motor.h"

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern TIM_HandleTypeDef htim2;
extern I2C_HandleTypeDef hi2c1;

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

void TIM2_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim2);
}

/* MPU6050 wird per Interrupt gelesen (mpu6050.c) */
void I2C1_EV_IRQHandler(void)
{
    HAL_I2C_EV_IRQHandler(&hi2c1);
}

void I2C1_ER_IRQHandler(void)
{
    HAL_I2C_ER_IRQHandler(&hi2c1);
}
