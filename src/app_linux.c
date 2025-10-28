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

// === Forward declarations for demos ===
void DemoUart_Start(const char* dev, uint32_t baud, int nb);  // from demo_uart.c

int main(void) {
    printf("=== OSAL Linux Demo App ===\r\n");

    // 1. Configure OSAL for Linux backend
    OSAL_Config cfg = {
        .backend = OSAL_BACKEND_LINUX,
        .log     = printf,
        .platform_ctx = NULL
    };

    if (OSAL_Init(&cfg) != OSAL_OK) {
        printf("[ERROR] OSAL_Init failed!\r\n");
        return -1;
    }

    // 2. Start LED demo (optional)
    // Uncomment if you want to see LED blinking as well
    // DemoBlink_Start();

    // 3. Start UART HAL demo
    //    - Device: "/dev/ttyPS1" (ZedBoard's second UART port)
    //    - Baud:   115200
    //    - Non-blocking open: 0 (blocking)
    DemoUart_Start("/dev/ttyPS0", 115200, 0);

    // 4. Let OSAL tasks run indefinitely
    //    In Linux backend, tasks are POSIX threads. We can just sleep forever.
    for (;;) {
        OSAL_TaskDelayMs(5000);
    }

    return 0;
}
