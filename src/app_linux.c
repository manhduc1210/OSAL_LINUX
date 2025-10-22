#include "osal_task.h"
#include "osal.h"
#include <stdio.h>

typedef struct {
    const char* chip_name;   // "gpiochip0"
    unsigned    line_base;   // 0 nếu LED ở CH1; 32 nếu ở CH2
    unsigned    count;       // 8
} OSAL_GpiodCtx;

void Demo1_Start(void);

int main(void) {
    OSAL_GpiodCtx ctx = {
        .chip_name = "gpiochip0",
        .line_base = 0,   // đổi thành 32 nếu LED nằm ở Channel 2
        .count     = 8
    };
    OSAL_Config cfg = {
        .backend = OSAL_BACKEND_LINUX,
        .log = printf,
        .platform_ctx = &ctx
    };
    if (OSAL_Init(&cfg) != OSAL_OK) { printf("OSAL_Init failed\n"); return -1; }

    printf("\n=== Demo 1: Blink + Log via OSAL (Linux + libgpiod) ===\n");
    Demo1_Start();
    for(;;) pause();
    return 0;
}
