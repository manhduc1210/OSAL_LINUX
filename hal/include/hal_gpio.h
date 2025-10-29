/**
 * @file hal_gpio.h
 * @brief Portable GPIO HAL (OS-agnostic). Linux backend uses libgpiod.
 *
 * This HAL focuses on:
 *  - Requesting LED output lines in bulk and writing an 8-bit value
 *  - Requesting button input lines in bulk and reading their states
 *
 * Extend as needed (edge events/callbacks) while keeping this API.
 */
#pragma once
#include "osal_types.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct HAL_Gpio HAL_Gpio;

typedef enum {
    HAL_GPIO_OK = 0,
    HAL_GPIO_EINVAL,
    HAL_GPIO_EIO,
    HAL_GPIO_ECFG
} HAL_GpioStatus;

typedef struct {
    const char* chip_name;     ///< e.g. "gpiochip0"
    int         led_base;      ///< first LED line offset (contiguous)
    uint8_t     led_count;     ///< number of LED lines (max 8 used by demo)
    int         btn0_offset;   ///< button 0 line
    int         btn1_offset;   ///< button 1 line
    uint8_t     leds_active_low; ///< 1 if LEDs are wired active-low
    uint8_t     btns_active_low; ///< 1 if buttons are wired active-low (pressed=0)
} HAL_GpioConfig;

/** Open GPIO HAL and request lines. Returns NULL on failure. */
HAL_Gpio* HAL_Gpio_Open(const HAL_GpioConfig* cfg, HAL_GpioStatus* out_st);

/** Release lines and free handle. */
void HAL_Gpio_Close(HAL_Gpio* h);

/** Write lower 8 bits to LED bank (applies active-low mapping internally). */
HAL_GpioStatus HAL_Gpio_WriteLeds(HAL_Gpio* h, uint8_t value);

/** Read BTN0/BTN1 (bit0=BTN0, bit1=BTN1). Returned logic is "pressed=1". */
HAL_GpioStatus HAL_Gpio_ReadBtns(HAL_Gpio* h, uint8_t* out_bits);

#ifdef __cplusplus
}
#endif
