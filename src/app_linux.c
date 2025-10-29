/**
 * @file app_linux.c
 * @brief Main entry for OSAL Linux demo application.
 *
 * This file initializes OSAL on Linux, then starts demo modules.
 * You can enable/disable demos (e.g., LED blink, UART HAL) from here.
 */

#include "osal.h"
#include "osal_task.h"
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <stdarg.h>

// === Forward declarations for demos ===
void DemoUart_Start(const char* dev, uint32_t baud, int nb);  // from demo_uart.c
void DemoUart_Stop(void);                                     // stop/cleanup

// === SIGINT handler (Ctrl+C) ===
static volatile sig_atomic_t g_stop_requested = 0;
static void on_sigint(int sig) { 
    (void)sig; 
    g_stop_requested = 1; 
}

int main(void) {
    printf("=== OSAL Linux Demo App (Ctrl+C to exit) ===\n");

    // Install Ctrl+C handler
    signal(SIGINT, on_sigint);

    // 1) OSAL init
    OSAL_Config cfg = {
        .backend = OSAL_BACKEND_LINUX,
        .log     = printf,
        .platform_ctx = NULL
    };
    if (OSAL_Init(&cfg) != OSAL_OK) {
        printf("[ERROR] OSAL_Init failed!\n");
        return -1;
    }

    // 3. Start UART HAL demo
    //    - Device: "/dev/ttyPS1" (ZedBoard's second UART port)
    //    - Baud:   115200
    //    - Non-blocking open: 0 (blocking)
    DemoUart_Start("/dev/ttyPS0", 115200, 0);

    // 4. Let OSAL tasks run indefinitely
    //    In Linux backend, tasks are POSIX threads. We can just sleep forever.
    while (!g_stop_requested) {
        OSAL_TaskDelayMs(200);
    }

    printf("\n[APP] Ctrl+C detected. Stopping...\n");
    // DemoUart_Stop();
    printf("[APP] Exit.\n");

    return 0;
}
