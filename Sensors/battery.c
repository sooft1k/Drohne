#include "battery.h"
#include "motor.h"
#include <stdio.h>

static ADC_HandleTypeDef hadc1; /* Akkuspannung, PC0 */
static ADC_HandleTypeDef hadc2; /* Strom, PA4 */

static float volt = 0.0f;  /* gefilterte Akkuspannung */
static float scale = 1.0f; /* Korrekturfaktor aus der Kalibrierung */
static uint8_t cells = 0;  /* erkannte Zellenzahl */
static BatteryState_t state = BAT_NONE;
static uint8_t primed = 0;      /* 1 = Filter laeuft, Akku ist erkannt */
static uint8_t absent = 0;      /* Messungen in Folge unter BAT_PRESENT_V */
static float low_time = 0.0f;   /* so lange liegt die Spannung schon schlechter als 'state' */
static float thr_cap = 100.0f;  /* aktuelle Gasgrenze in % */

/* Strom */
static float cur_mv_per_a = CURR_MV_PER_A_DEFAULT;
static float cur_zero_mv = 0.0f; /* Ausgangsspannung bei 0 A */
static float cur_mv = 0.0f;      /* gefilterte Pinspannung, mV */
static float amps = 0.0f;        /* gefilterter Strom */
static float mah = 0.0f;         /* verbrauchte Kapazitaet */
static float zero_acc = 0.0f;    /* automatischer Nullabgleich beim Anstecken */
static uint8_t zero_n = 0;

static float timer = 0.0f;
static uint8_t settle = 0; /* Zaehler, bis die Zellenerkennung greift */

/* Wie oft gemessen wird. 100 Hz reicht voellig und
 * haelt die Regelschleife frei. */
#define BAT_RATE_HZ 100.0f
/* Tiefpass, damit Stromspitzen der Motoren die Anzeige nicht zappeln lassen */
#define BAT_TAU 0.50f
/* Tiefpass auf den Strom: glaettet die PWM-Welligkeit des ESC */
#define CURR_TAU 0.10f
/* Unter dieser Spannung ist kein Akku dran (Betrieb nur ueber USB) */
#define BAT_PRESENT_V 5.0f
/* So viele Messungen in Folge darunter, bis der Akku als abgesteckt gilt */
#define BAT_ABSENT_COUNT 10
/* Wartezeit nach dem Anstecken bis zur Zellenerkennung (Messungen, 0,5 s) */
#define BAT_SETTLE_COUNT 50
/* Messungen fuer den automatischen Strom-Nullabgleich (0,5 s) */
#define CURR_ZERO_COUNT 50

static void adc_init(ADC_HandleTypeDef *h, ADC_TypeDef *inst, uint32_t channel)
{
    h->Instance = inst;
    h->Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    h->Init.Resolution = ADC_RESOLUTION_12B;
    h->Init.ScanConvMode = DISABLE;
    h->Init.ContinuousConvMode = DISABLE;
    h->Init.DiscontinuousConvMode = DISABLE;
    h->Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    h->Init.ExternalTrigConv = ADC_SOFTWARE_START;
    h->Init.DataAlign = ADC_DATAALIGN_RIGHT;
    h->Init.NbrOfConversion = 1;
    h->Init.DMAContinuousRequests = DISABLE;
    h->Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    if (HAL_ADC_Init(h) != HAL_OK)
        Error_Handler();

    ADC_ChannelConfTypeDef ch = {0};
    ch.Channel = channel;
    ch.Rank = 1;
    ch.SamplingTime = ADC_SAMPLETIME_144CYCLES; /* lang, weil die Quellen hochohmig sind */
    if (HAL_ADC_ConfigChannel(h, &ch) != HAL_OK)
        Error_Handler();
}

void Battery_Init(void)
{
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_ADC2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Mode = GPIO_MODE_ANALOG;
    g.Pull = GPIO_NOPULL;
    g.Pin = GPIO_PIN_0; /* PC0: Akkuspannung */
    HAL_GPIO_Init(GPIOC, &g);
    g.Pin = GPIO_PIN_4; /* PA4: Strom */
    HAL_GPIO_Init(GPIOA, &g);

    adc_init(&hadc1, ADC1, ADC_CHANNEL_10);
    adc_init(&hadc2, ADC2, ADC_CHANNEL_4);

    volt = 0.0f;
    cells = 0;
    state = BAT_NONE;
    primed = 0;
    settle = 0;
    timer = 0.0f;
    thr_cap = 100.0f;
}

/* Eine Wandlung, Rueckgabe Pinspannung in mV oder -1.
 * Dauert bei 144 Zyklen rund 7,4 us. */
static float measure_mv(ADC_HandleTypeDef *h)
{
    HAL_ADC_Start(h);
    if (HAL_ADC_PollForConversion(h, 2) != HAL_OK)
    {
        HAL_ADC_Stop(h);
        return -1.0f;
    }
    uint32_t raw = HAL_ADC_GetValue(h);
    HAL_ADC_Stop(h);
    return ((float)raw / 4095.0f) * BAT_VREF * 1000.0f;
}

/* Zustand aus der Zellspannung, ohne Entprellung */
static BatteryState_t state_from_cell(float cv)
{
    if (cv < BAT_CRIT_V)
        return BAT_CRITICAL;
    if (cv < BAT_WARN_V)
        return BAT_WARN;
    return BAT_OK;
}

void Battery_Update(float dt)
{
    /* Gasgrenze folgt dem Zustand: nach unten langsam, nach oben nur disarmed */
    const float target = (state == BAT_CRITICAL) ? BAT_THR_LIMIT_CRIT
                         : (state == BAT_WARN)   ? BAT_THR_LIMIT_WARN
                                                 : 100.0f;
    if (thr_cap > target)
    {
        thr_cap -= BAT_THR_SLEW * dt;
        if (thr_cap < target)
            thr_cap = target;
    }
    else if (!Motor_IsArmed())
    {
        thr_cap = target;
    }

    timer += dt;
    if (timer < (1.0f / BAT_RATE_HZ))
        return;
    const float step = timer;
    timer = 0.0f;

    const float mv_bat = measure_mv(&hadc1);
    const float mv_cur = measure_mv(&hadc2);
    if (mv_bat < 0.0f)
        return;

    const float teiler = (BAT_R_OBEN + BAT_R_UNTEN) / BAT_R_UNTEN; /* 11.0 bei 10k/1k */
    const float v = mv_bat * 0.001f * teiler * scale;

    /* ---------- Akku da? ----------
     * Kurz unter 5 V zaehlt nicht (Messfehler), erst BAT_ABSENT_COUNT in Folge. */
    if (v < BAT_PRESENT_V)
    {
        if (absent < 255)
            absent++;
        if (absent >= BAT_ABSENT_COUNT || !primed)
        {
            primed = 0;
            cells = 0;
            state = BAT_NONE;
            settle = 0;
            volt = v;
            amps = 0.0f;
        }
        return;
    }
    absent = 0;

    if (!primed)
    {
        /* Frisch angesteckt: Filter mit dem ersten Messwert starten statt
         * von 0 hochlaufen zu lassen - sonst liegt er bei der Zellenerkennung
         * erst bei rund 90 % und es werden zu wenige Zellen erkannt. */
        primed = 1;
        volt = v;
        settle = 0;
        cells = 0;
        mah = 0.0f;
        cur_mv = (mv_cur >= 0.0f) ? mv_cur : 0.0f;
        zero_acc = 0.0f;
        zero_n = 0;
    }
    else
    {
        const float a = step / (BAT_TAU + step);
        volt += a * (v - volt);
    }

    /* ---------- Strom und mAh ---------- */
    if (mv_cur >= 0.0f)
    {
        const float ac = step / (CURR_TAU + step);
        cur_mv += ac * (mv_cur - cur_mv);

        /* Nullabgleich in der ersten halben Sekunde nach dem Anstecken,
         * nur disarmed - dann stehen die Motoren. */
        if (zero_n < CURR_ZERO_COUNT && !Motor_IsArmed())
        {
            zero_acc += mv_cur;
            zero_n++;
            if (zero_n == CURR_ZERO_COUNT)
                cur_zero_mv = zero_acc / (float)CURR_ZERO_COUNT;
        }

        float i = (cur_mv - cur_zero_mv) / cur_mv_per_a;
        if (i < 0.0f)
            i = 0.0f;
        amps = i;
        mah += amps * step / 3.6f; /* A * s -> mAh */
    }

    /* ---------- Zellenzahl ----------
     * Einmal nach dem Anstecken bestimmen, danach bleibt sie fest - sonst
     * wuerde die Erkennung bei leerem Akku auf eine Zelle weniger springen.
     * Eine Zelle liegt zwischen 3,3 und 4,3 V. */
    if (cells == 0)
    {
        if (BAT_CELLS_DEFAULT > 0)
        {
            cells = BAT_CELLS_DEFAULT;
        }
        else
        {
            if (settle < BAT_SETTLE_COUNT)
            {
                settle++;
                return;
            }
            for (uint8_t c = 2; c <= 6; c++)
            {
                const float cv = volt / c;
                if (cv >= 3.30f && cv <= 4.30f)
                {
                    cells = c;
                    break;
                }
            }
            if (cells == 0)
                return; /* nichts Sinnvolles erkannt */
        }
        state = state_from_cell(volt / cells);
        low_time = 0.0f;
    }

    /* ---------- Zustand ----------
     * Disarmed: direkt aus der Ruhespannung.
     * Gearmt: nur schlechter werden, und erst wenn es BAT_DEBOUNCE_S lang
     * so bleibt - Spannungseinbrueche bei Vollgas loesen nichts aus. */
    const BatteryState_t now = state_from_cell(volt / cells);
    if (!Motor_IsArmed())
    {
        state = now;
        low_time = 0.0f;
    }
    else if (now != BAT_NONE && now > state)
    {
        low_time += step;
        if (low_time >= BAT_DEBOUNCE_S)
        {
            state = now;
            low_time = 0.0f;
        }
    }
    else
    {
        low_time = 0.0f;
    }
}

float Battery_Volt(void) { return volt; }
uint8_t Battery_Cells(void) { return cells; }
float Battery_CellVolt(void) { return (cells > 0) ? (volt / cells) : 0.0f; }
BatteryState_t Battery_State(void) { return state; }
float Battery_Current(void) { return amps; }
float Battery_MahUsed(void) { return mah; }
float Battery_ThrottleCap(void) { return thr_cap; }

uint8_t Battery_Percent(void)
{
    /* Grob linear zwischen 3,3 V (leer) und 4,2 V (voll) pro Zelle */
    if (cells == 0)
        return 0;
    float p = (volt / cells - 3.30f) / (4.20f - 3.30f) * 100.0f;
    if (p < 0.0f)
        p = 0.0f;
    if (p > 100.0f)
        p = 100.0f;
    return (uint8_t)p;
}

float Battery_LimitThrottle(float throttle)
{
    return (throttle > thr_cap) ? thr_cap : throttle;
}

const char *Battery_StateText(void)
{
    switch (state)
    {
    case BAT_OK:
        return "OK";
    case BAT_WARN:
        return "WARNUNG - landen";
    case BAT_CRITICAL:
        return "KRITISCH - sofort landen";
    default:
        return "kein Akku";
    }
}

void Battery_Calibrate(float real_volt)
{
    if (!(real_volt >= 5.0f) || volt < 5.0f)
        return;
    /* aktuellen Messwert auf den echten Wert ziehen */
    scale *= (real_volt / volt);
    volt = real_volt;
}

float Battery_GetScale(void) { return scale; }

uint8_t Battery_CurrentZero(void)
{
    if (Motor_IsArmed())
        return 0;
    cur_zero_mv = cur_mv;
    zero_n = CURR_ZERO_COUNT; /* automatischen Abgleich nicht mehr ueberschreiben */
    return 1;
}

uint8_t Battery_CurrentCalibrate(float real_amps)
{
    /* Braucht einen deutlichen Strom, sonst wird die Skalierung ungenau */
    const float d = cur_mv - cur_zero_mv;
    if (!(real_amps >= 1.0f) || d < 5.0f)
        return 0;
    cur_mv_per_a = d / real_amps;
    return 1;
}

float Battery_CurrentMvPerA(void) { return cur_mv_per_a; }
float Battery_CurrentZeroMv(void) { return cur_zero_mv; }
