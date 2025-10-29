
/**
 * @file demo_uart.c
 * @brief UART line-echo demo with clean shutdown support.
 *
 * Behavior:
 *  - Accumulates RX into a line buffer; on Enter (CR/LF), echoes the line back.
 *  - Supports backspace editing (BS/DEL).
 *  - Provides DemoUart_Stop() to gracefully exit (used by SIGINT handler in app).
 */

#include "osal.h"
#include "osal_task.h"
#include "hal_uart.h"
#include <stdio.h>
#include <string.h>
#include <signal.h> 

#define LINE_MAX 256

static HAL_Uart*       s_uart    = NULL;
static OSAL_TaskHandle s_rx      = NULL;
static volatile int    s_running = 0;

static void request_app_exit_via_sigint(void) {
    raise(SIGINT);
}

static void UartRxTask(void* arg) {
    (void)arg;

    char   line[LINE_MAX];
    size_t len = 0;
    int    last_was_cr_or_lf = 0; // collapse CRLF/LFCR

    while (s_running) {
        // Small buffer for chunked reads
        char buf[64];
        long n = HAL_Uart_Read(s_uart, buf, sizeof(buf), 50 /*ms*/);

        if (n <= 0) {
            // No data this cycle → yield a bit to keep system responsive
            OSAL_TaskDelayMs(10);
            continue;
        }

        for (long i = 0; i < n; ++i) {
            unsigned char c = (unsigned char)buf[i];
            if (!s_running) break;

            // === Detect Ctrl-C coming from the UART peer ===
            if (c == 0x03) {
                HAL_Uart_WriteString(s_uart,
                    "\r\n[INFO] UART Ctrl+C received. Exiting...\r\n");
                OSAL_LOG("[UART] Ctrl+C (0x03) received on UART. Requesting app exit.\r\n");
                s_running = 0;               // stop this task loop
                // request_app_exit_via_sigint(); // poke main to exit
                break;
            }

            // CR/LF handling
            if (c == '\r' || c == '\n') {
                if (last_was_cr_or_lf) { last_was_cr_or_lf = 0; continue; }
                last_was_cr_or_lf = 1;

                line[len] = 0;
                if (len > 0) {
                    // Echo the line back to the same UART
                    // (you will see it on your PC terminal connected to ttyPS1)
                    HAL_Uart_WriteString(s_uart, "\r\n[ECHO] ");
                    HAL_Uart_Write(s_uart, line, len);
                    HAL_Uart_WriteString(s_uart, "\r\n");

                    // Also log to the board console (ttyPS0)
                    // OSAL_LOG("[UART][LINE] len=%u: %s\r\n", (unsigned)len, line);
                } else {
                    // Empty line → just send newline to keep terminal tidy
                    HAL_Uart_WriteString(s_uart, "\r\n");
                }

                // Reset for next line
                len = 0;
                continue;
            } else {
                last_was_cr_or_lf = 0;
            }

            // Backspace / DEL
            if (c == 0x08 || c == 0x7F) {
                if (len > 0) {
                    len--;
                    // Optional: send backspace sequence to update remote terminal
                    // (move cursor left, erase char, move cursor left)
                    HAL_Uart_WriteString(s_uart, "\b \b");
                }
                continue;
            }

            // Append printable
            if (len < (LINE_MAX - 1)) {
                line[len++] = (char)c;
                // local echo while typing
                HAL_Uart_Write(s_uart, &c, 1);
            } else {
                line[len] = 0;
                HAL_Uart_WriteString(s_uart, "\r\n[WARN] line too long, flushed\r\n");
                OSAL_LOG("[UART][WARN] line buffer overflow, flushing\r\n");
                len = 0;
            }
        }
    }

    OSAL_LOG("[UART] RX task exiting...\r\n");
    request_app_exit_via_sigint();
}

void DemoUart_Start(const char* dev, uint32_t baud, int non_blocking) {
    HAL_UartStatus st;
    HAL_UartConfig cfg = {
        .device       = dev,
        .baud         = baud,
        .data_bits    = 8,
        .stop_bits    = 1,
        .parity       = HAL_UART_PARITY_NONE,
        .non_blocking = (uint8_t)(non_blocking ? 1 : 0),
        .hw_flow      = 0
    };

    // Fix typo in enum name (correct one is HAL_Uart_PARITY_NONE? or HAL_UART_PARITY_NONE)
    // Using HAL_UART_PARITY_NONE:
    cfg.parity = HAL_UART_PARITY_NONE;

    s_uart = HAL_Uart_Open(&cfg, &st);
    if (!s_uart) {
        OSAL_LOG("[DemoUART] open %s failed (%d)\r\n", dev, (int)st);
        return;
    }
    OSAL_LOG("[DemoUART] UART ready on %s @ %u bps\r\n", dev, (unsigned)baud);

    s_running = 1;
    OSAL_TaskAttr arx = { .name="UartRx", .stack_size=4096, .prio=22 };
    OSAL_TaskCreate(&s_rx, UartRxTask, NULL, &arx);

    HAL_Uart_WriteString(s_uart, "\r\n[INFO] Line-echo mode. Type text and press Enter.\r\n"
                                  "[INFO] Press Ctrl+C in console to exit.\r\n");
}

/**
 * @brief Stop demo and release resources. Safe to call from SIGINT handler.
 */
void DemoUart_Stop(void) {
    if (!s_uart) return;
    s_running = 0;
    // Give RX task a moment to break out of loops
    OSAL_TaskDelayMs(100);
    HAL_Uart_WriteString(s_uart, "\r\n[INFO] Stopping UART demo...\r\n");
    HAL_Uart_Close(s_uart);
    s_uart = NULL;
}
