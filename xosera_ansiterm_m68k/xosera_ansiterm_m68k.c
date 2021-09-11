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

#if defined(printf)        // this interferes with gcc format attributes
#undef printf
#endif

#define XV_PREP_REQUIRED        // require xv_prep()
#include "xosera_m68k_api.h"

#define DEBUG 0        // set to 1 for debugging

#if !defined(_NOINLINE)
#define _NOINLINE __attribute__((noinline))
#endif

#if DEBUG
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

static char dprint_buff[4096];
static void dprintf(const char * fmt, ...) __attribute__((format(printf, 1, 2)));
static void dprintf(const char * fmt, ...)
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

//
// rosco_m68k ANSI Terminal Functions
//

#define MAX_CSI_PARMS 16

enum e_term_flags
{
    TFLAG_8X8_FONT        = 1 << 0,        // use alternate 8x8 font
    TFLAG_CURSOR_INVERTED = 1 << 1,        // cursor currently inverted flag
    TFLAG_NO_AUTOWRAP     = 1 << 2,        // no cursor on input
    TFLAG_NEWLINE         = 1 << 3,        // no cursor on input
    TFLAG_HIDE_CURSOR     = 1 << 4,        // no cursor on input
    TFLAG_ATTRIB_BRIGHT   = 1 << 5,        // make colors bright
    TFLAG_ATTRIB_DIM      = 1 << 6,        // make colors dim
    TFLAG_ATTRIB_PASSTHRU = 1 << 7,        // pass through control chars as graphic [HIDDEN attribute]

};

enum e_term_state
{
    TSTATE_NORMAL,
    TSTATE_ESC,
    TSTATE_CSI_START,
    TSTATE_CSI,
    NUM_TSTATES
};

typedef struct _ansiterm_device
{
    uint16_t cur_addr;
    uint16_t vram_base;
    uint16_t vram_end;
    uint16_t vram_size;
    uint16_t csi_parms[MAX_CSI_PARMS];
    uint16_t cursor_save;
    uint8_t  state;
    uint8_t  flags;
    uint8_t  x, y, lcf;
    uint8_t  save_x, save_y, save_lcf;
    uint8_t  cols, rows;
    uint8_t  color;
    uint8_t  def_color;
    uint8_t  num_parms;
    uint8_t  intermediate_char;
} ansiterm_data;

ansiterm_data atd;

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

static void set_defaut_palette()
{
    static uint16_t def_colors16[16] = {
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
    xv_prep();
    xm_setw(XR_ADDR, XR_COLOR_MEM);
    uint16_t * cp = def_colors16;
    for (uint16_t i = 0; i < 16; i++)
    {
        xm_setw(XR_DATA, *cp++);
    };
}

static void xansi_calc_xy(ansiterm_data * td)
{
    uint32_t l = td->cur_addr - td->vram_base;
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
}

static uint16_t xansi_calc_addr(ansiterm_data * td, uint16_t x, uint16_t y)
{
    return td->vram_base + (uint16_t)(y * td->cols) + x;
}

static void xansi_calc_cur_addr(ansiterm_data * td)
{
    td->cur_addr = xansi_calc_addr(td, td->x, td->y);
}

// fully reset Xosera "text mode" with defaults that should make it visible
static void xansi_vidreset(ansiterm_data * td)
{
    xv_prep();
    bool alt_font = td->flags & TFLAG_8X8_FONT;
    // set xosera playfield A registers
    uint16_t tile_addr   = alt_font ? 0x800 : 0x0000;
    uint8_t  tile_height = alt_font ? 7 : 15;
    uint16_t cols        = xreg_getw(VID_HSIZE) >> 3;        // get pixel width / 8 for text columns

    wait_vsync();
    xreg_setw(PA_GFX_CTRL, MAKE_GFX_CTRL(0x00, 0, 0, 0, 0, 0));         // graphics mode
    xm_setw(XR_DATA, MAKE_TILE_CTRL(tile_addr, 0, tile_height));        // tile mode
    xm_setw(XR_DATA, td->vram_base);                                    // disp addr
    xm_setw(XR_DATA, cols);                                             // line len
    xm_setw(XR_DATA, 0x0000);                                           // hv scroll
    xm_setw(XR_DATA, 0x0000);                                           // line addr
    xm_setw(XR_DATA, 0x0000);                                           // unused
    xm_setw(XR_DATA, 0x0000);                                           // unused

    set_defaut_palette();
}

static void xansi_reset(ansiterm_data * td)
{
    xv_prep();
    uint8_t  th   = ((uint8_t)xreg_getw(PA_TILE_CTRL) & 0xf) + 1;        // get font height
    uint16_t rows = (xreg_getw(VID_VSIZE) + th - 1) / th;                // get text rows
    uint16_t cols = xreg_getw(VID_HSIZE) >> 3;                           // get text columns

    td->vram_base = 0;
    td->vram_size = cols * rows;
    td->vram_end  = td->vram_size;
    td->cols      = cols;
    td->rows      = rows;
    td->color     = 0x02;        // default dark-green on black
    td->def_color = 0x02;        // default dark-green on black

    if (td->x >= cols)
    {
        td->x   = cols - 1;
        td->lcf = 0;
    }
    if (td->y > rows)
    {
        td->y = rows - 1;
    }
    xansi_calc_cur_addr(td);
}

static void xansi_invertcursor(ansiterm_data * td)
{
    xv_prep();
    xm_setw(RW_INCR, 0x0000);
    xm_setw(RW_ADDR, td->cur_addr);
    uint16_t data = xm_getw(RW_DATA);
    // swap color attribute nibbles
    td->flags ^= TFLAG_CURSOR_INVERTED;
    if (td->flags & TFLAG_CURSOR_INVERTED)
    {
        td->cursor_save = data;
        if (data & 0x8000)
        {
            xm_setw(RW_DATA, (((td->color & ~0x08) & 0xf) << 12) | (data & 0x0fff));
        }
        else
        {
            xm_setw(RW_DATA, (((td->color | 0x08) & 0xf) << 12) | (data & 0x0fff));
        }
    }
    else
    {
        xm_setw(RW_DATA, (uint16_t)(td->cursor_save | (uint16_t)(data & 0xff)));
    }
}

static void xansi_clear(ansiterm_data * td, uint16_t start, uint16_t end)
{
    if (start > end)
    {
        uint16_t t = start;
        start      = end;
        end        = t;
    }
    xv_prep();
    xm_setw(WR_INCR, 1);
    xm_setw(WR_ADDR, start);
    xm_setbh(DATA, td->color);
    do
    {
        xm_setbl(DATA, ' ');
    } while (++start <= end);
}

static void xansi_cls(ansiterm_data * td)
{
    xansi_clear(td, td->vram_base, td->vram_end);
    td->x   = 0;
    td->y   = 0;
    td->lcf = 0;

    td->cur_addr = td->vram_base;
}

static void xansi_visualbell(ansiterm_data * td)
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

static void xansi_do_scroll(ansiterm_data * td)
{
    xv_prep();
    uint16_t i;
    for (i = td->vram_size - td->cols; i > 1; i -= 2)
    {
        xm_setl(DATA, xm_getl(DATA));
    }
    if (i)
    {
        xm_setw(DATA, xm_getw(DATA));
    }
    xm_setbh(DATA, td->color);
    for (uint16_t i = td->cols; i != 0; i--)
    {
        xm_setbl(DATA, ' ');
    }
}

static void xansi_scroll_up(ansiterm_data * td)
{
    xv_prep();
    xm_setw(WR_INCR, 1);
    xm_setw(RD_INCR, 1);
    xm_setw(WR_ADDR, td->vram_base);
    xm_setw(RD_ADDR, td->vram_base + td->cols);
    xansi_do_scroll(td);
}

static void xansi_scroll_down(ansiterm_data * td)
{
    xv_prep();
    xm_setw(WR_INCR, -1);
    xm_setw(RD_INCR, -1);
    xm_setw(WR_ADDR, (uint16_t)(td->vram_base + td->vram_size - 1));
    xm_setw(RD_ADDR, (uint16_t)(td->vram_base + td->vram_size - td->cols - 1));
    xansi_do_scroll(td);
}
static void xansi_check_lcf(ansiterm_data * td)
{
    if (td->lcf)
    {
        td->x = 0;
        td->y += 1;
        td->cur_addr += 1;

        if ((uint16_t)(td->cur_addr - td->vram_base) >= td->vram_size)
        {
            xansi_scroll_up(td);
            td->cur_addr = td->vram_base + (td->vram_size - td->cols);
        }
        td->lcf = 0;
    }
}

static void xansi_drawchar(ansiterm_data * td, char cdata)
{
    xv_prep();
    xansi_check_lcf(td);

    xm_setw(WR_ADDR, td->cur_addr);
    xm_setbh(DATA, td->color);
    xm_setbl(DATA, cdata);

    td->x += 1;
    if (td->x < td->cols)
    {
        td->cur_addr += 1;
    }
    else
    {
        td->x   = td->cols - 1;
        td->lcf = !(td->flags & TFLAG_NO_AUTOWRAP);
    }
}

static void xansi_processchar(ansiterm_data * td, char cdata)
{
    if ((int8_t)cdata < ' ' && !(td->flags & TFLAG_ATTRIB_PASSTHRU))
    {
        xansi_calc_xy(td);
        switch (cdata)
        {
            case '\a':
                xansi_visualbell(td);
                return;
            case '\b':
                if (td->x > 0)
                {
                    td->x -= 1;
                }
                break;
            case '\t':        // NOTE: wraps, VT100 clamped at right margin
                td->x = (uint8_t)(td->x & ~0x7) + 8;
                if ((uint8_t)(td->cols - td->x) < 8)
                {
                    td->x = 0;
                    td->y++;
                }
                break;
            case '\n':
                td->y++;
                break;
            case '\v':        // vertical tab is cursor up
                td->y--;
                if (td->y >= td->rows)
                {
                    td->y = 0;
                    xansi_scroll_down(td);
                }
                break;
            case '\f':        // FF clears screen
                xansi_cls(td);
                break;
            case '\r':
                td->x = 0;
                break;
            default:        // suppress others
                return;
        }
        if (td->y >= td->rows)
        {
            td->y = td->rows - 1;
            xansi_scroll_up(td);
        }
        xansi_calc_cur_addr(td);
        td->lcf = 0;
        return;
    }

    xansi_drawchar(td, cdata);
}

static void xansi_begin_esc(ansiterm_data * td)
{
    td->state             = TSTATE_ESC;
    td->intermediate_char = 0;
    td->num_parms         = 0;
    memset(td->csi_parms, 0, sizeof(td->csi_parms));
    xansi_calc_xy(td);
}

static void xansi_begin_csi(ansiterm_data * td)
{
    xansi_begin_esc(td);
    td->state = TSTATE_CSI;
}

static void xansi_process_csi(ansiterm_data * td, char cdata)
{
    LOG("<CSI>");
    if (td->intermediate_char)
    {
        LOGF("%c", td->intermediate_char);
    }
    for (uint16_t i = 0; i < td->num_parms; i++)
    {
        LOGF("%s%d", i ? ";" : "", td->csi_parms[i]);
    }
    LOGF("%c = ", cdata);

    // debug stuff
    uint8_t initial_flags = td->flags;
    uint8_t initial_col   = td->color;
    uint8_t initial_x     = td->x;
    uint8_t initial_y     = td->y;
    bool    def_flag      = false;
    (void)initial_flags;        // no warnings if unused
    (void)initial_col;          // no warnings if unused
    (void)initial_x;            // no warnings if unused
    (void)initial_y;            // no warnings if unused
    (void)def_flag;             // no warnings if unused

    uint16_t num_z = td->csi_parms[0];         // default zero
    uint16_t num   = num_z ? num_z : 1;        // default zero to one

    switch (cdata)
    {
        case 'f':        // Cursor Home / position (force)
        case 'H':        // Cursor Home / position
            td->x   = 0;
            td->y   = 0;
            td->lcf = 0;
            if (td->num_parms > 0 && td->csi_parms[0] < td->rows)
            {
                td->y = td->csi_parms[0] - 1;
            }
            if (td->num_parms > 1 && td->csi_parms[1] < td->cols)
            {
                td->x = td->csi_parms[1] - 1;
            }
            LOGF("[CPOS %d,%d]", td->x, td->y);
            break;
        case 'h':        // <CSI>?3h Select 132 Columns per Page
        case 'l':        // <CSI>?3l Select 80 Columns per Page
            if (td->intermediate_char == '?')
            {
                if (num == 3)
                {
                    uint16_t res = (cdata == 'h') ? 848 : 640;
                    xv_prep();
                    if (xreg_getw(VID_HSIZE) != res)
                    {
                        uint16_t config = (res == 640) ? 0 : 1;
                        LOGF("<reconfig #%d>\n", config);
                        xosera_init(config);
                        xansi_vidreset(td);
                        xansi_reset(td);
                        xansi_cls(td);
                        LOGF("[SET %dx%d MODE]", td->rows, td->cols);
                    }
                }
                else if (num == 7)
                {
                    if (cdata == 'l')
                    {
                        LOG("[WRAP OFF]");
                        td->flags |= TFLAG_NO_AUTOWRAP;
                    }
                    else
                    {
                        LOG("[WRAP ON]");
                        td->flags &= ~TFLAG_NO_AUTOWRAP;
                        if (td->x >= td->cols - 1)
                        {
                            td->lcf = 1;
                        }
                    }
                }
                else if (num == 25)
                {
                    if (cdata == 'l')
                    {
                        LOG("[CURSOR HIDE]");
                        td->flags |= TFLAG_HIDE_CURSOR;
                    }
                    else
                    {
                        LOG("[CURSOR SHOW]");
                        td->flags &= ~TFLAG_HIDE_CURSOR;
                    }
                }
            }
            else if (num == 20)
            {}
            break;
        case 's':        // save cursor
            LOG("[CURSOR SAVE]");
            td->save_x   = td->x;
            td->save_y   = td->y;
            td->save_lcf = td->lcf;
            break;
        case 'u':        // restore cursor
            LOG("[CURSOR RESTORE]");
            td->x   = td->save_x;
            td->y   = td->save_y;
            td->lcf = td->save_lcf;
            break;
        case 'A':        // Cursor Up
            td->y -= num;
            if (td->y >= td->rows)
            {
                td->y = 0;
            }
            LOGF("[CUP %d]", num);
            break;
        case 'B':        // Cursor Down
            td->y += num;
            if (td->y >= td->rows)
            {
                td->y = td->rows - 1;
            }
            LOGF("[CDOWN %d]", num);
            break;
        case 'C':        // Cursor Right
            td->x += num;
            if (td->x >= td->cols)
            {
                td->x = td->cols - 1;
            }
            LOGF("[CRIGHT %d]", num);
            break;
        case 'D':        // Cursor Left
            td->x -= num;
            if (td->x >= td->cols)
            {
                td->x = 0;
            }
            LOGF("[CLEFT %d]", num);
            break;
        case 'K':
            LOGF("[ERASE %s]", num_z == 0 ? "EOL" : num_z == 1 ? "SOL" : num_z == 2 ? "LINE" : "?");
            switch (num_z)
            {
                case 0:
                    xansi_clear(td, td->cur_addr, xansi_calc_addr(td, td->cols - 1, td->y));
                    break;
                case 1:
                    xansi_clear(td, xansi_calc_addr(td, 0, td->y), td->cur_addr);
                    break;
                case 2:
                    xansi_clear(td, xansi_calc_addr(td, 0, td->y), xansi_calc_addr(td, td->cols - 1, td->y));
                    break;
            }
            break;
        case 'J':
            LOGF("[ERASE %s]", num_z == 0 ? "DOWN" : num_z == 1 ? "UP" : num_z == 2 ? "SCREEN" : "?");
            switch (num_z)
            {
                case 0:
                    xansi_clear(td, xansi_calc_addr(td, 0, td->y), td->vram_end);
                    break;
                case 1:
                    xansi_clear(td, td->vram_base, xansi_calc_addr(td, td->cols - 1, td->y));
                    break;
                case 2:
                    xansi_cls(td);
                    break;
            }
            break;
        case 'm':
            if (td->num_parms == 0)
            {
                td->num_parms = 1;        // default argument is 0
            }
            for (uint16_t i = 0; i < td->num_parms; i++)
            {
                uint16_t attrib_num = td->csi_parms[i];
                uint8_t  col        = attrib_num % 10;
                def_flag            = false;

                if (attrib_num == 39)        // set default forground
                {
                    attrib_num = 30;
                    col        = td->def_color & 0xf;
                    def_flag   = true;
                }
                if (attrib_num == 49)        // set default forground
                {
                    attrib_num = 40;
                    col        = td->def_color >> 4;
                    def_flag   = true;
                }

                if (col < 8)        // if dim color
                {
                    if (attrib_num >= 90)        // if light color range
                    {
                        col += 8;               // make light color
                        attrib_num = 30;        // assume forground color
                        if (attrib_num >= 100)
                        {
                            attrib_num = 40;        // nope, background color
                        }
                    }
                    else if (td->flags & TFLAG_ATTRIB_BRIGHT)        // attrib bright?
                    {
                        col += 8;        // make light color
                    }
                }

                if (col >= 8 && td->flags & TFLAG_ATTRIB_DIM)        // attrib dim?
                {
                    col -= 8;        // make dim color
                }

                switch (attrib_num)
                {
                    case 0:        // attrib reset
                        td->flags &= ~(TFLAG_ATTRIB_BRIGHT | TFLAG_ATTRIB_DIM | TFLAG_ATTRIB_PASSTHRU);
                        td->color = td->def_color;
                        LOG("[RESET]");
                        break;
                    case 1:        // bright
                        td->color = td->color | 0x08;
                        td->flags &= ~TFLAG_ATTRIB_DIM;
                        td->flags |= TFLAG_ATTRIB_BRIGHT;
                        LOG("[BRIGHT]");
                        break;
                    case 2:        // dim
                        td->flags &= ~TFLAG_ATTRIB_BRIGHT;
                        td->flags |= TFLAG_ATTRIB_DIM;
                        td->color = td->color & ~0x08;
                        LOG("[DIM]");
                        break;
                    case 7:        // reverse
                        td->color = ((uint8_t)(td->color & 0xf0) >> 4) | (uint8_t)((td->color & 0x0f) << 4);
                        LOG("[REVERSE]");
                        break;
                    case 8:        // hidden (control graphic passthru)
                        LOG("[HIDDEN]");
                        td->flags |= TFLAG_ATTRIB_PASSTHRU;
                        break;
                    case 30:        // set foreground
                    case 31:
                    case 32:
                    case 33:
                    case 34:
                    case 35:
                    case 36:
                    case 37:
                        td->color = (uint8_t)(td->color & 0xf0) | col;
                        if (def_flag)
                        {
                            LOG("[FORE_DEFAULT]");
                        }
                        else
                        {
                            LOG("[FORE_COLOR]");
                        }
                        break;
                    case 40:        // set background
                    case 41:
                    case 42:
                    case 43:
                    case 44:
                    case 45:
                    case 46:
                    case 47:
                        td->color = (uint8_t)(td->color & 0x0f) | ((uint8_t)(col << 4));
                        if (def_flag)
                        {
                            LOG("[BACK_DEFAULT]");
                        }
                        else
                        {
                            LOG("[BACK_COLOR]");
                        }
                        break;
                    default:
                        LOGF("[%d ignored]", td->csi_parms[i]);
                        break;
                }
            }
    }

#if DEBUG
    if (initial_col != td->color)
    {
        LOGF(" {Color %02x -> %02x}", initial_col, td->color);
    }
    if ((initial_x != td->x) || (initial_y != td->y))
    {
        LOGF(" {CPos %d,%d -> %d,%d}", initial_x, initial_y, td->x, td->y);
    }
    if (initial_flags != td->flags)
    {
        LOGF(" {Flags %02x -> %02x}", initial_flags, td->flags);
    }
    LOG("\n");
#endif

    xansi_calc_cur_addr(td);
}

// external terminal functions
static void xansiterm_init()
{
    LOG("> ansiterm_init\n");

    memset(&atd, 0, sizeof(atd));
    ansiterm_data * td = &atd;

    xansi_vidreset(td);
    xansi_reset(td);
    td->vram_size <<= 1;
    td->rows <<= 1;        // clear double high for 8x8
    xansi_cls(td);
    td->vram_size >>= 1;
    td->rows >>= 1;
}

bool xansiterm_checkchar()
{
    ansiterm_data * td = &atd;
    xv_prep();

    xansi_check_lcf(td);
    bool char_ready = checkchar();
    bool cur_invert = td->flags & TFLAG_CURSOR_INVERTED;
    bool new_invert = (xm_getw(TIMER) & 0x800) && !char_ready && !(td->flags & TFLAG_HIDE_CURSOR);
    if (cur_invert != new_invert)
    {
        xansi_invertcursor(td);
    }

    return char_ready;
}

char xansiterm_readchar()
{
    ansiterm_data * td = &atd;
    if (td->flags & TFLAG_CURSOR_INVERTED)
    {
        xansi_invertcursor(td);
    }

    // TODO terminal query stuff?
    return readchar();
}

void xansiterm_putchar(char cdata)
{
    ansiterm_data * td = &atd;
    if (cdata == '\x1b')
    {
        xansi_begin_esc(td);
        return;
    }
    if (cdata == '\x9b')
    {
        xansi_begin_csi(td);
        return;
    }
    if (td->state == TSTATE_NORMAL)
    {
        xansi_processchar(td, cdata);
        return;
    }

    switch (td->state)
    {
        case TSTATE_ESC:
            td->state = TSTATE_NORMAL;
            switch (cdata)
            {
                case '\x1b':        // print 2nd ESC if passthru
                    if (td->flags & TFLAG_ATTRIB_PASSTHRU)
                    {
                        xansi_processchar(td, cdata);
                    }
                    else
                    {
                        xansi_begin_esc(td);
                    }
                    break;
                case '\x9b':        // 8-bit CSI
                    if (td->flags & TFLAG_ATTRIB_PASSTHRU)
                    {
                        xansi_processchar(td, cdata);
                    }
                    else
                    {
                        xansi_begin_csi(td);
                    }
                    break;
                case 'c':        // VT100 RIS reset device
                    LOG("[TERM RESET]\n");
                    td->flags = 0;
                    xansi_vidreset(td);
                    xansi_reset(td);
                    xansi_cls(td);
                    break;
                case '7':        // save cursor
                    LOG("[CURSOR SAVE]\n");
                    td->save_x   = td->x;
                    td->save_y   = td->y;
                    td->save_lcf = td->lcf;
                    break;
                case '8':        // restore cursor
                    LOG("[CURSOR RESTORE]\n");
                    td->x   = td->save_x;
                    td->y   = td->save_y;
                    td->lcf = td->save_lcf;
                    break;
                case '(':        // normal 8x16 font
                    td->flags &= ~TFLAG_8X8_FONT;
                    xansi_vidreset(td);
                    xansi_reset(td);
                    LOGF("[FONT %dx%d MODE]", td->rows, td->cols);
                    break;
                case ')':        // alternate 8x8 font
                    td->flags |= TFLAG_8X8_FONT;
                    xansi_vidreset(td);
                    xansi_reset(td);
                    LOGF("[FONT %dx%d MODE]", td->rows, td->cols);
                    break;
                case '[':        // VT100 CSI
                    xansi_begin_csi(td);
                    break;
                case 'D':        // VT100 IND move cursor down (^J)
                    LOG("[CDOWN]\n");
                    xansi_processchar(td, '\n');
                    break;
                case 'M':        // VT100 RI move cursor up (^K)
                    LOG("[CUP]\n");
                    xansi_processchar(td, '\v');
                    break;
                case 'E':        // VT100 NEL next line
                    LOG("[NEL]\n");
                    td->lcf = 0;
                    td->x   = 0;
                    td->y += 1;
                    if (td->y >= td->cols)
                    {
                        td->y = td->cols;
                        xansi_scroll_up(td);
                    }
                    break;
                default:
                    LOGF("> Ign: <ESC>%c (<ESC>0x%02x)\n", (cdata >= ' ' && cdata < 0x7f) ? cdata : ' ', cdata);
                    break;
            }
            xansi_calc_cur_addr(td);

            break;
        case TSTATE_CSI: {
            if (cdata == '\x1b')
            {
                xansi_begin_esc(td);
                break;
            }
            uint8_t cclass = cdata & 0xf0;
            if (cclass == 0x20)        // intermediate char
            {
                if (td->intermediate_char)
                {
                    LOGF("> Ign stomp inter %c with %c\n", td->intermediate_char, cdata);
                }
                td->intermediate_char = cdata;
            }
            else if (cclass == 0x30)        // parameter number
            {
                uint8_t d = (uint8_t)(cdata - '0');
                if (d <= 9)
                {
                    if (td->num_parms == 0)
                    {
                        td->num_parms = 1;
                    }
                    uint16_t v = td->csi_parms[td->num_parms - 1];
                    v *= (uint16_t)10;
                    v += d;
                    if (v > 9999)
                    {
                        v = 9999;
                    }
                    td->csi_parms[td->num_parms - 1] = v;
                }
                else if (cdata == ';')
                {
                    td->num_parms += 1;
                    if (td->num_parms >= MAX_CSI_PARMS)
                    {
                        LOG("> Too many CSI parms\n");
                        td->num_parms = MAX_CSI_PARMS - 1;
                    }
                }
                else if (cdata == '?')        // <CSI>?3l  or <CSI>?3h   for 80/132 cols
                {
                    td->intermediate_char = cdata;
                }
                else
                {
                    LOGF("> Unexpected CSI P...P: %c (0x%02x)\n", (cdata >= ' ' && cdata < 0x7f) ? cdata : ' ', cdata);
                }
            }
            else if (cclass >= 0x40)
            {
                td->state = TSTATE_NORMAL;
                xansi_process_csi(td, cdata);
            }
            else
            {
                td->state = TSTATE_NORMAL;
                LOGF("> End CSI char: %c (0x%02x)\n", (cdata >= ' ' && cdata < 0x7f) ? cdata : ' ', cdata);
            }
            break;
        }
        default:
            LOGF("> bad state: %d (reset to TSTATE_NORMAL)\n", td->state);
            td->state = TSTATE_NORMAL;
            break;
    }
}

// testing harness functions

static void tprint(const char * str)
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

static char tprint_buff[4096];
static void tprintf(const char * fmt, ...) __attribute__((format(printf, 1, 2)));
static void tprintf(const char * fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(tprint_buff, sizeof(tprint_buff), fmt, args);
    tprint(tprint_buff);
    va_end(args);
}

void test_attrib()
{
    static uint8_t cbg_tbl[] = {40, 41, 42, 43, 44, 45, 46, 47, 100, 101, 102, 103, 104, 105, 106, 107, 49};
    static uint8_t cfg_tbl[] = {30, 31, 32, 33, 34, 35, 36, 37, 90, 91, 92, 93, 94, 95, 96, 97, 39};
    for (uint16_t cbg = 0; cbg < sizeof(cbg_tbl); cbg++)
    {
        for (uint16_t cfg = 0; cfg < sizeof(cfg_tbl); cfg++)
        {
            for (uint16_t attr = 0; attr < 8; attr++)
            {
                if (attr > 2 && attr < 7)
                {
                    continue;
                }
                tprintf("\x1b[%d;%d;%dm ^[%d;%d;%dm \x1b[0m",
                        attr,
                        cbg_tbl[cbg],
                        cfg_tbl[cfg],
                        attr,
                        cbg_tbl[cbg],
                        cfg_tbl[cfg]);
            }
            if (checkchar())
            {
                readchar();

                while (!checkchar())
                    ;

                char c = readchar();
                if (c == 3)
                {
                    return;
                }
            }
        }
        tprint("\r\n");
    }
}

void xosera_ansiterm()
{
    LOG("> Terminal test started.\n");
    xosera_init(1);

    xansiterm_init();
    tprint("\x1b)Welcome to ANSI Terminal test\n\n");

    tprint("\nPress a key to start mega-test\n");
    tprint("     (key pauses)\n");

    while (!xansiterm_checkchar())
        ;

    xansiterm_readchar();

    test_attrib();

    tprint("\nEcho test, type ^A to exit...\n\n");

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

    tprint("\fExiting...\n");

    LOG("> Terminal test exiting.\n");
}
