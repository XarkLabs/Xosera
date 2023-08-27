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
#include <sdfat.h>

#define DELAY_TIME 5000        // human speed
// #define DELAY_TIME 1000        // impatient human speed
// #define DELAY_TIME 100        // machine speed

#include "xosera_api.h"

// Define rosco_m68k Xosera board base address pointer (See
// https://github.com/rosco-m68k/hardware-projects/blob/feature/xosera/xosera/code/pld/decoder/ic3_decoder.pld#L25)
volatile xmreg_t * const xosera_ptr = (volatile xmreg_t * const)0xf80060;        // rosco_m68k Xosera base

bool use_sd;

uint32_t vram_buffer[128 * 1024];
uint32_t vram_buffer2[128 * 1024];

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
        "move.l #6,%%d1\n"        // CHECKCHAR
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

static void dputc(char c)
{
#ifndef __INTELLISENSE__
    __asm__ __volatile__(
        "move.w %[chr],%%d0\n"
        "move.l #2,%%d1\n"        // SENDCHAR
        "trap   #14\n"
        :
        : [chr] "d"(c)
        : "d0", "d1");
#endif
}

static void dprint(const char * str)
{
    register char c;
    while ((c = *str++) != '\0')
    {
        if (c == '\n')
        {
            dputc('\r');
        }
        dputc(c);
    }
}

static char dprint_buff[4096];
static void dprintf(const char * fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(dprint_buff, sizeof(dprint_buff), fmt, args);
    dprint(dprint_buff);
    va_end(args);
}

uint16_t screen_addr;
uint8_t  text_color = 0x02;        // dark green on black
uint8_t  text_columns;
uint8_t  text_rows;
int8_t   text_h;
int8_t   text_v;

static void get_textmode_settings()
{
    //    int mode = 0;               // xv_reg_getw(gfxctrl);
    //    bool v_dbl     = mode & 2;
    int tile_size = 16;         //((xv_reg_getw(fontctrl) & 0xf) + 1) << (v_dbl ? 1 : 0);
    screen_addr   = 0;          // xv_reg_getw(dispstart);
    text_columns  = 106;        // xv_reg_getw(dispwidth);
    //    text_rows      = (xv_reg_getw(vidheight) + (tile_size - 1)) / tile_size;
    text_rows = 480 / tile_size;
}

static void xcls()
{
    get_textmode_settings();
    xv_setw(wr_addr, screen_addr);
    xv_setw(wr_inc, 1);
    xv_setbh(data, text_color);
    for (uint16_t i = 0; i < (text_columns * text_rows); i++)
    {
        xv_setbl(data, ' ');
    }
    xv_setw(wr_addr, screen_addr);
}

static void xmsg(int x, int y, int color, const char * msg)
{
    xv_setw(wr_addr, (y * text_columns) + x);
    xv_setbh(data, color);
    char c;
    while ((c = *msg++) != '\0')
    {
        xv_setbl(data, c);
    }
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
    dprintf("rosco_m68k: m68k CPU speed %d.%d MHz (%d.%d BogoMIPS)\n",
            MHz / 10,
            MHz % 10,
            count * 3 / 10000,
            ((count * 3) % 10000) / 10);

    return (MHz + 5) / 10;
}


uint32_t test_count;
void     xosera_gfx_test()
{
    printf("\033c\033[?25l");        // ANSI reset, disable input cursor

    dprintf("Xosera_gfx_test\n");

    dprintf("\nxosera_init(1)...");
    // wait for monitor to unblank
    bool success = xosera_init(1);
    dprintf("%s (%dx%d)\n", success ? "succeeded" : "FAILED", xv_reg_getw(vidwidth), xv_reg_getw(vidheight));

    if (delay_check(4000))
    {
        return;
    }

    while (true)
    {
        xv_reg_setw(gfxctrl, 0x0000);
        xcls();
        dprintf("*** Xosera_gfx_test iteration: %d\n", test_count++);
        rosco_m68k_CPUMHz();

        uint32_t githash   = (xv_reg_getw(githash_h) << 16) | xv_reg_getw(githash_l);
        uint16_t width     = xv_reg_getw(vidwidth);
        uint16_t height    = xv_reg_getw(vidheight);
        uint16_t feature   = xv_reg_getw(feature);
        uint16_t dispstart = xv_reg_getw(dispstart);
        uint16_t dispwidth = xv_reg_getw(dispwidth);
        uint16_t scrollxy  = xv_reg_getw(scrollxy);
        uint16_t gfxctrl   = xv_reg_getw(gfxctrl);

        dprintf("Xosera #%08x\n", githash);
        dprintf("Mode: %dx%d  Features:0x%04x\n", width, height, feature);
        dprintf(" dispstart:0x%04x dispwidth:0x%04x\n", dispstart, dispwidth);
        dprintf("  scrollxy:0x%04x   gfxctrl:0x%04x\n", scrollxy, gfxctrl);

        xv_reg_setw(gfxctrl, 0x0000);

        if (delay_check(4000))
        {
            break;
        }
    }

    xv_reg_setw(gfxctrl, 0x0000);

    dprintf("Exit!\n");
    while (checkchar())
    {
        readchar();
    }
}
