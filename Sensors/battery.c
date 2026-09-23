#include "battery.h"
#include <stdio.h>

static ADC_HandleTypeDef hadc1;

static float volt = 0.0f;  /* gefilterte Akkuspannung */
static float scale = 1.0f; /* Korrekturfaktor aus der Kalibrierung */
static uint8_t cells = 0;  /* erkannte Zellenzahl */
static BatteryState_t state = BAT_NONE;

static float timer = 0.0f;
static uint8_t settle = 0; /* Zaehler, bis die Zellenerkennung greift */

/* Wie oft gemessen wird. 100 Hz reicht voellig und
 * haelt die Regelschleife frei. */
#define BAT_RATE_HZ 100.0f
/* Tiefpass, damit Stromspitzen der Motoren die Anzeige nicht zappeln lassen */
#define BAT_TAU 0.50f

void Battery_Init(void)
{
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_0;
    g.Mode = GPIO_MODE_ANALOG;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &g);

    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode = DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    if (HAL_ADC_Init(&hadc1) != HAL_OK)
        Error_Handler();

    ADC_ChannelConfTypeDef ch = {0};
    ch.Channel = ADC_CHANNEL_10; /* PC0 */
    ch.Rank = 1;
    ch.SamplingTime = ADC_SAMPLETIME_144CYCLES; /* lang, weil der Teiler hochohmig ist */
    if (HAL_ADC_ConfigChannel(&hadc1, &ch) != HAL_OK)
        Error_Handler();

    volt = 0.0f;
    cells = 0;
    state = BAT_NONE;
    settle = 0;
    timer = 0.0f;
}

/* Eine Wandlung. Dauert bei 144 Zyklen rund 10 us. */
static float measure_once(void)
{
    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 2) != HAL_OK)
    {
        HAL_ADC_Stop(&hadc1);
        return -1.0f;
    }
    uint32_t raw = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);

    const float pin_v = ((float)raw / 4095.0f) * BAT_VREF;
    const float teiler = (BAT_R_OBEN + BAT_R_UNTEN) / BAT_R_UNTEN; /* 11.0 bei 10k/1k */
    return pin_v * teiler * scale;
}

void Battery_Update(float dt)
{
    timer += dt;
    if (timer < (1.0f / BAT_RATE_HZ))
        return;
    const float step = timer;
    timer = 0.0f;

    const float v = measure_once();
    if (v < 0.0f)
        return;

    /* Tiefpass */
    const float a = step / (BAT_TAU + step);
    volt += a * (v - volt);

    /* Unter 5 V ist offensichtlich kein Akku dran (Betrieb nur ueber USB) */
    if (volt < 5.0f)
    {
        cells = 0;
        state = BAT_NONE;
        settle = 0;
        return;
    }

    /* Zellenzahl einmalig bestimmen, sobald sich der Wert beruhigt hat.
     * Eine Zelle liegt zwischen 3,0 und 4,25 V - daraus laesst sich die
     * Zahl eindeutig ausrechnen. Danach bleibt sie fest, sonst wuerde die
     * Erkennung bei leerem Akku auf eine Zelle weniger springen. */
    if (cells == 0)
    {
        if (settle < 100) /* etwa eine Sekunde warten */
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

    const float cv = volt / cells;
    if (cv < BAT_CRIT_V)
        state = BAT_CRITICAL;
    else if (cv < BAT_WARN_V)
        state = BAT_WARN;
    else
        state = BAT_OK;
}

float Battery_Volt(void) { return volt; }
uint8_t Battery_Cells(void) { return cells; }
float Battery_CellVolt(void) { return (cells > 0) ? (volt / cells) : 0.0f; }
BatteryState_t Battery_State(void) { return state; }

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
    if (real_volt < 5.0f || volt < 5.0f)
        return;
    /* aktuellen Messwert auf den echten Wert ziehen */
    scale *= (real_volt / volt);
    volt = real_volt;
}

float Battery_GetScale(void) { return scale; }