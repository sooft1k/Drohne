#include "main.h"
#include "mpu6050.h"
#include "motor.h"
#include "command.h"
#include "control.h"
#include "mixer.h"
#include "battery.h"
#include "crsf.h"
#include "rc.h"
#include "arming.h"
#include <stdio.h>
#include <string.h>

I2C_HandleTypeDef hi2c1;
UART_HandleTypeDef huart1; /* ELRS-Empfaenger, CRSF */
UART_HandleTypeDef huart2; /* Debug-Monitor */
TIM_HandleTypeDef htim2;   /* Regeltakt 1 kHz */
TIM_HandleTypeDef htim3;   /* Motorsignal */
IWDG_HandleTypeDef hiwdg;  /* Watchdog */

uint8_t who = 0;
uint8_t mpu_ok = 0;
MPU6050_Data_t sensor;

/* Regeltakt: wird im TIM2-Interrupt gesetzt, in der Hauptschleife geleert */
volatile uint8_t loop_flag = 0;
volatile uint32_t loop_missed = 0; /* Zyklen, die nicht rechtzeitig fertig wurden */
volatile uint32_t loop_count = 0;  /* fuer die Frequenzmessung */
uint32_t loop_hz = 0;              /* gemessene Ist-Frequenz, von command.c gelesen */

/* Spaetestens so lange nach dem Takt muss der Sensor geantwortet haben.
 * Ein Lesevorgang dauert bei 400 kHz etwa 0,4 ms. */
#define IMU_DEADLINE_US 800

static uint32_t imu_recover_ms = 0;  /* letzte Bus-Recovery, HAL_GetTick() */
static uint8_t imu_fail_report = 0;  /* Sensorausfall noch nicht gemeldet */

static void SystemClock_Config(void);
static void GPIO_Init(void);
static void I2C1_Init(void);
static void USART1_Init(void);
static void USART2_Init(void);
static void TIM2_Loop_Init(void);
static void IWDG_Init(void);
static void DWT_Init(void);
static HAL_StatusTypeDef I2C1_Setup(void);
static void I2C1_Recover(void);

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

        /* Sensor fuer diesen Takt anstossen. Der Lesevorgang laeuft per
         * Interrupt im Hintergrund, die Hauptschleife holt ihn ab. */
        if (mpu_ok)
            MPU6050_StartRead(&hi2c1);
    }
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    GPIO_Init();
    DWT_Init();
    USART2_Init();
    I2C1_Init();
    Motor_Init(&htim3);
    Command_Init(&huart2);
    Control_Init();
    Battery_Init();
    USART1_Init();
    CRSF_Init(&huart1);
    RC_Init();

    printf("\r\n=== STM32F405 Drohnen FC Boot ===\r\n");
    printf("SysClock: %lu Hz\r\n", HAL_RCC_GetSysClockFreq());
    printf("Motoren: M1=PB0 M2=PB1 M3=PA6 M4=PA7, %lu Hz, Start %u us (disarmed)\r\n",
           Motor_UpdateRateHz(), MOTOR_US_IDLE);
    printf("Regeltakt: 1000 Hz | I2C: 400 kHz | Akku: PC0 | Modus: ANGLE\r\n");
    printf("ELRS: USART1 PA9/PA10, %d Baud (CRSF)\r\n", CRSF_BAUD);
    printf("Befehle: arm | disarm | status | loop | pid | mix | filt | bat | rc |\r\n");
    printf("         angle | rate | stream on/off | kipp | t <gas>\r\n");

    if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST))
        printf("!! Letzter Neustart durch den Watchdog - die Software hing !!\r\n");
    __HAL_RCC_CLEAR_RESET_FLAGS();

    who = 0;
    if (MPU6050_WhoAmI(&hi2c1, &who) == HAL_OK && who == 0x68)
    {
        printf("MPU6050 WHO_AM_I = 0x%02X\r\n", who);

        if (MPU6050_Init(&hi2c1) == HAL_OK)
        {
            printf("MPU6050 init OK (Gyro +-500 dps, Accel +-4g, DLPF 188 Hz)\r\n");
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
            printf("MPU6050 init FAILED - Arming gesperrt\r\n");
        }
    }
    else
    {
        printf("Kein MPU6050 erkannt - Arming gesperrt\r\n");
    }

    /* Watchdog und Regeltakt erst starten, wenn die Kalibrierung durch ist */
    IWDG_Init();
    TIM2_Loop_Init();

    printf(">> Bereit.\r\n");

    const float DT = 0.001f; /* 1 ms, fest vom Timer vorgegeben */

    uint32_t stream_div = 0; /* 1000 Hz / 20 = 50 Hz Ausgabe */
    uint32_t kipp_div = 0;   /* 1000 Hz / 200 = 5 Hz Ausgabe */
    uint32_t led_div = 0;
    uint32_t hz_stamp = HAL_GetTick();

    while (1)
    {
        /* --- warten auf den naechsten Takt --- */
        if (!loop_flag)
        {
            /* Leerlaufzeit: hier laufen die nicht-zeitkritischen Sachen */
            Command_Process();
            Arming_PrintPending();
            if (imu_fail_report && !Motor_IsArmed())
            {
                imu_fail_report = 0;
                printf(">> SENSOR AUSGEFALLEN - Motoren aus. Fehler bisher: %lu\r\n",
                       MPU6050_ErrorCount());
            }
            continue;
        }

        /* --- Sensordaten dieses Takts abholen ---
         * Der Lesevorgang laeuft seit dem TIM2-Interrupt (ca. 0,4 ms). Hier
         * wird nur gewartet, bis er fertig ist - hoechstens bis
         * IMU_DEADLINE_US nach dem Takt, dann zaehlt er als Fehler. */
        int imu = -1;
        if (mpu_ok)
        {
            while ((imu = MPU6050_ReadPoll(&sensor)) == 0 && TIM2->CNT < IMU_DEADLINE_US)
            {
            }
        }
        loop_flag = 0;
        loop_count++;

        /* ===== REGELZYKLUS - nichts Blockierendes hineinschreiben ===== */

        if (imu == 1)
        {
            MPU6050_Convert(&sensor, DT);
            MPU6050_UpdateAngles(&sensor, DT);
        }
        if (mpu_ok)
        {
            /* Einzelne Fehler: die Regelung rechnet mit den letzten Werten
             * weiter. Nach MPU6050_FAIL_LIMIT Fehlern in Folge gilt der
             * Sensor als ausgefallen (siehe unten). */
            MPU6050_ReportResult(imu == 1);
            if (imu != 1 && MPU6050_FailRun() >= 3 && HAL_GetTick() - imu_recover_ms >= 20)
            {
                imu_recover_ms = HAL_GetTick();
                I2C1_Recover();
            }
        }

        /* Ohne funktionierenden Sensor gibt es keine Lageregelung: sofort aus */
        if (Motor_IsArmed() && (!mpu_ok || MPU6050_Failed()))
        {
            Arming_Disarm();
            imu_fail_report = 1;
        }

        /* Funkempfang auswerten: Sollwerte, Failsafe, Arming */
        RC_Update(DT);

        float out_roll, out_pitch, out_yaw;
        Control_Update(&sensor, DT, &out_roll, &out_pitch, &out_yaw);

        Mixer_Update(setpoint.throttle, out_roll, out_pitch, out_yaw);

        Battery_Update(DT);

        /* Lebenszeichen fuer den Watchdog - nur wenn der Regelzyklus laeuft */
        HAL_IWDG_Refresh(&hiwdg);

        /* ===== ENDE REGELZYKLUS ===== */

        /* Ausgabe stark gedrosselt und nur disarmed - printf blockiert, eine
         * Zeile kostet rund 7 ms und wuerde gearmt den Regeltakt fressen.
         * Alle Werte in Koerperachsen, Vorzeichen siehe mixer.c. */
        if (Command_StreamOn() && !Motor_IsArmed() && ++stream_div >= 20)
        {
            stream_div = 0;
            printf("R:%6.1f P:%6.1f | Gx:%7.2f Gy:%7.2f Gz:%7.2f | roh Gx:%7.2f Gy:%7.2f\r\n",
                   sensor.roll, sensor.pitch,
                   sensor.gyro_x_dps, sensor.gyro_y_dps, sensor.gyro_z_dps,
                   sensor.gyro_x_raw_dps, sensor.gyro_y_raw_dps);
        }

        /* Kipptest: was der Mixer bei 50 % Gas aus den aktuellen Reglerausgaben
         * machen wuerde. Nur disarmed - die Motoren bleiben dabei aus. */
        if (Command_KippOn() && !Motor_IsArmed() && ++kipp_div >= 200)
        {
            kipp_div = 0;
            float m[4];
            Mixer_Preview(50.0f, out_roll, out_pitch, out_yaw, m);
            printf("R:%6.1f P:%6.1f Gz:%7.1f | M1 %5.1f  M2 %5.1f  M3 %5.1f  M4 %5.1f\r\n",
                   sensor.roll, sensor.pitch, sensor.gyro_z_dps,
                   m[0], m[1], m[2], m[3]);
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
    if (I2C1_Setup() != HAL_OK)
        Error_Handler();

    /* Gleiche Prioritaet wie der Regeltakt (TIM2): die beiden unterbrechen
     * sich nicht gegenseitig, TIM2 startet die Lesevorgaenge. */
    HAL_NVIC_SetPriority(I2C1_EV_IRQn, 4, 0);
    HAL_NVIC_SetPriority(I2C1_ER_IRQn, 4, 0);
    HAL_NVIC_EnableIRQ(I2C1_EV_IRQn);
    HAL_NVIC_EnableIRQ(I2C1_ER_IRQn);
}

static HAL_StatusTypeDef I2C1_Setup(void)
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
    return HAL_I2C_Init(&hi2c1);
}

/* Wartet us Mikrosekunden mit dem Taktzaehler. Nur fuer die Bus-Recovery. */
static void delay_us(uint32_t us)
{
    const uint32_t start = DWT->CYCCNT;
    const uint32_t n = us * (SystemCoreClock / 1000000U);
    while (DWT->CYCCNT - start < n)
    {
    }
}

/* I2C-Bus und Peripherie neu aufsetzen, wenn das Lesen mehrmals scheitert.
 * Typischer Fall: eine Uebertragung wurde mittendrin abgebrochen, der
 * Sensor haelt SDA fest und das BUSY-Flag der Peripherie bleibt stehen.
 * Dauert etwa 0,2 ms, disarmed zusaetzlich etwa 15 ms fuer MPU6050_Init
 * (hoechstens 6 Transfers x 3 ms + 10 ms Pause - unter der Watchdog-Zeit). */
static void I2C1_Recover(void)
{
    MPU6050_BlockReads(1);
    HAL_NVIC_DisableIRQ(I2C1_EV_IRQn);
    HAL_NVIC_DisableIRQ(I2C1_ER_IRQn);
    HAL_I2C_DeInit(&hi2c1);

    /* SCL (PB6) und SDA (PB7) als Open-Drain-Ausgang. Haelt der Sensor SDA
     * fest, mit bis zu 9 Takten auf SCL freitakten, danach ein STOP. */
    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    g.Mode = GPIO_MODE_OUTPUT_OD;
    g.Pull = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_SET);
    HAL_GPIO_Init(GPIOB, &g);
    delay_us(5);

    for (int i = 0; i < 9 && HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7) == GPIO_PIN_RESET; i++)
    {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
        delay_us(5);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
        delay_us(5);
    }

    /* STOP: SDA geht hoch, waehrend SCL hoch ist */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
    delay_us(5);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET);
    delay_us(5);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
    delay_us(5);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);
    delay_us(5);

    /* Peripherie komplett zuruecksetzen - das loescht auch ein haengendes
     * BUSY-Flag - und neu einrichten (Pins wieder auf I2C) */
    __HAL_RCC_I2C1_FORCE_RESET();
    __HAL_RCC_I2C1_RELEASE_RESET();
    (void)I2C1_Setup();

    __HAL_I2C_CLEAR_FLAG(&hi2c1, I2C_FLAG_AF);
    HAL_NVIC_ClearPendingIRQ(I2C1_EV_IRQn);
    HAL_NVIC_ClearPendingIRQ(I2C1_ER_IRQn);
    HAL_NVIC_EnableIRQ(I2C1_EV_IRQn);
    HAL_NVIC_EnableIRQ(I2C1_ER_IRQn);

    /* Hatte der Sensor die Spannung verloren, ist er wieder im Schlafmodus
     * und muss neu eingestellt werden. Das blockiert kurz, deshalb nur
     * disarmed und nur bei freiem Bus (sonst warten die HAL-Aufrufe je 25 ms). */
    if (!Motor_IsArmed() && __HAL_I2C_GET_FLAG(&hi2c1, I2C_FLAG_BUSY) == RESET)
    {
        HAL_IWDG_Refresh(&hiwdg);
        (void)MPU6050_InitTimeout(&hi2c1, 3);
        HAL_IWDG_Refresh(&hiwdg);
    }

    MPU6050_BlockReads(0);
}

/* USART1 fuer den ELRS-Empfaenger: PA9 = TX, PA10 = RX, 420000 Baud */
static void USART1_Init(void)
{
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_9 | GPIO_PIN_10;
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &g);

    huart1.Instance = USART1;
    huart1.Init.BaudRate = CRSF_BAUD;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK)
        Error_Handler();

    /* Prioritaet zwischen Regeltakt (4) und Debug-UART (6) */
    HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
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

/* Watchdog: setzt den Prozessor zurueck, wenn der Regelzyklus laenger als
 * etwa 100 ms nicht laeuft (LSI 32 kHz / 32 = 1 kHz, 100 Schritte; wegen der
 * LSI-Toleranz 17..47 kHz tatsaechlich 68..188 ms). Nach dem Neustart sind
 * die Motoren aus (disarmed). Beim Debuggen haelt er mit dem Prozessor an. */
static void IWDG_Init(void)
{
    __HAL_DBGMCU_FREEZE_IWDG();
    hiwdg.Instance = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_32;
    hiwdg.Init.Reload = 100;
    if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
        Error_Handler();
}

/* Taktzaehler des Cortex-M4 fuer Mikrosekunden-Wartezeiten */
static void DWT_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void SysTick_Handler(void) { HAL_IncTick(); }

void Error_Handler(void)
{
    Motor_EmergencyStop(); /* zuerst die Motoren */
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
    Motor_EmergencyStop();
    while (1)
    {
    }
}