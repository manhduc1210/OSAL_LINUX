/**
 * @file hal_i2c_linux.c
 * @brief Linux backend for HAL I2C using /dev/i2c-X and I2C_SLAVE ioctl.
 *
 * Requires kernel i2c-dev driver enabled, so /dev/i2c-* exists.
 */

#include "hal_i2c.h"
#include "osal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>

#ifndef I2C_SLAVE
#include <linux/i2c-dev.h>   // most systems need this
#endif

struct HAL_I2cBus {
    int         fd;
    char        dev_name[64];
    uint32_t    bus_speed_hz;  // not enforced by Linux backend, just for info
};

/* internal: set current device address on this bus */
static HAL_I2cStatus _i2c_set_addr(struct HAL_I2cBus* bus, uint8_t addr7) {
    if (!bus) return HAL_I2C_EINVAL;
    if (ioctl(bus->fd, I2C_SLAVE, addr7) < 0) {
        OSAL_LOG("[I2C][LINUX] ioctl I2C_SLAVE 0x%02X failed (errno=%d)\r\n",
                 addr7, errno);
        return HAL_I2C_ENODEV;
    }
    return HAL_I2C_OK;
}

HAL_I2cBus* HAL_I2cBus_Open(const HAL_I2cBusConfig* cfg, HAL_I2cStatus* out_status) {
    if (!cfg || !cfg->bus_name || !cfg->bus_name[0]) {
        if (out_status) *out_status = HAL_I2C_EINVAL;
        return NULL;
    }

    HAL_I2cBus* bus = (HAL_I2cBus*)calloc(1, sizeof(*bus));
    if (!bus) {
        if (out_status) *out_status = HAL_I2C_EIO;
        return NULL;
    }

    int fd = open(cfg->bus_name, O_RDWR);
    if (fd < 0) {
        OSAL_LOG("[I2C][LINUX] open %s failed (errno=%d)\r\n",
                 cfg->bus_name, errno);
        free(bus);
        if (out_status) *out_status = HAL_I2C_EBUS;
        return NULL;
    }

    bus->fd = fd;
    strncpy(bus->dev_name, cfg->bus_name, sizeof(bus->dev_name)-1);
    bus->bus_speed_hz = cfg->bus_speed_hz;

    OSAL_LOG("[I2C][LINUX] opened bus %s (speed hint %u Hz)\r\n",
             bus->dev_name, (unsigned)bus->bus_speed_hz);

    if (out_status) *out_status = HAL_I2C_OK;
    return bus;
}

void HAL_I2cBus_Close(HAL_I2cBus* bus) {
    if (!bus) return;
    if (bus->fd >= 0) close(bus->fd);
    free(bus);
}

/**
 * @brief Probe: try to talk to addr7. We set the slave addr, then do a zero-length
 *               write or a 1-byte dummy read depending on kernel and device.
 * Simplest portable trick: set address + try a 1-byte read. Many devices NACK if no register,
 * but a pure NACK will show as read() < 0 with errno=EIO.
 * We'll interpret EIO as "no device".
 */
HAL_I2cStatus HAL_I2c_Probe(HAL_I2cBus* bus, uint8_t addr7) {
    if (!bus) return HAL_I2C_EINVAL;
    HAL_I2cStatus st = _i2c_set_addr(bus, addr7);
    if (st != HAL_I2C_OK) return st;

    uint8_t dummy;
    ssize_t r = read(bus->fd, &dummy, 1);
    if (r == 1 || r == 0) {
        // got something (some devices allow read without reg)
        return HAL_I2C_OK;
    } else {
        // Many devices NACK => read fails with EIO; treat as not present
        return HAL_I2C_ENODEV;
    }
}

HAL_I2cStatus HAL_I2c_Write(HAL_I2cBus* bus, uint8_t addr7,
                            const uint8_t* data_out, size_t len)
{
    if (!bus || !data_out) return HAL_I2C_EINVAL;
    HAL_I2cStatus st = _i2c_set_addr(bus, addr7);
    if (st != HAL_I2C_OK) return st;

    ssize_t w = write(bus->fd, data_out, len);
    if ((size_t)w != len) {
        OSAL_LOG("[I2C][LINUX] write addr 0x%02X len %u failed (errno=%d, wrote=%d)\r\n",
                 addr7, (unsigned)len, errno, (int)w);
        return HAL_I2C_EIO;
    }
    return HAL_I2C_OK;
}

HAL_I2cStatus HAL_I2c_Read(HAL_I2cBus* bus, uint8_t addr7,
                           uint8_t* data_in, size_t len)
{
    if (!bus || !data_in) return HAL_I2C_EINVAL;
    HAL_I2cStatus st = _i2c_set_addr(bus, addr7);
    if (st != HAL_I2C_OK) return st;

    ssize_t r = read(bus->fd, data_in, len);
    if ((size_t)r != len) {
        OSAL_LOG("[I2C][LINUX] read addr 0x%02X len %u failed (errno=%d, read=%d)\r\n",
                 addr7, (unsigned)len, errno, (int)r);
        return HAL_I2C_EIO;
    }
    return HAL_I2C_OK;
}

HAL_I2cStatus HAL_I2c_WriteReg8(HAL_I2cBus* bus, uint8_t addr7,
                                uint8_t reg,
                                const uint8_t* data_out, size_t len)
{
    if (!bus || !data_out) return HAL_I2C_EINVAL;

    // Build buffer: [reg][data...]
    uint8_t tmp[256];
    if (len + 1 > sizeof(tmp)) return HAL_I2C_EINVAL;

    tmp[0] = reg;
    memcpy(&tmp[1], data_out, len);

    return HAL_I2c_Write(bus, addr7, tmp, len + 1);
}

HAL_I2cStatus HAL_I2c_ReadReg8(HAL_I2cBus* bus, uint8_t addr7,
                               uint8_t reg,
                               uint8_t* data_in, size_t len)
{
    if (!bus || !data_in) return HAL_I2C_EINVAL;

    // Typical sequence:
    //   write() [reg]
    //   read() len bytes
    HAL_I2cStatus st = _i2c_set_addr(bus, addr7);
    if (st != HAL_I2C_OK) return st;

    // Send register index first
    ssize_t w = write(bus->fd, &reg, 1);
    if (w != 1) {
        OSAL_LOG("[I2C][LINUX] write(reg=0x%02X) addr 0x%02X failed (errno=%d, wrote=%d)\r\n",
                 reg, addr7, errno, (int)w);
        return HAL_I2C_EIO;
    }

    // Now read response
    ssize_t r = read(bus->fd, data_in, len);
    if ((size_t)r != len) {
        OSAL_LOG("[I2C][LINUX] read after reg 0x%02X addr 0x%02X len %u failed (errno=%d, read=%d)\r\n",
                 reg, addr7, (unsigned)len, errno, (int)r);
        return HAL_I2C_EIO;
    }
    return HAL_I2C_OK;
}
