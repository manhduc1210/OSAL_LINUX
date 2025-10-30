/**
 * @file hal_i2c_mock.c
 * @brief Mock backend for HAL I2C. No real hardware access.
 *
 * We emulate a single I2C peripheral at address 0x48:
 *  - Register 0x00/0x01 hold the current temperature (12-bit format like TMP102).
 *  - Writing to 0xF0 with 1 byte value sets the mock temperature directly in raw units.
 *
 * This lets us exercise HAL_I2c_* API without physical hardware.
 */

#include "hal_i2c.h"
#include "osal.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* -------- internal mock sensor state -------- */

typedef struct {
    // mock temperature in "raw TMP102 units":
    // raw * 0.0625 °C is the actual temperature.
    // We'll advance it over time.
    uint16_t temp_raw_12b;  // only top 12 bits are meaningful
} MockTempSensor;

static MockTempSensor g_sensor = {
    .temp_raw_12b = 25.0f / 0.0625f  // ~25°C initial -> raw ~400 decimal
};

/* We'll also keep a fake "I2C bus" handle struct that matches HAL_I2cBus */
struct HAL_I2cBus {
    const char* bus_name;
    uint32_t    bus_speed_hz;
    uint8_t     current_addr7;
};

/* helper: convert temp_raw_12b into 2 bytes hi/lo like TMP102 register 0x00 */
static void _encode_temp_bytes(uint16_t raw12, uint8_t out[2]) {
    // TMP102 style: 12-bit value in bits [15:4]
    // Example: raw12=0x190 (~25C). We place it <<4:
    uint16_t v = (uint16_t)(raw12 << 4);
    out[0] = (v >> 8) & 0xFF;
    out[1] = v & 0xFF;
}

/* simulate temperature drifting +0.5C every call, wrap after ~30C -> 25C again */
static void _mock_update_temp(void) {
    float tempC = (float)g_sensor.temp_raw_12b * 0.0625f;
    tempC += 0.5f;
    if (tempC > 30.0f) {
        tempC = 25.0f;
    }
    g_sensor.temp_raw_12b = (uint16_t)(tempC / 0.0625f);
}

/* -------- HAL API implementation (mock) -------- */

HAL_I2cBus* HAL_I2cBus_Open(const HAL_I2cBusConfig* cfg, HAL_I2cStatus* out_status) {
    if (!cfg || !cfg->bus_name || !cfg->bus_name[0]) {
        if (out_status) *out_status = HAL_I2C_EINVAL;
        return NULL;
    }

    HAL_I2cBus* b = (HAL_I2cBus*)calloc(1, sizeof(*b));
    if (!b) {
        if (out_status) *out_status = HAL_I2C_EBUS;
        return NULL;
    }

    b->bus_name = cfg->bus_name;
    b->bus_speed_hz = cfg->bus_speed_hz;
    b->current_addr7 = 0x00;

    OSAL_LOG("[I2C][MOCK] open bus '%s' speed_hint=%u\r\n",
             cfg->bus_name, (unsigned)cfg->bus_speed_hz);

    if (out_status) *out_status = HAL_I2C_OK;
    return b;
}

void HAL_I2cBus_Close(HAL_I2cBus* bus) {
    if (!bus) return;
    OSAL_LOG("[I2C][MOCK] close bus '%s'\r\n",
             bus->bus_name ? bus->bus_name : "(null)");
    free(bus);
}

static void _set_addr(HAL_I2cBus* bus, uint8_t addr7) {
    bus->current_addr7 = addr7;
}

HAL_I2cStatus HAL_I2c_Probe(HAL_I2cBus* bus, uint8_t addr7) {
    if (!bus) return HAL_I2C_EINVAL;
    // In mock world, only 0x48 exists.
    if (addr7 == 0x48) {
        return HAL_I2C_OK;
    }
    return HAL_I2C_ENODEV;
}

HAL_I2cStatus HAL_I2c_Write(HAL_I2cBus* bus,
                            uint8_t addr7,
                            const uint8_t* data_out,
                            size_t len)
{
    if (!bus || !data_out) return HAL_I2C_EINVAL;
    _set_addr(bus, addr7);

    // We only emulate addr 0x48
    if (addr7 != 0x48) {
        return HAL_I2C_ENODEV;
    }

    // Interpret certain writes as "control commands" to the mock sensor
    // Convention:
    //   [0xF0, new_raw12_lowbyte]  -> set mock temp directly
    // We won't emulate anything else for now.
    if (len >= 2 && data_out[0] == 0xF0) {
        uint8_t raw_lo = data_out[1];
        // We'll just slam raw_lo into lower 8 bits of temp_raw_12b,
        // upper 4 bits keep old value for fun
        g_sensor.temp_raw_12b = (g_sensor.temp_raw_12b & 0xF00) | raw_lo;
        OSAL_LOG("[I2C][MOCK] Write cmd 0xF0 -> temp_raw_12b now %u\r\n",
                 (unsigned)g_sensor.temp_raw_12b);
    } else {
        // For other writes, just log them.
        OSAL_LOG("[I2C][MOCK] Write addr=0x%02X, len=%u\r\n",
                 addr7, (unsigned)len);
    }

    return HAL_I2C_OK;
}

HAL_I2cStatus HAL_I2c_Read(HAL_I2cBus* bus,
                           uint8_t addr7,
                           uint8_t* data_in,
                           size_t len)
{
    if (!bus || !data_in) return HAL_I2C_EINVAL;
    _set_addr(bus, addr7);

    if (addr7 != 0x48) {
        // no such device in mock
        memset(data_in, 0xFF, len);
        return HAL_I2C_ENODEV;
    }

    // Raw read with no register pointer:
    // We'll respond with current temp bytes repeating.
    // First update temp to simulate drift
    _mock_update_temp();

    uint8_t tbuf[2];
    _encode_temp_bytes(g_sensor.temp_raw_12b, tbuf);

    for (size_t i = 0; i < len; ++i) {
        data_in[i] = tbuf[i % 2];
    }

    OSAL_LOG("[I2C][MOCK] Read addr=0x%02X len=%u -> temp_raw=%u\r\n",
             addr7, (unsigned)len, (unsigned)g_sensor.temp_raw_12b);

    return HAL_I2C_OK;
}

HAL_I2cStatus HAL_I2c_WriteReg8(HAL_I2cBus* bus,
                                uint8_t addr7,
                                uint8_t reg,
                                const uint8_t* data_out,
                                size_t len)
{
    if (!bus || !data_out) return HAL_I2C_EINVAL;
    _set_addr(bus, addr7);

    if (addr7 != 0x48) {
        return HAL_I2C_ENODEV;
    }

    // We'll interpret register writes:
    //   reg == 0xF0 -> set mock temp raw directly from first data byte
    if (reg == 0xF0 && len >= 1) {
        // overwrite low 8 bits
        g_sensor.temp_raw_12b = (g_sensor.temp_raw_12b & 0xF00) | data_out[0];
        OSAL_LOG("[I2C][MOCK] WriteReg8 0xF0=%u\r\n",
                 (unsigned)data_out[0]);
    } else {
        OSAL_LOG("[I2C][MOCK] WriteReg8 reg=0x%02X len=%u\r\n",
                 reg, (unsigned)len);
    }

    return HAL_I2C_OK;
}

HAL_I2cStatus HAL_I2c_ReadReg8(HAL_I2cBus* bus,
                               uint8_t addr7,
                               uint8_t reg,
                               uint8_t* data_in,
                               size_t len)
{
    if (!bus || !data_in) return HAL_I2C_EINVAL;
    _set_addr(bus, addr7);

    if (addr7 != 0x48) {
        memset(data_in, 0xEE, len);
        return HAL_I2C_ENODEV;
    }

    // We only emulate register 0x00/0x01 -> temperature bytes
    // (like reading the TMP102 temperature register starting at 0x00)
    // Flow for a real readReg8 is: [write reg][read len]
    // We'll do: update temp, encode bytes, then copy out starting at 'reg'.
    _mock_update_temp();

    uint8_t tbuf[2];
    _encode_temp_bytes(g_sensor.temp_raw_12b, tbuf);

    for (size_t i = 0; i < len; ++i) {
        uint8_t val = 0xFF;
        if (reg + i == 0x00) val = tbuf[0];
        else if (reg + i == 0x01) val = tbuf[1];
        else {
            // unknown register -> return 0xFF
        }
        data_in[i] = val;
    }

    OSAL_LOG("[I2C][MOCK] ReadReg8 reg=0x%02X len=%u -> raw=%u\r\n",
             reg, (unsigned)len, (unsigned)g_sensor.temp_raw_12b);

    return HAL_I2C_OK;
}
