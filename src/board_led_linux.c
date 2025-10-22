// // board_led_linux.c
// #include "board_led.h"
// #include "osal.h"
// #include <stdint.h>
// #include <fcntl.h>
// #include <sys/mman.h>
// #include <unistd.h>
// #include <string.h>

// static volatile uint32_t* gpio_base = NULL;
// static int uio_fd = -1;

// #define GPIO_DATA_CH1   0x00
// #define GPIO_TRI_CH1    0x04

// void BoardLed_Init(void){
//     if (!g_osal.cfg.platform_ctx) { OSAL_LOG("[LED] missing platform_ctx\r\n"); return; }
//     OSAL_LinuxCtx* ctx = (OSAL_LinuxCtx*)g_osal.cfg.platform_ctx;

//     uio_fd = open(ctx->uio_path, O_RDWR);
//     if (uio_fd < 0) { OSAL_LOG("[LED] open %s failed\r\n", ctx->uio_path); return; }

//     gpio_base = (uint32_t*)mmap(NULL, ctx->map_size, PROT_READ|PROT_WRITE, MAP_SHARED, uio_fd, 0);
//     if (gpio_base == MAP_FAILED) { OSAL_LOG("[LED] mmap failed\r\n"); gpio_base=NULL; close(uio_fd); uio_fd=-1; return; }

//     // Set all outputs + clear
//     gpio_base[GPIO_TRI_CH1/4] = 0x00000000u;
//     gpio_base[GPIO_DATA_CH1/4]= 0x00000000u;
//     OSAL_LOG("[LED] UIO ok: %s (size=0x%lx)\r\n", ctx->uio_path, (unsigned long)ctx->map_size);
// }

// void BoardLed_Set(uint8_t on){
//     if (!gpio_base) return;
//     gpio_base[GPIO_DATA_CH1/4] = on ? 0xFFu : 0x00u;
// }

// board_led_linux.c (MOCK – không cần UIO/devmem)
#include "board_led.h"
#include "osal.h"
#include <stdint.h>
#include <stdio.h>

static uint8_t s_state = 0;   // 0x00: OFF hết, 0xFF: ON hết
static int     s_inited = 0;

void BoardLed_Init(void)
{
    // Không đụng phần cứng – chỉ log để biết đã init
    s_state = 0x00;
    s_inited = 1;
    OSAL_LOG("[LED][MOCK] Init OK (no hardware)\r\n");
}

void BoardLed_Set(uint8_t on)
{
    if (!s_inited) {
        // Cho chắc chắn: nếu app quên gọi Init, vẫn không crash
        BoardLed_Init();
    }

    s_state = on ? 0xFFu : 0x00u;

    // Thay vì ghi thanh ghi, ta chỉ log ra trạng thái 8 LED
    // Bạn có thể tinh chỉnh chuỗi hiển thị nếu muốn
    OSAL_LOG("[LED][MOCK] write = 0x%02X (%s)\r\n",
             (unsigned)s_state,
             on ? "ALL ON" : "ALL OFF");
}

