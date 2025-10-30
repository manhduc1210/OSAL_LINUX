/**
 * @file demo_i2c_expander_counter.c
 * @brief OSAL demo: BTN0++ , BTN1=reset , LEDs show counter (via I2C expander)
 */

#include "expander_drv.h"
#include "osal.h"
#include "osal_task.h"
#include <stdio.h>

static Expander        s_io;
static OSAL_TaskHandle s_task;
static volatile int    s_run = 0;

#define BTN0_MASK (1u << 0)
#define BTN1_MASK (1u << 1)
#define POLL_MS   20

static void I2cExpanderTask(void* arg)
{
    (void)arg;
    uint8_t last_state = 0xFF;
    uint8_t counter    = 0;

    Expander_Init(&s_io);
    Expander_WriteOutputs(&s_io, 0x00);

    OSAL_LOG("[I2C-GPIO] Task started (BTN0=inc, BTN1=reset)\r\n");

    while (s_run) {
        OSAL_TaskDelayMs(POLL_MS);

        uint8_t curr;
        if (Expander_ReadInputs(&s_io, &curr) != HAL_I2C_OK)
            continue;

        // Active-low buttons
        uint8_t pressed = (~curr) & 0x03;

        // BTN0 edge detect
        if ((pressed & BTN0_MASK) && !(~last_state & BTN0_MASK)) {
            if (counter < 255) counter++;
        }

        // BTN1 edge detect
        if ((pressed & BTN1_MASK) && !(~last_state & BTN1_MASK)) {
            counter = 0;
        }

        Expander_WriteOutputs(&s_io, counter);
        last_state = curr;

        OSAL_LOG("[I2C-GPIO] Cnt=%3u BTN0=%u BTN1=%u\r\n",
                 counter,
                 !!(pressed & BTN0_MASK),
                 !!(pressed & BTN1_MASK));
    }

    Expander_WriteOutputs(&s_io, 0x00);
    OSAL_LOG("[I2C-GPIO] Task exit\r\n");
}

void DemoI2cExpander_Start(HAL_I2cBus* bus, uint8_t addr7)
{
    s_io.bus   = bus;
    s_io.addr7 = addr7;
    s_run      = 1;

    OSAL_TaskAttr a = { .name = "I2CExpander", .stack_size = 2048, .prio = 20 };
    OSAL_TaskCreate(&s_task, I2cExpanderTask, NULL, &a);
}

void DemoI2cExpander_Stop(void)
{
    s_run = 0;
    OSAL_TaskDelayMs(100);
}
