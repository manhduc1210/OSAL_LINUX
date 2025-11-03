/**
 * @file demo_oled_spi.c
 * @brief OSAL task demo for SPI OLED using HAL_SPI.
 *
 * Flow:
 *  - We construct an OledCtx with:
 *      + spi handle
 *      + callbacks to drive DC/RESET/CS pins
 *  - We init panel (reset + init seq)
 *  - We clear screen
 *  - We draw "OSAL OK" on row 0
 *
 * NOTE: Right now set_dc/set_rst/set_cs are stub callbacks that only log.
 * On real hardware you will route those to HAL_GPIO to actually toggle lines.
 */

#include "osal.h"
#include "osal_task.h"
#include "hal_spi.h"
#include "oled_spi_drv.h"

#include <stdio.h>

static HAL_SpiBus*     s_oled_spi = NULL;
static OSAL_TaskHandle s_oled_task;
static volatile int    s_oled_run = 0;

/* ---- Stub GPIO callbacks ----
 * Replace these with real HAL_GPIO control of
 * OLED_DC, OLED_RST, OLED_CS if those pins are on GPIO.
 */
static void OLED_SetDC(int level)
{
    OSAL_LOG("[OLED GPIO] DC=%d\r\n", level);
    // TODO: call HAL_Gpio_Write(dc_pin, level);
}

static void OLED_SetRST(int level)
{
    OSAL_LOG("[OLED GPIO] RST=%d\r\n", level);
    // TODO: call HAL_Gpio_Write(rst_pin, level);
}

static void OLED_SetCS(int level)
{
    OSAL_LOG("[OLED GPIO] CS=%d\r\n", level);
    // TODO: if your board doesn't use hw chip select,
    // manually assert/deassert here.
    //
    // If SPI controller already manages CS, you can leave this empty.
    (void)level;
}

static void OledTask(void* arg)
{
    (void)arg;

    OledCtx ctx = {
        .spi     = s_oled_spi,
        .set_dc  = OLED_SetDC,
        .set_rst = OLED_SetRST,
        .set_cs  = OLED_SetCS,
        .width   = 128,
        .height  = 64,
    };

    OSAL_LOG("[OLED DEMO] Task start\r\n");

    if (OLED_Init(&ctx) < 0) {
        OSAL_LOG("[OLED DEMO] OLED_Init failed\r\n");
    } else {
        OSAL_LOG("[OLED DEMO] OLED_Init OK\r\n");
    }

    // Main loop: update text repeatedly
    while (s_oled_run) {
        OLED_DrawTextRow0(&ctx, "OSAL OK");
        OSAL_TaskDelayMs(1000);
    }

    OSAL_LOG("[OLED DEMO] Task exit\r\n");
}

void DemoOledSpi_Start(HAL_SpiBus* spi_bus)
{
    s_oled_spi = spi_bus;
    s_oled_run = 1;

    OSAL_TaskAttr attr = {
        .name       = "OledTask",
        .prio       = 18,
        .stack_size = 2048
    };

    OSAL_TaskCreate(&s_oled_task, OledTask, NULL, &attr);
}

void DemoOledSpi_Stop(void)
{
    s_oled_run = 0;
    OSAL_TaskDelayMs(200);
}
