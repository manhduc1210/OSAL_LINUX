#include "unity.h"
#include "osal.h"
#include "osal_types.h"

/* A simple log function to verify that OSAL keeps the pointer */
static int s_log_called = 0;
static void test_log(const char *fmt, ...) {
    /* just mark it was called */
    s_log_called = 1;
}

void setUp(void)
{
    s_log_called = 0;
    /* nothing else – each test will init OSAL itself */
}

void tearDown(void)
{
    /* make sure OSAL is deinitialized after each test */
    OSAL_Deinit();
}

/* ===== Tests ===== */

void test_OSAL_Init_null_config_should_fail(void)
{
    OSAL_Status st = OSAL_Init(NULL);
    TEST_ASSERT_EQUAL(OSAL_EINVAL, st);
    TEST_ASSERT_EQUAL(0, g_osal.initialized);
}

void test_OSAL_Init_valid_config_should_pass(void)
{
    OSAL_Config cfg = {
        .backend = OSAL_BACKEND_LINUX,
        .log     = test_log,
        .platform_ctx = NULL,
    };

    OSAL_Status st = OSAL_Init(&cfg);
    TEST_ASSERT_EQUAL(OSAL_OK, st);
    TEST_ASSERT_EQUAL(1, g_osal.initialized);
    TEST_ASSERT_EQUAL(OSAL_BACKEND_LINUX, g_osal.cfg.backend);
    /* call OSAL_LOG to make sure log pointer is kept */
    OSAL_LOG("[TEST] hello\r\n");
    TEST_ASSERT_EQUAL(1, s_log_called);
}

void test_OSAL_Deinit_should_clear_initialized_flag(void)
{
    OSAL_Config cfg = {
        .backend = OSAL_BACKEND_LINUX,
        .log     = NULL,
        .platform_ctx = NULL,
    };
    TEST_ASSERT_EQUAL(OSAL_OK, OSAL_Init(&cfg));
    TEST_ASSERT_EQUAL(1, g_osal.initialized);

    OSAL_Deinit();
    TEST_ASSERT_EQUAL(0, g_osal.initialized);
}

/* Unity main */
int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_OSAL_Init_null_config_should_fail);
    RUN_TEST(test_OSAL_Init_valid_config_should_pass);
    RUN_TEST(test_OSAL_Deinit_should_clear_initialized_flag);

    return UNITY_END();
}
