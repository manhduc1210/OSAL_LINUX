// tests/ns1_hw/test_hal_gpio_linux_hw.c
#include "unity.h"
#include "hal_gpio.h"
#include <sys/stat.h>
#include <stdio.h>

static int has_gpiochip0(void) {
    struct stat st;
    return stat("/dev/gpiochip0", &st) == 0;
}

void setUp(void) {}
void tearDown(void) {}

void test_open_and_toggle_gpio(void) {
    if (!has_gpiochip0()) {
        TEST_IGNORE_MESSAGE("/dev/gpiochip0 not found, skip HW test");
    }

    HAL_GpioChipConfig chip_cfg = {
        .chip_name = "gpiochip0"
    };
    HAL_GpioChip* chip = NULL;
    HAL_GpioStatus st = HAL_GpioChip_Open(&chip_cfg, &chip);
    TEST_ASSERT_EQUAL(HAL_GPIO_OK, st);
    TEST_ASSERT_NOT_NULL(chip);

    HAL_GpioLineConfig line_cfg = {
        .offset  = 0,                   // đổi theo board bạn
        .dir     = HAL_GPIO_DIR_OUT,
        .active  = HAL_GPIO_ACTIVE_HIGH,
        .initial = 0,
    };

    HAL_GpioLine* line = NULL;
    st = HAL_GpioLine_Request(chip, &line_cfg, &line);
    TEST_ASSERT_EQUAL(HAL_GPIO_OK, st);
    TEST_ASSERT_NOT_NULL(line);

    st = HAL_GpioLine_Write(line, 1);
    TEST_ASSERT_EQUAL(HAL_GPIO_OK, st);

    st = HAL_GpioLine_Toggle(line);
    TEST_ASSERT_EQUAL(HAL_GPIO_OK, st);

    HAL_GpioLine_Release(line);
    HAL_GpioChip_Close(chip);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_open_and_toggle_gpio);
    return UNITY_END();
}
