/**
 * @file demo_spi_basic.c
 * @brief Very small SPI HAL demo with optional MOCK backend.
 *
 * Steps:
 *  1) Open bus (HW: "/dev/spidev0.0", or MOCK: "mock:SEED")
 *  2) Write 4 bytes via HAL_Spi_Write()
 *  3) Read  8 bytes via HAL_Spi_Read()  (mock returns from seed / appended data)
 *  4) Get bus info via HAL_Spi_GetInfo()
 *  5) Close
 *
 * Build modes:
 *   - HW test:   set SPIDEV_PATH "/dev/spidev0.0" (and loop MOSI->MISO if you want echo)
 *   - MOCK test: set SPIDEV_PATH "mock:HelloMock" to preload buffer
 */

#include "osal.h"
#include "hal_spi.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#ifndef SPIDEV_PATH
//#define SPIDEV_PATH "/dev/spidev0.0"
#define SPIDEV_PATH "mock:HelloMock"
#endif

void Demo_Spi_Basic(void)
{
    printf("[DEMO] SPI basic demo start\n");

    /* 1) Open bus */
    HAL_SpiStatus st;
    HAL_SpiConfig cfg = {
        .dev_name      = SPIDEV_PATH,        // HW: "/dev/spidev0.0"  |  MOCK: "mock:seed"
        .mode          = HAL_SPI_MODE0,
        .max_speed_hz  = 1000000,
        .bits_per_word = 8,
        .lsb_first     = 0
    };
    HAL_SpiBus* bus = HAL_Spi_Open(&cfg, &st);
    if (!bus || st != HAL_SPI_OK) {
        printf("[DEMO] HAL_Spi_Open failed (%d)\n", st);
        return;
    }
    printf("[DEMO] Open OK on %s\n", SPIDEV_PATH);

    /* 2) Write 4 bytes */
    uint8_t tx[4] = { 0x11, 0x22, 0x33, 0x44 };
    st = HAL_Spi_Write(bus, tx, sizeof(tx));
    printf("[DEMO] Write %zu bytes -> %s\n", sizeof(tx), (st==HAL_SPI_OK)?"OK":"FAIL");

    /* 3) Read 8 bytes */
    uint8_t rx[8] = {0};
    st = HAL_Spi_Read(bus, rx, sizeof(rx));
    printf("[DEMO] Read %zu bytes -> %s\n", sizeof(rx), (st==HAL_SPI_OK)?"OK":"FAIL");
    if (st == HAL_SPI_OK) {
        printf("[DEMO] RX: ");
        for (size_t i=0;i<sizeof(rx);++i) printf("%02X ", rx[i]);
        printf("\n");
    }

    /* 4) Get bus info */
    HAL_SpiInfo info;
    st = HAL_Spi_GetInfo(bus, &info);
    if (st == HAL_SPI_OK) {
        printf("[DEMO] Info: name=%.*s mode=%u bpw=%u lsb=%u speed=%u\n",
            (int)sizeof(info.name), info.name, info.mode, info.bits_per_word,
            info.lsb_first, info.speed_hz);
    } else {
        printf("[DEMO] GetInfo failed (%d)\n", st);
    }

    /* 5) Close */
    HAL_Spi_Close(bus);
    printf("[DEMO] Close done. SPI basic demo end.\n");
}
