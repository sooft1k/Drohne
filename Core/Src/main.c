#include "main.h"
#include "mpu6050.h"
#include "motor.h"
#include "command.h"
#include "control.h"
#include "mixer.h"
#include <stdio.h>
#include <string.h>

I2C_HandleTypeDef hi2c1;
UART_HandleTypeDef huart2;
TIM_HandleTypeDef htim2; /* Regeltakt 1 kHz */
TIM_HandleTypeDef htim3; /* Motorsignal */

uint8_t who = 0;
uint8_t mpu_ok = 0;
MPU6050_Data_t sensor;

/* Regeltakt: wird im TIM2-Interrupt gesetzt, in der Hauptschleife geleert */
volatile uint8_t loop_flag = 0;
volatile uint32_t loop_missed = 0; /* Zyklen, die nicht rechtzeitig fertig wurden */
volatile uint32_t loop_count = 0;  /* fuer die Frequenzmessung */
uint32_t loop_hz = 0;              /* gemessene Ist-Frequenz, von command.c gelesen */

static void SystemClock_Config(void);
static void GPIO_Init(void);
static void I2C1_Init(void);
static void USART2_Init(void);
static void TIM2_Loop_Init(void);

int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}

int _write(int file, char *ptr, int len)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)ptr, len, HAL_MAX_DELAY);
    return len;
}

/* Wird von HAL bei jedem Timer-Ueberlauf gerufen */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2)
    {
        if (loop_flag)
            loop_missed++; /* vorheriger Zyklus war noch nicht fertig */
        loop_flag = 1;
    }
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    GPIO_Init();
    USART2_Init();
    I2C1_Init();
    Motor_Init(&htim3);
    Command_Init(&huart2);
    Control_Init();

    printf("\r\n=== STM32F405 Drohnen FC Boot ===\r\n");
    printf("SysClock: %lu Hz\r\n", HAL_RCC_GetSysClockFreq());
    printf("Motoren: M1=PB0 M2=PB1 M3=PA6 M4=PA7, %lu Hz, Start %u us (disarmed)\r\n",
           Motor_UpdateRateHz(), MOTOR_US_IDLE);
    printf("Regeltakt: 1000 Hz | I2C: 400 kHz | Modus: ANGLE\r\n");
    printf("Befehle: arm | disarm | status | loop | pid | mix | angle | rate |\r\n");
    printf("         stream on/off | t <gas> | 0-100 | m1..m4 <wert> | rrp/arp <wert>\r\n");

    who = 0;
    if (MPU6050_WhoAmI(&hi2c1, &who) == HAL_OK && who == 0x68)
    {
        printf("MPU6050 WHO_AM_I = 0x%02X\r\n", who);

        if (MPU6050_Init(&hi2c1) == HAL_OK)
        {
            printf("MPU6050 init OK\r\n");
            printf("Kalibriere Gyro... NICHT bewegen!\r\n");
            MPU6050_Calibrate(&hi2c1, &sensor, 1000);
            printf("Offsets: GX=%.1f GY=%.1f GZ=%.1f\r\n",
                   sensor.gyro_x_offset, sensor.gyro_y_offset, sensor.gyro_z_offset);

            sensor.roll = 0.0f;
            sensor.pitch = 0.0f;
            sensor.yaw = 0.0f;
            mpu_ok = 1;
        }
        else
        {
            printf("MPU6050 init FAILED - laufe ohne Sensor weiter\r\n");
        }
    }
    else
    {
        printf("Kein MPU6050 erkannt - Motorbetrieb ohne Sensor moeglich\r\n");
    }

    /* Regeltakt erst starten wenn die Kalibrierung durch ist */
    TIM2_Loop_Init();

    printf(">> Bereit.\r\n");

    const float DT = 0.001f; /* 1 ms, fest vom Timer vorgegeben */

    uint32_t stream_div = 0; /* 1000 Hz / 20 = 50 Hz Ausgabe */
    uint32_t led_div = 0;
    uint32_t hz_stamp = HAL_GetTick();

    while (1)
    {
        /* --- warten auf den naechsten Takt --- */
        if (!loop_flag)
        {
            /* Leerlaufzeit: hier laufen die nicht-zeitkritischen Sachen */
            Command_Process();
            continue;
        }
        loop_flag = 0;
        loop_count++;

        /* ===== REGELZYKLUS - nichts Blockierendes hineinschreiben ===== */

        if (mpu_ok && MPU6050_ReadAll(&hi2c1, &sensor) == HAL_OK)
        {
            MPU6050_Convert(&sensor);
            MPU6050_UpdateAngles(&sensor, DT);
        }

        float out_roll, out_pitch, out_yaw;
        Control_Update(&sensor, DT, &out_roll, &out_pitch, &out_yaw);

        Mixer_Update(setpoint.throttle, out_roll, out_pitch, out_yaw);

        /* ===== ENDE REGELZYKLUS ===== */

        /* Ausgabe stark gedrosselt - printf blockiert und darf den Takt nicht fressen */
        if (Command_StreamOn() && ++stream_div >= 20)
        {
            stream_div = 0;
            printf("Roll:%7.2f  Pitch:%7.2f  Yaw:%7.2f  | GX:%7.2f GY:%7.2f GZ:%7.2f\r\n",
                   sensor.roll, sensor.pitch, sensor.yaw,
                   sensor.gyro_x_dps, sensor.gyro_y_dps, sensor.gyro_z_dps);
        }

        /* LED blinkt mit ~2 Hz als Lebenszeichen */
        if (++led_div >= 250)
        {
            led_div = 0;
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        }

        /* Ist-Frequenz einmal pro Sekunde festhalten */
        uint32_t now = HAL_GetTick();
        if (now - hz_stamp >= 1000)
        {
            hz_stamp = now;
            loop_hz = loop_count;
            loop_count = 0;
        }
    }
}

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM = 8;
    osc.PLL.PLLN = 336;
    osc.PLL.PLLP = RCC_PLLP_DIV2;
    osc.PLL.PLLQ = 7;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
        Error_Handler();

    clk.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV4;
    clk.APB2CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK)
        Error_Handler();
}

static void GPIO_Init(void)
{
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    g.Pin = GPIO_PIN_13;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &g);
}

/* I2C mit 400 kHz - bei 100 kHz dauert ein Sensor-Lesevorgang
 * rund 1,4 ms und 1000 Hz waeren unmoeglich. */
static void I2C1_Init(void)
{
    __HAL_RCC_I2C1_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    g.Mode = GPIO_MODE_AF_OD;
    g.Pull = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &g);

    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 400000;
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK)
        Error_Handler();
}

static void USART2_Init(void)
{
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &g);

    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart2) != HAL_OK)
        Error_Handler();

    HAL_NVIC_SetPriority(USART2_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}

/* TIM2 als Regeltakt: 84 MHz / 84 = 1 MHz Tick, 1000 Ticks = 1 ms = 1000 Hz */
static void TIM2_Loop_Init(void)
{
    __HAL_RCC_TIM2_CLK_ENABLE();

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 84 - 1;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 1000 - 1;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
        Error_Handler();

    /* Prioritaet hoeher als UART, damit der Takt nicht verschluckt wird */
    HAL_NVIC_SetPriority(TIM2_IRQn, 4, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);

    HAL_TIM_Base_Start_IT(&htim2);
}

void SysTick_Handler(void) { HAL_IncTick(); }

void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        for (volatile int i = 0; i < 1000000; i++)
            ;
    }
}

void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
    while (1)
    {
    }
}