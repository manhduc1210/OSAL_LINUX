/**
 * @file hal_i2c.h
 * @brief Portable I2C HAL API (OS-agnostic). Backend on Linux uses /dev/i2c-*.
 *
 * Goals:
 *  - Open a bus (e.g. "/dev/i2c-0")
 *  - Talk to slave devices by 7-bit address
 *  - Support common register read/write patterns
 *  - Allow raw write+read transactions
 */

#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct HAL_I2cBus HAL_I2cBus;

typedef enum {
    HAL_I2C_OK = 0,
    HAL_I2C_EINVAL,
    HAL_I2C_EIO,
    HAL_I2C_ENODEV,
    HAL_I2C_EBUS
} HAL_I2cStatus;

/**
 * @brief Bus config
 * bus_name is backend-specific string. On Linux it's "/dev/i2c-0", "/dev/i2c-1", ...
 * In other OS it could be an index or pointer to controller registers.
 */
typedef struct {
    const char* bus_name;
    uint32_t    bus_speed_hz;   // optional hint; Linux backend may ignore
} HAL_I2cBusConfig;

/**
 * @brief Open an I2C bus for communication.
 * Return NULL on failure.
 */
HAL_I2cBus* HAL_I2cBus_Open(const HAL_I2cBusConfig* cfg, HAL_I2cStatus* out_status);

/**
 * @brief Close a bus handle.
 */
void HAL_I2cBus_Close(HAL_I2cBus* bus);

/**
 * @brief Probe a 7-bit address. Returns HAL_I2C_OK if device ACKs.
 */
HAL_I2cStatus HAL_I2c_Probe(HAL_I2cBus* bus, uint8_t addr7);

/**
 * @brief Write raw bytes to a device (no register index). Typical for streaming DACs, etc.
 * data_out: pointer to bytes to send
 * len: number of bytes to send
 */
HAL_I2cStatus HAL_I2c_Write(HAL_I2cBus* bus, uint8_t addr7,
                            const uint8_t* data_out, size_t len);

/**
 * @brief Read raw bytes from a device (no internal register index first).
 * data_in: buffer to fill
 * len: number of bytes to read
 */
HAL_I2cStatus HAL_I2c_Read(HAL_I2cBus* bus, uint8_t addr7,
                           uint8_t* data_in, size_t len);

/**
 * @brief Common "register write": write [reg_addr, data...] in one shot.
 * Example: configure a sensor register.
 */
HAL_I2cStatus HAL_I2c_WriteReg8(HAL_I2cBus* bus, uint8_t addr7,
                                uint8_t reg,
                                const uint8_t* data_out, size_t len);

/**
 * @brief Common "register read": write [reg_addr] then read N bytes.
 * Typical for sensors where you read from a register map.
 */
HAL_I2cStatus HAL_I2c_ReadReg8(HAL_I2cBus* bus, uint8_t addr7,
                               uint8_t reg,
                               uint8_t* data_in, size_t len);

/**
 * @brief Convenience: read one 8-bit register.
 */
static inline HAL_I2cStatus HAL_I2c_ReadReg8_U8(HAL_I2cBus* bus,
                                                uint8_t addr7,
                                                uint8_t reg,
                                                uint8_t* out_val)
{
    return HAL_I2c_ReadReg8(bus, addr7, reg, out_val, 1);
}

/**
 * @brief Convenience: write one 8-bit register.
 */
static inline HAL_I2cStatus HAL_I2c_WriteReg8_U8(HAL_I2cBus* bus,
                                                 uint8_t addr7,
                                                 uint8_t reg,
                                                 uint8_t val)
{
    return HAL_I2c_WriteReg8(bus, addr7, reg, &val, 1);
}

#ifdef __cplusplus
}
#endif
