/**
 * @file demo_i2c_temp_mock.c
 * @brief Demo task that talks to the mock I2C sensor at addr 0x48
 *        using the generic HAL_I2c_* API.
 *
 * This proves the HAL API works end-to-end even without real hardware.
 */

#include "hal_i2c.h"
#include "osal.h"
#include "osal_task.h"
#include <stdint.h>

static HAL_I2cBus*     s_bus   = NULL;
static OSAL_TaskHandle s_task  = NULL;
static volatile int    s_run   = 0;

static float _decode_temp_c_from_raw12bytes(uint8_t hi, uint8_t lo)
{
    // we encoded temp like TMP102: raw12 << 4 in [hi:lo]
    // => raw12 = ((hi << 8) | lo) >> 4
    uint16_t v16 = ((uint16_t)hi << 8) | lo;
    uint16_t raw12 = (v16 >> 4) & 0x0FFF;
    // interpret as positive only for simplicity
    return raw12 * 0.0625f; // each LSB = 0.0625C
}

static void I2cMockTask(void* arg)
{
    (void)arg;
    uint32_t tick_ms = 0;
    uint32_t last_log = 0;
    uint32_t last_kick = 0;

    // 1) "probe" device 0x48
    if (HAL_I2c_Probe(s_bus, 0x48) == HAL_I2C_OK) {
        OSAL_LOG("[I2C MOCK DEMO] Found mock device at 0x48\r\n");
    } else {
        OSAL_LOG("[I2C MOCK DEMO] Device 0x48 not found (shouldn't happen in mock)\r\n");
    }

    while (s_run) {
        OSAL_TaskDelayMs(100);
        tick_ms += 100;

        // Every 1000ms: read register 0x00..0x01 = temperature
        if ((tick_ms - last_log) >= 1000) {
            last_log = tick_ms;

            uint8_t raw[2] = {0};
            HAL_I2cStatus st = HAL_I2c_ReadReg8(s_bus, 0x48, 0x00, raw, 2);
            if (st == HAL_I2C_OK) {
                float tempC = _decode_temp_c_from_raw12bytes(raw[0], raw[1]);
                OSAL_LOG("[I2C MOCK DEMO] Temp mock = %.2f C (raw:0x%02X 0x%02X)\r\n",
                         tempC, raw[0], raw[1]);
            } else {
                OSAL_LOG("[I2C MOCK DEMO] ReadReg8 failed (%d)\r\n", (int)st);
            }
        }

        // Every 10 seconds: write a control reg 0xF0 to "kick" sensor
        // This simulates configuration/calibration write using HAL_I2c_WriteReg8.
        if ((tick_ms - last_kick) >= 10000) {
            last_kick = tick_ms;
            uint8_t new_lowbyte = 0x20; // arbitrary tweak for mock
            HAL_I2cStatus st2 = HAL_I2c_WriteReg8(s_bus, 0x48, 0xF0, &new_lowbyte, 1);
            OSAL_LOG("[I2C MOCK DEMO] WriteReg8 0xF0=0x%02X status=%d\r\n",
                     new_lowbyte, (int)st2);
        }
    }

    OSAL_LOG("[I2C MOCK DEMO] Task exit\r\n");
}

void DemoI2cTempMock_Start(void)
{
    HAL_I2cStatus st;
    HAL_I2cBusConfig cfg = {
        .bus_name     = "mock-bus-0",
        .bus_speed_hz = 100000
    };
    s_bus = HAL_I2cBus_Open(&cfg, &st);
    if (!s_bus) {
        OSAL_LOG("[I2C MOCK DEMO] Bus open failed (%d)\r\n", (int)st);
        return;
    }

    s_run = 1;

    OSAL_TaskAttr a = {
        .name       = "I2cMock",
        .stack_size = 2048,
        .prio       = 20
    };
    OSAL_TaskCreate(&s_task, I2cMockTask, NULL, &a);

    OSAL_LOG("[I2C MOCK DEMO] started\r\n");
}

void DemoI2cTempMock_Stop(void)
{
    s_run = 0;
    OSAL_TaskDelayMs(200);

    if (s_bus) {
        HAL_I2cBus_Close(s_bus);
        s_bus = NULL;
    }

    OSAL_LOG("[I2C MOCK DEMO] stopped\r\n");
}
