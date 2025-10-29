/**
 * @file hal_gpio_linux.c
 * @brief Linux backend for HAL GPIO (libgpiod).
 */
#include "hal_gpio.h"
#include "osal.h"

#include <gpiod.h>
#include <string.h>
#include <stdlib.h>

struct HAL_Gpio {
    struct gpiod_chip* chip;

    // LEDs
    struct gpiod_line_bulk led_bulk;
    unsigned               led_count;
    int                    led_offsets[8];
    int                    leds_active_low;

    // Buttons
    struct gpiod_line_bulk btn_bulk;
    unsigned               btn_count;
    int                    btn_offsets[2];
    int                    btns_active_low;
};

static int _req_leds(struct HAL_Gpio* h, const HAL_GpioConfig* c) {
    h->led_count = c->led_count;
    if (h->led_count == 0 || h->led_count > 8) return -1;

    for (unsigned i = 0; i < h->led_count; ++i)
        h->led_offsets[i] = c->led_base + (int)i;

    if (gpiod_chip_get_lines(h->chip, h->led_offsets, h->led_count, &h->led_bulk) < 0)
        return -2;

    // Request as outputs, initial 0
    int init_vals[8] = {0};
    if (gpiod_line_request_bulk_output(&h->led_bulk, "hal_leds", init_vals) < 0)
        return -3;

    h->leds_active_low = c->leds_active_low ? 1 : 0;
    return 0;
}

static int _req_btns(struct HAL_Gpio* h, const HAL_GpioConfig* c) {
    h->btn_count = 2;
    h->btn_offsets[0] = c->btn0_offset;
    h->btn_offsets[1] = c->btn1_offset;

    if (gpiod_chip_get_lines(h->chip, h->btn_offsets, h->btn_count, &h->btn_bulk) < 0)
        return -2;

    if (gpiod_line_request_bulk_input(&h->btn_bulk, "hal_btns") < 0)
        return -3;

    h->btns_active_low = c->btns_active_low ? 1 : 0;
    return 0;
}

HAL_Gpio* HAL_Gpio_Open(const HAL_GpioConfig* cfg, HAL_GpioStatus* out_st) {
    if (!cfg || !cfg->chip_name) {
        if (out_st) *out_st = HAL_GPIO_EINVAL;
        return NULL;
    }

    struct HAL_Gpio* h = (struct HAL_Gpio*)calloc(1, sizeof(*h));
    // OSAL_LOG("h : %s \r\n", h);
    if (!h) { 
        if (out_st) *out_st = HAL_GPIO_EIO; 
        return NULL; 
    }

    // OSAL_LOG("chip_name : %s \r\n", cfg->chip_name);

    h->chip = gpiod_chip_open_by_name(cfg->chip_name);
    if (!h->chip) {
        OSAL_LOG("[GPIO][LINUX] open chip %s failed\r\n", cfg->chip_name);
        if (out_st) *out_st = HAL_GPIO_EIO; 
        free(h); 
        return NULL;
    }

    if (_req_leds(h, cfg) != 0 || _req_btns(h, cfg) != 0) {
        if (out_st) *out_st = HAL_GPIO_ECFG;
        gpiod_chip_close(h->chip); 
        free(h);
        return NULL;
    }

    if (out_st) *out_st = HAL_GPIO_OK;
    OSAL_LOG("[GPIO][LINUX] chip=%s leds[%u] base=%d btns=(%d,%d)\r\n",
             cfg->chip_name, (unsigned)cfg->led_count, cfg->led_base,
             cfg->btn0_offset, cfg->btn1_offset);
    return h;
}

void HAL_Gpio_Close(HAL_Gpio* h) {
    if (!h) return;
    gpiod_line_release_bulk(&h->led_bulk);
    gpiod_line_release_bulk(&h->btn_bulk);
    if (h->chip) gpiod_chip_close(h->chip);
    free(h);
}

HAL_GpioStatus HAL_Gpio_WriteLeds(HAL_Gpio* h, uint8_t value) {
    if (!h) return HAL_GPIO_EINVAL;

    int vals[8];
    for (unsigned i = 0; i < h->led_count; ++i) {
        int bit = (value >> i) & 1;
        vals[i] = h->leds_active_low ? !bit : bit;
    }
    if (gpiod_line_set_value_bulk(&h->led_bulk, vals) < 0)
        return HAL_GPIO_EIO;
    return HAL_GPIO_OK;
}

HAL_GpioStatus HAL_Gpio_ReadBtns(HAL_Gpio* h, uint8_t* out_bits) {
    if (!h || !out_bits) return HAL_GPIO_EINVAL;
    int vals[2] = {0,0};
    if (gpiod_line_get_value_bulk(&h->btn_bulk, vals) < 0)
        return HAL_GPIO_EIO;

    // Normalize to "pressed=1"
    uint8_t b0 = (uint8_t)(vals[0] ? 1 : 0);
    uint8_t b1 = (uint8_t)(vals[1] ? 1 : 0);
    if (h->btns_active_low) { b0 = !b0; b1 = !b1; }

    *out_bits = (uint8_t)((b0 ? 1 : 0) | (b1 ? 2 : 0));
    return HAL_GPIO_OK;
}
