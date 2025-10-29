/**
 * @file demo_gpio_hal.c
 * @brief Demo: BTN0 increments LED counter (max 255), BTN1 resets counter.
 *
 * - Debounce mềm 5ms: chỉ nhận 1 lần khi nút chuyển trạng thái nhả→nhấn.
 * - LEDs hiển thị giá trị counter (8-bit). Khi đạt 255 thì không tăng nữa.
 */
#include "osal.h"
#include "osal_task.h"
#include "hal_gpio.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    const char* chip_name; // "gpiochip0"
    int  led_base;         // e.g., 0 or 32 (first LED line)
    int  led_count;        // number of LEDs, up to 8
    int  btn0_offset;      // button 0 line offset
    int  btn1_offset;      // button 1 line offset
    int  leds_active_low;  // 1 if LEDs active-low
    int  btns_active_low;  // 1 if buttons active-low (pressed=0)
} DemoGpioCfg;

static HAL_Gpio*       s_gpio  = NULL;
static OSAL_TaskHandle s_task  = NULL;
static volatile int    s_run   = 0;
static uint8_t         s_count = 0;

static void GpioTask(void* arg) {
    (void)arg;
    const int debounce_ms = 5;
    uint8_t last_raw = 0, stable = 0, last_stable = 0;
    uint32_t stable_time = 0;

    HAL_Gpio_WriteLeds(s_gpio, s_count);

    while (s_run) {
        // Sample buttons
        uint8_t raw = 0;
        if (HAL_Gpio_ReadBtns(s_gpio, &raw) != HAL_GPIO_OK) {
            OSAL_TaskDelayMs(10);
            continue;
        }

        // Simple debounce: require state stable for N ms
        if (raw == last_raw) {
            stable_time += 5;
            if (stable_time >= (uint32_t)debounce_ms) {
                stable = raw;
            }
        } else {
            stable_time = 0;
        }
        last_raw = raw;

        // Detect rising edge of "pressed=1"
        uint8_t rising = (uint8_t)(stable & (~last_stable));
        last_stable = stable;

        // BTN0 pressed? increment (saturate at 255)
        if (rising & 0x01) {
            if (s_count < 255) s_count++;
            OSAL_LOG("[GPIO][BTN0] increment -> %u\r\n", (unsigned)s_count);
            HAL_Gpio_WriteLeds(s_gpio, s_count);
        }
        // BTN1 pressed? reset
        if (rising & 0x02) {
            s_count = 0;
            OSAL_LOG("[GPIO][BTN1] reset -> %u\r\n", (unsigned)s_count);
            HAL_Gpio_WriteLeds(s_gpio, s_count);
        }

        OSAL_TaskDelayMs(5);
    }
    OSAL_LOG("[GPIO] task exit\r\n");
}

void DemoGpio_Start(const HAL_GpioConfig* cfg) {
    if (!cfg) return;
    // OSAL_LOG("chip_name : %s \r\n", cfg->chip_name);
    HAL_GpioStatus st;
    s_gpio = HAL_Gpio_Open(cfg, &st);
    if (!s_gpio) {
        OSAL_LOG("[DemoGPIO] open failed (%d)\r\n", (int)st);
        return;
    }

    s_run = 1;
    OSAL_TaskAttr a = { .name="GpioDemo", .stack_size=2048, .prio=18 };
    OSAL_TaskCreate(&s_task, GpioTask, NULL, &a);

    OSAL_LOG("[DemoGPIO] started (BTN0=+1 up to 255, BTN1=reset)\r\n");
}

void DemoGpio_Stop(void) {
    s_run = 0;
    OSAL_TaskDelayMs(50);
    if (s_gpio) {
        HAL_Gpio_Close(s_gpio);
        s_gpio = NULL;
    }
    OSAL_LOG("[DemoGPIO] stopped\r\n");
}
