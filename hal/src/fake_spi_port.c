// fake_spi_port.c
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <linux/spi/spidev.h>
#include <stdint.h>
#include <stddef.h>
#include <inttypes.h>

static int      s_fake_fd   = 100;
static uint8_t  s_mode      = 0;        // MODE0
static uint8_t  s_bpw       = 8;
static uint32_t s_speed_hz  = 1000000;
static uint8_t  s_buf[256]  = "HelloMock";
static size_t   s_buf_len   = 9;

/* reset buffer mỗi lần open để test nào cũng đọc được "Hello..." */
int hal_spi_port_open(const char* path, int flags)
{
    (void)flags;
    if (strcmp(path, "/dev/spidev9.9") == 0) {
        errno = 2;
        return -1;
    }

    strcpy((char*)s_buf, "HelloMock");
    s_buf_len = 9;

    printf("[FAKE SPI] open %s -> fd=%d\n", path, s_fake_fd);
    return s_fake_fd;
}

int hal_spi_port_close(int fd)
{
    printf("[FAKE SPI] close fd=%d\n", fd);
    return 0;
}

static int is_spi_message_ioctl(unsigned long req)
{
    return (req & 0xFFFF0000UL) == (SPI_IOC_MESSAGE(1) & 0xFFFF0000UL);
}

int hal_spi_port_ioctl(int fd, unsigned long req, void* arg)
{
    (void)fd;
    // printf("[FAKE DEBUG] ioctl req=0x%lX\n", req);

    /* cấu hình SPI */
    switch (req) {
    case SPI_IOC_WR_MODE:
        s_mode = *(uint8_t*)arg;
        return 0;
    case SPI_IOC_WR_BITS_PER_WORD:
        s_bpw = *(uint8_t*)arg;
        return 0;
    case SPI_IOC_WR_MAX_SPEED_HZ:
        s_speed_hz = *(uint32_t*)arg;
        return 0;
    case SPI_IOC_RD_MODE:
        *(uint8_t*)arg = s_mode;
        return 0;
    case SPI_IOC_RD_BITS_PER_WORD:
        *(uint8_t*)arg = s_bpw;
        return 0;
    case SPI_IOC_RD_MAX_SPEED_HZ:
        *(uint32_t*)arg = s_speed_hz;
        return 0;
    default:
        break;
    }

    /* transfer 1 message */
    if (is_spi_message_ioctl(req)) {
        struct spi_ioc_transfer* x = (struct spi_ioc_transfer*)arg;

        // printf("[FAKE DEBUG] SPI_IOC_MESSAGE detected!\n");
        // printf("  xfer.len = %u\n", (unsigned)x->len);
        // printf("  xfer.tx_buf = 0x%lX\n", (unsigned long)x->tx_buf);
        // printf("  xfer.rx_buf = 0x%lX\n", (unsigned long)x->rx_buf);

        uint8_t* rx = NULL;
        if (x->rx_buf)
            rx = (uint8_t*)(uintptr_t)x->rx_buf;

        size_t len = x->len;

        /* KHÔNG copy TX vào s_buf nữa — giữ nguyên "HelloMock" */
        /* chỉ copy từ s_buf ra RX để test đọc */

        if (rx && len) {
            for (size_t i = 0; i < len; i++) {
                rx[i] = s_buf[i % s_buf_len];
            }
            // printf("[FAKE DEBUG] Copied %zu bytes from fake buffer\n", len);
            // printf("[FAKE DEBUG] First RX byte = %u ('%c')\n", rx[0], rx[0]);
        }

        return (int)len;
    }

    /* các ioctl khác (burst, segments) -> fail để test negative */
    errno = EINVAL;
    return -1;
}
