
/**
 * @file demo_uart.c
 * @brief OSAL demo using the portable HAL UART on Linux.
 *
 * Creates two OSAL tasks:
 *  - UartTxTask: periodically transmits a text line
 *  - UartRxTask: polls the UART with a short timeout and logs any received data
 *
 * Hook this demo into your existing `app_linux.c` by calling DemoUart_Start().
 */
#include "osal.h"
#include "osal_task.h"
#include "hal_uart.h"
#include <stdio.h>
#include <string.h>

static HAL_Uart* s_uart = NULL;
static OSAL_TaskHandle s_tx = NULL;
static OSAL_TaskHandle s_rx = NULL;

/** Periodic TX: send a message every 1 second. */
static void UartTxTask(void* arg) {
    (void)arg;
    uint32_t tick = 0;
    for (;;) {
        char buf[128];
        int n = snprintf(buf, sizeof(buf), "[TX] Hello from OSAL UART! tick=%u\r\n", (unsigned)tick++);
        (void)HAL_Uart_Write(s_uart, buf, (size_t)n);
        OSAL_LOG("[UART][TX] wrote %d bytes\r\n", n);
        OSAL_TaskDelayMs(1000);
    }
}

/** RX loop: poll for up to 200 ms; if bytes arrive, dump them via OSAL_LOG. */
static void UartRxTask(void* arg) {
    (void)arg;
    char rbuf[128];
    for (;;) {
        long n = HAL_Uart_Read(s_uart, rbuf, sizeof(rbuf)-1, 200 /*ms*/);
        if (n > 0) {
            rbuf[n] = 0; // ensure zero-terminated for pretty logging
            OSAL_LOG("[UART][RX] %ld bytes: %s", n, rbuf);
        } else {
            // No data this cycle: yield a bit to keep system responsive
            OSAL_TaskDelayMs(50);
        }
    }
}

/**
 * @brief Entrypoint to start the UART demo.
 * @param dev          UART device path (e.g., "/dev/ttyPS1", "/dev/ttyUSB0")
 * @param baud         Baudrate (e.g., 115200)
 * @param non_blocking Open port with O_NONBLOCK if nonzero
 */
void DemoUart_Start(const char* dev, uint32_t baud, int non_blocking) {
    HAL_UartStatus st;
    HAL_UartConfig cfg = {
        .device = dev,
        .baud = baud,
        .data_bits = 8,
        .stop_bits = 1,
        .parity = HAL_UART_PARITY_NONE,
        .non_blocking = (uint8_t)(non_blocking ? 1 : 0),
        .hw_flow = 0
    };
    s_uart = HAL_Uart_Open(&cfg, &st);
    if (!s_uart) {
        OSAL_LOG("[DemoUART] open %s failed (%d)\r\n", dev, (int)st);
        return;
    }
    OSAL_LOG("[DemoUART] UART ready on %s @ %u bps\r\n", dev, (unsigned)baud);

    // Create two tasks (priorities are example values; adjust to your system)
    OSAL_TaskAttr a1 = { .name="UartTx", .stack_size=4096, .prio=20 };
    OSAL_TaskAttr a2 = { .name="UartRx", .stack_size=4096, .prio=22 };

    OSAL_TaskCreate(&s_tx, UartTxTask, NULL, &a1);
    OSAL_TaskCreate(&s_rx, UartRxTask, NULL, &a2);
}
