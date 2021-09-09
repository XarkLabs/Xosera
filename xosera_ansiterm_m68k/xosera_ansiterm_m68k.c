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
 * Proto-ANSI terminal emulation WIP
 * ------------------------------------------------------------
 */

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <basicio.h>
#include <machine.h>

#define XV_PREP_REQUIRED        // require xv_prep()
#include "xosera_m68k_api.h"

#define TEST 1

#if !defined(_NOINLINE)
#define _NOINLINE __attribute__((noinline))
#endif

#if !defined(NUM_ELEMENTS)
#define NUM_ELEMENTS(arr) (sizeof((arr)) / sizeof((arr)[0]))
#endif
// rosco_m68k ANSI Terminal Functions

typedef struct _ansiterm_device
{
    uint16_t vram_base;
    uint16_t vram_size;
    uint8_t  cols, rows;
    uint8_t  color, pad;
    uint8_t  cur_x, cur_y;
    uint16_t cur_addr;

} ansiterm_data;

ansiterm_data atd;

// Xosera default 16-color palette
uint16_t def_colors16[256] = {
    0x0000,
    0x000a,
    0x00a0,
    0x00aa,
    0x0a00,
    0x0a0a,
    0x0aa0,
    0x0aaa,
    0x0555,
    0x055f,
    0x05f5,
    0x05ff,
    0x0f55,
    0x0f5f,
};

#if TEST

#if !defined(checkchar)        // newer rosco_m68k library addition, this is in case not present
_NOINLINE bool checkchar()
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

_NOINLINE bool delay_check(int ms)
{
    xv_prep();
    uint16_t tms = ms * 10;
    uint16_t t   = xm_getw(TIMER);
    while (tms > (xm_getw(TIMER) - t))
    {
        if (checkchar())
        {
            return true;
        }
    }

    return false;
}

_NOINLINE static void dputc(char c)
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

_NOINLINE static void dprint(const char * str)
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

static char           dprint_buff[4096];
_NOINLINE static void dprintf(const char * fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(dprint_buff, sizeof(dprint_buff), fmt, args);
    dprint(dprint_buff);
    va_end(args);
}

#define LOG(msg)       dprintf(msg)
#define LOGF(fmt, ...) dprintf(fmt, ##__VA_ARGS__)

#else
#define LOG(msg)
#define LOGF(fmt, ...)
#endif

// terminal support

_NOINLINE static void wait_vsync()
{
    const uint16_t VSYNC_MARGIN_LINES = 20;        // scanlines into vblank early enough for register writes

    xv_prep();

    uint16_t vsize = xreg_getw(VID_VSIZE);
    uint16_t sline = xreg_getw(SCANLINE);
    // if in first 'margin' lines of vblank, return now
    if ((sline & 0x8000) && ((sline & 0x7ff) < (vsize + VSYNC_MARGIN_LINES)))
        return;
    // wait until not vblank (if in vblank)
    while (xreg_getw(SCANLINE) & 0x8000)
        ;
    // wait until vlbank
    while (!(xreg_getw(SCANLINE) & 0x8000))
        ;
}

_NOINLINE void set_defaut_palette()
{
    xv_prep();
    xm_setw(XR_ADDR, XR_COLOR_MEM);
    uint16_t * cp = def_colors16;
    for (uint16_t i = 0; i < 16; i++)
    {
        xm_setw(XR_DATA, *cp++);
    };
}

// fully reset Xosera "text mode" with defaults that should make it visible
_NOINLINE void text_reset(ansiterm_data * td)
{
    xv_prep();
    uint16_t cols = xreg_getw(VID_HSIZE) >> 3;        // get pixel width / 8 for 8x16 text columns
    uint16_t rows = xreg_getw(VID_VSIZE) >> 4;        // get pixel height / 16 for 8x16 text rows
    // set xosera playfield A registers
    wait_vsync();
    xreg_setw(PA_GFX_CTRL, MAKE_GFX_CTRL(0x00, 0, 0, 0, 0, 0));        // graphics mode
    xm_setw(XR_DATA, MAKE_TILE_CTRL(0x0000, 0, 15));                   // tile mode
    xm_setw(XR_DATA, 0x0000);                                          // disp addr
    xm_setw(XR_DATA, cols);                                            // line len
    xm_setw(XR_DATA, 0x0000);                                          // hv scroll
    xm_setw(XR_DATA, 0x0000);                                          // line addr
    xm_setw(XR_DATA, 0x0000);                                          // unused
    xm_setw(XR_DATA, 0x0000);                                          // unused

    set_defaut_palette();

    td->vram_base = 0;
    td->vram_size = cols * rows;
    td->cols      = cols;
    td->rows      = rows;
    td->cur_x     = 0;
    td->cur_y     = 0;
    td->color     = 0x02;        // default dark-green on black
}

// terminal functions

void cls(ansiterm_data * td)
{
    xv_prep();

    xm_setw(WR_INCR, 1);
    xm_setw(WR_ADDR, td->vram_base);
    xm_setbh(DATA, td->color);
    for (uint16_t i = td->rows * td->cols; i != 0; i--)
    {
        xm_setbl(DATA, ' ');
    }
    xm_setw(WR_ADDR, td->vram_base);
    td->cur_x    = 0;
    td->cur_y    = 0;
    td->cur_addr = td->vram_base;
}

void ansiterm_init(ansiterm_data * td)
{
    text_reset(td);
    cls(td);
}

void ansiterm_scroll_up(ansiterm_data * td)
{
    xv_prep();
    xm_setw(WR_INCR, 1);
    xm_setw(RD_INCR, 1);
    xm_setw(WR_ADDR, td->vram_base);
    xm_setw(RD_ADDR, td->vram_base + td->cols);
    for (uint16_t i = td->vram_size - td->cols; i != 0; i--)
    {
        xm_setw(DATA, xm_getw(DATA));
    }
    xm_setbh(DATA, td->color);
    for (uint16_t i = td->cols; i != 0; i--)
    {
        xm_setbl(DATA, ' ');
    }
}

void ansiterm_drawchar(ansiterm_data * td, char c)
{
    xv_prep();
    xm_setw(WR_INCR, 1);
    xm_setw(WR_ADDR, td->cur_addr++);
    xm_setbh(DATA, td->color);
    xm_setbl(DATA, c);

    if ((td->cur_addr - td->vram_base) > td->vram_size)
    {
        ansiterm_scroll_up(td);
        td->cur_addr -= td->cols;
    }
}

void ansiterm_putchar(char c)
{
    ansiterm_drawchar(&atd, c);
}

// testing harness

_NOINLINE void xmsg(ansiterm_data * td, const char * msg)
{
    xv_prep();

    xm_setw(WR_ADDR, td->cur_addr);
    xm_setbh(DATA, td->color);
    char c;
    while ((c = *msg++) != '\0')
    {
        xm_setbl(DATA, c);
    }
    xm_setw(WR_ADDR, td->cur_addr);
}

_NOINLINE static void tprint(const char * str)
{
    register char c;
    while ((c = *str++) != '\0')
    {
        if (c == '\n')
        {
            ansiterm_putchar('\r');
        }
        ansiterm_putchar(c);
    }
}

static char           tprint_buff[4096];
_NOINLINE static void tprintf(const char * fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(tprint_buff, sizeof(tprint_buff), fmt, args);
    tprint(tprint_buff);
    va_end(args);
}

void xosera_ansiterm()
{
    LOG("ANSI terminal started.\n");
    xosera_init(0);
    xv_delay(100);
    ansiterm_init(&atd);

    do
    {
        tprint("ANSI Terminal Test! \n");
    } while (!checkchar());

    cls(&atd);
    xmsg(&atd, "ANSI terminal test exit.");

    LOG("ANSI terminal test exit.\n");
}
