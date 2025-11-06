#include "unity.h"
#include "hal_gpio.h"
#include "osal.h"
#include <stdio.h>
#include "hal_gpio_linux_int.h"

/* fake_gpiod_min.c provides this for event injection */
extern void fake_gpiod_inject_event(int line_offset, int event_type);

/* ---- Test setup/teardown ---- */
void setUp(void)
{
}

void tearDown(void)
{
}

/* ============================================================
 *                    CHIP TEST CASES
 * ============================================================ */

void test_chip_open_ok(void)
{
    HAL_GpioChipConfig cfg = { .chip_name = "gpiochip0" };
    HAL_GpioChip *chip = NULL;

    HAL_GpioStatus st = HAL_GpioChip_Open(&cfg, &chip);

    TEST_ASSERT_EQUAL(HAL_GPIO_OK, st);
    TEST_ASSERT_NOT_NULL(chip);

    HAL_GpioChip_Close(chip);
}

void test_chip_open_invalid_name(void)
{
    HAL_GpioChipConfig cfg = { .chip_name = "gpiochipX" };
    HAL_GpioChip *chip = NULL;

    HAL_GpioStatus st = HAL_GpioChip_Open(&cfg, &chip);

    TEST_ASSERT_EQUAL(HAL_GPIO_EIO, st);
    TEST_ASSERT_NULL(chip);
}

/* ============================================================
 *                    LINE REQUEST TESTS
 * ============================================================ */

void test_line_request_output_by_offset_ok(void)
{
    HAL_GpioChipConfig cfg = { .chip_name = "gpiochip0" };
    HAL_GpioChip *chip = NULL;
    TEST_ASSERT_EQUAL(HAL_GPIO_OK, HAL_GpioChip_Open(&cfg, &chip));

    HAL_GpioLineConfig lc = {
        .offset = 1,
        .name = NULL,
        .dir = HAL_GPIO_DIR_OUT,
        .active = HAL_GPIO_ACTIVE_HIGH,
        .initial = 0
    };
    HAL_GpioLine *line = NULL;

    HAL_GpioStatus st = HAL_GpioLine_Request(chip, &lc, &line);
    TEST_ASSERT_EQUAL(HAL_GPIO_OK, st);
    TEST_ASSERT_NOT_NULL(line);

    HAL_GpioLine_Release(line);
    HAL_GpioChip_Close(chip);
}

void test_line_request_invalid_offset(void)
{
    HAL_GpioChipConfig cfg = { .chip_name = "gpiochip0" };
    HAL_GpioChip *chip = NULL;
    TEST_ASSERT_EQUAL(HAL_GPIO_OK, HAL_GpioChip_Open(&cfg, &chip));

    HAL_GpioLineConfig lc = {
        .offset = 999, /* invalid offset */
        .dir = HAL_GPIO_DIR_OUT
    };
    HAL_GpioLine *line = NULL;

    HAL_GpioStatus st = HAL_GpioLine_Request(chip, &lc, &line);
    TEST_ASSERT_EQUAL(HAL_GPIO_EIO, st);
    TEST_ASSERT_NULL(line);

    HAL_GpioChip_Close(chip);
}

/* ============================================================
 *                    READ/WRITE TESTS
 * ============================================================ */

void test_line_write_and_readback(void)
{
    HAL_GpioChipConfig cfg = { .chip_name = "gpiochip0" };
    HAL_GpioChip *chip = NULL;
    TEST_ASSERT_EQUAL(HAL_GPIO_OK, HAL_GpioChip_Open(&cfg, &chip));

    HAL_GpioLineConfig lc = {
        .offset = 2,
        .dir = HAL_GPIO_DIR_OUT,
        .active = HAL_GPIO_ACTIVE_HIGH,
        .initial = 0
    };
    HAL_GpioLine *line = NULL;
    TEST_ASSERT_EQUAL(HAL_GPIO_OK, HAL_GpioLine_Request(chip, &lc, &line));

    /* write 1 */
    TEST_ASSERT_EQUAL(HAL_GPIO_OK, HAL_GpioLine_Write(line, 1));

    /* read back */
    int val = 0;
    TEST_ASSERT_EQUAL(HAL_GPIO_OK, HAL_GpioLine_Read(line, &val));
    TEST_ASSERT_EQUAL(1, val);

    HAL_GpioLine_Release(line);
    HAL_GpioChip_Close(chip);
}

void test_line_write_invalid_direction(void)
{
    HAL_GpioChipConfig cfg = { .chip_name = "gpiochip0" };
    HAL_GpioChip *chip = NULL;
    TEST_ASSERT_EQUAL(HAL_GPIO_OK, HAL_GpioChip_Open(&cfg, &chip));

    HAL_GpioLineConfig lc = {
        .offset = 3,
        .dir = HAL_GPIO_DIR_IN,
        .active = HAL_GPIO_ACTIVE_HIGH
    };
    HAL_GpioLine *line = NULL;
    TEST_ASSERT_EQUAL(HAL_GPIO_OK, HAL_GpioLine_Request(chip, &lc, &line));

    /* try to write on input line */
    HAL_GpioStatus st = HAL_GpioLine_Write(line, 1);
    TEST_ASSERT_EQUAL(HAL_GPIO_EINVAL, st);

    HAL_GpioLine_Release(line);
    HAL_GpioChip_Close(chip);
}

/* ============================================================
 *                    TOGGLE + GROUP TESTS
 * ============================================================ */

void test_line_toggle_ok(void)
{
    HAL_GpioChipConfig cfg = { .chip_name = "gpiochip0" };
    HAL_GpioChip *chip = NULL;
    TEST_ASSERT_EQUAL(HAL_GPIO_OK, HAL_GpioChip_Open(&cfg, &chip));

    HAL_GpioLineConfig lc = {
        .offset = 0,
        .dir = HAL_GPIO_DIR_OUT,
        .active = HAL_GPIO_ACTIVE_HIGH,
        .initial = 0
    };
    HAL_GpioLine *line = NULL;
    TEST_ASSERT_EQUAL(HAL_GPIO_OK, HAL_GpioLine_Request(chip, &lc, &line));

    TEST_ASSERT_EQUAL(HAL_GPIO_OK, HAL_GpioLine_Toggle(line));
    int val = 0;
    TEST_ASSERT_EQUAL(HAL_GPIO_OK, HAL_GpioLine_Read(line, &val));
    TEST_ASSERT_EQUAL(1, val);

    HAL_GpioLine_Release(line);
    HAL_GpioChip_Close(chip);
}

void test_group_write_and_read_bitmap(void)
{
    HAL_GpioChipConfig cfg = { .chip_name = "gpiochip0" };
    HAL_GpioChip *chip = NULL;
    TEST_ASSERT_EQUAL(HAL_GPIO_OK, HAL_GpioChip_Open(&cfg, &chip));

    HAL_GpioLine *lines[3] = {0};
    for (int i = 0; i < 3; ++i) {
        HAL_GpioLineConfig lc = {
            .offset = i,
            .dir = HAL_GPIO_DIR_OUT,
            .active = HAL_GPIO_ACTIVE_HIGH,
            .initial = 0
        };
        TEST_ASSERT_EQUAL(HAL_GPIO_OK, HAL_GpioLine_Request(chip, &lc, &lines[i]));
    }

    HAL_GpioGroup grp = { .lines = lines, .count = 3 };
    TEST_ASSERT_EQUAL(HAL_GPIO_OK, HAL_GpioGroup_WriteMask(&grp, 0x7, 0b101));

    uint32_t bitmap = 0;
    TEST_ASSERT_EQUAL(HAL_GPIO_OK, HAL_GpioGroup_ReadBitmap(&grp, &bitmap));
    TEST_ASSERT_EQUAL(0b101, bitmap);

    for (int i = 0; i < 3; ++i)
        HAL_GpioLine_Release(lines[i]);
    HAL_GpioChip_Close(chip);
}

/* ============================================================
 *                    EVENT TESTS
 * ============================================================ */

void test_wait_event_ok(void)
{
    HAL_GpioChipConfig cfg = { .chip_name = "gpiochip0" };
    HAL_GpioChip *chip = NULL;
    TEST_ASSERT_EQUAL(HAL_GPIO_OK, HAL_GpioChip_Open(&cfg, &chip));

    HAL_GpioLineConfig lc = {
        .offset = 4,
        .dir = HAL_GPIO_DIR_IN,
        .active = HAL_GPIO_ACTIVE_HIGH,
        .edge = HAL_GPIO_EDGE_RISING
    };
    HAL_GpioLine *line = NULL;
    TEST_ASSERT_EQUAL(HAL_GPIO_OK, HAL_GpioLine_Request(chip, &lc, &line));

    /* inject fake event */
    fake_gpiod_inject_event(4, 1 /* GPIOD_LINE_EVENT_RISING_EDGE */);

    HAL_GpioEvent ev;
    HAL_GpioStatus st = HAL_GpioLine_WaitEvent(line, 100, &ev);
    TEST_ASSERT_EQUAL(HAL_GPIO_OK, st);
    TEST_ASSERT_EQUAL(HAL_GPIO_EDGE_RISING, ev.edge);

    HAL_GpioLine_Release(line);
    HAL_GpioChip_Close(chip);
}

void test_wait_event_timeout(void)
{
    HAL_GpioChipConfig cfg = { .chip_name = "gpiochip0" };
    HAL_GpioChip *chip = NULL;
    TEST_ASSERT_EQUAL(HAL_GPIO_OK, HAL_GpioChip_Open(&cfg, &chip));

    HAL_GpioLineConfig lc = {
        .offset = 5,
        .dir = HAL_GPIO_DIR_IN,
        .active = HAL_GPIO_ACTIVE_HIGH,
        .edge = HAL_GPIO_EDGE_BOTH
    };
    HAL_GpioLine *line = NULL;
    TEST_ASSERT_EQUAL(HAL_GPIO_OK, HAL_GpioLine_Request(chip, &lc, &line));

    HAL_GpioEvent ev;
    HAL_GpioStatus st = HAL_GpioLine_WaitEvent(line, 10, &ev);
    TEST_ASSERT_EQUAL(HAL_GPIO_ENOENT, st); /* timeout simulated */

    HAL_GpioLine_Release(line);
    HAL_GpioChip_Close(chip);
}

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

/* ============================================================
 *                    MAIN ENTRY
 * ============================================================ */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_chip_open_ok);
    RUN_TEST(test_chip_open_invalid_name);
    RUN_TEST(test_line_request_output_by_offset_ok);
    RUN_TEST(test_line_request_invalid_offset);
    RUN_TEST(test_line_write_and_readback);
    RUN_TEST(test_line_write_invalid_direction);
    RUN_TEST(test_line_toggle_ok);
    RUN_TEST(test_group_write_and_read_bitmap);
    RUN_TEST(test_wait_event_ok);
    RUN_TEST(test_wait_event_timeout);
    RUN_TEST(test_logical_to_physical_active_high);
    RUN_TEST(test_logical_to_physical_active_low);
    RUN_TEST(test_physical_to_logical_active_low);
    RUN_TEST(test_timespec_to_ns);

    return UNITY_END();
}
