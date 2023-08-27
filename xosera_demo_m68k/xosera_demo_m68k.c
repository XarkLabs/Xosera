/*
 * vim: set et ts=4 sw=4
 *------------------------------------------------------------
 *                                  ___ ___ _
 *  ___ ___ ___ ___ ___       _____|  _| . | |_
 * |  _| . |_ -|  _| . |     |     | . | . | '_|
 * |_| |___|___|___|___|_____|_|_|_|___|___|_,_|
 *                     |_____|
 * ------------------------------------------------------------
 * Copyright (c) 2021 Xark
 * MIT License
 *
 * Test and tech-demo for Xosera FPGA "graphics card"
 * ------------------------------------------------------------
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <basicio.h>
#include <machine.h>

#define DELAY_TIME 5000        // human speed
// #define DELAY_TIME 1000        // impatient human speed
// #define DELAY_TIME 100        // machine speed

#include "xosera_api.h"

// Define rosco_m68k Xosera board base address pointer (See
// https://github.com/rosco-m68k/hardware-projects/blob/feature/xosera/xosera/code/pld/decoder/ic3_decoder.pld#L25)
volatile xmreg_t * const xosera_ptr = (volatile xmreg_t * const)0xf80060;        // rosco_m68k Xosera base

// dummy global variable
uint32_t global;        // this is used to prevent the compiler from optimizing out tests

// timer helpers
static uint32_t start_tick;

void timer_start()
{
    uint32_t ts = _TIMER_100HZ;
    uint32_t t;
    // this waits for a "fresh tick" to reduce timing jitter
    while ((t = _TIMER_100HZ) == ts)
        ;
    start_tick = t;
}

uint32_t timer_stop()
{
    uint32_t stop_tick = _TIMER_100HZ;

    return (stop_tick - start_tick) * 10;
}

#if !defined(checkchar)        // newer rosco_m68k library addition, this is in case not present
bool checkchar()
{
    int rc;
    __asm__ __volatile__(
        "move.l #6,%%d1\n"
        "trap   #14\n"
        "move.b %%d0,%[rc]\n"
        "ext.w  %[rc]\n"
        "ext.l  %[rc]\n"
        : [rc] "=d"(rc)
        :
        : "d0", "d1");
    return rc != 0;
}
#endif

bool delay_check(int ms)
{
    while (ms > 0)
    {
        if (checkchar())
        {
            return true;
        }

        int d = ms;
        if (d > 100)
        {
            d = 100;
        }
        delay(d);
        ms -= d;
    }

    return false;
}

uint16_t screen_addr;
uint8_t  text_color = 0x02;        // dark green on black
uint8_t  text_columns;
uint8_t  text_rows;
int8_t   text_h;
int8_t   text_v;

static void get_textmode_settings()
{
    int  mode      = xv_reg_getw(gfxctrl);
    bool v_dbl     = mode & 2;
    int  tile_size = ((xv_reg_getw(fontctrl) & 0xf) + 1) << (v_dbl ? 1 : 0);
    screen_addr    = xv_reg_getw(dispstart);
    text_columns   = xv_reg_getw(dispwidth);
    text_rows      = (xv_reg_getw(vidheight) + (tile_size - 1)) / tile_size;
}

static void xpos(uint8_t h, uint8_t v)
{
    text_h = h;
    text_v = v;
}

static void xcolor(uint8_t color)
{
    text_color = color;
}

static void xhome()
{
    get_textmode_settings();
    xpos(0, 0);
}

static void xcls()
{
    // clear screen
    xhome();
    xv_setw(wr_addr, screen_addr);
    xv_setw(wr_inc, 1);
    xv_setbh(data, text_color);
    for (uint16_t i = 0; i < (text_columns * text_rows); i++)
    {
        xv_setbl(data, ' ');
    }
    xv_setw(wr_addr, screen_addr);
}

static void xprint(const char * str)
{
    xv_setw(wr_inc, 1);
    xv_setw(wr_addr, screen_addr + (text_v * text_columns) + text_h);
    xv_setbh(data, text_color);

    char c;
    while ((c = *str++) != '\0')
    {
        if (c >= ' ')
        {
            xv_setbl(data, c);
            if (++text_h >= text_columns)
            {
                text_h = 0;
                if (++text_v >= text_rows)
                {
                    text_v = 0;
                }
            }
            continue;
        }
        switch (c)
        {
            case '\r':
                text_h = 0;
                xv_setw(wr_addr, screen_addr + (text_v * text_columns) + text_h);
                break;
            case '\n':
                text_h = 0;
                if (++text_v >= text_rows)
                {
                    text_v = text_rows - 1;
                }
                xv_setw(wr_addr, screen_addr + (text_v * text_columns) + text_h);
                break;
            case '\b':
                if (--text_h < 0)
                {
                    text_h = text_columns - 1;
                    if (--text_v < 0)
                    {
                        text_v = 0;
                    }
                }
                xv_setw(wr_addr, screen_addr + (text_v * text_columns) + text_h);
                break;
            case '\f':
                xcls();
                break;
            default:
                break;
        }
    }
}

static void xprint_rainbow(const char * str)
{
    xv_setw(wr_inc, 1);
    xv_setw(wr_addr, screen_addr + (text_v * text_columns) + text_h);
    xv_setbh(data, text_color);

    char c;
    while ((c = *str++) != '\0')
    {
        if ((uint8_t)c >= ' ')
        {
            xv_setbl(data, c);
            if (++text_h >= text_columns)
            {
                text_h = 0;
                if (++text_v >= text_rows)
                {
                    text_v = 0;
                }
            }
            continue;
        }
        switch (c)
        {
            case '\r':
                text_h = 0;
                xv_setw(wr_addr, screen_addr + (text_v * text_columns) + text_h);
                break;
            case '\n':
                text_h = 0;
                if (++text_v >= text_rows)
                {
                    text_v = text_rows - 1;
                }
                xv_setw(wr_addr, screen_addr + (text_v * text_columns) + text_h);
                text_color = ((text_color + 1) & 0xf);
                if (!text_color)
                {
                    text_color++;
                }
                xv_setbh(data, text_color);
                break;
            case '\b':
                if (--text_h < 0)
                {
                    text_h = text_columns - 1;
                    if (--text_v < 0)
                    {
                        text_v = 0;
                    }
                }
                xv_setw(wr_addr, screen_addr + (text_v * text_columns) + text_h);
                break;
            case '\f':
                xcls();
                break;
            default:
                break;
        }
    }
}


static char xprint_buff[4096];
static void xprintf(const char * fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(xprint_buff, sizeof(xprint_buff), fmt, args);
    xprint(xprint_buff);
    va_end(args);
}

const char blurb[] =
    "\n"
    "Xosera is an FPGA based video adapter designed with the rosco_m68k retro\n"
    "computer in mind. Inspired in concept by it's \"namesake\" the Commander X16's\n"
    "VERA, Xosera is an original open-source video adapter design, built with open-\n"
    "source tools, that is being tailored with features appropriate for a Motorola\n"
    "68K era retro computer, such as the rosco_m68k (or even an 8-bit CPU).\n"
    "\n"
    "  \xf9  VGA or HDMI/DVI output at 848x480 or 640x480 (16:9 or 4:3 @ 60Hz)\n"
    "  \xf9  256 color palette out of 4096 colors (12-bit RGB)\n"
    "  \xf9  128KB of embedded video RAM (16-bit words @33/25 MHz)\n"
    "  \xf9  Register based interface with 16 16-bit registers\n"
    "  \xf9  Read/write VRAM with programmable read/write address increment\n"
    "  \xf9  Fast 8-bit bus interface (using MOVEP) for rosco_m68k (by Ross Bamford)\n"
    "  \xf9  Fonts writable in VRAM or in dedicated 8KB of font memory\n"
    "  \xf9  Multiple fonts (2KB per 8x8 fonts, 4K per 8x16 font)\n"
    "  \xf9  8x8 or 8x16 character tile size (or truncated e.g., 8x10)\n"
    "  \xf9  Character tile based modes with color attribute byte\n"
    "  \xf9  Horizontal and/or veritical pixel doubling (e.g. 424x240 or 320x240)\n"
    "  \xf9  Smooth horizontal and vertical tile scrolling\n"
    "  \xf9  2-color full-res bitmap mode (with attribute per 8 pixels, ala Sinclair)\n"
    "  \xf9  TODO: Two 16 color \"planes\" or combined for 256 colors\n"
    "  \xf9  TODO: Bit-mapped 16 and 256 color graphics modes\n"
    "  \xf9  TODO: 16-color tile mode with \"game\" attributes (e.g., mirroring)\n"
    "  \xf9  TODO: \"Blitter\" for fast VRAM copy & fill operations\n"
    "  \xf9  TODO: 2-D operations \"blitter\" with modulo and shifting/masking\n"
    "  \xf9  TODO: At least one \"cursor\" sprite (or more)\n"
    "  \xf9  TODO: Wavetable stereo audio (spare debug GPIOs for now)\n";

void test_blurb()
{
    // Show some text
    //    xcls();
    xprint_rainbow(blurb);
}

void test_hello()
{
    static const char test_string[] = "Xosera is mostly running happily on rosco_m68k";
    static uint16_t   test_read[sizeof(test_string)];

    xcls();
    xv_setw(wr_inc, 1);                            // set write inc
    xv_setw(wr_addr, 0x0000);                      // set write address
    xv_setw(data, 0x0200 | test_string[0]);        // set full word
    for (size_t i = 1; i < sizeof(test_string) - 1; i++)
    {
        if (i == sizeof(test_string) - 5)
        {
            xv_setbh(data, 0x04);        // test setting bh only (saved, VRAM not altered)
        }
        xv_setbl(data, test_string[i]);        // set byte, will use continue using previous high byte (0x20)
    }

    // read test
    xv_setw(rd_inc, 1);
    xv_setw(rd_addr, 0x0000);
    xpos(0, 8);
    xcolor(0x07);
    xprintf("Read VRAM test, with auto-increment.\n\n");
    xprintf("  Start rd_addr=0x0000, rd_inc=0x0001\n\n");

    uint16_t * tp = test_read;
    for (uint16_t c = 0; c < (sizeof(test_string) - 1); c++)
    {
        *tp++ = xv_getw(data);
    }

    xv_setw(wr_inc, 1);                        // set write inc
    xv_setw(wr_addr, text_columns * 2);        // set write address

    bool good = true;
    for (size_t i = 0; i < sizeof(test_string) - 1; i++)
    {
        uint16_t v = test_read[i];
        xv_setw(data, v);
        if ((v & 0xff) != test_string[i])
        {
            good = false;
        }
    }
    uint16_t end_addr = xv_getw(rd_addr);
    // incremented one extra, because data was already pre-read
    if (end_addr != sizeof(test_string))
    {
        good = false;
    }
    xprintf("Ending rd_addr=0x%04x.  Test: ", end_addr);
    xcolor(good ? 0x02 : 0x4f);
    xprintf("%s\n", good ? "good" : "BAD!");
    xcolor(0x02);
}

void test_mono_bitmap()
{
    xv_reg_setw(gfxctrl, 0x0040);
}


uint16_t rosco_m68k_CPUMHz()
{
    uint32_t count;
    uint32_t tv;
    __asm__ __volatile__(
        "   moveq.l #0,%[count]\n"
        "   move.w  _TIMER_100HZ+2.w,%[tv]\n"
        "0: cmp.w   _TIMER_100HZ+2.w,%[tv]\n"
        "   beq.s   0b\n"
        "   move.w  _TIMER_100HZ+2.w,%[tv]\n"
        "1: addq.w  #1,%[count]\n"                   //   4  cycles
        "   cmp.w   _TIMER_100HZ+2.w,%[tv]\n"        //  12  cycles
        "   beq.s   1b\n"                            // 10/8 cycles (taken/not)
        : [count] "=d"(count), [tv] "=&d"(tv)
        :
        :);
    uint16_t MHz = ((count * 26) + 500) / 1000;
    xprintf("rosco_m68k: m68k CPU speed %d.%d MHz (%d.%d BogoMIPS)\n",
            MHz / 10,
            MHz % 10,
            count * 3 / 10000,
            ((count * 3) % 10000) / 10);

    return (MHz + 5) / 10;
}

uint32_t test_count;
void     xosera_demo()
{
    printf("\033c\033[?25l");        // ANSI reset, disable input cursor

    printf("\nxosera_init(1)...");
    // wait for monitor to unblank
    bool success = xosera_init(1);
    printf("%s (%dx%d)\n", success ? "succeeded" : "FAILED", xv_reg_getw(vidwidth), xv_reg_getw(vidheight));

    if (delay_check(5000))
    {
        return;
    }

    while (true)
    {
        xcolor(0x02);
        xcls();
        xprintf("*** xosera_demo_m68k iteration: %d\n", test_count++);
        rosco_m68k_CPUMHz();

        uint32_t githash   = (xv_reg_getw(githash_h) << 16) | xv_reg_getw(githash_l);
        uint16_t width     = xv_reg_getw(vidwidth);
        uint16_t height    = xv_reg_getw(vidheight);
        uint16_t feature   = xv_reg_getw(feature);
        uint16_t dispstart = xv_reg_getw(dispstart);
        uint16_t dispwidth = xv_reg_getw(dispwidth);
        uint16_t scrollxy  = xv_reg_getw(scrollxy);
        uint16_t gfxctrl   = xv_reg_getw(gfxctrl);

        xprintf("Xosera #%08x\n", githash);
        xprintf("Mode: %dx%d  Features:0x%04x\n", width, height, feature);
        xprintf("dispstart:0x%04x dispwidth:0x%04x\n", dispstart, dispwidth);
        xprintf(" scrollxy:0x%04x   gfxctrl:0x%04x\n", scrollxy, gfxctrl);

        if (delay_check(DELAY_TIME))
        {
            break;
        }

        xcolor(0x02);
        xcls();
        rosco_m68k_CPUMHz();

        test_blurb();
        if (delay_check(DELAY_TIME))
        {
            break;
        }

        test_hello();
        if (delay_check(DELAY_TIME))
        {
            break;
        }

        test_mono_bitmap();
        if (delay_check(DELAY_TIME))
        {
            break;
        }
    }

    while (checkchar())
    {
        readchar();
    }
}
