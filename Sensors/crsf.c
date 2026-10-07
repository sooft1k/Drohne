#include "crsf.h"
#include <string.h>

#define CRSF_TYPE_RC_CHANNELS 0x16

static UART_HandleTypeDef *crsf_uart = 0;
static uint8_t rx_byte;

/* Empfangspuffer und Zustandsmaschine */
static uint8_t buf[CRSF_FRAME_MAX];
static uint8_t pos = 0;
static uint8_t frame_len = 0;

static volatile uint16_t ch_us[CRSF_CHANNELS];
static volatile uint32_t last_frame_ms = 0;
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
    if (last_frame_ms == 0)
        return 0xFFFFFFFF;
    return HAL_GetTick() - last_frame_ms;
}

uint8_t CRSF_LinkUp(void)
{
    return (CRSF_AgeMs() < CRSF_TIMEOUT_MS) ? 1 : 0;
}

uint32_t CRSF_FrameCount(void) { return frame_count; }
uint32_t CRSF_ErrorCount(void) { return error_count; }

/* Bei Rahmenfehlern bleibt der Interrupt sonst stehen. */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (crsf_uart && huart->Instance == crsf_uart->Instance)
    {
        error_count++;
        pos = 0;
        __HAL_UART_CLEAR_OREFLAG(huart);
        __HAL_UART_CLEAR_NEFLAG(huart);
        __HAL_UART_CLEAR_FEFLAG(huart);
        HAL_UART_Receive_IT(crsf_uart, &rx_byte, 1);
    }
}