/* fake_spi_port.c
 * This file is compiled ONLY in unit tests.
 * It overrides the weak hooks in hal_spi_linux.c
 */

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <linux/spi/spidev.h>

/* simple fake state */
static int      s_fake_fd         = 100;           // arbitrary
static uint8_t  s_fake_buf[256]   = "HelloMock";   // fake data to read back
static size_t   s_fake_buf_len    = 9;

int hal_spi_port_open(const char* path, int flags)
{
    (void)flags;
    /* emulate: /dev/spidev9.9 does not exist */
    if (strcmp(path, "/dev/spidev9.9") == 0) {
        errno = 2;
        return -1;
    }
    /* otherwise succeed and return fake fd */
    printf("[FAKE SPI] open %s -> fd=%d\n", path, s_fake_fd);
    return s_fake_fd;
}

int hal_spi_port_close(int fd)
{
    printf("[FAKE SPI] close fd=%d\n", fd);
    return 0;
}

int hal_spi_port_ioctl(int fd, unsigned long req, void* arg)
{
    (void)fd;
    /* handle basic config ioctls */
    switch (req) {
    case SPI_IOC_WR_MODE:
    case SPI_IOC_WR_BITS_PER_WORD:
    case SPI_IOC_WR_MAX_SPEED_HZ:
    case SPI_IOC_RD_MODE:
    case SPI_IOC_RD_BITS_PER_WORD:
    case SPI_IOC_RD_MAX_SPEED_HZ:
        return 0;   // pretend success

    default:
        break;
    }

    /* handle transfer message */
    if ((req & 0xFFFF0000) == SPI_IOC_MESSAGE(0)) {
        struct spi_ioc_transfer* xfer = (struct spi_ioc_transfer*)arg;
        // we expect only 1 transfer in unit tests
        uint8_t* tx = (uint8_t*)xfer->tx_buf;
        uint8_t* rx = (uint8_t*)xfer->rx_buf;
        size_t   len = xfer->len;

        /* if there is TX, we can copy it into our fake buffer (like mock) */
        if (tx && len) {
            size_t cpy = (len < sizeof(s_fake_buf)) ? len : sizeof(s_fake_buf);
            memcpy(s_fake_buf, tx, cpy);
            s_fake_buf_len = cpy;
        }

        /* if RX, return from buffer */
        if (rx && len) {
            size_t i;
            for (i = 0; i < len; i++) {
                rx[i] = s_fake_buf[i % s_fake_buf_len];
            }
        }

        return (int)len;  // success
    }

    errno = EINVAL;
    return -1;
}
