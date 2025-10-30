/**
 * @file expander_drv.h
 * @brief Simple I2C GPIO expander driver (MCP23008-like).
 *
 * Abstracts HAL_I2C so upper app can read buttons & write LEDs.
 */
#pragma once
#include "hal_i2c.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    HAL_I2cBus* bus;   ///< opened I2C bus handle
    uint8_t     addr7; ///< expander 7-bit I2C address (e.g. 0x20)
} Expander;

/**
 * @brief Initialize expander: configure I/O direction & clear outputs.
 * Bit0-1 = input (buttons), Bit2-7 = output (LEDs).
 */
HAL_I2cStatus Expander_Init(Expander* io);

/**
 * @brief Read all 8 GPIO bits (1=input high, 0=low).
 */
HAL_I2cStatus Expander_ReadInputs(Expander* io, uint8_t* out_state);

/**
 * @brief Write 8 output bits (bit2..7 drive LEDs; bit0-1 ignored).
 */
HAL_I2cStatus Expander_WriteOutputs(Expander* io, uint8_t value);

#ifdef __cplusplus
}
#endif
