#include "oled_spi_drv.h"
#include "osal.h"
#include <string.h>

/* 6x8 font for ASCII 32..127 (placeholder mini font).
 * NOTE: for brevity, we won't actually dump a huge font table here,
 * but in a real implementation you'd have a static uint8_t font6x8[96][6].
 * We'll just pseudo-send dummy pattern so you see how HAL_Spi_Write is called.
 */

/* Helper: send command byte(s) */
static int _oled_cmd(OledCtx* ctx, const uint8_t* cmd, size_t len)
{
    if (!ctx || !ctx->spi || !ctx->set_dc) return -1;

    // optional manual CS assert if provided
    if (ctx->set_cs) ctx->set_cs(0);

    ctx->set_dc(0); // command mode
    HAL_SpiStatus st = HAL_Spi_Write(ctx->spi, cmd, len);

    if (ctx->set_cs) ctx->set_cs(1);
    return (st == HAL_SPI_OK) ? 0 : -1;
}

/* Helper: send data bytes */
static int _oled_data(OledCtx* ctx, const uint8_t* data, size_t len)
{
    if (!ctx || !ctx->spi || !ctx->set_dc) return -1;

    if (ctx->set_cs) ctx->set_cs(0);

    ctx->set_dc(1); // data mode
    HAL_SpiStatus st = HAL_Spi_Write(ctx->spi, data, len);

    if (ctx->set_cs) ctx->set_cs(1);
    return (st == HAL_SPI_OK) ? 0 : -1;
}

int OLED_Init(OledCtx* ctx)
{
    if (!ctx || !ctx->spi) return -1;
    if (!ctx->width)  ctx->width  = 128;
    if (!ctx->height) ctx->height = 64;

    // Hardware reset pulse
    if (ctx->set_rst) {
        ctx->set_rst(0);
        OSAL_TaskDelayMs(10);
        ctx->set_rst(1);
        OSAL_TaskDelayMs(10);
    }

    // Typical SSD1306-ish init sequence (highly simplified)
    const uint8_t init_seq[] = {
        0xAE,       // display off
        0x20, 0x00, // horizontal addressing mode
        0x81, 0x7F, // contrast
        0xA4,       // resume RAM content display
        0xA6,       // normal display (not inverted)
        0xAF        // display on
    };

    if (_oled_cmd(ctx, init_seq, sizeof(init_seq)) < 0) {
        OSAL_LOG("[OLED] init_seq failed\r\n");
        return -1;
    }

    // Clear after init
    return OLED_Clear(ctx);
}

int OLED_Clear(OledCtx* ctx)
{
    if (!ctx) return -1;

    // On SSD1306, display is arranged in pages (8-pixel tall rows).
    // We'll just write zeros to entire (width * height/8) buffer.
    int pages = ctx->height / 8;
    int bytes_per_page = ctx->width;

    for (int p = 0; p < pages; ++p) {
        // set page address + column start
        uint8_t cmds[3] = {
            (uint8_t)(0xB0 + p), // set page address
            0x00,                // lower column
            0x10                 // higher column
        };
        if (_oled_cmd(ctx, cmds, sizeof(cmds)) < 0)
            return -1;

        // fill this page with 0x00
        uint8_t linebuf[128];
        memset(linebuf, 0x00, sizeof(linebuf));

        if (_oled_data(ctx, linebuf, bytes_per_page) < 0)
            return -1;
    }

    return 0;
}

int OLED_DrawTextRow0(OledCtx* ctx, const char* txt)
{
    if (!ctx || !txt) return -1;

    // We'll render max (width/6) chars, 1 page tall (8px)
    int max_chars = ctx->width / 6;
    int len = 0;
    while (txt[len] && len < max_chars) len++;

    // Position cursor at page 0, column 0
    uint8_t cmds[3] = { 0xB0 + 0, 0x00, 0x10 };
    if (_oled_cmd(ctx, cmds, sizeof(cmds)) < 0)
        return -1;

    // Build a buffer of fake 6x8 font data (placeholder pattern)
    // NOTE: real code would lookup actual bitmap for each char
    uint8_t buf[128];
    int out_i = 0;
    for (int i = 0; i < len && out_i+6 <= (int)sizeof(buf); ++i) {
        char c = txt[i];
        // fake pattern: just alternating stripes so you SEE something
        buf[out_i++] = 0x3C;
        buf[out_i++] = 0x42;
        buf[out_i++] = 0xA5;
        buf[out_i++] = 0x81;
        buf[out_i++] = 0x42;
        buf[out_i++] = 0x3C;
        (void)c; // ignore real glyph for now
    }

    // Send that data
    if (_oled_data(ctx, buf, out_i) < 0)
        return -1;

    return 0;
}
