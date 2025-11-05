// src/hal_gpio_linux_int.h
#pragma once

#include "hal_gpio.h"
#include <time.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint64_t _timespec_to_ns(const struct timespec* ts);
int      _logical_to_physical(const HAL_GpioLineConfig* c, int logical);
int      _physical_to_logical(const HAL_GpioLineConfig* c, int physical);

#ifdef __cplusplus
}
#endif
