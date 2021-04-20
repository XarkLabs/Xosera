// xvid_spi.cpp - xvid FTDI SPI test utility
//
// vim: set et ts=4 sw=4
//
// Copyright (c) 2020 Xark - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include "ftdi_spi.h"

enum
{
    // register 16-bit read/write (no side effects)
    XVID_AUX_ADDR,        // reg 0: TODO video data (as set by VID_CTRL)
    XVID_CONST,           // reg 1: TODO CPU data (instead of read from VRAM)
    XVID_RD_ADDR,         // reg 2: address to read from VRAM
    XVID_WR_ADDR,         // reg 3: address to write from VRAM

    // special, odd byte write triggers
    XVID_DATA,            // reg 4: read/write word from/to VRAM RD/WR
    XVID_DATA_2,          // reg 5: read/write word from/to VRAM RD/WR (for 32-bit)
    XVID_AUX_DATA,        // reg 6: aux data (font/audio)
    XVID_COUNT,           // reg 7: TODO blitter "repeat" count/trigger

    // write only, 16-bit
    XVID_RD_INC,           // reg 9: read addr increment value
    XVID_WR_INC,           // reg A: write addr increment value
    XVID_WR_MOD,           // reg C: TODO write modulo width for 2D blit
    XVID_RD_MOD,           // reg B: TODO read modulo width for 2D blit
    XVID_WIDTH,            // reg 8: TODO width for 2D blit
    XVID_BLIT_CTRL,        // reg D: TODO
    XVID_UNUSED_E,         // reg E: TODO
    XVID_UNUSED_F,         // reg F: TODO

    // AUX access using AUX_ADDR/AUX_DATA
    AUX_VID             = 0x0000,        // 0-8191 8-bit address (bits 15:8 ignored writing)
    AUX_VID_W_DISPSTART = 0x0000,        // display start address
    AUX_VID_W_TILEWIDTH = 0x0001,        // tile line width (usually WIDTH/8)
    AUX_VID_W_SCROLLXY  = 0x0002,        // [10:8] H fine scroll, [3:0] V fine scroll
    AUX_VID_W_FONTCTRL  = 0x0003,        // [9:8] 2KB font bank, [3:0] font height
    AUX_VID_W_GFXCTRL   = 0x0004,        // [0] h pix double
    AUX_VID_W_UNUSED5   = 0x0005,
    AUX_VID_W_UNUSED6   = 0x0006,
    AUX_VID_W_UNUSED7   = 0x0007,
    AUX_VID_R_WIDTH     = 0x0000,        // display resolution width
    AUX_VID_R_HEIGHT    = 0x0001,        // display resolution height
    AUX_VID_R_FEATURES  = 0x0002,        // [15] = 1 (test)
    AUX_VID_R_SCANLINE  = 0x0003,        // [15] V blank, [14:11] zero [10:0] V line
    AUX_W_FONT          = 0x4000,        // 0x4000-0x5FFF 8K byte font memory (even byte [15:8] ignored)
    AUX_W_COLORTBL      = 0x8000,        // 0x8000-0x80FF 256 word color lookup table (0xXRGB)
    AUX_W_AUD           = 0xc000         // 0xC000-0x??? TODO (audio registers)
};

static void hexdump(size_t num, uint8_t * mem)
{
    for (size_t i = 0; i < num; i++)
    {
        printf("%02x", mem[i]);
        if (i != num - 1)
        {
            printf(", ");
        }
    }
    printf("\n");
}

void delay_ms(int ms)
{
    usleep(ms * 1000);
}


// SPI "bus command" message format. Always sends/receives two bytes:
//              +---+---+---+---+---+---+---+---+
// Command byte |WR | 0 | 0 |BS |     REGNUM    |
//              +---+---+---+---+---+---+---+---+
// WR bit     = 0 for read, 1 for write
// BS bit     = 0 for even, 1 for odd byte of register
// REGNUM     = Xosera 4-bit register number
// Command byte SPI reply will be 0xCB (for command byte)
//
//              +---+---+---+---+---+---+---+---+
// Data byte    |Wr 8-bit data / ignored for Rd |
//              +---+---+---+---+---+---+---+---+
//
// Data byte SPI reply will be the Xosera register contents for a read or a "dummy" byte for a write.
//
// NOTE: Actually for writes, the "dummy" reply byte will be the value Xosera "would" have written to the bus had this
// been a read operation.  For read/write registers this will be the previous register contents before the write
// command, or for write-only registers it will be a "mirrored" readable register. This behavior should not be relied
// upon, but can be a handy artifact for debugging/testing.

enum
{
    SPI_CMD_WR      = 0x80,
    SPI_CMD_BYTESEL = 0x10,
    SPI_CMD_REGMASK = 0x0f
};

#define DEBUG_HEXDUMP 0

#define MAX_SEND    1024
#define FLUSH_QUEUE 1020

static uint8_t   send_buffer[MAX_SEND];
static uint8_t   xmit_buffer[MAX_SEND];
static uint8_t * send_ptr = send_buffer;
static uint8_t   read_value;

size_t spi_queue_len()
{
    size_t len = send_ptr - send_buffer;
    return len;
}

inline int spi_queue_flush()
{
    size_t len = spi_queue_len();
    if (len)
    {
        host_spi_cs(false);        // select
        memcpy(xmit_buffer, send_buffer, len);
        host_spi_xfer_bytes(len, xmit_buffer);
        host_spi_cs(true);        // de-select

#if DEBUG_HEXDUMP
        printf("SENT[%02zu]: ", len);
        hexdump(len, send_buffer);
        printf("RCVD[%02zu]: ", len);
        hexdump(len, xmit_buffer);
#endif
        send_ptr = send_buffer;
    }

    return len;
}

inline void spi_queue_cmd(uint8_t cmd, uint8_t data)
{
    *send_ptr++ = cmd;
    *send_ptr++ = data;

    if (spi_queue_len() > FLUSH_QUEUE)
    {
        spi_queue_flush();
    }
}

void delay(int ms)
{
    spi_queue_flush();
    delay_ms(ms);
}

static inline void xvid_setw(uint8_t r, uint16_t word)
{
    spi_queue_cmd(SPI_CMD_WR | (r & SPI_CMD_REGMASK), (word >> 8) & 0xff);
    spi_queue_cmd(SPI_CMD_WR | SPI_CMD_BYTESEL | (r & SPI_CMD_REGMASK), word & 0xff);
}

static inline void xvid_setlb(uint8_t r, uint8_t lsb)
{
    spi_queue_cmd(SPI_CMD_WR | SPI_CMD_BYTESEL | (r & SPI_CMD_REGMASK), lsb & 0xff);
}

static inline void xvid_sethb(uint8_t r, uint8_t msb)
{
    spi_queue_cmd(SPI_CMD_WR | (r & SPI_CMD_REGMASK), msb & 0xff);
}

static inline uint16_t xvid_getw(uint8_t r)
{
    spi_queue_cmd((r & SPI_CMD_REGMASK), 0xff);
    spi_queue_cmd(SPI_CMD_BYTESEL | (r & SPI_CMD_REGMASK), 0xff);
    size_t len = spi_queue_flush();
    return (xmit_buffer[len - 3] << 8) | xmit_buffer[len - 1];
}

// bytesel = LSB (default) or 0 for MSB
static inline uint8_t xvid_getb(uint8_t r, uint8_t bytesel = 1)
{
    spi_queue_cmd((r & SPI_CMD_REGMASK) | (bytesel ? SPI_CMD_BYTESEL : 0), 0xff);
    size_t len = spi_queue_flush();
    return xmit_buffer[len - 1];
}

static inline uint8_t xvid_getlb(uint8_t r)
{
    return xvid_getb(r, 1);
};
static inline uint8_t xvid_gethb(uint8_t r)
{
    return xvid_getb(r, 0);
}

static void xcolor(uint8_t color)
{
    uint16_t wa = xvid_getw(XVID_WR_ADDR);
    xvid_sethb(XVID_DATA, color);
    xvid_setw(XVID_WR_ADDR, wa);
}

static bool     error_flag;
static uint32_t errors;
static uint8_t  cur_color = 0x02;        // color for status line (green or red after error)
static uint8_t  ln        = 0;           // current line number
static uint16_t width;                   // in pixels
static uint16_t height;                  // in pixels
static uint16_t features;                // feature bits
static uint8_t  columns;                 // in texts chars (words)
static uint8_t  rows;                    // in texts chars (words)
static uint16_t addr;
static uint16_t data;
static uint16_t rdata;

#include "buddy_font.h"

static void spi_reset()
{
    spi_queue_flush();
    host_spi_cs(false);        // de-select
    spi_queue_cmd(0xff, 0xff);
    spi_queue_cmd(0xff, 0xff);
    spi_queue_cmd(0xff, 0xff);
    spi_queue_cmd(0xff, 0xff);
    spi_queue_cmd(0xff, 0xff);
    spi_queue_cmd(0xff, 0xff);
    spi_queue_flush();
    host_spi_cs(true);        // de-select
}

static void sync_Xosera()
{
    printf("Waiting for SPI sync...\n");
    do
    {
        spi_reset();
        delay_ms(100);
        xvid_setw(XVID_CONST, 0xB007);
    } while (xvid_getw(XVID_CONST) != 0xB007);
    printf("Good.\n");
}

static void reboot_Xosera(uint8_t config)
{
#if 1
    printf("Xosera resetting, switching to config #%d...\n", config & 0x3);
    host_spi_cs(true);        // de-select
    delay_ms(10);
    xvid_setw(XVID_BLIT_CTRL, 0x8080 | ((config & 0x3) << 8));        // reboot FPGA to config
    do
    {
        spi_queue_flush();
        host_spi_cs(true);        // de-select
        delay_ms(100);
        xvid_setw(XVID_RD_ADDR, 0x1234);
        xvid_setw(XVID_CONST, 0xABCD);
        spi_queue_flush();

    } while (xvid_getw(XVID_RD_ADDR) != 0x1234 || xvid_getw(XVID_CONST) != 0xABCD);
#endif

    xvid_setw(XVID_AUX_ADDR, AUX_VID_R_WIDTH);        // select width
    width = xvid_getw(XVID_AUX_DATA);

    xvid_setw(XVID_AUX_ADDR, AUX_VID_R_HEIGHT);        // select height
    height = xvid_getw(XVID_AUX_DATA);

    xvid_setw(XVID_AUX_ADDR, AUX_VID_R_FEATURES);        // select features
    features = xvid_getw(XVID_AUX_DATA);

    printf("(%dx%d, features=0x%04x) ready.\n", width, height, features);

    //    width  = 848;
    //    height = 480;

    columns = width / 8;
    rows    = height / 16;
    addr    = columns;
}

// read scanline register and wait for non-visible line (bit[15])
static void wait_vsync(uint16_t num = 1)
{
    uint8_t v_flag;
    while (num--)
    {
        do
        {
            xvid_setw(XVID_AUX_ADDR, AUX_VID_R_SCANLINE);        // set scanline reg
            v_flag = xvid_gethb(XVID_AUX_DATA);                  // read scanline upper byte
        } while (!(v_flag & 0x80));                              // loop if on visible line
    }
}

static void xhome()
{
    // home wr addr
    xvid_setw(XVID_WR_INC, 1);
    xvid_setw(XVID_WR_ADDR, 0);
    xcolor(cur_color);        // green-on-black
    ln = 0;
}

static void xpos(uint8_t h, uint8_t v)
{
    xvid_setw(XVID_WR_INC, 1);
    xvid_setw(XVID_WR_ADDR, (v * columns) + h);
    xcolor(cur_color);        // green-on-black
    ln = v;
}

static void xcls(uint8_t v = ' ')
{
    // clear screen
    xhome();
    for (uint16_t i = 0; i < columns * rows; i++)
    {
        xvid_setlb(XVID_DATA, v);
    }
    xvid_setw(XVID_WR_ADDR, 0);
}

static void xprint(const char * s)
{
    uint8_t c;
    while ((c = *s++) != '\0')
    {
        if (c == '\n')
        {
            xvid_setw(XVID_WR_ADDR, ++ln * columns);
            continue;
        }
        xvid_setlb(XVID_DATA, c);
    }
}

static void xprint_rainbow(uint8_t color, const char * s)
{
    uint8_t c;
    xcolor(color);
    while ((c = *s++) != '\0')
    {
        if (c == '\n')
        {
            ln += 1;
            xvid_setw(XVID_WR_ADDR, ln * columns);
            color = (color + 1) & 0xf;
            if (color == 0)        // skip black
                color++;
            xcolor(color);
            continue;
        }
        xvid_setlb(XVID_DATA, c);
    }
}

static void xprint_hex(uint16_t v)
{
    static const char hex[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
    xvid_setw(XVID_DATA, (cur_color << 8) | hex[((v >> 12) & 0xf)]);
    xvid_setw(XVID_DATA, (cur_color << 8) | hex[((v >> 8) & 0xf)]);
    xvid_setw(XVID_DATA, (cur_color << 8) | hex[((v >> 4) & 0xf)]);
    xvid_setw(XVID_DATA, (cur_color << 8) | hex[((v >> 0) & 0xf)]);
}

static void xprint_int(uint32_t n)
{
    uint32_t poten = 100000000;
    uint32_t v     = n;
    if (v > 999999999) v = 999999999;
    while (poten)
    {
        uint8_t d = v / poten;
        if (d || n > poten)
        {
            xvid_setlb(XVID_DATA, '0' + d);
        }
        v -= d * poten;
        poten = poten / 10;
    }
}

static void xprint_dec(uint16_t n)
{
    uint16_t poten = 10000;
    uint16_t v     = n;
    while (poten)
    {
        uint8_t d = v / poten;
        if (d || n > poten)
        {
            xvid_setlb(XVID_DATA, '0' + d);
        }
        else
        {
            xvid_setlb(XVID_DATA, ' ');
        }
        v -= d * poten;
        poten = poten / 10;
    }
}

const uint16_t defpal[16] = {
    0x0000,        // black
    0x000A,        // blue
    0x00A0,        // green
    0x00AA,        // cyan
    0x0A00,        // red
    0x0A0A,        // magenta
    0x0AA0,        // brown
    0x0AAA,        // light gray
    0x0555,        // dark gray
    0x055F,        // light blue
    0x05F5,        // light green
    0x05FF,        // light cyan
    0x0F55,        // light red
    0x0F5F,        // light magenta
    0x0FF5,        // yellow
    0x0FFF         // white
};

const char blurb[] =
    "\n\n"
    "    Xosera is an FPGA based video adapter designed with the rosco_m68k retro computer\n"
    "    in mind. Inspired in concept by it's \"namesake\" the Commander X16's VERA, Xosera\n"
    "    is an original open-source video adapter design, built with open-source tools, that\n"
    "    is being tailored with features appropriate for a Motorola 68K era retro computer.\n"
    "    \n"
    "        \xf9   VGA or HDMI (DVI) output at 848x480@60Hz or 640x480@60Hz (16:9 or 4:3)\n"
    "        \xf9   16 or 256 color palette out of 4096 colors (12-bit RGB)\n"
    "        \xf9   128KB of embedded video RAM (16-bit words @33/25 MHz)\n"
    "        \xf9   Character tile based modes with color attribute byte\n"
    "        \xf9   Pixel doubled bitmap modes (e.g. 424x240 or 320x240)\n"
    "        \xf9   Smooth horizontal and vertical tile scrolling\n"
    "        \xf9   8x8 or 8x16 character tile size (or truncated e.g., 8x10)\n"
    "        \xf9   Register based interface with 16 16-bit registers\n"
    "        \xf9   Read/write VRAM with programmable read/write address increment\n"
    "        \xf9   Full speed 8-bit bus interface (with MOVEP) for rosco_m68k (by Ross Bamford)\n"
    "        \xf9   8KB of font RAM with multiple fonts (2KB per 8x8 fonts, 4K per 8x16 font)\n"
    "        \xf9   \"Blitter\" for fast VRAM copy & fill operations (TODO, but used at init)\n"
    "        \xf9   2-D operations \"blitter\" with modulo and shifting/masking (TODO)\n"
    "        \xf9   Dual overlayed \"planes\" of video (TODO)\n"
    "        \xf9   Wavetable stereo audio (TODO, spare debug IO for now)\n"
    "        \xf9   Bit-mapped 16 and 256 color graphics modes (256 color TODO)\n"
    "        \xf9   16-color tile mode with \"game\" attributes (e.g., mirroring) (TODO)\n"
    "        \xf9   At least one \"cursor\" sprite (and likely more, TODO)\n";

void show_blurb()
{
    // Show some text
    printf("Blurb text\n");
    xcls();
    xprint(blurb);
    delay(500);

    // 2nd font (ST 8x8)
    printf("ST 8x8 font\n");
    xvid_setw(XVID_AUX_ADDR, AUX_VID_W_FONTCTRL);        // A_font_ctrl
    xvid_setw(XVID_AUX_DATA, 0x0207);                    // 2nd font in bank 2, 8 high
    delay(500);

    // 3rd font (hex 8x8 debug)
    printf("hex 8x8 font\n");
    xvid_setw(XVID_AUX_ADDR, AUX_VID_W_FONTCTRL);        // A_font_ctrl
    xvid_setw(XVID_AUX_DATA, 0x0307);                    // 3st font in bank 3, 8 high
    delay(500);

    // restore 1st font (ST 8x16)
    printf("ST 8x16 font\n");
    xvid_setw(XVID_AUX_ADDR, AUX_VID_W_FONTCTRL);        // A_font_ctrl
    xvid_setw(XVID_AUX_DATA, 0x000F);                    // back to 1st font in bank 0, 16 high
    delay(500);

    // shrink font height
    printf("Shrink font height\n");
    for (int v = 15; v >= 0; v--)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_VID_W_FONTCTRL);        // A_font_ctrl
        // set font height and switch to 8x8 font when < 8
        xvid_setw(XVID_AUX_DATA, (v < 8 ? 0x0200 : 0) | v);

        wait_vsync(1);
    }

    printf("Grow font height\n");
    for (int v = 0; v < 16; v++)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_VID_W_FONTCTRL);        // A_font_ctrl
        // set font height and switch to 8x8 font when < 8
        xvid_setw(XVID_AUX_DATA, (v < 8 ? 0x0200 : 0) | v);
        wait_vsync(1);
    }

    // restore 1st font (ST 8x16)
    printf("ST 8x16 font\n");
    xvid_setw(XVID_AUX_ADDR, AUX_VID_W_FONTCTRL);        // A_font_ctrl
    xvid_setw(XVID_AUX_DATA, 0x000F);                    // back to 1st font in bank 0, 16 high
    delay(500);

    printf("Scroll via video VRAM display address\n");
    int16_t r = 0;
    for (uint16_t i = 0; i < (rows); i++)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_VID_W_DISPSTART);        // set text start addr
        xvid_setw(XVID_AUX_DATA, r * columns);                // to one line down
        for (int8_t f = 0; f < 16; f++)
        {
            xvid_setw(XVID_AUX_ADDR, AUX_VID_W_SCROLLXY);        // v fine scroll
            xvid_setw(XVID_AUX_DATA, f);

            wait_vsync(1);
        }
        if (++r > rows + 10)
        {
            r = -rows;
        }
    }
    xvid_setw(XVID_AUX_ADDR, AUX_VID_W_DISPSTART);        // reset text start addr
    xvid_setw(XVID_AUX_DATA, 0x0000);
    xvid_setw(XVID_AUX_ADDR, AUX_VID_W_SCROLLXY);        // reset text start addr
    xvid_setw(XVID_AUX_DATA, 0x0000);
    delay(500);

    for (uint8_t i = 0; i < 16; i++)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | i);        // use WR address for palette index
        xvid_setw(XVID_AUX_DATA, defpal[i]);                 // set palette data
    }

#if 0
    printf("Horizontal fine scroll\n");
    for (int x = 0; x < 8; x++)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_VID_W_SCROLLXY);        // scroll
        // set font height and switch to 8x8 font when < 8
        xvid_setw(XVID_AUX_DATA, x);
        delay(100);
    }
    for (int x = 7; x > 0; x--)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_VID_W_SCROLLXY);        // scroll
        // set font height and switch to 8x8 font when < 8
        xvid_setw(XVID_AUX_DATA, x);
        delay(100);
    }
    delay(1000);
    printf("Vertical fine scroll\n");
    for (int x = 0; x < 16; x++)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_VID_W_SCROLLXY);        // scroll
        // set font height and switch to 8x8 font when < 8
        xvid_setw(XVID_AUX_DATA, x << 8);
        delay(100);
    }
    for (int x = 15; x > 0; x--)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_VID_W_SCROLLXY);        // scroll
        // set font height and switch to 8x8 font when < 8
        xvid_setw(XVID_AUX_DATA, x << 8);
        delay(100);
    }
    delay(1000);
#endif
}

static void problem(const char * msg, uint16_t addr, uint16_t rdata, uint16_t vdata)
{
    errors++;
    printf("%s at 0x%04x, rd=%04x, vs %04x, errors %d\n", msg, addr, rdata, vdata, errors);
    error_flag = true;
}


static const uint16_t data_pat[8] = {0xA5A5, 0x5A5A, 0xFFFF, 0x0123, 0x4567, 0x89AB, 0xCDEF, 0x0220};

void test_reg_access()
{
    xcls();
    xprint("Xosera read/write register self-test...\n");

    for (uint8_t r = XVID_AUX_ADDR; r <= XVID_WR_ADDR; r++)
    {
        xhome();
        xpos(4, 4 + r);
        xprint("Register: ");
        switch (r)
        {
            case XVID_AUX_ADDR: {
                xprint("XVID_AUX_ADDR");
            }
            break;
            case XVID_CONST: {
                xprint("XVID_CONST   ");
            }
            break;
            case XVID_RD_ADDR: {
                xprint("XVID_RD_ADDR ");
            }
            break;
            case XVID_WR_ADDR: {
                xprint("XVID_WR_ADDR ");
            }
            break;
        }
        xprint(" <=> ");

        uint16_t cp = xvid_getw(XVID_WR_ADDR);
        for (int i = 0; i < 8; i++)
        {
            uint16_t v = data_pat[i];
            xvid_setw(XVID_WR_ADDR, cp);
            xcolor(cur_color);
            xprint_hex(v);
            xvid_setw(r, v);
            rdata = xvid_getw(r);
            if (rdata != v)
            {
                problem("reg verify", r, rdata, v);
                break;
            }
        }
        if (!error_flag)
        {
            xvid_setw(XVID_WR_ADDR, cp);
            xcolor(0x0a);
            xprint("PASSED");
            xcolor(cur_color);
        }
        else
        {
            xvid_setw(XVID_WR_ADDR, cp);
            xcolor(cur_color);
            xprint("FAILED");
        }
    }

    if (error_flag)
    {
        xpos(0, 8);
        xprint("Register self-test FAILED!");
        delay(2000);
    }
    else
    {
        xpos(0, 8);
        xprint("Register self-test passed.");
    }

    xpos(0, 12);
    xprint("VRAM read/write check...");

    delay(1000);

    xpos(4, 14);
    xprint("VRAM[");
    uint16_t ap = xvid_getw(XVID_WR_ADDR);
    xprint("    ] <=> ");
    uint16_t vp = xvid_getw(XVID_WR_ADDR);

    for (uint8_t i = 0; i < 8; i++)
    {
        uint16_t v = data_pat[i];
        xvid_setw(XVID_WR_ADDR, vp);
        xcolor(cur_color);
        xprint_hex(v);

        for (int a = 0x600; a < 0x10000; a++)
        {
            if ((a & 0xfff) == 0xfff)
            {
                xvid_setw(XVID_WR_ADDR, ap);
                xcolor(cur_color);
                xprint_hex(a);
            }
            xvid_setw(XVID_WR_ADDR, a);
            xvid_setw(XVID_DATA, v);
            xvid_setw(XVID_RD_ADDR, a);
            rdata = xvid_getw(XVID_DATA);
            if (rdata != v)
            {
                problem("VRAM test", a, rdata, v);
                break;
            }
        }
        if (error_flag)
        {
            break;
        }
    }

    if (error_flag)
    {
        xpos(0, 16);
        xprint("VRAM check FAILED!");
    }
    else
    {
        xpos(0, 16);
        xprint("VRAM check passed.");
    }

    delay(2000);
}

void draw_buddy()
{
    xvid_setw(XVID_AUX_ADDR, AUX_VID_W_FONTCTRL);        // A_font_ctrl
    xvid_setw(XVID_AUX_DATA, 0x0207);                    // 2nd font in bank 2, 8 high
    rows <<= 1;

    xcls(0xff);
    for (int y = 0; y < 16; y++)
    {
        xvid_setw(XVID_WR_ADDR, y * columns);
        for (int x = 0; x < 16; x++)
        {
            xvid_setw(XVID_DATA, 0x0f00 | (y * 16 + x));
        }
    }
    for (uint16_t a = 0; a < 2048; a++)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_W_FONT | 4096 | a);
        xvid_setw(XVID_AUX_DATA, buddy_font[a]);
    }

    delay(2000);
    xvid_setw(XVID_AUX_ADDR, AUX_VID_W_GFXCTRL);        // use WR address for palette index
    xvid_setw(XVID_AUX_DATA, 0x0003);                   // set palette data
    delay(2000);
    xvid_setw(XVID_AUX_ADDR, AUX_VID_W_GFXCTRL);        // use WR address for palette index
    xvid_setw(XVID_AUX_DATA, 0x0000);                   // set palette data
    delay(2000);
    xvid_setw(XVID_AUX_ADDR, AUX_VID_W_FONTCTRL);        // A_font_ctrl
    xvid_setw(XVID_AUX_DATA, 0x000F);                    // back to 1st font in bank 0, 16 high
}

int main(int argc, char ** argv)
{
    (void)argc;
    (void)argv;
    if (host_spi_open() < 0)
    {
        exit(EXIT_FAILURE);
    }

    sync_Xosera();

    reboot_Xosera(0);

    delay(5000);        // let the stunning boot logo display. :)

    xcls();
    xprint("Xosera Retro Graphics Adapter: Mode ");
    xprint_int(width);
    xprint("x");
    xprint_int(height);
    xprint(" (SPI/FTDI PC tester)\n\n");

    for (int i = 0; i < 409; i++)
    {
        uint8_t c = (i & 0xf) ? (i & 0xf) : 1;
        xcolor(c);
        xprint("Hello! ");
    }

    delay(2000);

    xcolor(0xf);
    xcls();
    draw_buddy();

    for (uint16_t k = 0; k < 2000; k++)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_VID_R_SCANLINE);        // set scanline reg
        uint16_t l = xvid_getw(XVID_AUX_DATA);               // read scanline
        l          = l | ((0xf - (l & 0xf)) << 8);           // invert blue for some red
        xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | 0);        // set palette entry #0
        xvid_setw(XVID_AUX_DATA, l);                         // set palette data
    }
    for (uint8_t i = 0; i < 16; i++)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | i);        // use WR address for palette index
        xvid_setw(XVID_AUX_DATA, defpal[i]);                 // set palette data
    }

    //    test_reg_access();

    show_blurb();

    delay_ms(2000);

    xhome();

    xprint_rainbow(1, blurb);

    delay_ms(2000);

    xvid_setw(XVID_AUX_ADDR, AUX_VID_W_GFXCTRL);        // use WR address for palette index
    xvid_setw(XVID_AUX_DATA, 0x0001);                   // set palette data

    delay_ms(2000);

    exit(EXIT_SUCCESS);
}
