#include "crsf.h"
#include <string.h>

#define CRSF_TYPE_RC_CHANNELS 0x16
#define CRSF_TYPE_BATTERY 0x08

static UART_HandleTypeDef *crsf_uart = 0;
static uint8_t rx_byte;

/* Empfangspuffer und Zustandsmaschine */
static uint8_t buf[CRSF_FRAME_MAX];
static uint8_t pos = 0;
static uint8_t frame_len = 0;

static volatile uint16_t ch_us[CRSF_CHANNELS];
static volatile uint32_t last_frame_ms = 0;
static volatile uint8_t have_frame = 0; /* 1 = schon mindestens ein gueltiges Paket */
static volatile uint32_t frame_count = 0;
static volatile uint32_t error_count = 0;

/* CRC8 mit Polynom 0xD5, so schreibt es CRSF vor. */
static uint8_t crc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++)
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0xD5) : (uint8_t)(crc << 1);
    }
    return crc;
}

/* Rohwert 172..1811 in Mikrosekunden 988..2012 umrechnen */
static uint16_t to_us(uint16_t raw)
{
    int32_t us = ((int32_t)raw * 1024) / 1639 + 881;
    if (us < 800)
        us = 800;
    if (us > 2200)
        us = 2200;
    return (uint16_t)us;
}

/* 22 Bytes enthalten 16 Kanaele zu je 11 Bit, luecklos aneinandergereiht. */
static void unpack_channels(const uint8_t *p)
{
    uint32_t bits = 0;
    uint8_t nbits = 0;
    uint8_t idx = 0;

    for (uint8_t i = 0; i < 22 && idx < CRSF_CHANNELS; i++)
    {
        bits |= ((uint32_t)p[i]) << nbits;
        nbits += 8;
        while (nbits >= 11 && idx < CRSF_CHANNELS)
        {
            ch_us[idx++] = to_us((uint16_t)(bits & 0x7FF));
            bits >>= 11;
            nbits -= 11;
        }
    }
}

void CRSF_Init(UART_HandleTypeDef *huart)
{
    crsf_uart = huart;
    pos = 0;
    frame_len = 0;
    frame_count = 0;
    error_count = 0;
    last_frame_ms = 0;
    have_frame = 0;

    /* Startwerte: alles Mitte, Gas ganz unten.
     * Falls nie ein Paket kommt, steht damit nichts auf Vollgas. */
    for (uint8_t i = 0; i < CRSF_CHANNELS; i++)
        ch_us[i] = 1500;
    ch_us[2] = 988;

    HAL_UART_Receive_IT(crsf_uart, &rx_byte, 1);
}

void CRSF_RxByte(void)
{
    uint8_t c = rx_byte;

    if (pos == 0)
    {
        /* Erstes Byte ist die Adresse. 0xC8 ist der Flight Controller,
         * 0xEE kommt von manchen Empfaengern. Alles andere verwerfen. */
        if (c == 0xC8 || c == 0xEE)
            buf[pos++] = c;
    }
    else if (pos == 1)
    {
        /* Laengenbyte pruefen, bevor wir darauf vertrauen */
        if (c >= 2 && c <= (CRSF_FRAME_MAX - 2))
        {
            buf[pos++] = c;
            frame_len = c;
        }
        else
        {
            pos = 0;
            error_count++;
        }
    }
    else
    {
        buf[pos++] = c;

        if (pos >= (uint8_t)(frame_len + 2))
        {
            /* Paket vollstaendig. CRC laeuft ueber Typ und Daten,
             * also ab Index 2 bis vor das letzte Byte. */
            uint8_t calc = crc8(&buf[2], (uint8_t)(frame_len - 1));
            uint8_t recv = buf[frame_len + 1];

            if (calc == recv)
            {
                if (buf[2] == CRSF_TYPE_RC_CHANNELS && frame_len == 24)
                {
                    unpack_channels(&buf[3]);
                    last_frame_ms = HAL_GetTick();
                    have_frame = 1;
                    frame_count++;
                }
                /* andere Pakettypen ignorieren wir */
            }
            else
            {
                error_count++;
            }
            pos = 0;
        }
    }

    HAL_UART_Receive_IT(crsf_uart, &rx_byte, 1);
}

uint16_t CRSF_Channel(uint8_t ch)
{
    if (ch < 1 || ch > CRSF_CHANNELS)
        return 1500;
    return ch_us[ch - 1];
}

uint32_t CRSF_AgeMs(void)
{
    if (!have_frame)
        return 0xFFFFFFFF;

    /* Zeitstempel ZUERST lesen, dann die aktuelle Zeit. Andersherum kann
     * genau dazwischen ein Paket ankommen (Zeitstempel = jetzt + 1 ms),
     * die Differenz wird negativ, also 0xFFFFFFFF -> "kein Signal". */
    const uint32_t last = last_frame_ms;
    return HAL_GetTick() - last;
}

uint8_t CRSF_LinkUp(void)
{
    return (CRSF_AgeMs() < CRSF_TIMEOUT_MS) ? 1 : 0;
}

/* Akku-Telemetrie an den Empfaenger (Typ 0x08). Der ELRS-Empfaenger
 * leitet sie an die Funke weiter, dort erscheinen die Sensoren "RxBt",
 * "Curr", "Capa" und "Bat%" (Telemetrie -> Sensoren suchen).
 * Nicht blockierend: laeuft die vorige Uebertragung noch, wird ausgelassen. */
void CRSF_SendBattery(float volt, float amps, float mah, uint8_t percent)
{
    static uint8_t tx[12]; /* muss bis zum Ende der Uebertragung gueltig bleiben */

    if (!crsf_uart || HAL_UART_GetState(crsf_uart) == HAL_UART_STATE_BUSY_TX ||
        HAL_UART_GetState(crsf_uart) == HAL_UART_STATE_BUSY_TX_RX)
        return;

    if (volt < 0.0f)
        volt = 0.0f;
    if (amps < 0.0f)
        amps = 0.0f;
    if (mah < 0.0f)
        mah = 0.0f;
    const uint32_t v = (uint32_t)(volt * 10.0f + 0.5f);  /* 0,1 V */
    const uint32_t a = (uint32_t)(amps * 10.0f + 0.5f);  /* 0,1 A */
    const uint32_t c = (uint32_t)(mah + 0.5f);           /* mAh  */

    tx[0] = 0xC8;              /* Adresse: Flight Controller */
    tx[1] = 10;                /* Laenge: Typ + 8 Daten + CRC */
    tx[2] = CRSF_TYPE_BATTERY;
    tx[3] = (uint8_t)(v >> 8);
    tx[4] = (uint8_t)v;
    tx[5] = (uint8_t)(a >> 8);
    tx[6] = (uint8_t)a;
    tx[7] = (uint8_t)(c >> 16);
    tx[8] = (uint8_t)(c >> 8);
    tx[9] = (uint8_t)c;
    tx[10] = percent;
    tx[11] = crc8(&tx[2], 9);

    HAL_UART_Transmit_IT(crsf_uart, tx, sizeof(tx));
}

uint32_t CRSF_FrameCount(void) { return frame_count; }
uint32_t CRSF_ErrorCount(void) { return error_count; }

/* Bei Ueberlauf (ORE) beendet die HAL den Empfang - neu starten. Bei Rahmen-
 * oder Rauschfehlern laeuft er weiter; das angefangene Paket wird verworfen
 * (pos = 0), die CRC haette es ohnehin abgelehnt.
 * Aufruf aus HAL_UART_ErrorCallback (command.c). */
void CRSF_RxError(void)
{
    if (!crsf_uart)
        return;
    error_count++;
    pos = 0;
    __HAL_UART_CLEAR_OREFLAG(crsf_uart);
    __HAL_UART_CLEAR_NEFLAG(crsf_uart);
    __HAL_UART_CLEAR_FEFLAG(crsf_uart);
    HAL_UART_Receive_IT(crsf_uart, &rx_byte, 1);
}