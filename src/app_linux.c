// // app_linux.c (tối giản, giữ nguyên Demo1_Start)
// #include "osal_task.h"
// #include "osal.h"
// #include <stdio.h>
// #include "board_led.h"

// void Demo1_Start(void);

// int main(void) {
//     OSAL_LinuxCtx linux_ctx = {
//         .uio_path = "/dev/uio0",
//         .map_size = 0x1000
//     };
//     OSAL_Config cfg = {
//         .backend = OSAL_BACKEND_LINUX,
//         .log = printf,              // log ra stdout
//         .platform_ctx = &linux_ctx  // truyền UIO ctx cho BoardLed
//     };
//     if (OSAL_Init(&cfg) != OSAL_OK) { printf("OSAL_Init failed\n"); return -1; }

//     printf("\n=== Demo 1: Blink + Log via OSAL (Linux) ===\n");
//     Demo1_Start();

//     // Giữ tiến trình sống: tuỳ bạn join các thread, hoặc sleep vô hạn
//     for(;;) pause();
//     return 0;
// }

#include "osal_task.h"
#include "osal.h"
#include <stdio.h>

void Demo1_Start(void);

int main(void) {
    OSAL_Config cfg = {
        .backend = OSAL_BACKEND_LINUX,
        .log = printf,          // log ra stdout
        .platform_ctx = NULL    // MOCK: không cần gì thêm
    };
    if (OSAL_Init(&cfg) != OSAL_OK) {
        printf("OSAL_Init failed\n");
        return -1;
    }

    printf("\n=== Demo 1: Blink + Log via OSAL (Linux MOCK) ===\n");
    Demo1_Start();

    // Giữ tiến trình sống (tuỳ bạn)
    for(;;);
    return 0;
}
