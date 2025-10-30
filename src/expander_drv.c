#include "expander_drv.h"
#include "osal.h"
#include <string.h>

/* --- MCP23008 register map (simplified) --- */
#define REG_IODIR   0x00  // 1=input, 0=output
#define REG_GPIO    0x09  // read = inputs
#define REG_OLAT    0x0A  // write = outputs

HAL_I2cStatus Expander_Init(Expander* io)
{
    if (!io || !io->bus) return HAL_I2C_EINVAL;

    // Configure direction: 0b00000011 => bit0,1 input; bit2-7 output
    uint8_t dir = 0x03;
    HAL_I2cStatus st = HAL_I2c_WriteReg8(io->bus, io->addr7, REG_IODIR, &dir, 1);
    if (st != HAL_I2C_OK) return st;

    // Clear outputs
    uint8_t zero = 0x00;
    st = HAL_I2c_WriteReg8(io->bus, io->addr7, REG_OLAT, &zero, 1);
    OSAL_LOG("[EXPANDER] init IODIR=0x%02X OLAT=0x00 status=%d\r\n", dir, (int)st);
    return st;
}

HAL_I2cStatus Expander_ReadInputs(Expander* io, uint8_t* out_state)
{
    if (!io || !out_state) return HAL_I2C_EINVAL;
    return HAL_I2c_ReadReg8(io->bus, io->addr7, REG_GPIO, out_state, 1);
}

HAL_I2cStatus Expander_WriteOutputs(Expander* io, uint8_t value)
{
    if (!io) return HAL_I2C_EINVAL;
    uint8_t val = value & 0xFC; // mask out bit0-1 (inputs)
    return HAL_I2c_WriteReg8(io->bus, io->addr7, REG_OLAT, &val, 1);
}
