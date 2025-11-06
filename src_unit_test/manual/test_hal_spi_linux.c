#include "unity.h"
#include "hal_spi.h"
#include <string.h>

static HAL_SpiBus* s_bus = NULL;

void setUp(void)
{
    HAL_SpiConfig cfg = {
        .dev_name     = "/dev/spidev0.0",   // real name, but in test it is faked
        .mode         = HAL_SPI_MODE0,
        .max_speed_hz = 1000000,
        .bits_per_word= 8,
        .lsb_first    = 0
    };
    HAL_SpiStatus st;
    s_bus = HAL_Spi_Open(&cfg, &st);
    TEST_ASSERT_NOT_NULL(s_bus);
    TEST_ASSERT_EQUAL_INT(HAL_SPI_OK, st);
}

void tearDown(void)
{
    if (s_bus) {
        HAL_Spi_Close(s_bus);
        s_bus = NULL;
    }
}

/* negative: open with NULL cfg */
void test_spi_open_null_cfg(void)
{
    HAL_SpiStatus st;
    HAL_SpiBus* b = HAL_Spi_Open(NULL, &st);
    TEST_ASSERT_NULL(b);
    TEST_ASSERT_EQUAL_INT(HAL_SPI_EINVAL, st);
}

/* negative: open invalid dev -> our fake returns -1 */
void test_spi_open_invalid_dev(void)
{
    HAL_SpiConfig cfg = {
        .dev_name = "/dev/spidev9.9"
    };
    HAL_SpiStatus st;
    HAL_SpiBus* b = HAL_Spi_Open(&cfg, &st);
    TEST_ASSERT_NULL(b);
    TEST_ASSERT_EQUAL_INT(HAL_SPI_EBUS, st);
}

/* positive: write-only */
void test_spi_transfer_write_only(void)
{
    uint8_t tx[4] = {1,2,3,4};
    HAL_SpiStatus st = HAL_Spi_Transfer(s_bus, tx, NULL, sizeof(tx));
    TEST_ASSERT_EQUAL_INT(HAL_SPI_OK, st);
}

/* positive: read-only -> fake returns "Hello..." */
void test_spi_read_only(void)
{
    uint8_t rx[5] = {0};
    HAL_SpiStatus st = HAL_Spi_Read(s_bus, rx, sizeof(rx));
    // printf("[Debug]test_spi_read_only::status = %s\r\n",st);
    // printf("[Debug]test_spi_read_only::rx = %s\r\n",rx);
    TEST_ASSERT_EQUAL_INT(HAL_SPI_OK, st);
    TEST_ASSERT_EQUAL_UINT8('H', rx[0]);
}

/* positive: full duplex */
void test_spi_full_duplex(void)
{
    uint8_t tx[3] = {0xAA, 0xBB, 0xCC};
    uint8_t rx[3] = {0};
    HAL_SpiStatus st = HAL_Spi_Transfer(s_bus, tx, rx, sizeof(tx));
    // printf("[Debug]test_spi_full_duplex::status = %s\r\n",st);
    // printf("[Debug]test_spi_full_duplex::rx = %s\r\n",rx);
    // printf("[Debug]test_spi_full_duplex::tx = %s\r\n",tx);
    TEST_ASSERT_EQUAL_INT(HAL_SPI_OK, st);
    /* our fake copies from buffer ("HelloMock"), so first char is 'H' */
    TEST_ASSERT_EQUAL_UINT8('H', rx[0]);
}

/* negative: null bus -> should now return EINVAL (lib đã check từ đầu) */
void test_spi_transfer_null_bus(void)
{
    uint8_t tx[2] = {1,2};
    HAL_SpiStatus st = HAL_Spi_Transfer(NULL, tx, NULL, sizeof(tx));
    TEST_ASSERT_EQUAL_INT(HAL_SPI_EINVAL, st);
}

/* negative: zero len */
void test_spi_transfer_zero_len(void)
{
    uint8_t tx[1] = {0};
    HAL_SpiStatus st = HAL_Spi_Transfer(s_bus, tx, NULL, 0);
    TEST_ASSERT_EQUAL_INT(HAL_SPI_EINVAL, st);
}

/* --------------------------------------------------------------------------
 * Positive: HAL_Spi_Write should send TX buffer successfully
 * -------------------------------------------------------------------------- */
void test_spi_write_ok(void)
{
    uint8_t tx[4] = { 0x11, 0x22, 0x33, 0x44 };

    HAL_SpiStatus st = HAL_Spi_Write(s_bus, tx, sizeof(tx));
    TEST_ASSERT_EQUAL_INT(HAL_SPI_OK, st);
}

/* --------------------------------------------------------------------------
 * Negative: HAL_Spi_Write with NULL bus should return EINVAL
 * -------------------------------------------------------------------------- */
void test_spi_write_null_bus(void)
{
    uint8_t tx[2] = { 0xAA, 0xBB };

    HAL_SpiStatus st = HAL_Spi_Write(NULL, tx, sizeof(tx));
    TEST_ASSERT_EQUAL_INT(HAL_SPI_EINVAL, st);
}

/* --------------------------------------------------------------------------
 * GetInfo: should fill out struct with data from bus
 * -------------------------------------------------------------------------- */
void test_spi_get_info(void)
{
    HAL_SpiInfo info;
    HAL_SpiStatus st = HAL_Spi_GetInfo(s_bus, &info);
    TEST_ASSERT_EQUAL_INT(HAL_SPI_OK, st);

    /* name should be the device we opened in setUp() */
    TEST_ASSERT_TRUE_MESSAGE(
        strncmp(info.name, "/dev/spidev", 11) == 0,
        "SPI info name does not start with /dev/spidev"
    );

    /* default config we set in setUp() */
    TEST_ASSERT_EQUAL_UINT8(0, info.mode);       // HAL_SPI_MODE0
    TEST_ASSERT_EQUAL_UINT8(8, info.bits_per_word);
    TEST_ASSERT_EQUAL_UINT8(0, info.lsb_first);

    /* we added this field in hal_spi.h */
    TEST_ASSERT_TRUE(info.max_speed_hz > 0);
}

/* --------------------------------------------------------------------------
 * Burst transfer: with our fake backend this should fail,
 * because fake does not implement multi-transfer ioctl properly.
 * -------------------------------------------------------------------------- */
void test_spi_burst_should_fail(void)
{
    uint8_t rx[4] = {0};
    HAL_SpiStatus st = HAL_Spi_BurstTransfer(s_bus,
                                             NULL,     /* tx */
                                             rx,
                                             sizeof(rx),
                                             0 /* delay */);
    TEST_ASSERT_EQUAL_INT(HAL_SPI_EIO, st);
}

/* --------------------------------------------------------------------------
 * TransferSegments: typical pattern is CMD -> DUMMY -> READ
 * our fake port does not support this advanced ioctl, so it should fail.
 * -------------------------------------------------------------------------- */
void test_spi_transfer_segments_should_fail(void)
{
    uint8_t cmd[1] = { 0x9F };  // typical "read JEDEC ID"
    uint8_t dummy[2] = { 0 };
    uint8_t rx[3] = { 0 };

    HAL_SpiStatus st = HAL_Spi_TransferSegments(s_bus,
                                                cmd, sizeof(cmd),
                                                dummy, sizeof(dummy),
                                                rx, sizeof(rx));
    TEST_ASSERT_EQUAL_INT(HAL_SPI_EIO, st);
}

/* --------------------------------------------------------------------------
 * SetSpeed: our fake ioctl will not update speed, so call should fail.
 * -------------------------------------------------------------------------- */
void test_spi_set_speed_update(void)
{
    uint32_t new_speed = 2000000;
    HAL_SpiStatus st = HAL_Spi_SetSpeed(s_bus, new_speed);  // 2 MHz
    TEST_ASSERT_EQUAL_INT(HAL_SPI_OK, st);

    HAL_SpiInfo info;
    st = HAL_Spi_GetInfo(s_bus, &info);
    TEST_ASSERT_EQUAL_INT(HAL_SPI_OK, st);

    TEST_ASSERT_EQUAL_UINT32(new_speed, info.max_speed_hz);

    printf("[TEST DEBUG] Speed updated -> %u Hz\n", info.max_speed_hz);
}

/* --------------------------------------------------------------------------
 * Close with NULL should be safe
 * -------------------------------------------------------------------------- */
void test_spi_close_null_ok(void)
{
    HAL_Spi_Close(NULL);
    TEST_PASS();   // if no crash -> pass
}

/* --------------------------------------------------------------------------
 * Close a freshly opened bus should not crash
 * -------------------------------------------------------------------------- */
void test_spi_close_valid_ok(void)
{
    HAL_SpiConfig cfg = {
        .dev_name     = "/dev/spidev0.0",
        .mode         = HAL_SPI_MODE0,
        .max_speed_hz = 1000000,
        .bits_per_word= 8,
        .lsb_first    = 0
    };
    HAL_SpiStatus st;
    HAL_SpiBus* b = HAL_Spi_Open(&cfg, &st);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_EQUAL_INT(HAL_SPI_OK, st);

    HAL_Spi_Close(b);

    TEST_PASS();
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_spi_open_null_cfg);
    RUN_TEST(test_spi_open_invalid_dev);
    RUN_TEST(test_spi_transfer_write_only);
    RUN_TEST(test_spi_write_ok);
    RUN_TEST(test_spi_write_null_bus);
    RUN_TEST(test_spi_read_only);
    RUN_TEST(test_spi_full_duplex);
    RUN_TEST(test_spi_transfer_null_bus);
    RUN_TEST(test_spi_transfer_zero_len);
    RUN_TEST(test_spi_get_info);
    RUN_TEST(test_spi_burst_should_fail);
    RUN_TEST(test_spi_transfer_segments_should_fail);
    RUN_TEST(test_spi_set_speed_update);
    RUN_TEST(test_spi_close_null_ok);
    RUN_TEST(test_spi_close_valid_ok);

    return UNITY_END();
}
