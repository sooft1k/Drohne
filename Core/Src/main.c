#include "main.h"
#include "mpu6050.h"
#include "motor.h"
#include "command.h"
#include <stdio.h>
#include <string.h>

I2C_HandleTypeDef hi2c1;
UART_HandleTypeDef huart2;
TIM_HandleTypeDef htim3;
uint8_t who = 0;
uint8_t mpu_ok = 0; /* 1 = Sensor erkannt und nutzbar */
MPU6050_Data_t sensor;

static void SystemClock_Config(void);
static void GPIO_Init(void);
static void I2C1_Init(void);
static void USART2_Init(void);

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

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    GPIO_Init();
    USART2_Init();
    I2C1_Init();
    Motor_Init(&htim3);
    Command_Init(&huart2);

    printf("\r\n=== STM32F405 Drohnen FC Boot ===\r\n");
    printf("SysClock: %lu Hz\r\n", HAL_RCC_GetSysClockFreq());
    printf("Motoren PWM: M1=PB0 M2=PB1 M3=PA6 M4=PA7, 50 Hz, Start 1000 us (disarmed)\r\n");
    printf("Befehle: arm | disarm | status | stream on/off | 0-100 | m1..m4 <wert>\r\n");

    /* --- MPU6050 nur nutzen wenn er wirklich da ist --- */
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

    printf(">> Bereit.\r\n");

    uint32_t last = HAL_GetTick();

    while (1)
    {
        Command_Process();

        if (mpu_ok && MPU6050_ReadAll(&hi2c1, &sensor) == HAL_OK)
        {
            uint32_t now = HAL_GetTick();
            float dt = (now - last) / 1000.0f;
            last = now;

            MPU6050_Convert(&sensor);
            MPU6050_UpdateAngles(&sensor, dt);

            if (Command_StreamOn())
            {
                printf("Roll:%7.2f  Pitch:%7.2f  Yaw:%7.2f  | GX:%7.2f GY:%7.2f GZ:%7.2f\r\n",
                       sensor.roll, sensor.pitch, sensor.yaw,
                       sensor.gyro_x_dps, sensor.gyro_y_dps, sensor.gyro_z_dps);
            }
        }

        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        HAL_Delay(10); /* ~100 Hz */
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

static void I2C1_Init(void)
{
    __HAL_RCC_I2C1_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    g.Mode = GPIO_MODE_AF_OD;
    g.Pull = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &g);

    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 100000;
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

    HAL_NVIC_SetPriority(USART2_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
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