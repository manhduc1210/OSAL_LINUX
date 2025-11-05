// tests/test_hal_gpio_linux_logic.c
#include "unity.h"
#include "hal_gpio_linux_int.h"
#include <time.h>

void setUp(void) {}
void tearDown(void) {}

void test_logical_to_physical_active_high(void) {
    HAL_GpioLineConfig cfg = {0};
    cfg.active = HAL_GPIO_ACTIVE_HIGH;

    TEST_ASSERT_EQUAL(1, _logical_to_physical(&cfg, 1));
    TEST_ASSERT_EQUAL(0, _logical_to_physical(&cfg, 0));
}

void test_logical_to_physical_active_low(void) {
    HAL_GpioLineConfig cfg = {0};
    cfg.active = HAL_GPIO_ACTIVE_LOW;

    TEST_ASSERT_EQUAL(0, _logical_to_physical(&cfg, 1));
    TEST_ASSERT_EQUAL(1, _logical_to_physical(&cfg, 0));
}

void test_physical_to_logical_active_low(void) {
    HAL_GpioLineConfig cfg = {0};
    cfg.active = HAL_GPIO_ACTIVE_LOW;

    TEST_ASSERT_EQUAL(1, _physical_to_logical(&cfg, 0));
    TEST_ASSERT_EQUAL(0, _physical_to_logical(&cfg, 1));
}

void test_timespec_to_ns(void) {
    struct timespec ts = { .tv_sec = 1, .tv_nsec = 500000000 };
    uint64_t ns = _timespec_to_ns(&ts);
    TEST_ASSERT_EQUAL_UINT64(1500000000ull, ns);
}

int main(void) {
    // UnitySetVerbose(1); 
    UNITY_BEGIN();
    RUN_TEST(test_logical_to_physical_active_high);
    RUN_TEST(test_logical_to_physical_active_low);
    RUN_TEST(test_physical_to_logical_active_low);
    RUN_TEST(test_timespec_to_ns);
    return UNITY_END();
}
