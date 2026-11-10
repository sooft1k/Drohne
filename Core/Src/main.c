#include "main.h"
#include "icm42688.h"
#include "auxsens.h"
#include "tof.h"
#include "gps.h"
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

SPI_HandleTypeDef hspi1;   /* ICM-42688-P */
UART_HandleTypeDef huart1; /* ELRS-Empfaenger, CRSF */
UART_HandleTypeDef huart2; /* Debug-Monitor */
UART_HandleTypeDef huart3; /* GPS */
TIM_HandleTypeDef htim2;   /* Regeltakt 1 kHz */
TIM_HandleTypeDef htim3;   /* Motorsignal */
IWDG_HandleTypeDef hiwdg;  /* Watchdog */

uint8_t who = 0;
uint8_t imu_ok = 0;
ICM42688_Data_t sensor;

/* Regeltakt: wird im TIM2-Interrupt gesetzt, in der Hauptschleife geleert */
volatile uint8_t loop_flag = 0;
volatile uint32_t loop_missed = 0; /* Zyklen, die nicht rechtzeitig fertig wurden */
volatile uint32_t loop_count = 0;  /* fuer die Frequenzmessung */
uint32_t loop_hz = 0;              /* gemessene Ist-Frequenz, von command.c gelesen */

/* Gemessene Zykluszeit wird auf diesen Bereich begrenzt (Sekunden).
 * Normal sind 0,001 s. Laengere Pausen gibt es nur disarmed, wenn das
 * Terminal schreibt. */
#define DT_MIN 0.0002f
#define DT_MAX 0.020f

static uint32_t imu_recover_ms = 0;  /* letzter Neustart des Sensors, HAL_GetTick() */
static uint8_t imu_fail_report = 0;  /* Sensorausfall noch nicht gemeldet */

static void SystemClock_Config(void);
static void GPIO_Init(void);
static void SPI1_Init(void);
static void USART1_Init(void);
static void USART2_Init(void);
static void USART3_Init(void);
static void TIM2_Loop_Init(void);
static void IWDG_Init(void);
static void DWT_Init(void);

/* Terminal-Ausgabe. Solange gearmt ist, wird nichts gesendet:
 * HAL_UART_Transmit wartet bei 115200 Baud rund 87 us pro Zeichen, eine
 * Zeile kostet mehrere Millisekunden - das wuerde den Regeltakt verzoegern. */
int __io_putchar(int ch)
{
    if (!Motor_IsArmed())
        HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}

int _write(int file, char *ptr, int len)
{
    (void)file;
    if (!Motor_IsArmed())
        HAL_UART_Transmit(&huart2, (uint8_t *)ptr, (uint16_t)len, HAL_MAX_DELAY);
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
    DWT_Init();
    USART2_Init();
    SPI1_Init();
    Motor_Init(&htim3);
    Command_Init(&huart2);
    Control_Init();
    Battery_Init();
    USART1_Init();
    CRSF_Init(&huart1);
    RC_Init();
    USART3_Init();
    GPS_Init(&huart3);
    AuxSens_Init();
    Tof_Init();

    printf("\r\n=== STM32F405 Drohnen FC Boot ===\r\n");
    printf("SysClock: %lu Hz\r\n", HAL_RCC_GetSysClockFreq());
    printf("Motoren: M1=PB0 M2=PB1 M3=PC6 M4=PC7, %lu Hz, Stopp %u us, Gas %.1f-%.1f us\r\n",
           Motor_UpdateRateHz(), MOTOR_US_STOP, MOTOR_US_MIN, MOTOR_US_MAX);
    printf("Regeltakt: 1000 Hz | IMU: SPI1 10,5 MHz | Akku: PC0 | Strom: PC1 | Modus: ANGLE\r\n");
    printf("ELRS: USART1 PA9/PA10, %d Baud (CRSF)\r\n", CRSF_BAUD);
    printf("GPS: USART3 PB10/PB11, %d Baud\r\n", GPS_BAUD);
    printf("Baro: %s | Kompass: %s\r\n", AuxSens_BaroOk() ? "OK" : "FEHLT",
           AuxSens_MagOk() ? "OK" : "FEHLT");
    printf("ToF: VL53L1X an I2C3 PA8/PC9, XSHUT PC8, INT PC2: %s\r\n", Tof_Ok() ? "OK" : "FEHLT");
    printf("Befehle: arm | disarm | status | loop | pid | mix | filt | bat | rc | sens | tof |\r\n");
    printf("         angle | rate | stream on/off | kipp | t <gas> | ja |\r\n");
    printf("         rrp/arp <wert> | glpf | alpf | tau | batcal | curzero | curcal\r\n");
    printf("Arm-Schalter: Kanal %u (AUX%u), Modus: Kanal %u\r\n", RC_CH_ARM, RC_CH_ARM - 4, RC_CH_MODE);

    if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST))
        printf("!! Letzter Neustart durch den Watchdog - die Software hing !!\r\n");
    __HAL_RCC_CLEAR_RESET_FLAGS();

    who = 0;
    if (ICM42688_WhoAmI(&hspi1, &who) == HAL_OK && who == ICM42688_WHO_AM_I_VALUE)
    {
        printf("ICM-42688-P WHO_AM_I = 0x%02X\r\n", who);

        if (ICM42688_Init(&hspi1) == HAL_OK)
        {
            printf("ICM-42688-P init OK (Gyro +-2000 dps, Accel +-4g, Filter 250 Hz)\r\n");
            HAL_Delay(50); /* der Gyro braucht 30 ms bis zu gueltigen Werten */
            /* Kalibrieren, bis die Drohne dabei still lag - hoechstens 5 Versuche */
            ICM42688_CalResult_t cal = ICM42688_CAL_BEWEGT;
            for (int versuch = 1; versuch <= 5 && cal != ICM42688_CAL_OK; versuch++)
            {
                printf("Kalibriere Gyro (Versuch %d)... NICHT bewegen!\r\n", versuch);
                cal = ICM42688_Calibrate(&hspi1, &sensor, 1000);
                if (cal == ICM42688_CAL_BEWEGT)
                    printf("Bewegung erkannt - nochmal.\r\n");
                else if (cal == ICM42688_CAL_LESEFEHLER)
                    printf("Zu viele Lesefehler - nochmal.\r\n");
            }

            if (cal == ICM42688_CAL_OK)
            {
                printf("Offsets: GX=%.1f GY=%.1f GZ=%.1f\r\n",
                       sensor.gyro_x_offset, sensor.gyro_y_offset, sensor.gyro_z_offset);

                /* Lagewinkel gleich aus dem Accel, nicht ab 0 einschwingen */
                if (ICM42688_ReadAll(&hspi1, &sensor) == HAL_OK)
                {
                    ICM42688_Convert(&sensor, 0.001f);
                    ICM42688_InitAngles(&sensor);
                }
                imu_ok = 1;
            }
            else
            {
                printf("Kalibrierung fehlgeschlagen - Arming gesperrt. Neu starten.\r\n");
            }
        }
        else
        {
            printf("ICM-42688-P init FAILED - Arming gesperrt\r\n");
        }
    }
    else
    {
        printf("Kein ICM-42688-P erkannt - Arming gesperrt\r\n");
    }

    /* Watchdog und Regeltakt erst starten, wenn die Kalibrierung durch ist */
    IWDG_Init();
    TIM2_Loop_Init();

    printf(">> Bereit.\r\n");

    uint32_t cyc_last = DWT->CYCCNT; /* Zeitstempel des letzten Regelzyklus */
    float imu_dt = 0.0f;             /* Zeit seit der letzten gueltigen Sensormessung */

    uint32_t stream_div = 0; /* 1000 Hz / 20 = 50 Hz Ausgabe */
    uint32_t kipp_div = 0;   /* 1000 Hz / 200 = 5 Hz Ausgabe */
    uint32_t led_div = 0;
    uint32_t tlm_div = 0;    /* 1000 Hz / 200 = 5 Hz Telemetrie */
    uint32_t hz_stamp = HAL_GetTick();

    while (1)
    {
        /* --- warten auf den naechsten Takt --- */
        if (!loop_flag)
        {
            /* Leerlaufzeit: hier laufen die nicht-zeitkritischen Sachen */
            Command_Process();
            GPS_Process();
            AuxSens_Process();
            Tof_Process();
            if (!Motor_IsArmed())
                Arming_PrintPending();
            Arming_PrintBypassWarning(); /* auch gearmt, sendet per Interrupt */
            if (imu_fail_report && !Motor_IsArmed())
            {
                imu_fail_report = 0;
                printf(">> SENSOR AUSGEFALLEN - Motoren aus. Fehler bisher: %lu\r\n",
                       ICM42688_ErrorCount());
            }
            continue;
        }

        /* --- Sensordaten dieses Takts lesen, dauert rund 12 us --- */
        int imu = -1;
        if (imu_ok)
            imu = ICM42688_Read(&hspi1, &sensor);
        loop_flag = 0;
        loop_count++;

        /* Zeit seit dem letzten Regelzyklus messen statt 1 ms anzunehmen.
         * Filter, Lagewinkel und PID rechnen damit richtig, auch wenn ein
         * Takt ausfaellt. Taktzaehler: 168 MHz, laeuft nach 25 s ueber,
         * die vorzeichenlose Differenz stimmt trotzdem. */
        const uint32_t cyc = DWT->CYCCNT;
        float dt = (float)(cyc - cyc_last) / (float)SystemCoreClock;
        cyc_last = cyc;
        if (dt < DT_MIN)
            dt = DT_MIN;
        if (dt > DT_MAX)
            dt = DT_MAX;

        /* ===== REGELZYKLUS - nichts Blockierendes hineinschreiben ===== */

        /* Die Lagewinkel bekommen die Zeit seit der letzten GUELTIGEN Messung -
         * faellt ein Lesevorgang aus, geht die Drehung dazwischen nicht verloren. */
        imu_dt += dt;
        if (imu_dt > DT_MAX)
            imu_dt = DT_MAX;
        if (imu == 1)
        {
            ICM42688_Convert(&sensor, imu_dt);
            ICM42688_UpdateAngles(&sensor, imu_dt);
            imu_dt = 0.0f;
        }
        if (imu_ok)
        {
            /* Einzelne Fehler: die Regelung rechnet mit den letzten Werten
             * weiter. Nach ICM42688_FAIL_LIMIT Fehlern in Folge gilt der
             * Sensor als ausgefallen (siehe unten). */
            ICM42688_ReportResult(imu == 1);
            /* Hatte der Sensor die Spannung verloren, muss er neu eingestellt
             * werden. Das blockiert kurz, deshalb nur disarmed. */
            if (imu != 1 && ICM42688_FailRun() >= 3 && !Motor_IsArmed() &&
                HAL_GetTick() - imu_recover_ms >= 100)
            {
                imu_recover_ms = HAL_GetTick();
                HAL_IWDG_Refresh(&hiwdg);
                (void)ICM42688_Init(&hspi1);
                HAL_IWDG_Refresh(&hiwdg);
            }
        }

        /* Ohne funktionierenden Sensor gibt es keine Lageregelung: sofort aus */
        if (Motor_IsArmed() && (!imu_ok || ICM42688_Failed()))
        {
            Arming_Disarm();
            imu_fail_report = 1;
        }

        /* Funkempfang auswerten: Sollwerte, Failsafe, Arming */
        RC_Update(dt);

        /* Terminal-Arm: ohne Zeichen vom Terminal nach 1 s aus */
        Command_ArmWatchdog();

        float out_roll, out_pitch, out_yaw;
        Control_Update(&sensor, dt, Mixer_IsSaturated(), &out_roll, &out_pitch, &out_yaw);

        /* Bei niedrigem Akku wird das Gas begrenzt (battery.h), mit batbypass nicht */
        Mixer_Update(Arming_LimitThrottle(setpoint.throttle), out_roll, out_pitch, out_yaw);

        Battery_Update(dt);

        /* Lebenszeichen fuer den Watchdog - nur wenn der Regelzyklus laeuft */
        HAL_IWDG_Refresh(&hiwdg);

        /* ===== ENDE REGELZYKLUS ===== */

        /* Ausgabe stark gedrosselt und nur disarmed - printf blockiert, eine
         * Zeile kostet rund 7 ms. Gearmt sendet _write ohnehin nichts.
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

        /* LED als Lebenszeichen: blinkt mit 2 Hz, bei Akku-Warnung schnell
         * (5 Hz), bei kritischem Akku sehr schnell (12,5 Hz). Ohne Sensor
         * (fehlt oder Kalibrierung gescheitert) leuchtet sie dauernd -
         * dann laesst sich nicht armen. */
        const uint32_t led_period = (Battery_State() == BAT_CRITICAL) ? 40
                                    : (Battery_State() == BAT_WARN)   ? 100
                                                                      : 250;
        if (!imu_ok)
        {
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET); /* aktiv-low: an */
        }
        else if (++led_div >= led_period)
        {
            led_div = 0;
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        }

        /* Akku-Telemetrie an die Funke, 5x pro Sekunde. Dort kann man fuer
         * Spannung oder Restladung eine Ansage einstellen. */
        if (++tlm_div >= 200)
        {
            tlm_div = 0;
            if (Battery_Cells() > 0)
                CRSF_SendBattery(Battery_Volt(), Battery_Current(), Battery_MahUsed(),
                                 Battery_Percent());
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

    /* PA4: Chip-Select der IMU, im Ruhezustand hoch */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
    g.Pin = GPIO_PIN_4;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &g);
}

/* SPI1 fuer die IMU: PA5 SCK, PA6 MISO, PA7 MOSI. 84 MHz / 8 = 10,5 MHz, der
 * Sensor kann bis 24 MHz. Mode 0: SCK ruht auf low, so entsteht beim ersten
 * Einschalten der Peripherie keine Taktflanke. */
static void SPI1_Init(void)
{
    __HAL_RCC_SPI1_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &g);

    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 10;
    if (HAL_SPI_Init(&hspi1) != HAL_OK)
        Error_Handler();
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

/* USART3 fuer das GPS: PB10 = TX, PB11 = RX */
static void USART3_Init(void)
{
    __HAL_RCC_USART3_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_10 | GPIO_PIN_11;
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOB, &g);

    huart3.Instance = USART3;
    huart3.Init.BaudRate = GPS_BAUD;
    huart3.Init.WordLength = UART_WORDLENGTH_8B;
    huart3.Init.StopBits = UART_STOPBITS_1;
    huart3.Init.Parity = UART_PARITY_NONE;
    huart3.Init.Mode = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart3) != HAL_OK)
        Error_Handler();

    HAL_NVIC_SetPriority(USART3_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(USART3_IRQn);
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