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

#if TEST
#if !defined ASSERT
#define ASSERT(e)                                                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(e))                                                                                                      \
        {                                                                                                              \
            LOGF("\n\aASSERTION: %s:%d : (%s) is false\n", __FILE__, __LINE__, XM_STR(e));                             \
        }                                                                                                              \
    } while (false)
#endif
#else
#define ASSERT(e)
#endif

// rosco_m68k ANSI Terminal Functions

#define MAX_CSI_ARGS 4

enum e_term_flags
{
    TFLAG_NO_CURSOR         = 1 << 0,        // no cursor on input
    TFLAG_CURSOR_INVERTED   = 1 << 1,        // cursor currently inverted flag
    TFLAG_CTRLCHAR_PASSTHRU = 1 << 2,        // pass through control chars as graphic [HIDDEN attribute]
};

enum e_term_state
{
    TSTATE_NORMAL,
    TSTATE_ESC,
    TSTATE_CSI,
    NUM_TSTATES
};

typedef struct _ansiterm_device
{
    uint16_t vram_base;
    uint16_t vram_size;
    uint16_t cur_addr;
    uint16_t csi_args[MAX_CSI_ARGS];
    uint8_t  num_args;
    uint8_t  flags;
    uint8_t  state;
    uint8_t  color;
    uint8_t  cols, rows;
    uint8_t  x, y;

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

// terminal xosera support

_NOINLINE static void wait_vsync()
{
    xv_prep();

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

// terminal functions

static void xansi_calc_xy(ansiterm_data * td)
{
    uint32_t l = (uint16_t)(td->cur_addr - td->vram_base);
    uint16_t r;
    // GCC is annoying me and not using perfect opcode that gives division and remainder result
    __asm__ __volatile__(
        "divu.w %[w],%[l]\n"
        "move.l %[l],%[r]\n"
        "swap.w %[r]\n"
        : [l] "+d"(l), [r] "=d"(r)
        : [w] "d"((uint16_t)td->cols));
    td->y = l;
    td->x = r;
    ASSERT(td->y < td->rows && td->x < td->cols);
}

static void xansi_calc_cur_addr(ansiterm_data * td)
{
    td->cur_addr = td->vram_base + (uint16_t)(td->y * td->cols) + td->x;
    ASSERT((uint16_t)(td->cur_addr - td->vram_base) < td->vram_size);
}

static void xansi_cls(ansiterm_data * td)
{
    xv_prep();

    xm_setw(WR_INCR, 1);
    xm_setw(WR_ADDR, td->vram_base);
    xm_setbh(DATA, td->color);
    for (uint16_t i = td->vram_size; i != 0; i--)
    {
        xm_setbl(DATA, ' ');
    }
    xm_setw(WR_ADDR, td->vram_base);
    td->x        = 0;
    td->y        = 0;
    td->cur_addr = td->vram_base;
}

static _NOINLINE void xansi_visualbell(ansiterm_data * td)
{
    xv_prep();

    xm_setw(RD_INCR, 1);
    xm_setw(WR_INCR, 1);
    for (int l = 0; l < 2; l++)
    {
        xm_setw(RD_ADDR, td->vram_base);
        xm_setw(WR_ADDR, td->vram_base);
        for (uint16_t i = td->vram_size; i != 0; i--)
        {
            uint16_t data = xm_getw(DATA);
            xm_setw(DATA,
                    (((uint16_t)(data & 0xf000) >> 4) | (uint16_t)((data & 0x0f00) << 4) | (uint16_t)(data & 0xff)));
        }
    }
}

// fully reset Xosera "text mode" with defaults that should make it visible
_NOINLINE void xansi_reset(ansiterm_data * td)
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
    td->cur_addr  = 0;
    td->cols      = cols;
    td->rows      = rows;
    td->x         = 0;
    td->y         = 0;
    td->color     = 0x02;        // default dark-green on black

    xansi_cls(td);
}

static void xansi_scroll_up(ansiterm_data * td)
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

static void xansi_scroll_down(ansiterm_data * td)
{
    xv_prep();
    xm_setw(WR_INCR, -1);
    xm_setw(RD_INCR, -1);
    xm_setw(WR_ADDR, (uint16_t)(td->vram_base + td->vram_size - 1));
    xm_setw(RD_ADDR, (uint16_t)(td->vram_base + td->vram_size - td->cols - 1));
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

static void xansi_drawchar(ansiterm_data * td, char cdata)
{
    xv_prep();

    xm_setw(WR_INCR, 1);
    xm_setw(WR_ADDR, td->cur_addr++);
    xm_setbh(DATA, td->color);
    xm_setbl(DATA, cdata);

    if ((uint16_t)(td->cur_addr - td->vram_base) >= td->vram_size)
    {
        xansi_scroll_up(td);
        td->cur_addr = td->vram_base + (td->vram_size - td->cols);
    }
}

static void xansi_processchar(ansiterm_data * td, char cdata)
{
    if ((int8_t)cdata < ' ' && !(td->flags & TFLAG_CTRLCHAR_PASSTHRU))
    {
        switch (cdata)
        {
            case '\a':
                xansi_visualbell(td);
                return;
            case '\b':
                if (td->cur_addr > td->vram_base)
                {
                    td->cur_addr--;
                }
                return;
            case '\t':
                xansi_calc_xy(td);
                td->x = (td->x & ~0x7) + 8;
                if (((int16_t)td->cols - (int16_t)td->x) < 8)
                {
                    td->x = 0;
                    td->y++;
                    if (td->y >= td->rows)
                    {
                        xansi_scroll_up(td);
                        td->y = td->rows - 1;
                    }
                }
                xansi_calc_cur_addr(td);
                return;
            case '\n':
            case '\v':        // vertical tab processas as LF
                td->cur_addr += td->cols;
                if (td->cur_addr >= (uint16_t)(td->vram_base + td->vram_size))
                {
                    xansi_scroll_up(td);
                    td->cur_addr -= td->cols;
                }
                return;
            case '\f':        // FF clears screen
                xansi_cls(td);
                return;
            case '\r':
                xansi_calc_xy(td);
                td->x = 0;
                xansi_calc_cur_addr(td);
                return;
            default:        // other control chars printed
                break;
        }
    }

    xansi_drawchar(td, cdata);
}

// external terminal functions
static void xansiterm_init()
{
    LOG("ANSI: ansiterm_init\n");

    memset(&atd, 0, sizeof(atd));
    ansiterm_data * td = &atd;

    xansi_reset(td);
    xansi_cls(td);
}

bool xansiterm_checkchar()
{
    ansiterm_data * td = &atd;

    bool res = checkchar();

    if (!(td->flags & TFLAG_NO_CURSOR))
    {
        xv_prep();
        bool invert   = (xm_getw(TIMER) & 0x800) && !res;
        bool inverted = td->flags & TFLAG_CURSOR_INVERTED;
        if (inverted != invert)
        {
            td->flags ^= TFLAG_CURSOR_INVERTED;
            xm_setw(RW_INCR, 0x0000);        // NOTE: double incr hazard...
            xm_setw(RW_ADDR, td->cur_addr);
            uint16_t data = xm_getw(RW_DATA);
            // swap color attribute nibbles
            xm_setw(RW_DATA,
                    (((uint16_t)(data & 0xf000) >> 4) | (uint16_t)((data & 0x0f00) << 4) | (uint16_t)(data & 0xff)));
        }
    }

    return res;
}

char xansiterm_readchar()
{
    return readchar();
}

void xansiterm_putchar(char cdata)
{
    ansiterm_data * td = &atd;
    switch (td->state)
    {
        // fall through
        case TSTATE_NORMAL:
            if (cdata == '\x1b')
            {
                td->state = TSTATE_ESC;
                break;
            }
            xansi_processchar(td, cdata);
            break;
        case TSTATE_ESC:
            td->state = TSTATE_NORMAL;
            switch (cdata)
            {
                case '\x1b':        // print 2nd ESC
                    xansi_processchar(td, cdata);
                    break;
                case 'c':        // VT100 reset device
                    xansi_reset(td);
                    break;
                case '[':        // VT100 CSI
                    td->state = TSTATE_CSI;
                    break;
                case '(':        // TODO: VT100 font 0
                    LOG("ANSI: set FONT 0\n");
                    break;
                case ')':        // TODO: VT100 font 1
                    LOG("ANSI: set FONT 1\n");
                    break;
                case 'D':        // VT100 scroll down
                    xansi_scroll_down(td);
                    break;
                case 'M':        // VT100 scroll up
                    xansi_scroll_up(td);
                    break;
                default:
                    LOGF("ANSI: Ignoring <ESC>%c (<ESC>0x%02x)\n", (cdata >= ' ' && cdata < 0x7f) ? cdata : ' ', cdata);
                    break;
            }
            break;
        case TSTATE_CSI:
            td->state = TSTATE_NORMAL;
            switch (cdata)
            {
                case '\x1b':        // print 2nd ESC
                    xansi_processchar(td, cdata);
                    break;
                default:
                    LOGF("ANSI: Ignoring CSI <ESC>[%c (<ESC>0x%02x)\n",
                         (cdata >= ' ' && cdata < 0x7f) ? cdata : ' ',
                         cdata);
                    break;
            }
            break;
            break;
        default:
            LOGF("ANSI: bad state: %d (reset to TSTATE_NORMAL)\n", td->state);
            td->state = TSTATE_NORMAL;
            break;
    }
}

// testing harness functions

_NOINLINE static void tprint(const char * str)
{
    register char c;
    while ((c = *str++) != '\0')
    {
        if (c == '\n')
        {
            xansiterm_putchar('\r');
        }
        xansiterm_putchar(c);
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
    LOG("ANSI: Terminal test started.\n");
    xosera_init(1);
    xv_delay(3000);

    xansiterm_init();

    tprint("Welcome to ANSI Terminal test\n\n");

    tprint("Echo test, hit ^A to exit...\n\n");

    while (true)
    {
        while (!xansiterm_checkchar())
            ;

        char cdata = xansiterm_readchar();
        if (cdata == 1)
        {
            break;
        }
        xansiterm_putchar(cdata);
    }

    tprint("\n\nExiting...\n");

    atd.color = 0x02;
    xansiterm_putchar('\f');

    LOG("ANSI: Terminal test exiting.\n");
}
