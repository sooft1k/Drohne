#ifndef STM32F4XX_HAL_H
#define STM32F4XX_HAL_H

/* Minimaler Ersatz fuer die HAL, damit Firmware-Module auf dem PC
 * kompilieren. Nur was die getesteten Header brauchen. */

#include <stdint.h>
#include <stddef.h>

typedef enum
{
    HAL_OK = 0,
    HAL_ERROR,
    HAL_BUSY,
    HAL_TIMEOUT
} HAL_StatusTypeDef;

typedef struct { int dummy; } I2C_HandleTypeDef;
typedef struct { int dummy; } TIM_HandleTypeDef;
typedef struct { int dummy; } ADC_HandleTypeDef;
typedef struct { uint32_t ErrorCode; } UART_HandleTypeDef;

typedef struct
{
    volatile uint32_t CCMR1, CCMR2, CCR1, CCR2, CCR3, CCR4;
} TIM_TypeDef;
extern TIM_TypeDef stub_tim3;
#define TIM3 (&stub_tim3)
#define TIM_CCMR1_OC1PE (1u << 3)
#define TIM_CCMR1_OC2PE (1u << 11)
#define TIM_CCMR2_OC3PE (1u << 3)
#define TIM_CCMR2_OC4PE (1u << 11)

HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *huart, const uint8_t *pData, uint16_t Size);

#endif /* STM32F4XX_HAL_H */
