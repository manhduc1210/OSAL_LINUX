/**
 * @file demo_i2c_temp.c
 * @brief I2C demo (mock sensor):
 *        - Open I2C bus via HAL_I2c
 *        - "Scan" bus logically
 *        - Periodically read a mock temperature instead of real hardware
 *
 * Behavior:
 *   - Every 1 second, we log current "temperature"
 *   - Every 5 seconds, temperature changes (ramps up, wraps around)
 *
 * Integration:
 *   Call DemoI2cTemp_Start("/dev/i2c-0") from app_linux.c (or whichever bus).
 *   No physical sensor is actually accessed.
 */

#include "hal_i2c.h"
#include "osal.h"
#include "osal_task.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static HAL_I2cBus*     s_bus   = NULL;     // I2C bus handle (can still be opened)
static OSAL_TaskHandle s_task  = NULL;     // OSAL task handle
static volatile int    s_run   = 0;        // run flag

// mock sensor state
static float    s_mock_temp_c      = 25.0f;   // start temp
static uint32_t s_mock_tick_ms     = 0;       // ms counter since start
static uint32_t s_mock_last_stepms = 0;       // last time we "changed temp"

// This function simulates temperature behavior instead of reading a real sensor.
// - Temperature drifts upward by +0.5 °C every 5 seconds, until 30 °C
// - Then it jumps back down to ~25 °C and repeats.
static void MockSensor_Update(uint32_t now_ms)
{
    const uint32_t STEP_PERIOD_MS = 5000; // 5 seconds
    if ((now_ms - s_mock_last_stepms) >= STEP_PERIOD_MS) {
        s_mock_last_stepms = now_ms;

        // increase temperature gently
        s_mock_temp_c += 0.5f;
        if (s_mock_temp_c > 30.0f) {
            s_mock_temp_c = 25.0f; // wrap/restart
        }
    }
}

// The I2C demo task:
// - (Optionally) scan bus on startup for illustrative logging
// - Loop forever while s_run == 1
// - Update mock temperature model based on elapsed ms
// - Print temperature once per second
static void I2cTask(void* arg)
{
    (void)arg;

    // --------------------------------------------------------------------
    // 1) "Scan" the bus conceptually. We won't probe hardware now because
    //    there's maybe no physical sensor and we don't want spam errors.
    //    But we still log which /dev/i2c-* we believe we're using.
    // --------------------------------------------------------------------
    OSAL_LOG("[I2C DEMO] mock-mode active (no real sensor)\r\n");

    // --------------------------------------------------------------------
    // 2) Main loop: update mock temperature, log once per second
    // --------------------------------------------------------------------
    uint32_t last_log_ms = 0;

    while (s_run) {
        // A millisecond-ish tick for our mock logic
        // We don't have a global millisecond tick in this file,
        // so we'll fake it by accumulating delay ourselves.
        OSAL_TaskDelayMs(100); // sleep ~100 ms each iteration
        s_mock_tick_ms += 100;

        // update mock sensor model every ~5 seconds
        MockSensor_Update(s_mock_tick_ms);

        // log once per 1000 ms
        if ((s_mock_tick_ms - last_log_ms) >= 1000) {
            last_log_ms = s_mock_tick_ms;

            OSAL_LOG("[I2C DEMO] mock Temp = %.2f C\r\n", s_mock_temp_c);
            // This is where you'd normally also:
            //  - send temp over UART
            //  - show on OLED
            //  - publish to shared data structure, etc.
        }
    }

    OSAL_LOG("[I2C DEMO] task exit\r\n");
}

// Public API: Start the demo
// bus_name: e.g. "/dev/i2c-0". In mock mode we still try to open the bus,
//           because we want to demonstrate HAL_I2cBus_Open() path.
//           If the bus doesn't exist (e.g. no /dev/i2c-0), we just log an error
//           and continue with mock anyway.
void DemoI2cTemp_Start(const char* bus_name)
{
    HAL_I2cStatus st;

    // Try to open bus. If it fails (no /dev/i2c-*), we still run mock task.
    HAL_I2cBusConfig cfg = {
        .bus_name     = bus_name,   // "/dev/i2c-0" or "/dev/i2c-1" etc.
        .bus_speed_hz = 100000      // hint only
    };

    s_bus = HAL_I2cBus_Open(&cfg, &st);
    if (!s_bus) {
        OSAL_LOG("[I2C DEMO] WARNING: cannot open %s (%d). Running pure mock.\r\n",
                 bus_name ? bus_name : "(null)", (int)st);
    } else {
        OSAL_LOG("[I2C DEMO] I2C bus opened: %s\r\n", bus_name);
    }

    // init mock state
    s_mock_temp_c      = 25.0f;
    s_mock_tick_ms     = 0;
    s_mock_last_stepms = 0;

    s_run = 1;
    OSAL_TaskAttr a = {
        .name       = "I2cTempMock",
        .stack_size = 2048,
        .prio       = 20
    };
    OSAL_TaskCreate(&s_task, I2cTask, NULL, &a);

    OSAL_LOG("[I2C DEMO] started mock temperature task\r\n");
}

// Public API: Stop the demo, release bus if opened
void DemoI2cTemp_Stop(void)
{
    s_run = 0;
    OSAL_TaskDelayMs(150); // small grace period for task to exit log

    if (s_bus) {
        HAL_I2cBus_Close(s_bus);
        s_bus = NULL;
    }

    OSAL_LOG("[I2C DEMO] stopped\r\n");
}
