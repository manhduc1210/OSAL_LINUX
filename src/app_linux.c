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
#include "demo_gpio_hal.h"

// === Forward declarations for demos ===
void DemoUart_Start(const char* dev, uint32_t baud, int nb);  // from demo_uart.c
void DemoUart_Stop(void);                                     // stop/cleanup
void DemoGpio_Start(const DemoGpioCfg* cfg);

// === SIGINT handler (Ctrl+C) ===
static volatile sig_atomic_t g_stop_requested = 0;
static void on_sigint(int sig) { 
    (void)sig; 
    g_stop_requested = 1; 
}

int main(void) {
    printf("=== OSAL Linux Demo App (Ctrl+C to exit) ===\n");

    // Install Ctrl+C handler
    // signal(SIGINT, on_sigint);

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
    // DemoUart_Start("/dev/ttyPS0", 115200, 0);

    // TODO: Fill offsets from your board (use `gpioinfo`)
    // DemoGpioCfg gpio_cfg = {
    //     .chip_name = "gpiochip0",
    //     .led_offsets = { /* LSB..MSB */ 0,1,2,3,4,5,6,7 }, // ví dụ
    //     .led_count = 8,
    //     .btn0_offset = 8,   // ví dụ BTN0
    //     .btn1_offset = 9,   // ví dụ BTN1
    //     .leds_active_low = 0,
    //     .btns_active_low = 1,  // thường nút kéo-up: pressed=0
    //     .debounce_ms = 10
    // };
    // DemoGpio_Start(&gpio_cfg);

    DemoI2cTemp_Start("/dev/i2c-0");

    // 4. Let OSAL tasks run indefinitely
    //    In Linux backend, tasks are POSIX threads. We can just sleep forever.
    // while (!g_stop_requested) {
    //     OSAL_TaskDelayMs(200);
    // }
    for (;;) OSAL_TaskDelayMs(1000);
    // printf("\n[APP] Ctrl+C detected. Stopping...\n");
    // DemoUart_Stop();
    // printf("[APP] Exit.\n");

    return 0;
}
