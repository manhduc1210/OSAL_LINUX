/**
 * @file oled_spi_drv.h
 * @brief Minimal SPI OLED (SSD1306-like) driver using HAL_SPI + HAL_GPIO style hooks.
 *
 * We assume:
 *  - 128x64 monochrome display
 *  - SPI 4-wire interface (CS, SCLK, MOSI, D/C, RES)
 *
 * NOTE: We don't depend directly on HAL_GPIO here to keep this driver portable.
 * Instead we ask the app to give us 2 callback functions to control DC and RST.
 */

#pragma once
#include <stdint.h>
#include "hal_spi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*OledPinFn)(int level); // level=0 or 1

typedef struct {
    HAL_SpiBus* spi;
    OledPinFn   set_dc;   ///< set D/C pin: 0=command,1=data
    OledPinFn   set_rst;  ///< pulse reset low/high
    // (optional) set_cs override if CS is not hardware-controlled
    OledPinFn   set_cs;
    uint8_t     width;
    uint8_t     height;
} OledCtx;

/**
 * @brief Initialize OLED controller (reset, send init commands, clear).
 */
int OLED_Init(OledCtx* ctx);

/**
 * @brief Clear entire screen (all pixels off).
 */
int OLED_Clear(OledCtx* ctx);

/**
 * @brief Draw a single ASCII string at row 0 (top), monospaced 6x8 font.
 * This is just a super simple demo renderer, not a full text engine.
 */
int OLED_DrawTextRow0(OledCtx* ctx, const char* txt);

#ifdef __cplusplus
}
#endif
