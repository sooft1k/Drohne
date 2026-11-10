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
#define XSHUT_PORT GPIOC
#define XSHUT_PIN GPIO_PIN_8
#define INT_PORT GPIOC
#define INT_PIN GPIO_PIN_2

/* ---------- Register ---------- */
#define REG_VHV_LOOP_BOUND 0x0008
#define REG_VHV_START 0x000B
#define REG_CONFIG_START 0x002D  /* Default-Konfiguration 0x2D..0x87 */
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
    T_BOOT,      /* warten, bis die Sensor-Firmware laeuft */
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

static TofState_t state = T_FAIL;
static uint32_t state_ms = 0;
static uint8_t ok = 0;

static uint8_t xfer = X_IDLE;
static volatile uint8_t xfer_done = 0; /* 0 = laeuft, 1 = fertig, 2 = Fehler */
static uint32_t xfer_ms = 0;
static uint8_t rx[RESULT_LEN];
static uint8_t tx;

static uint16_t dist_mm = 0;
static uint8_t range_status = 0xFF; /* Rohwert, 9 = gueltig, 0xFF = noch keiner */
static uint32_t data_ms = 0;        /* letzte Messung (auch ungueltige) */
static uint32_t valid_ms = 0;       /* letzte gueltige Messung */
static uint32_t poll_ms = 0;
static uint32_t recover_ms = 0;
static uint32_t count = 0;
static uint32_t errors = 0;
static uint16_t fail_run = 0;
static const char *fail_reason = "noch nicht gestartet";

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
static HAL_StatusTypeDef wr(uint16_t reg, const uint8_t *d, uint16_t n)
{
    return HAL_I2C_Mem_Write(&hi2c3, TOF_I2C_ADDR, reg, I2C_MEMADD_SIZE_16BIT,
                             (uint8_t *)d, n, BLOCK_TIMEOUT_MS);
}

static HAL_StatusTypeDef rd(uint16_t reg, uint8_t *d, uint16_t n)
{
    return HAL_I2C_Mem_Read(&hi2c3, TOF_I2C_ADDR, reg, I2C_MEMADD_SIZE_16BIT,
                            d, n, BLOCK_TIMEOUT_MS);
}

static HAL_StatusTypeDef wr8(uint16_t reg, uint8_t v)
{
    return wr(reg, &v, 1);
}

static HAL_StatusTypeDef wr16(uint16_t reg, uint16_t v)
{
    const uint8_t b[2] = {(uint8_t)(v >> 8), (uint8_t)v};
    return wr(reg, b, 2);
}

static HAL_StatusTypeDef wr32(uint16_t reg, uint32_t v)
{
    const uint8_t b[4] = {(uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v};
    return wr(reg, b, 4);
}

/* Bei aktiv-low (0x30 Bit 4 = 1) ist eine Messung fertig, wenn Bit 0 = 0 ist
 * (VL53L1X_CheckForDataReady mit Polaritaet 0) */
static uint8_t status_ready(uint8_t tio)
{
    return (tio & 0x01) == 0;
}

/* ---------- Ablauf ---------- */

static void enter(TofState_t s, uint32_t now)
{
    state = s;
    state_ms = now;
}

static void fail(const char *why, uint32_t now)
{
    ok = 0;
    fail_reason = why;
    HAL_GPIO_WritePin(XSHUT_PORT, XSHUT_PIN, GPIO_PIN_RESET); /* Sensor aus */
    enter(T_FAIL, now);
}

static void restart(uint32_t now)
{
    HAL_GPIO_WritePin(XSHUT_PORT, XSHUT_PIN, GPIO_PIN_RESET);
    enter(T_RESET, now);
}

/* Messmodus Long (bis etwa 4 m), 50 ms Messzeit, alle 50 ms eine Messung.
 * Werte aus VL53L1X_SetDistanceMode(2), VL53L1X_SetTimingBudgetInMs(50)
 * und VL53L1X_SetInterMeasurementInMs(50). */
static HAL_StatusTypeDef configure_ranging(void)
{
    uint8_t b[2] = {0, 0};
    uint32_t st = HAL_OK;
    st |= wr8(REG_PHASECAL_TIMEOUT, 0x0A);
    st |= wr8(REG_VCSEL_PERIOD_A, 0x0F);
    st |= wr8(REG_VCSEL_PERIOD_B, 0x0D);
    st |= wr8(REG_VALID_PHASE_HIGH, 0xB8);
    st |= wr16(REG_WOI_SD0, 0x0F0D);
    st |= wr16(REG_INITIAL_PHASE_SD0, 0x0E0E);
    st |= wr16(REG_TIMEOUT_A, 0x00AD);
    st |= wr16(REG_TIMEOUT_B, 0x00C6);

    st |= rd(REG_OSC_CALIBRATE, b, 2);
    const uint32_t pll = (((uint32_t)b[0] << 8) | b[1]) & 0x3FF;
    st |= wr32(REG_INTERMEASUREMENT, (uint32_t)((float)(pll * PERIOD_MS) * 1.075f));
    return st == HAL_OK ? HAL_OK : HAL_ERROR;
}

/* Ein Schritt der Einrichtung. Blockiert hoechstens einige Millisekunden
 * (die Default-Konfiguration mit 91 Bytes dauert rund 2,5 ms). */
static void init_step(uint32_t now)
{
    switch (state)
    {
    case T_RESET:
        if (now - state_ms >= XSHUT_LOW_MS)
        {
            HAL_GPIO_WritePin(XSHUT_PORT, XSHUT_PIN, GPIO_PIN_SET);
            enter(T_BOOT, now);
        }
        break;

    case T_BOOT:
    {
        uint8_t s = 0;
        if (rd(REG_SYSTEM_STATUS, &s, 1) == HAL_OK && (s & 0x01))
        {
            uint8_t id[2] = {0, 0};
            if (rd(REG_MODEL_ID, id, 2) != HAL_OK)
            {
                fail("Modell-ID nicht lesbar", now);
                break;
            }
            if ((((uint16_t)id[0] << 8) | id[1]) != TOF_MODEL_ID)
            {
                fail("falsche Modell-ID (kein VL53L1X?)", now);
                break;
            }
            /* Default-Konfiguration inkl. 2V8-Modus, dann erste Messung starten
             * (wie VL53L1X_SensorInit) */
            if (wr(REG_CONFIG_START, default_cfg, sizeof(default_cfg)) != HAL_OK ||
                wr8(REG_MODE_START, 0x40) != HAL_OK)
            {
                fail("Konfiguration fehlgeschlagen", now);
                break;
            }
            enter(T_FIRST, now);
        }
        else if (now - state_ms > BOOT_TIMEOUT_MS)
        {
            fail("antwortet nicht", now);
        }
        break;
    }

    case T_FIRST:
    {
        uint8_t tio = 0xFF;
        if (rd(REG_GPIO_TIO_STATUS, &tio, 1) == HAL_OK && status_ready(tio))
        {
            /* Rest von VL53L1X_SensorInit: Interrupt loeschen, stoppen,
             * VHV-Grenzen setzen. Danach Messmodus und Dauermessung. */
            uint32_t st = HAL_OK;
            st |= wr8(REG_INT_CLEAR, 0x01);
            st |= wr8(REG_MODE_START, 0x00);
            st |= wr8(REG_VHV_LOOP_BOUND, 0x09);
            st |= wr8(REG_VHV_START, 0x00);
            st |= configure_ranging();
            st |= wr8(REG_INT_CLEAR, 0x01);
            st |= wr8(REG_MODE_START, 0x40);
            if (st != HAL_OK)
            {
                fail("Einrichtung fehlgeschlagen", now);
                break;
            }
            ok = 1;
            fail_run = 0;
            data_ms = now;
            poll_ms = now;
            xfer = X_IDLE;
            xfer_done = 0;
            enter(T_RUN, now);
        }
        else if (now - state_ms > FIRST_RANGE_TIMEOUT_MS)
        {
            fail("erste Messung kommt nicht", now);
        }
        break;
    }

    default:
        break;
    }
}

void Tof_Init(void)
{
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* XSHUT: Ausgang, startet low (Sensor aus) */
    HAL_GPIO_WritePin(XSHUT_PORT, XSHUT_PIN, GPIO_PIN_RESET);
    GPIO_InitTypeDef g = {0};
    g.Pin = XSHUT_PIN;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(XSHUT_PORT, &g);

    /* INT (GPIO1): Eingang, 10 kOhm Pullup auf der Platine (R15), aktiv-low */
    g.Pin = INT_PIN;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(INT_PORT, &g);

    if (I2C3_Setup() != HAL_OK)
    {
        fail("I2C3 nicht eingerichtet", HAL_GetTick());
        return;
    }

    /* Wie I2C1: unter dem Regeltakt (4) und den UARTs (5, 6) */
    HAL_NVIC_SetPriority(I2C3_EV_IRQn, 7, 0);
    HAL_NVIC_SetPriority(I2C3_ER_IRQn, 7, 0);
    HAL_NVIC_EnableIRQ(I2C3_EV_IRQn);
    HAL_NVIC_EnableIRQ(I2C3_ER_IRQn);

    /* Beim Start die Einrichtung komplett durchlaufen. Der Watchdog laeuft
     * hier noch nicht. */
    restart(HAL_GetTick());
    const uint32_t start = HAL_GetTick();
    while (state != T_RUN && state != T_FAIL &&
           HAL_GetTick() - start < XSHUT_LOW_MS + BOOT_TIMEOUT_MS + FIRST_RANGE_TIMEOUT_MS + 100)
    {
        init_step(HAL_GetTick());
        HAL_Delay(1);
    }
    if (state != T_RUN && state != T_FAIL)
        fail("Zeitueberschreitung beim Start", HAL_GetTick());
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

static void xfer_error(uint32_t now)
{
    errors++;
    if (fail_run < 0xFFFF)
        fail_run++;
    xfer = X_IDLE;
    xfer_done = 0;
    if (now - recover_ms >= RECOVER_MS)
    {
        recover_ms = now;
        I2C3_Recover();
    }
    if (fail_run >= FAIL_LIMIT)
        fail("zu viele I2C-Fehler", now);
}

static uint8_t bus_free(void)
{
    return __HAL_I2C_GET_FLAG(&hi2c3, I2C_FLAG_BUSY) == RESET &&
           HAL_I2C_GetState(&hi2c3) == HAL_I2C_STATE_READY;
}

static void start_read(uint8_t what, uint16_t reg, uint16_t len, uint32_t now)
{
    if (!bus_free())
    {
        xfer_error(now);
        return;
    }
    xfer = what;
    xfer_done = 0;
    xfer_ms = now;
    if (HAL_I2C_Mem_Read_IT(&hi2c3, TOF_I2C_ADDR, reg, I2C_MEMADD_SIZE_16BIT, rx, len) != HAL_OK)
        xfer_error(now);
}

static void start_clear(uint32_t now)
{
    tx = 0x01;
    xfer = X_CLEAR;
    xfer_done = 0;
    xfer_ms = now;
    if (HAL_I2C_Mem_Write_IT(&hi2c3, TOF_I2C_ADDR, REG_INT_CLEAR, I2C_MEMADD_SIZE_16BIT, &tx, 1) != HAL_OK)
        xfer_error(now);
}

static void run_step(uint32_t now)
{
    if (xfer != X_IDLE)
    {
        const uint8_t d = xfer_done; /* einmal lesen, der Interrupt aendert ihn */
        if (d == 0)
        {
            if (now - xfer_ms > XFER_TIMEOUT_MS)
                xfer_error(now);
            return;
        }
        if (d == 2)
        {
            xfer_error(now);
            return;
        }

        fail_run = 0;
        const uint8_t was = xfer;
        xfer = X_IDLE;
        xfer_done = 0;

        if (was == X_POLL)
        {
            if (status_ready(rx[0]))
                start_read(X_RESULT, REG_RANGE_STATUS, RESULT_LEN, now);
        }
        else if (was == X_RESULT)
        {
            range_status = rx[0] & 0x1F;
            data_ms = now;
            count++;
            if (range_status == RANGE_VALID)
            {
                dist_mm = ((uint16_t)rx[RESULT_MM_IDX] << 8) | rx[RESULT_MM_IDX + 1];
                valid_ms = now;
            }
            start_clear(now); /* gibt die naechste Messung frei */
        }
        return; /* X_CLEAR fertig: nichts zu tun */
    }

    if (now - data_ms > STALE_MS)
    {
        fail("keine Messungen mehr", now);
        return;
    }

    /* Neue Messung: die INT-Leitung geht auf low. Schweigt sie laenger als
     * zwei Messperioden (Leitung defekt?), wird das Statusregister abgefragt. */
    if (HAL_GPIO_ReadPin(INT_PORT, INT_PIN) == GPIO_PIN_RESET)
    {
        start_read(X_RESULT, REG_RANGE_STATUS, RESULT_LEN, now);
    }
    else if (now - data_ms > 2 * PERIOD_MS && now - poll_ms >= POLL_MS)
    {
        poll_ms = now;
        start_read(X_POLL, REG_GPIO_TIO_STATUS, 1, now);
    }
}

void Tof_Process(void)
{
    const uint32_t now = HAL_GetTick();

    switch (state)
    {
    case T_RUN:
        run_step(now);
        break;

    case T_FAIL:
        /* Neustart nur disarmed: die Einrichtung blockiert jeweils einige ms */
        if (!Motor_IsArmed() && now - state_ms >= REINIT_MS)
            restart(now);
        break;

    default:
        if (Motor_IsArmed())
            break; /* Einrichtung pausiert, solange gearmt ist */
        HAL_IWDG_Refresh(&hiwdg);
        init_step(now);
        HAL_IWDG_Refresh(&hiwdg);
        break;
    }
}

uint8_t Tof_Ok(void) { return ok; }

uint16_t Tof_DistanceMm(uint32_t max_age_ms)
{
    if (!ok || valid_ms == 0 || HAL_GetTick() - valid_ms > max_age_ms)
        return 0;
    return dist_mm;
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
    if (!ok)
    {
        printf(">> ToF: FEHLT (%s) | Fehler %lu\r\n", fail_reason, errors);
        return;
    }
    if (range_status == RANGE_VALID)
        printf(">> ToF: %u mm | %lu Messungen | Fehler %lu\r\n", dist_mm, count, errors);
    else
        printf(">> ToF: --- %s (Status %u) | %lu Messungen | Fehler %lu\r\n",
               status_text(range_status), range_status, count, errors);
}
