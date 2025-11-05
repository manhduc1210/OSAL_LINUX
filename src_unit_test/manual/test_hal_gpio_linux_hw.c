#include "unity.h"
#include "hal_gpio.h"
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include "hal_gpio_linux_int.h"

/* ===== helper detect HW ===== */
static int has_gpiochip0(void) {
    struct stat st;
    return stat("/dev/gpiochip0", &st) == 0;
}

void setUp(void) {}
void tearDown(void) {}

/* ===== 1. negative: open fail =====
   thử mở tên chip không tồn tại → phải trả về HAL_GPIO_EIO
*/
void test_chip_open_fail(void) {
    HAL_GpioChipConfig bad_cfg = {
        .chip_name = "gpiochip999"   // chip error
    };
    HAL_GpioChip* chip = NULL;
    HAL_GpioStatus st = HAL_GpioChip_Open(&bad_cfg, &chip);
    printf("[DEBUG] With gpiochip999 status = %d\n", st);
    TEST_ASSERT_EQUAL(HAL_GPIO_EIO, st);
    TEST_ASSERT_NULL(chip);
}

/* ===== 2. positive: open ok, request output, write/read lại ===== */
void test_line_write_read_ok(void) {
    if (!has_gpiochip0()) {
        TEST_IGNORE_MESSAGE("/dev/gpiochip0 not found -> skip HW tests");
    }

    HAL_GpioChipConfig cfg = { .chip_name = "gpiochip0" };
    HAL_GpioChip* chip = NULL;
    HAL_GpioStatus st = HAL_GpioChip_Open(&cfg, &chip);
    TEST_ASSERT_EQUAL(HAL_GPIO_OK, st);
    TEST_ASSERT_NOT_NULL(chip);

    HAL_GpioLineConfig lc = {
        .offset  = 0,                    // đổi theo board
        .dir     = HAL_GPIO_DIR_OUT,
        .active  = HAL_GPIO_ACTIVE_HIGH,
        .initial = 0,
    };
    HAL_GpioLine* line = NULL;
    st = HAL_GpioLine_Request(chip, &lc, &line);
    TEST_ASSERT_EQUAL(HAL_GPIO_OK, st);
    TEST_ASSERT_NOT_NULL(line);

    /* write 1 */
    st = HAL_GpioLine_Write(line, 1);
    TEST_ASSERT_EQUAL(HAL_GPIO_OK, st);

    /* read lại (với một số board nếu line không loopback thì giá trị có thể không đọc được,
       nhưng HAL của bạn vẫn gọi gpiod_line_get_value → ta vẫn test được luồng trả về) */
    int val = 0;
    st = HAL_GpioLine_Read(line, &val);
    /* chỉ assert được return code */
    TEST_ASSERT(st == HAL_GPIO_OK || st == HAL_GPIO_EIO);

    HAL_GpioLine_Release(line);
    HAL_GpioChip_Close(chip);
}

/* ===== 3. negative: request line sai offset ===== */
void test_line_request_invalid_offset(void) {
    if (!has_gpiochip0()) {
        TEST_IGNORE_MESSAGE("/dev/gpiochip0 not found -> skip HW tests");
    }

    HAL_GpioChipConfig cfg = { .chip_name = "gpiochip0" };
    HAL_GpioChip* chip = NULL;
    HAL_GpioStatus st = HAL_GpioChip_Open(&cfg, &chip);
    TEST_ASSERT_EQUAL(HAL_GPIO_OK, st);

    HAL_GpioLineConfig bad_lc = {
        .offset  = -1,   // không có name, offset < 0 → EINVAL
        .dir     = HAL_GPIO_DIR_OUT,
        .active  = HAL_GPIO_ACTIVE_HIGH,
    };
    HAL_GpioLine* line = NULL;
    st = HAL_GpioLine_Request(chip, &bad_lc, &line);
    TEST_ASSERT_EQUAL(HAL_GPIO_EINVAL, st);
    TEST_ASSERT_NULL(line);

    HAL_GpioChip_Close(chip);
}

/* ===== 4. test WaitEvent: line không đăng ký event → phải trả về ENOSUP ===== */
void test_line_wait_event_not_supported(void) {
    if (!has_gpiochip0()) {
        TEST_IGNORE_MESSAGE("/dev/gpiochip0 not found -> skip HW tests");
    }

    HAL_GpioChipConfig cfg = { .chip_name = "gpiochip0" };
    HAL_GpioChip* chip = NULL;
    HAL_GpioStatus st = HAL_GpioChip_Open(&cfg, &chip);
    TEST_ASSERT_EQUAL(HAL_GPIO_OK, st);

    /* request input nhưng edge = NONE → have_event = 0 */
    HAL_GpioLineConfig lc = {
        .offset = 0,             // đổi theo board
        .dir    = HAL_GPIO_DIR_IN,
        .edge   = HAL_GPIO_EDGE_NONE,
        .active = HAL_GPIO_ACTIVE_HIGH,
    };
    HAL_GpioLine* line = NULL;
    st = HAL_GpioLine_Request(chip, &lc, &line);
    TEST_ASSERT_EQUAL(HAL_GPIO_OK, st);

    HAL_GpioEvent ev;
    st = HAL_GpioLine_WaitEvent(line, 0, &ev);
    TEST_ASSERT_EQUAL(HAL_GPIO_ENOSUP, st);

    HAL_GpioLine_Release(line);
    HAL_GpioChip_Close(chip);
}

/* ===== 5. test WaitEvent: timeout =====
   chỉ chạy nếu bạn cấu hình offset thành 1 line có event.
   Ở đây mình vẫn viết test để bạn bật lên khi board có nút.
*/
void test_line_wait_event_timeout(void) {
    if (!has_gpiochip0()) {
        TEST_IGNORE_MESSAGE("/dev/gpiochip0 not found -> skip HW tests");
    }

    HAL_GpioChipConfig cfg = { .chip_name = "gpiochip0" };
    HAL_GpioChip* chip = NULL;
    HAL_GpioStatus st = HAL_GpioChip_Open(&cfg, &chip);
    TEST_ASSERT_EQUAL(HAL_GPIO_OK, st);

    HAL_GpioLineConfig lc = {
        .offset      = 0,                // phải là line input có thể tạo event
        .dir         = HAL_GPIO_DIR_IN,
        .edge        = HAL_GPIO_EDGE_BOTH,
        .active      = HAL_GPIO_ACTIVE_HIGH,
        .debounce_ms = 0,
    };
    HAL_GpioLine* line = NULL;
    st = HAL_GpioLine_Request(chip, &lc, &line);
    if (st != HAL_GPIO_OK) {
        TEST_IGNORE_MESSAGE("Cannot request line with events on this board");
    }

    HAL_GpioEvent ev;
    st = HAL_GpioLine_WaitEvent(line, 10 /* ms */, &ev);
    /* nếu không có ai bấm thì sẽ timeout → ENOENT */
    TEST_ASSERT(st == HAL_GPIO_ENOENT || st == HAL_GPIO_OK);

    HAL_GpioLine_Release(line);
    HAL_GpioChip_Close(chip);
}

/* ===== 6. Group WriteMask / ReadBitmap ===== */
void test_group_write_and_read(void) {
    if (!has_gpiochip0()) {
        TEST_IGNORE_MESSAGE("/dev/gpiochip0 not found -> skip HW tests");
    }

    HAL_GpioChipConfig cfg = { .chip_name = "gpiochip0" };
    HAL_GpioChip* chip = NULL;
    HAL_GpioStatus st = HAL_GpioChip_Open(&cfg, &chip);
    TEST_ASSERT_EQUAL(HAL_GPIO_OK, st);

    /* tạo 2 line output liên tiếp: offset 0 và 1 (bạn chỉnh lại theo board) */
    HAL_GpioLineConfig lc0 = {
        .offset  = 0,
        .dir     = HAL_GPIO_DIR_OUT,
        .active  = HAL_GPIO_ACTIVE_HIGH,
        .initial = 0,
    };
    HAL_GpioLineConfig lc1 = lc0;
    lc1.offset = 1;

    HAL_GpioLine* l0 = NULL;
    HAL_GpioLine* l1 = NULL;
    st = HAL_GpioLine_Request(chip, &lc0, &l0);
    TEST_ASSERT_EQUAL(HAL_GPIO_OK, st);
    st = HAL_GpioLine_Request(chip, &lc1, &l1);
    TEST_ASSERT_EQUAL(HAL_GPIO_OK, st);

    HAL_GpioLine* arr[2] = { l0, l1 };
    HAL_GpioGroup grp = {
        .lines = arr,
        .count = 2
    };

    /* Ghi mask: chỉ set bit0 = 1, bit1 = 0 */
    st = HAL_GpioGroup_WriteMask(&grp, 0x3, 0x1);
    TEST_ASSERT_EQUAL(HAL_GPIO_OK, st);

    /* Đọc lại bitmap */
    uint32_t bm = 0;
    st = HAL_GpioGroup_ReadBitmap(&grp, &bm);
    /* vì đọc lại output qua gpiod có thể không phải lúc nào cũng đúng vật lý nên chỉ assert ret OK */
    TEST_ASSERT_EQUAL(HAL_GPIO_OK, st);

    HAL_GpioLine_Release(l1);
    HAL_GpioLine_Release(l0);
    HAL_GpioChip_Close(chip);
}

/* ===== 7. negative cho group ===== */
void test_group_invalid_args(void) {
    uint32_t bm = 0;
    HAL_GpioStatus st;

    st = HAL_GpioGroup_ReadBitmap(NULL, &bm);
    TEST_ASSERT_EQUAL(HAL_GPIO_EINVAL, st);

    st = HAL_GpioGroup_WriteMask(NULL, 0x1, 0x1);
    TEST_ASSERT_EQUAL(HAL_GPIO_EINVAL, st);  // trong code của bạn trả EINVAL nếu grp = NULL
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

int main(void) {
    UNITY_BEGIN();
    // test with hw
    RUN_TEST(test_chip_open_fail);
    RUN_TEST(test_line_write_read_ok);
    RUN_TEST(test_line_request_invalid_offset);
    RUN_TEST(test_line_wait_event_not_supported);
    RUN_TEST(test_line_wait_event_timeout);
    RUN_TEST(test_group_write_and_read);
    RUN_TEST(test_group_invalid_args);
    // test with logic
    RUN_TEST(test_logical_to_physical_active_high);
    RUN_TEST(test_logical_to_physical_active_low);
    RUN_TEST(test_physical_to_logical_active_low);
    RUN_TEST(test_timespec_to_ns);

    return UNITY_END();
}