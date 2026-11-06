#include "main.h"
#include "auxsens.h"
#include "bmp388.h"
#include "qmc5883l.h"
#include "motor.h"
#include <stdio.h>

I2C_HandleTypeDef hi2c1;
extern IWDG_HandleTypeDef hiwdg; /* aus main.c */

/* Abwechselnd lesen, jeder Sensor kommt alle 20 ms dran (50 Hz) */
#define SLOT_MS 10
/* 8 Bytes dauern bei 400 kHz rund 0,25 ms. Danach gilt der Transfer als haengend. */
#define XFER_TIMEOUT_MS 5
/* Fehler in Folge, ab denen ein Sensor als ausgefallen gilt */
#define FAIL_LIMIT 10
/* So lange ohne neuen Wert, dann gilt der Sensor ebenfalls als ausgefallen */
#define STALE_MS 1000
/* Abstand der Neustart-Versuche, nur disarmed */
#define REINIT_MS 1000
/* Abstand der Bus-Recovery */
#define RECOVER_MS 100
/* Timeout pro Transfer bei einem Neustart im Betrieb, passt zum Watchdog */
#define REINIT_TIMEOUT_MS 3

typedef enum
{
    DEV_BARO = 0,
    DEV_MAG,
    DEV_COUNT
} Dev_t;

typedef struct
{
    uint8_t ok;
    uint16_t fail_run;
    uint32_t errors;
    uint32_t last_ok_ms;
} DevState_t;

enum
{
    X_IDLE = 0,
    X_RUNNING,
    X_DONE_OK,
    X_DONE_ERR
};

static DevState_t dev[DEV_COUNT];
static uint8_t rx[8];
static volatile uint8_t xfer = X_IDLE;
static Dev_t cur = DEV_BARO;
static uint32_t xfer_ms = 0;
static uint32_t slot_ms = 0;
static uint32_t reinit_ms = 0;
static uint32_t recover_ms = 0;

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

/* I2C-Bus und Peripherie neu aufsetzen, wenn ein Transfer haengt.
 * Typischer Fall: eine Uebertragung wurde mittendrin abgebrochen, ein Sensor
 * haelt SDA fest und das BUSY-Flag der Peripherie bleibt stehen.
 * Dauert etwa 0,2 ms und laeuft deshalb auch gearmt. */
static void I2C1_Recover(void)
{
    HAL_NVIC_DisableIRQ(I2C1_EV_IRQn);
    HAL_NVIC_DisableIRQ(I2C1_ER_IRQn);
    HAL_I2C_DeInit(&hi2c1);

    /* SCL (PB6) und SDA (PB7) als Open-Drain-Ausgang. Haelt ein Sensor SDA
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
    xfer = X_IDLE;
    HAL_NVIC_ClearPendingIRQ(I2C1_EV_IRQn);
    HAL_NVIC_ClearPendingIRQ(I2C1_ER_IRQn);
    HAL_NVIC_EnableIRQ(I2C1_EV_IRQn);
    HAL_NVIC_EnableIRQ(I2C1_ER_IRQn);
}

static HAL_StatusTypeDef dev_init(Dev_t d, uint32_t timeout)
{
    return (d == DEV_BARO) ? BMP388_Init(&hi2c1, timeout) : QMC5883L_Init(&hi2c1, timeout);
}

static void dev_mark_ok(Dev_t d, uint32_t now)
{
    dev[d].ok = 1;
    dev[d].fail_run = 0;
    dev[d].last_ok_ms = now;
}

void AuxSens_Init(void)
{
    if (I2C1_Setup() != HAL_OK)
        Error_Handler();

    /* Unter dem Regeltakt (4) und den UARTs (5, 6): die Sensoren hier sind
     * nicht zeitkritisch */
    HAL_NVIC_SetPriority(I2C1_EV_IRQn, 7, 0);
    HAL_NVIC_SetPriority(I2C1_ER_IRQn, 7, 0);
    HAL_NVIC_EnableIRQ(I2C1_EV_IRQn);
    HAL_NVIC_EnableIRQ(I2C1_ER_IRQn);

    const uint32_t now = HAL_GetTick();
    for (int i = 0; i < DEV_COUNT; i++)
    {
        if (dev_init((Dev_t)i, 100) == HAL_OK)
            dev_mark_ok((Dev_t)i, now);
    }
}

void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C1)
        xfer = X_DONE_OK;
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C1)
        xfer = X_DONE_ERR;
}

static void dev_fail(Dev_t d)
{
    dev[d].errors++;
    if (dev[d].fail_run < 0xFFFF)
        dev[d].fail_run++;
    if (dev[d].fail_run >= FAIL_LIMIT)
        dev[d].ok = 0;
}

static void recover_limited(uint32_t now)
{
    if (now - recover_ms >= RECOVER_MS)
    {
        recover_ms = now;
        I2C1_Recover();
    }
}

static void read_start(Dev_t d, uint32_t now)
{
    if (__HAL_I2C_GET_FLAG(&hi2c1, I2C_FLAG_BUSY) != RESET ||
        HAL_I2C_GetState(&hi2c1) != HAL_I2C_STATE_READY)
    {
        dev_fail(d);
        recover_limited(now);
        return;
    }

    const uint16_t addr = (d == DEV_BARO) ? BMP388_ADDR : QMC5883L_ADDR;
    const uint16_t reg = (d == DEV_BARO) ? BMP388_READ_REG : QMC5883L_READ_REG;
    const uint16_t len = (d == DEV_BARO) ? BMP388_READ_LEN : QMC5883L_READ_LEN;

    cur = d;
    xfer_ms = now;
    xfer = X_RUNNING;
    if (HAL_I2C_Mem_Read_IT(&hi2c1, addr, reg, I2C_MEMADD_SIZE_8BIT, rx, len) != HAL_OK)
    {
        xfer = X_IDLE;
        dev_fail(d);
    }
}

/* Ausgefallene Sensoren neu einrichten. Blockiert einige Millisekunden, deshalb
 * nur disarmed und nur bei freiem Bus (sonst warten die HAL-Aufrufe je 25 ms). */
static void reinit_failed(uint32_t now)
{
    if (Motor_IsArmed() || now - reinit_ms < REINIT_MS)
        return;
    reinit_ms = now;

    for (int i = 0; i < DEV_COUNT; i++)
    {
        if (dev[i].ok || __HAL_I2C_GET_FLAG(&hi2c1, I2C_FLAG_BUSY) != RESET)
            continue;
        HAL_IWDG_Refresh(&hiwdg);
        if (dev_init((Dev_t)i, REINIT_TIMEOUT_MS) == HAL_OK)
            dev_mark_ok((Dev_t)i, HAL_GetTick());
        HAL_IWDG_Refresh(&hiwdg);
    }
}

void AuxSens_Process(void)
{
    const uint32_t now = HAL_GetTick();

    /* Einmal lesen: der Interrupt kann den Wert jederzeit aendern */
    const uint8_t x = xfer;

    if (x == X_RUNNING)
    {
        if (now - xfer_ms > XFER_TIMEOUT_MS)
        {
            dev_fail(cur);
            xfer_ms = now; /* zaehlt nur einmal, falls die Recovery noch warten muss */
            recover_limited(now);
        }
        return;
    }

    if (x == X_DONE_OK)
    {
        uint8_t fresh = 1;
        if (cur == DEV_BARO)
            fresh = BMP388_Parse(rx);
        else
            QMC5883L_Parse(rx);
        dev[cur].fail_run = 0;
        if (fresh)
            dev[cur].last_ok_ms = now;
        xfer = X_IDLE;
    }
    else if (x == X_DONE_ERR)
    {
        dev_fail(cur);
        xfer = X_IDLE;
    }

    for (int i = 0; i < DEV_COUNT; i++)
    {
        if (dev[i].ok && now - dev[i].last_ok_ms > STALE_MS)
            dev[i].ok = 0;
    }

    reinit_failed(now);

    if (now - slot_ms < SLOT_MS)
        return;
    slot_ms = now;

    /* Immer den anderen Sensor nehmen. Faellt der aus, kommt nur einer dran. */
    const Dev_t next = (cur == DEV_BARO) ? DEV_MAG : DEV_BARO;
    if (dev[next].ok)
        read_start(next, now);
    else if (dev[cur].ok)
        read_start(cur, now);
}

uint8_t AuxSens_BaroOk(void) { return dev[DEV_BARO].ok; }
uint8_t AuxSens_MagOk(void) { return dev[DEV_MAG].ok; }

void AuxSens_Print(void)
{
    if (dev[DEV_BARO].ok)
        printf(">> Baro: %.0f Pa | %.1f C | Hoehe %.2f m | Fehler %lu\r\n",
               BMP388_PressurePa(), BMP388_TempC(), BMP388_AltitudeM(),
               dev[DEV_BARO].errors);
    else
        printf(">> Baro: FEHLT | Fehler %lu\r\n", dev[DEV_BARO].errors);

    if (dev[DEV_MAG].ok)
        printf(">> Kompass: X %.3f Y %.3f Z %.3f Gauss%s | Fehler %lu\r\n",
               QMC5883L_X(), QMC5883L_Y(), QMC5883L_Z(),
               QMC5883L_Overflow() ? " UEBERLAUF" : "", dev[DEV_MAG].errors);
    else
        printf(">> Kompass: FEHLT | Fehler %lu\r\n", dev[DEV_MAG].errors);
}
