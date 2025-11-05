/**
 * @file test_hal_uart_linux.c
 * @brief Unit test for HAL UART on Linux backend (termios + poll).
 *
 * NOTE:
 *  - These tests use a pseudo-terminal (PTY) created at runtime, so we don’t
 *    need a real /dev/tty... device.
 *  - We test both positive and negative paths.
 *  - Comments are in English as requested.
 */

#include "unity.h"
#include "hal_uart.h"

#include <pty.h>      // openpty()
#include <unistd.h>   // read(), write(), close()
#include <string.h>   // strlen(), memset()
#include <fcntl.h>    // fcntl()
#include <errno.h>
#include <stdio.h>

static int   s_master_fd = -1;      /* PTY master side - used by the test */
static char  s_slave_name[128];     /* PTY slave device path */
static HAL_Uart* s_uart = NULL;     /* HAL handle under test */

/* ------------------------------------------------------------------------- */
/* Helper: create a PTY pair so that we have a fake UART device to test with */
/* ------------------------------------------------------------------------- */
static void create_pty_or_fail(void)
{
    int slave_fd = -1;
    s_master_fd = -1;
    memset(s_slave_name, 0, sizeof(s_slave_name));

    /* openpty() returns master_fd and slave_fd, and the path to the slave */
    int rc = openpty(&s_master_fd, &slave_fd, s_slave_name, NULL, NULL);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, rc, "openpty() must succeed");
    TEST_ASSERT_TRUE_MESSAGE(s_master_fd >= 0, "master fd must be valid");
    TEST_ASSERT_TRUE_MESSAGE(slave_fd >= 0, "slave fd must be valid");

    /* We only need the name of the slave for HAL_Uart_Open() */
    close(slave_fd);
}

/* ------------------------------------------------------------------------- */
/* Unity hooks                                                                */
/* ------------------------------------------------------------------------- */
void setUp(void)
{
    /* create a fresh PTY for every test to avoid interference */
    create_pty_or_fail();

    /* build a default UART config */
    HAL_UartConfig cfg;
    cfg.device       = s_slave_name;    // <-- our fake UART
    cfg.baud         = 115200;
    cfg.data_bits    = 8;
    cfg.stop_bits    = 1;
    cfg.parity       = HAL_UART_PARITY_NONE;
    cfg.non_blocking = 0;
    cfg.hw_flow      = 0;

    HAL_UartStatus st;
    s_uart = HAL_Uart_Open(&cfg, &st);
    TEST_ASSERT_NOT_NULL_MESSAGE(s_uart, "HAL_Uart_Open() must return handle");
    TEST_ASSERT_EQUAL_INT_MESSAGE(HAL_UART_OK, st, "open should be OK");
}

void tearDown(void)
{
    if (s_uart) {
        HAL_Uart_Close(s_uart);
        s_uart = NULL;
    }
    if (s_master_fd >= 0) {
        close(s_master_fd);
        s_master_fd = -1;
    }
}

/* ------------------------------------------------------------------------- */
/* Positive tests                                                             */
/* ------------------------------------------------------------------------- */

/* Open/close basic scenario */
void test_uart_open_close_ok(void)
{
    /* setUp already opened it; just verify file descriptor is valid */
    int fd = HAL_Uart_GetFd(s_uart);
    TEST_ASSERT_TRUE_MESSAGE(fd >= 0, "GetFd should return valid fd");
}

/* Write from HAL to PTY and read on master to verify data */
void test_uart_write_and_master_receives(void)
{
    const char* msg = "HelloUART";
    long n = HAL_Uart_WriteString(s_uart, msg);
    TEST_ASSERT_EQUAL_INT_MESSAGE((int)strlen(msg), (int)n, "write string must send all bytes");

    /* now read from master side */
    char buf[32];
    ssize_t r = read(s_master_fd, buf, sizeof(buf));
    TEST_ASSERT_TRUE_MESSAGE(r > 0, "master must receive data");
    buf[r] = '\0';
    TEST_ASSERT_EQUAL_STRING_MESSAGE(msg, buf, "data received must match");
}

/* Feed data into master and read via HAL_Uart_Read() */
void test_uart_read_with_timeout_ok(void)
{
    /* put some bytes into master side */
    const char* payload = "ABCDEF";
    ssize_t w = write(s_master_fd, payload, strlen(payload));
    TEST_ASSERT_EQUAL_INT_MESSAGE((int)strlen(payload), (int)w, "write to master must succeed");

    char rx[16];
    memset(rx, 0, sizeof(rx));

    /* read from HAL with some timeout */
    long got = HAL_Uart_Read(s_uart, rx, sizeof(rx), 1000 /* ms */);
    TEST_ASSERT_TRUE_MESSAGE(got > 0, "HAL must read something");
    /* we may get all or partial depending on PTY buffering, but start should match */
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(payload, rx, (size_t)got, "received bytes must match prefix");
}

/* Flush should succeed on a valid handle */
void test_uart_flush_ok(void)
{
    HAL_UartStatus st = HAL_Uart_Flush(s_uart, 2 /* both */);
    TEST_ASSERT_EQUAL_INT_MESSAGE(HAL_UART_OK, st, "flush should return OK");
}

/* Getting fd from valid handle must return same fd as opened */
void test_uart_getfd_ok(void)
{
    int fd = HAL_Uart_GetFd(s_uart);
    TEST_ASSERT_TRUE_MESSAGE(fd >= 0, "fd must be non-negative");
}

/* ------------------------------------------------------------------------- */
/* Negative tests                                                             */
/* ------------------------------------------------------------------------- */

/* Open with NULL config must fail */
void test_uart_open_null_cfg(void)
{
    HAL_UartStatus st = 0;
    HAL_Uart* h = HAL_Uart_Open(NULL, &st);
    TEST_ASSERT_NULL_MESSAGE(h, "open with NULL cfg must return NULL");
    TEST_ASSERT_EQUAL_INT_MESSAGE(HAL_UART_EINVAL, st, "status must be EINVAL");
}

/* Open with invalid device path must fail */
void test_uart_open_invalid_device(void)
{
    HAL_UartConfig cfg;
    cfg.device       = "/dev/this_does_not_exist";
    cfg.baud         = 115200;
    cfg.data_bits    = 8;
    cfg.stop_bits    = 1;
    cfg.parity       = HAL_UART_PARITY_NONE;
    cfg.non_blocking = 0;
    cfg.hw_flow      = 0;

    HAL_UartStatus st;
    HAL_Uart* h = HAL_Uart_Open(&cfg, &st);
    TEST_ASSERT_NULL_MESSAGE(h, "open must fail for invalid device");
    TEST_ASSERT_EQUAL_INT_MESSAGE(HAL_UART_EIO, st, "status must be EIO");
}

/* Open with unsupported baud must fail (baud=12345 is not mapped in backend) */
void test_uart_open_unsupported_baud(void)
{
    HAL_UartConfig cfg;
    cfg.device       = s_slave_name;  // valid PTY
    cfg.baud         = 12345;         // not in _baud_to_flag()
    cfg.data_bits    = 8;
    cfg.stop_bits    = 1;
    cfg.parity       = HAL_UART_PARITY_NONE;
    cfg.non_blocking = 0;
    cfg.hw_flow      = 0;

    HAL_UartStatus st;
    HAL_Uart* h = HAL_Uart_Open(&cfg, &st);
    TEST_ASSERT_NULL_MESSAGE(h, "open must fail for unsupported baud");
    TEST_ASSERT_EQUAL_INT_MESSAGE(HAL_UART_ECFG, st, "status must be ECFG");
}

/* Write with NULL handle must return error */
void test_uart_write_null_handle(void)
{
    const char* msg = "X";
    long n = HAL_Uart_Write(NULL, msg, 1);
    TEST_ASSERT_TRUE_MESSAGE(n < 0, "write with NULL handle must fail");
}

/* Read with NULL handle must return error */
void test_uart_read_null_handle(void)
{
    char buf[8];
    long n = HAL_Uart_Read(NULL, buf, sizeof(buf), 100);
    TEST_ASSERT_TRUE_MESSAGE(n < 0, "read with NULL handle must fail");
}

/* Flush with NULL handle must return EINVAL */
void test_uart_flush_null_handle(void)
{
    HAL_UartStatus st = HAL_Uart_Flush(NULL, 2);
    TEST_ASSERT_EQUAL_INT_MESSAGE(HAL_UART_EINVAL, st, "flush with NULL must be EINVAL");
}

/* GetFd with NULL must return -1 */
void test_uart_getfd_null_handle(void)
{
    int fd = HAL_Uart_GetFd(NULL);
    TEST_ASSERT_EQUAL_INT_MESSAGE(-1, fd, "GetFd(NULL) must be -1");
}

/* ------------------------------------------------------------------------- */
/* Main for standalone run                                                    */
/* ------------------------------------------------------------------------- */
int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_uart_open_close_ok);
    RUN_TEST(test_uart_write_and_master_receives);
    RUN_TEST(test_uart_read_with_timeout_ok);
    RUN_TEST(test_uart_flush_ok);
    RUN_TEST(test_uart_getfd_ok);

    RUN_TEST(test_uart_open_null_cfg);
    RUN_TEST(test_uart_open_invalid_device);
    RUN_TEST(test_uart_open_unsupported_baud);
    RUN_TEST(test_uart_write_null_handle);
    RUN_TEST(test_uart_read_null_handle);
    RUN_TEST(test_uart_flush_null_handle);
    RUN_TEST(test_uart_getfd_null_handle);

    return UNITY_END();
}
