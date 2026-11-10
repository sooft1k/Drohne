#include "main.h"
#include "tof.h"
#include "motor.h"
#include <stdio.h>

/* Register-Init und Ablauf nach dem Ultra Lite Driver (ULD) von ST,
 * VL53L1X_api.c (BSD-3-Clause, (c) 2018 STMicroelectronics). Uebernommen sind
 * die Default-Konfiguration und die Registerwerte fuer Messmodus und Messzeit. */

I2C_HandleTypeDef hi2c3;
extern IWDG_HandleTypeDef hiwdg; /* aus main.c */

/* ---------- Pins ---------- */
#define SCL_PORT GPIOA
#define SCL_PIN GPIO_PIN_8
#define SDA_PORT GPIOC
#define SDA_PIN GPIO_PIN_9

typedef struct
{
    GPIO_TypeDef *xshut_port;
    uint16_t xshut_pin;
    GPIO_TypeDef *int_port;
    uint16_t int_pin;
    const char *name; /* Bauteil im Schaltplan */
} TofPins_t;

static const TofPins_t pins[TOF_COUNT] = {
    {GPIOC, GPIO_PIN_8, GPIOC, GPIO_PIN_2, "U8"},
    {GPIOB, GPIO_PIN_12, GPIOB, GPIO_PIN_4, "U9"},
    {GPIOB, GPIO_PIN_13, GPIOB, GPIO_PIN_5, "U10"},
    {GPIOB, GPIO_PIN_14, GPIOB, GPIO_PIN_8, "U12"},
    {GPIOB, GPIO_PIN_15, GPIOB, GPIO_PIN_9, "U11"},
};

/* ---------- Register ---------- */
#define REG_I2C_ADDRESS 0x0001
#define REG_VHV_LOOP_BOUND 0x0008
#define REG_VHV_START 0x000B
#define REG_CONFIG_START 0x002D /* Default-Konfiguration 0x2D..0x87 */
#define REG_GPIO_TIO_STATUS 0x0031
#define REG_PHASECAL_TIMEOUT 0x004B
#define REG_TIMEOUT_A 0x005E
#define REG_VCSEL_PERIOD_A 0x0060
#define REG_TIMEOUT_B 0x0061
#define REG_VCSEL_PERIOD_B 0x0063
#define REG_VALID_PHASE_HIGH 0x0069
#define REG_INTERMEASUREMENT 0x006C
#define REG_WOI_SD0 0x0078
#define REG_INITIAL_PHASE_SD0 0x007A
#define REG_INT_CLEAR 0x0086
#define REG_MODE_START 0x0087
#define REG_RANGE_STATUS 0x0089
#define REG_RANGE_MM 0x0096
#define REG_OSC_CALIBRATE 0x00DE
#define REG_SYSTEM_STATUS 0x00E5
#define REG_MODEL_ID 0x010F

/* Ergebnisblock 0x0089..0x0097 in einem Rutsch: Status an [0], Entfernung an [13..14] */
#define RESULT_LEN (REG_RANGE_MM - REG_RANGE_STATUS + 2)
#define RESULT_MM_IDX (REG_RANGE_MM - REG_RANGE_STATUS)

/* Rohwert im Statusregister fuer eine gueltige Messung */
#define RANGE_VALID 9

/* ---------- Zeiten ---------- */
#define PERIOD_MS 50              /* Messzeit und Messabstand: 20 Hz */
#define XSHUT_LOW_MS 2
#define BOOT_TIMEOUT_MS 100       /* Datenblatt: Boot dauert hoechstens 1,2 ms */
#define FIRST_RANGE_TIMEOUT_MS 500
#define XFER_TIMEOUT_MS 5         /* 19 Bytes dauern bei 400 kHz rund 0,5 ms */
#define BLOCK_TIMEOUT_MS 5        /* blockierende Zugriffe bei der Einrichtung */
#define STALE_MS 500              /* so lange ohne Messung: Sensor gilt als ausgefallen */
#define REINIT_MS 1000            /* Abstand der Neustart-Versuche, nur disarmed */
#define RECOVER_MS 100            /* Abstand der Bus-Recovery */
#define POLL_MS 10                /* Abfrage per Register, falls die INT-Leitung schweigt */
#define FAIL_LIMIT 10             /* I2C-Fehler in Folge bis zum Neustart */

/* Default-Konfiguration fuer 0x2D..0x87 aus dem ULD
 * (VL51L1X_DEFAULT_CONFIGURATION). Geaendert sind nur 0x2E und 0x2F
 * (2V8-Modus, weil Versorgung und Pullups an 3,3 V liegen) und 0x30
 * (Interrupt aktiv-low, passt zur Open-Drain-Leitung mit Pullup). */
static const uint8_t default_cfg[] = {
    0x00, /* 0x2d: kein Fast-Mode-Plus */
    0x01, /* 0x2e: I2C-Pins im 2V8-Modus (ULD: 0x00 = 1V8) */
    0x01, /* 0x2f: GPIO1 im 2V8-Modus (ULD: 0x00 = 1V8) */
    0x11, /* 0x30: Interrupt aktiv-low (ULD: 0x01 = aktiv-high) */
    0x02, 0x00, 0x02, 0x08, 0x00, 0x08, 0x10, 0x01, 0x01, 0x00, 0x00, 0x00, /* 0x31..0x3c */
    0x00, 0xff, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x0b, 0x00, /* 0x3d..0x48 */
    0x00, 0x02, 0x0a, 0x21, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0xc8, /* 0x49..0x54 */
    0x00, 0x00, 0x38, 0xff, 0x01, 0x00, 0x08, 0x00, 0x00, 0x01, 0xcc, 0x0f, /* 0x55..0x60 */
    0x01, 0xf1, 0x0d, 0x01, 0x68, 0x00, 0x80, 0x08, 0xb8, 0x00, 0x00, 0x00, /* 0x61..0x6c */
    0x00, 0x0f, 0x89, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x0f, /* 0x6d..0x78 */
    0x0d, 0x0e, 0x0e, 0x00, 0x00, 0x02, 0xc7, 0xff, 0x9B, 0x00, 0x00, 0x00, /* 0x79..0x84 */
    0x01, 0x00, 0x00                                                        /* 0x85..0x87 */
};
_Static_assert(sizeof(default_cfg) == 0x87 - 0x2D + 1, "ULD-Konfiguration 0x2D..0x87");

typedef enum
{
    T_RESET = 0, /* XSHUT low */
    T_BOOT,      /* warten, bis die Sensor-Firmware laeuft, dann Adresse vergeben */
    T_FIRST,     /* erste Messung (Temperaturkalibrierung VHV) */
    T_RUN,       /* misst, Lesen per Interrupt */
    T_FAIL       /* ausgefallen, Neustart nach REINIT_MS */
} TofState_t;

enum
{
    X_IDLE = 0,
    X_POLL,   /* GPIO__TIO_HV_STATUS lesen */
    X_RESULT, /* Ergebnisblock lesen */
    X_CLEAR   /* Interrupt loeschen, gibt die naechste Messung frei */
};

typedef struct
{
    TofState_t state;
    uint32_t state_ms;
    uint8_t ok;
    uint16_t addr; /* aktuelle Adresse, 8-Bit-Schreibweise der HAL */
    uint16_t dist_mm;
    uint8_t range_status; /* Rohwert, 9 = gueltig, 0xFF = noch keiner */
    uint32_t data_ms;     /* letzte Messung (auch ungueltige) */
    uint32_t valid_ms;    /* letzte gueltige Messung */
    uint32_t poll_ms;
    uint32_t count;
    uint32_t errors;
    uint16_t fail_run;
    const char *fail_reason;
} Tof_t;

static Tof_t tof[TOF_COUNT];

/* Der Bus wird geteilt: immer nur eine Uebertragung, xfer_owner ist der Sensor */
static uint8_t xfer = X_IDLE;
static uint8_t xfer_owner = 0;
static volatile uint8_t xfer_done = 0; /* 0 = laeuft, 1 = fertig, 2 = Fehler */
static uint32_t xfer_ms = 0;
static uint8_t rx[RESULT_LEN];
static uint8_t tx;
static uint8_t rr = 0; /* reihum: zuletzt bediente Sensor */
static uint32_t recover_ms = 0;

static uint16_t own_addr(uint8_t i)
{
    return (uint16_t)((TOF_I2C_ADDR_FIRST + i) << 1);
}

/* ---------- I2C ---------- */

static HAL_StatusTypeDef I2C3_Setup(void)
{
    __HAL_RCC_I2C3_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Mode = GPIO_MODE_AF_OD;
    g.Pull = GPIO_NOPULL; /* 4,7 kOhm auf der Platine (R16, R17) */
    g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF4_I2C3;
    g.Pin = SCL_PIN;
    HAL_GPIO_Init(SCL_PORT, &g);
    g.Pin = SDA_PIN;
    HAL_GPIO_Init(SDA_PORT, &g);

    hi2c3.Instance = I2C3;
    hi2c3.Init.ClockSpeed = 400000;
    hi2c3.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c3.Init.OwnAddress1 = 0;
    hi2c3.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c3.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c3.Init.OwnAddress2 = 0;
    hi2c3.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c3.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    return HAL_I2C_Init(&hi2c3);
}

static void delay_us(uint32_t us)
{
    const uint32_t start = DWT->CYCCNT;
    const uint32_t n = us * (SystemCoreClock / 1000000U);
    while (DWT->CYCCNT - start < n)
    {
    }
}

/* Haengenden Bus freitakten und die Peripherie neu aufsetzen, wie I2C1_Recover
 * in auxsens.c. Dauert etwa 0,2 ms und laeuft deshalb auch gearmt. */
static void I2C3_Recover(void)
{
    HAL_NVIC_DisableIRQ(I2C3_EV_IRQn);
    HAL_NVIC_DisableIRQ(I2C3_ER_IRQn);
    HAL_I2C_DeInit(&hi2c3);

    GPIO_InitTypeDef g = {0};
    g.Mode = GPIO_MODE_OUTPUT_OD;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_WritePin(SCL_PORT, SCL_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(SDA_PORT, SDA_PIN, GPIO_PIN_SET);
    g.Pin = SCL_PIN;
    HAL_GPIO_Init(SCL_PORT, &g);
    g.Pin = SDA_PIN;
    HAL_GPIO_Init(SDA_PORT, &g);
    delay_us(5);

    for (int i = 0; i < 9 && HAL_GPIO_ReadPin(SDA_PORT, SDA_PIN) == GPIO_PIN_RESET; i++)
    {
        HAL_GPIO_WritePin(SCL_PORT, SCL_PIN, GPIO_PIN_RESET);
        delay_us(5);
        HAL_GPIO_WritePin(SCL_PORT, SCL_PIN, GPIO_PIN_SET);
        delay_us(5);
    }

    /* STOP: SDA geht hoch, waehrend SCL hoch ist */
    HAL_GPIO_WritePin(SCL_PORT, SCL_PIN, GPIO_PIN_RESET);
    delay_us(5);
    HAL_GPIO_WritePin(SDA_PORT, SDA_PIN, GPIO_PIN_RESET);
    delay_us(5);
    HAL_GPIO_WritePin(SCL_PORT, SCL_PIN, GPIO_PIN_SET);
    delay_us(5);
    HAL_GPIO_WritePin(SDA_PORT, SDA_PIN, GPIO_PIN_SET);
    delay_us(5);

    __HAL_RCC_I2C3_FORCE_RESET();
    __HAL_RCC_I2C3_RELEASE_RESET();
    (void)I2C3_Setup();

    __HAL_I2C_CLEAR_FLAG(&hi2c3, I2C_FLAG_AF);
    xfer = X_IDLE;
    xfer_done = 0;
    HAL_NVIC_ClearPendingIRQ(I2C3_EV_IRQn);
    HAL_NVIC_ClearPendingIRQ(I2C3_ER_IRQn);
    HAL_NVIC_EnableIRQ(I2C3_EV_IRQn);
    HAL_NVIC_EnableIRQ(I2C3_ER_IRQn);
}

/* Blockierende Zugriffe, nur fuer die Einrichtung (disarmed).
 * Register sind 16 Bit breit, Werte stehen MSB zuerst. */
static HAL_StatusTypeDef wr(uint16_t a, uint16_t reg, const uint8_t *d, uint16_t n)
{
    return HAL_I2C_Mem_Write(&hi2c3, a, reg, I2C_MEMADD_SIZE_16BIT, (uint8_t *)d, n, BLOCK_TIMEOUT_MS);
}

static HAL_StatusTypeDef rd(uint16_t a, uint16_t reg, uint8_t *d, uint16_t n)
{
    return HAL_I2C_Mem_Read(&hi2c3, a, reg, I2C_MEMADD_SIZE_16BIT, d, n, BLOCK_TIMEOUT_MS);
}

static HAL_StatusTypeDef wr8(uint16_t a, uint16_t reg, uint8_t v)
{
    return wr(a, reg, &v, 1);
}

static HAL_StatusTypeDef wr16(uint16_t a, uint16_t reg, uint16_t v)
{
    const uint8_t b[2] = {(uint8_t)(v >> 8), (uint8_t)v};
    return wr(a, reg, b, 2);
}

static HAL_StatusTypeDef wr32(uint16_t a, uint16_t reg, uint32_t v)
{
    const uint8_t b[4] = {(uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v};
    return wr(a, reg, b, 4);
}

/* Bei aktiv-low (0x30 Bit 4 = 1) ist eine Messung fertig, wenn Bit 0 = 0 ist
 * (VL53L1X_CheckForDataReady mit Polaritaet 0) */
static uint8_t status_ready(uint8_t tio)
{
    return (tio & 0x01) == 0;
}

/* ---------- Ablauf ---------- */

static void enter(uint8_t i, TofState_t s, uint32_t now)
{
    tof[i].state = s;
    tof[i].state_ms = now;
}

static void fail(uint8_t i, const char *why, uint32_t now)
{
    tof[i].ok = 0;
    tof[i].fail_reason = why;
    HAL_GPIO_WritePin(pins[i].xshut_port, pins[i].xshut_pin, GPIO_PIN_RESET); /* Sensor aus */
    enter(i, T_FAIL, now);
}

static void restart(uint8_t i, uint32_t now)
{
    HAL_GPIO_WritePin(pins[i].xshut_port, pins[i].xshut_pin, GPIO_PIN_RESET);
    tof[i].addr = TOF_I2C_ADDR_DEFAULT << 1; /* nach XSHUT wieder Werksadresse */
    enter(i, T_RESET, now);
}

static uint8_t initializing(uint8_t i)
{
    return tof[i].state == T_RESET || tof[i].state == T_BOOT || tof[i].state == T_FIRST;
}

/* Messmodus Long (bis etwa 4 m), 50 ms Messzeit, alle 50 ms eine Messung.
 * Werte aus VL53L1X_SetDistanceMode(2), VL53L1X_SetTimingBudgetInMs(50)
 * und VL53L1X_SetInterMeasurementInMs(50). */
static HAL_StatusTypeDef configure_ranging(uint16_t a)
{
    uint8_t b[2] = {0, 0};
    uint32_t st = HAL_OK;
    st |= wr8(a, REG_PHASECAL_TIMEOUT, 0x0A);
    st |= wr8(a, REG_VCSEL_PERIOD_A, 0x0F);
    st |= wr8(a, REG_VCSEL_PERIOD_B, 0x0D);
    st |= wr8(a, REG_VALID_PHASE_HIGH, 0xB8);
    st |= wr16(a, REG_WOI_SD0, 0x0F0D);
    st |= wr16(a, REG_INITIAL_PHASE_SD0, 0x0E0E);
    st |= wr16(a, REG_TIMEOUT_A, 0x00AD);
    st |= wr16(a, REG_TIMEOUT_B, 0x00C6);

    st |= rd(a, REG_OSC_CALIBRATE, b, 2);
    const uint32_t pll = (((uint32_t)b[0] << 8) | b[1]) & 0x3FF;
    st |= wr32(a, REG_INTERMEASUREMENT, (uint32_t)((float)(pll * PERIOD_MS) * 1.075f));
    return st == HAL_OK ? HAL_OK : HAL_ERROR;
}

/* Ein Schritt der Einrichtung von Sensor i. Blockiert hoechstens einige
 * Millisekunden (die Default-Konfiguration mit 91 Bytes dauert rund 2,5 ms).
 * Es darf immer nur ein Sensor in der Einrichtung sein, weil alle mit der
 * Werksadresse 0x29 starten. */
static void init_step(uint8_t i, uint32_t now)
{
    Tof_t *t = &tof[i];
    switch (t->state)
    {
    case T_RESET:
        if (now - t->state_ms >= XSHUT_LOW_MS)
        {
            HAL_GPIO_WritePin(pins[i].xshut_port, pins[i].xshut_pin, GPIO_PIN_SET);
            enter(i, T_BOOT, now);
        }
        break;

    case T_BOOT:
    {
        uint8_t s = 0;
        if (rd(t->addr, REG_SYSTEM_STATUS, &s, 1) == HAL_OK && (s & 0x01))
        {
            uint8_t id[2] = {0, 0};
            if (rd(t->addr, REG_MODEL_ID, id, 2) != HAL_OK)
            {
                fail(i, "Modell-ID nicht lesbar", now);
                break;
            }
            if ((((uint16_t)id[0] << 8) | id[1]) != TOF_MODEL_ID)
            {
                fail(i, "falsche Modell-ID (kein VL53L1X?)", now);
                break;
            }
            /* Eigene Adresse vergeben (VL53L1X_SetI2CAddress), ab jetzt
             * antwortet der Sensor nur noch darauf */
            if (wr8(t->addr, REG_I2C_ADDRESS, TOF_I2C_ADDR_FIRST + i) != HAL_OK)
            {
                fail(i, "Adresse nicht gesetzt", now);
                break;
            }
            t->addr = own_addr(i);
            /* Default-Konfiguration inkl. 2V8-Modus, dann erste Messung starten
             * (wie VL53L1X_SensorInit) */
            if (wr(t->addr, REG_CONFIG_START, default_cfg, sizeof(default_cfg)) != HAL_OK ||
                wr8(t->addr, REG_MODE_START, 0x40) != HAL_OK)
            {
                fail(i, "Konfiguration fehlgeschlagen", now);
                break;
            }
            enter(i, T_FIRST, now);
        }
        else if (now - t->state_ms > BOOT_TIMEOUT_MS)
        {
            fail(i, "antwortet nicht", now);
        }
        break;
    }

    case T_FIRST:
    {
        uint8_t tio = 0xFF;
        if (rd(t->addr, REG_GPIO_TIO_STATUS, &tio, 1) == HAL_OK && status_ready(tio))
        {
            /* Rest von VL53L1X_SensorInit: Interrupt loeschen, stoppen,
             * VHV-Grenzen setzen. Danach Messmodus und Dauermessung. */
            uint32_t st = HAL_OK;
            st |= wr8(t->addr, REG_INT_CLEAR, 0x01);
            st |= wr8(t->addr, REG_MODE_START, 0x00);
            st |= wr8(t->addr, REG_VHV_LOOP_BOUND, 0x09);
            st |= wr8(t->addr, REG_VHV_START, 0x00);
            st |= configure_ranging(t->addr);
            st |= wr8(t->addr, REG_INT_CLEAR, 0x01);
            st |= wr8(t->addr, REG_MODE_START, 0x40);
            if (st != HAL_OK)
            {
                fail(i, "Einrichtung fehlgeschlagen", now);
                break;
            }
            t->ok = 1;
            t->fail_run = 0;
            t->data_ms = now;
            t->poll_ms = now;
            enter(i, T_RUN, now);
        }
        else if (now - t->state_ms > FIRST_RANGE_TIMEOUT_MS)
        {
            fail(i, "erste Messung kommt nicht", now);
        }
        break;
    }

    default:
        break;
    }
}

void Tof_Init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    for (uint8_t i = 0; i < TOF_COUNT; i++)
    {
        tof[i].state = T_FAIL;
        tof[i].range_status = 0xFF;
        tof[i].fail_reason = "noch nicht gestartet";
        tof[i].addr = TOF_I2C_ADDR_DEFAULT << 1;

        /* XSHUT: Ausgang, startet low (alle Sensoren aus) */
        HAL_GPIO_WritePin(pins[i].xshut_port, pins[i].xshut_pin, GPIO_PIN_RESET);
        GPIO_InitTypeDef g = {0};
        g.Pin = pins[i].xshut_pin;
        g.Mode = GPIO_MODE_OUTPUT_PP;
        g.Pull = GPIO_NOPULL;
        g.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(pins[i].xshut_port, &g);

        /* INT (GPIO1): Eingang, 10 kOhm Pullup auf der Platine, aktiv-low */
        g.Pin = pins[i].int_pin;
        g.Mode = GPIO_MODE_INPUT;
        g.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(pins[i].int_port, &g);
    }

    if (I2C3_Setup() != HAL_OK)
    {
        for (uint8_t i = 0; i < TOF_COUNT; i++)
            fail(i, "I2C3 nicht eingerichtet", HAL_GetTick());
        return;
    }

    /* Wie I2C1: unter dem Regeltakt (4) und den UARTs (5, 6) */
    HAL_NVIC_SetPriority(I2C3_EV_IRQn, 7, 0);
    HAL_NVIC_SetPriority(I2C3_ER_IRQn, 7, 0);
    HAL_NVIC_EnableIRQ(I2C3_EV_IRQn);
    HAL_NVIC_EnableIRQ(I2C3_ER_IRQn);

    /* Beim Start alle Sensoren nacheinander einrichten. Der Watchdog laeuft
     * hier noch nicht. */
    for (uint8_t i = 0; i < TOF_COUNT; i++)
    {
        restart(i, HAL_GetTick());
        const uint32_t start = HAL_GetTick();
        while (initializing(i) &&
               HAL_GetTick() - start < XSHUT_LOW_MS + BOOT_TIMEOUT_MS + FIRST_RANGE_TIMEOUT_MS + 100)
        {
            init_step(i, HAL_GetTick());
            HAL_Delay(1);
        }
        if (initializing(i))
            fail(i, "Zeitueberschreitung beim Start", HAL_GetTick());
    }

    /* Die ersten Sensoren haben schon gemessen, waehrend die anderen
     * eingerichtet wurden: Zeitstempel neu setzen, sonst gelten sie als alt */
    const uint32_t now = HAL_GetTick();
    for (uint8_t i = 0; i < TOF_COUNT; i++)
    {
        tof[i].data_ms = now;
        tof[i].poll_ms = now;
    }
}

/* ---------- Betrieb ---------- */

void Tof_I2CDone(uint8_t success)
{
    xfer_done = success ? 1 : 2;
}

/* Schreiben per Interrupt nutzt nur der ToF (Interrupt loeschen) */
void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C3)
        Tof_I2CDone(1);
}

static void xfer_error(uint8_t i, uint32_t now)
{
    tof[i].errors++;
    if (tof[i].fail_run < 0xFFFF)
        tof[i].fail_run++;
    xfer = X_IDLE;
    xfer_done = 0;
    if (now - recover_ms >= RECOVER_MS)
    {
        recover_ms = now;
        I2C3_Recover();
    }
    if (tof[i].fail_run >= FAIL_LIMIT)
        fail(i, "zu viele I2C-Fehler", now);
}

static uint8_t bus_free(void)
{
    return __HAL_I2C_GET_FLAG(&hi2c3, I2C_FLAG_BUSY) == RESET &&
           HAL_I2C_GetState(&hi2c3) == HAL_I2C_STATE_READY;
}

static void start_read(uint8_t i, uint8_t what, uint16_t reg, uint16_t len, uint32_t now)
{
    if (!bus_free())
    {
        xfer_error(i, now);
        return;
    }
    xfer = what;
    xfer_owner = i;
    xfer_done = 0;
    xfer_ms = now;
    if (HAL_I2C_Mem_Read_IT(&hi2c3, tof[i].addr, reg, I2C_MEMADD_SIZE_16BIT, rx, len) != HAL_OK)
        xfer_error(i, now);
}

static void start_clear(uint8_t i, uint32_t now)
{
    tx = 0x01;
    xfer = X_CLEAR;
    xfer_owner = i;
    xfer_done = 0;
    xfer_ms = now;
    if (HAL_I2C_Mem_Write_IT(&hi2c3, tof[i].addr, REG_INT_CLEAR, I2C_MEMADD_SIZE_16BIT, &tx, 1) != HAL_OK)
        xfer_error(i, now);
}

/* Laufende Uebertragung auswerten. Gibt 1 zurueck, solange der Bus belegt ist. */
static uint8_t xfer_step(uint32_t now)
{
    if (xfer == X_IDLE)
        return 0;

    const uint8_t i = xfer_owner;
    Tof_t *t = &tof[i];
    const uint8_t d = xfer_done; /* einmal lesen, der Interrupt aendert ihn */
    if (d == 0)
    {
        if (now - xfer_ms > XFER_TIMEOUT_MS)
            xfer_error(i, now);
        return xfer != X_IDLE;
    }
    if (d == 2)
    {
        xfer_error(i, now);
        return 0;
    }

    t->fail_run = 0;
    const uint8_t was = xfer;
    xfer = X_IDLE;
    xfer_done = 0;

    if (t->state != T_RUN)
        return 0; /* Sensor ist inzwischen ausgefallen */

    if (was == X_POLL)
    {
        if (status_ready(rx[0]))
            start_read(i, X_RESULT, REG_RANGE_STATUS, RESULT_LEN, now);
    }
    else if (was == X_RESULT)
    {
        t->range_status = rx[0] & 0x1F;
        t->data_ms = now;
        t->count++;
        if (t->range_status == RANGE_VALID)
        {
            t->dist_mm = ((uint16_t)rx[RESULT_MM_IDX] << 8) | rx[RESULT_MM_IDX + 1];
            t->valid_ms = now;
        }
        start_clear(i, now); /* gibt die naechste Messung frei */
    }
    return xfer != X_IDLE;
}

/* Naechsten Sensor mit neuer Messung bedienen, reihum */
static void run_step(uint32_t now)
{
    for (uint8_t i = 0; i < TOF_COUNT; i++)
        if (tof[i].state == T_RUN && now - tof[i].data_ms > STALE_MS)
            fail(i, "keine Messungen mehr", now);

    for (uint8_t k = 1; k <= TOF_COUNT; k++)
    {
        const uint8_t i = (uint8_t)((rr + k) % TOF_COUNT);
        Tof_t *t = &tof[i];
        if (t->state != T_RUN)
            continue;

        /* Neue Messung: die INT-Leitung geht auf low. Schweigt sie laenger als
         * zwei Messperioden (Leitung defekt?), wird das Statusregister abgefragt. */
        if (HAL_GPIO_ReadPin(pins[i].int_port, pins[i].int_pin) == GPIO_PIN_RESET)
        {
            rr = i;
            start_read(i, X_RESULT, REG_RANGE_STATUS, RESULT_LEN, now);
            return;
        }
        if (now - t->data_ms > 2 * PERIOD_MS && now - t->poll_ms >= POLL_MS)
        {
            rr = i;
            t->poll_ms = now;
            start_read(i, X_POLL, REG_GPIO_TIO_STATUS, 1, now);
            return;
        }
    }
}

/* Einrichtung, nur disarmed und bei freiem Bus: immer nur ein Sensor zugleich */
static uint8_t init_work(uint32_t now)
{
    for (uint8_t i = 0; i < TOF_COUNT; i++)
    {
        if (initializing(i))
        {
            HAL_IWDG_Refresh(&hiwdg);
            init_step(i, now);
            HAL_IWDG_Refresh(&hiwdg);
            return 1;
        }
    }
    for (uint8_t i = 0; i < TOF_COUNT; i++)
    {
        if (tof[i].state == T_FAIL && now - tof[i].state_ms >= REINIT_MS)
        {
            restart(i, now);
            return 1;
        }
    }
    return 0;
}

void Tof_Process(void)
{
    const uint32_t now = HAL_GetTick();

    if (xfer_step(now))
        return; /* Bus belegt */

    /* Einrichtung blockiert jeweils einige ms: pausiert, solange gearmt ist */
    if (!Motor_IsArmed() && init_work(now))
        return;

    run_step(now);
}

uint8_t Tof_Ok(uint8_t i)
{
    return i < TOF_COUNT && tof[i].ok;
}

uint8_t Tof_OkCount(void)
{
    uint8_t n = 0;
    for (uint8_t i = 0; i < TOF_COUNT; i++)
        n += tof[i].ok;
    return n;
}

uint16_t Tof_DistanceMm(uint8_t i, uint32_t max_age_ms)
{
    if (i >= TOF_COUNT)
        return 0;
    const Tof_t *t = &tof[i];
    if (!t->ok || t->valid_ms == 0 || HAL_GetTick() - t->valid_ms > max_age_ms)
        return 0;
    return t->dist_mm;
}

/* Rohwerte aus RESULT__RANGE_STATUS, Bedeutung wie VL53L1X_GetRangeStatus */
static const char *status_text(uint8_t s)
{
    switch (s)
    {
    case 9: return "gueltig";
    case 6: return "Messung zu ungenau (Sigma)";
    case 4: return "Signal zu schwach";
    case 8: return "unter Mindestabstand";
    case 5: return "ausserhalb Messbereich";
    case 7: return "Wraparound (Ziel zu weit)";
    case 0xFF: return "noch keine Messung";
    default: return "ungueltig";
    }
}

void Tof_Print(void)
{
    printf(">> ToF: %u von %u messen\r\n", Tof_OkCount(), TOF_COUNT);
    for (uint8_t i = 0; i < TOF_COUNT; i++)
    {
        const Tof_t *t = &tof[i];
        if (!t->ok)
            printf(">>   %u (%s): FEHLT (%s) | Fehler %lu\r\n", i + 1, pins[i].name, t->fail_reason, t->errors);
        else if (t->range_status == RANGE_VALID)
            printf(">>   %u (%s): %u mm | %lu Messungen | Fehler %lu\r\n", i + 1, pins[i].name,
                   t->dist_mm, t->count, t->errors);
        else
            printf(">>   %u (%s): --- %s (Status %u) | %lu Messungen | Fehler %lu\r\n", i + 1, pins[i].name,
                   status_text(t->range_status), t->range_status, t->count, t->errors);
    }
}
