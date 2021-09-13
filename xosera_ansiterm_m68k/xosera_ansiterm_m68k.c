/*
 * vim: set et ts=4 sw=4
 *------------------------------------------------------------
 *                                  ___ ___ _
 *  ___ ___ ___ ___ ___       _____|  _| . | |_
 * |  _| . |_ -|  _| . |     |     | . | . | '_|
 * |_| |___|___|___|___|_____|_|_|_|___|___|_,_|
 *                     |_____|
 *  __ __
 * |  |  |___ ___ ___ ___ ___
 * |-   -| . |_ -| -_|  _| .'|
 * |__|__|___|___|___|_| |__,|
 *
 * ------------------------------------------------------------
 * Copyright (c) 2021 Xark
 * MIT License
 *
 * rosco_m68k + Xosera VT100/ANSI terminal driver
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

#include "xosera_ansiterm_m68k.h"

#define XV_PREP_REQUIRED        // require xv_prep() functtion
#include "xosera_m68k_api.h"

#define MAX_CSI_PARMS 16        // additional parameters overwrite 16th

enum e_term_flags
{
    TFLAG_8X8_FONT        = 1 << 0,        // use alternate 8x8 font
    TFLAG_NO_AUTOWRAP     = 1 << 1,        // don't wrap to next line at EOL
    TFLAG_NEWLINE         = 1 << 2,        // no cursor on input
    TFLAG_HIDE_CURSOR     = 1 << 3,        // no cursor on input
    TFLAG_ATTRIB_BRIGHT   = 1 << 4,        // make colors bright
    TFLAG_ATTRIB_DIM      = 1 << 5,        // make colors dim
    TFLAG_ATTRIB_REVERSE  = 1 << 6,        // reverse fore/back colors
    TFLAG_ATTRIB_PASSTHRU = 1 << 7,        // pass through control chars as graphic [HIDDEN attribute]
};

enum e_term_state
{
    TSTATE_NORMAL,
    TSTATE_ESC,
    TSTATE_CSI
};

typedef struct xansiterm_data
{
    uint16_t cur_addr;                        // next VRAM address to draw text
    uint16_t vram_base;                       // base VRAM address for text screen
    uint16_t vram_size;                       // size of text screen in current mode (init clears to allow 8x8 font)
    uint16_t vram_end;                        // ending address for text screen in current mode
    uint16_t cursor_save;                     // word under input cursor
    uint16_t csi_parms[MAX_CSI_PARMS];        // CSI parameter storage
    uint8_t  intermediate_char;               // CSI intermediate character (only one supported)
    uint8_t  num_parms;                       // number of parsed CSI parameters
    uint8_t  def_color;                       // default terminal colors // TODO: good item for ICP
    uint8_t  cur_color;                       // logical colors before attribute modifications (high/low nibble)
    uint8_t  state;                           // current ANSI parsing state (e_term_state)
    uint8_t  flags;                           // various terminal flags (e_term_flags)
    uint8_t  color;                           // effective current background and forground color (high/low nibble)
    uint8_t  cols, rows;                      // text columns and rows in current mode (zero based)
    uint8_t  x, y;                            // current x and y cursor position (zero based)
    uint8_t  save_x, save_y;                  // storage to save/restore cursor postion
    bool     lcf;                             // flag for delayed last column wrap flag (PITA)
    bool     save_lcf;                        // storeage to save/restore lcf with cursor position
    bool     cursor_drawn;                    // flag if cursor_save data valid
} xansiterm_data;

xansiterm_data _private_xansiterm_data __attribute__((section(".text")));        // NOTE: address must be < 32KB

// high speed small inline functions

// get xansiterm data (data needs to be in first 32KB of memory)
static inline xansiterm_data * get_xansi_data()
{
    xansiterm_data * ptr;
    __asm__ __volatile__("   lea.l   _private_xansiterm_data.w,%[ptr]" : [ptr] "=a"(ptr));
    return ptr;
}

#if DEBUG
// verify x, y matches td->cur_addr VRAM address
static inline void xansi_check_xy(xansiterm_data * td)
{
    uint16_t r;
    uint32_t l = td->cur_addr - td->vram_base;
    // GCC is annoying me and not using perfect opcode that gives division and remainder result
    __asm__ __volatile__(
        "divu.w %[w],%[l]\n"
        "move.l %[l],%[r]\n"
        "swap.w %[r]\n"
        : [l] "+d"(l), [r] "=d"(r)
        : [w] "d"((uint16_t)td->cols));
    if (td->y != (l & 0xffff) || td->x != r)
    {
        if ((l & 0xffff) - td->y == 1 && td->lcf)
        {
            LOG("MISMATCH ok\n");
        }
        else
        {
            LOGF(
                "MISMATCH: addr %04x @ %u,%u should be %u,%u\n", td->cur_addr, td->x, td->y, r, (uint16_t)(l & 0xffff));
            while (true)
                ;
        }
    }
}
#endif

// calculate VRAM address from x, y
static inline uint16_t xansi_calc_addr(xansiterm_data * td, uint16_t x, uint16_t y)
{
    return td->vram_base + (uint16_t)(y * td->cols) + x;
}

// calculate td->cur_addr from td->x, td->y
static inline void xansi_calc_cur_addr(xansiterm_data * td)
{
    td->cur_addr = xansi_calc_addr(td, td->x, td->y);
}

static void xansi_scroll_up(xansiterm_data * td);
static void xansi_check_lcf(xansiterm_data * td)
{
    if (td->lcf)
    {
        td->lcf = 0;
        if ((uint16_t)(td->cur_addr - td->vram_base) >= td->vram_size)
        {
            td->cur_addr = td->vram_base + (td->vram_size - td->cols);
            xansi_scroll_up(td);
        }
    }
}

// draw character into VRAM at td->cur_addr
static inline void xansi_drawchar(xansiterm_data * td, char cdata)
{
    xv_prep();
    xansi_check_lcf(td);

    xm_setw(WR_ADDR, td->cur_addr++);
    xm_setbh(DATA, td->color);
    xm_setbl(DATA, cdata);

    td->x++;
    if (td->x >= td->cols)
    {
        if (td->flags & TFLAG_NO_AUTOWRAP)
        {
            td->x = td->cols - 1;
        }
        else
        {
            td->x = 0;
            td->y++;
            if (td->y >= td->rows)
            {
                td->y = td->rows - 1;
            }
            td->lcf = true;
        }
    }
}

// functions where speed is nice (but inline is too much)
static _NOINLINE void xansi_clear(xansiterm_data * td, uint16_t start, uint16_t end)
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

// unrolled for 16-bytes per loop, so no inline please
static _NOINLINE void xansi_do_scroll(xansiterm_data * td)
{
    xv_prep();
    uint16_t i;
    for (i = td->vram_size - td->cols; i > 8; i -= 8)
    {
        xm_setl(DATA, xm_getl(DATA));
        xm_setl(DATA, xm_getl(DATA));
        xm_setl(DATA, xm_getl(DATA));
        xm_setl(DATA, xm_getl(DATA));
    }
    while (i--)
    {
        xm_setw(DATA, xm_getw(DATA));
    }
    // clear new line
    xm_setbh(DATA, td->color);
    for (uint16_t i = 0; i < td->cols; i++)
    {
        xm_setbl(DATA, ' ');
    }
}

// functions that don't need to be so fast, so optimize for size
#pragma GCC push_options
#pragma GCC optimize("-Os")

static void set_defaut_palette()
{
    static const uint16_t def_colors16[16] = {
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
    const uint16_t * cp = def_colors16;
    for (uint16_t i = 0; i < 16; i++)
    {
        xm_setw(XR_DATA, *cp++);
    };
}

// fully reset Xosera "text mode" with defaults that should make it visible
static void xansi_vidreset(xansiterm_data * td)
{
    xv_prep();
    bool alt_font = td->flags & TFLAG_8X8_FONT;
    // set xosera playfield A registers
    uint16_t tile_addr   = alt_font ? 0x800 : 0x0000;
    uint8_t  tile_height = alt_font ? 7 : 15;
    uint16_t cols        = xreg_getw(VID_HSIZE) >> 3;        // get pixel width / 8 for text columns

    while (!(xreg_getw(SCANLINE) & 0x8000))
        ;

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

static void xansi_reset(xansiterm_data * td)
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
    td->def_color = 0x02;        // default dark-green on black
    td->cur_color = 0x02;        // default dark-green on black
    td->color     = 0x02;        // default dark-green on black

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

static void xansi_draw_cursor(xansiterm_data * td, bool onoff)
{
    xv_prep();

    if (onoff && !td->cursor_drawn)        // cursor on, but not drawn
    {
        td->cursor_drawn = true;

        xm_setw(RW_INCR, 0x0000);
        xm_setw(RW_ADDR, td->cur_addr);
        uint16_t data   = xm_getw(RW_DATA);
        td->cursor_save = data;

        // calculate cursor color
        // use current colors reversed, but alter dim/bright for improved contrast with colors under cursor
        uint16_t cursor_color = ((uint16_t)(td->color & 0xf0) << 4) | ((uint16_t)(td->color & 0x0f) << 12);
        uint16_t cursor_char  = ((cursor_color ^ td->cursor_save) & 0x88ff) ^ 0x8800;        // constast + char
        cursor_char ^= cursor_color;        // cursor_color with for/back dim/bright toggled for constrast

        xm_setw(RW_DATA, cursor_char);        // draw char with cursor colors
    }
    else if (!onoff && td->cursor_drawn)        // cursor off, but drawn
    {
        td->cursor_drawn = false;
        xm_setw(RW_INCR, 0x0000);
        xm_setw(RW_ADDR, td->cur_addr);
        xm_setw(RW_DATA, td->cursor_save);
    }
}

static void xansi_visualbell(xansiterm_data * td)
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

// clear screen (fullheight always clears the full 8x8 height)
static void xansi_cls(xansiterm_data * td, bool fullheight)
{
    // if using 8x8 font, already fullheight
    if (td->flags & TFLAG_8X8_FONT)
    {
        fullheight = false;
    }
    xansi_clear(td, fullheight ? td->vram_base : td->vram_base << 1, fullheight ? td->vram_end << 1 : td->vram_end);
    td->x   = 0;
    td->y   = 0;
    td->lcf = 0;

    td->cur_addr = td->vram_base;
}

static void xansi_scroll_up(xansiterm_data * td)
{
    xv_prep();
    xm_setw(WR_INCR, 1);
    xm_setw(RD_INCR, 1);
    xm_setw(WR_ADDR, td->vram_base);
    xm_setw(RD_ADDR, td->vram_base + td->cols);
    xansi_do_scroll(td);
}

static void xansi_scroll_down(xansiterm_data * td)
{
    xv_prep();
    xm_setw(WR_INCR, -1);
    xm_setw(RD_INCR, -1);
    xm_setw(WR_ADDR, (uint16_t)(td->vram_end - 1));
    xm_setw(RD_ADDR, (uint16_t)(td->vram_end - 1 - td->cols));
    xansi_do_scroll(td);
}

#pragma GCC push_options

static void xansi_processchar(xansiterm_data * td, char cdata)
{
    if ((int8_t)cdata < ' ' && !(td->flags & TFLAG_ATTRIB_PASSTHRU))
    {
        //        xansi_calc_xy(td);
        switch (cdata)
        {
            case '\a':
                // VT:    \a  alert (visual bell)
                LOG("[BELL]");
                xansi_visualbell(td);
                return;        // fast out (no lcf clear)
            case '\b':
                // VT:  \b  backspace
                LOG("[BS]");
                if (td->x > 0)
                {
                    td->x -= 1;
                    td->cur_addr--;
                }
                break;
            case '\t':
                // VT:    \t tab (8 character)
                LOG("[TAB]");
                uint8_t nx = (uint8_t)(td->x & ~0x7) + 8;
                if ((uint8_t)(td->cols - nx) < 8)
                {
                    td->cur_addr -= td->x;
                    td->cur_addr += td->cols;
                    td->x = 0;
                    td->y++;
                }
                else
                {
                    td->x = (uint8_t)(td->x & ~0x7) + 8;
                }
                break;
            case '\n':
                // VT:    \n  line feed (or LF+CR if NEWLINE mode)
                LOG("[LF]");
                td->cur_addr += td->cols;
                td->y++;
                if (td->flags & TFLAG_NEWLINE)
                {
                    td->cur_addr -= td->x;
                    td->x = 0;
                }
                break;
            case '\v':
                // VT:    \v  vertical tab EXTENSION: cursor up vs another LF
                LOG("[VT]");
                td->cur_addr -= td->cols;
                td->y--;
                if (td->y >= td->rows)
                {
                    td->cur_addr += td->cols;
                    td->y += 1;
                    xansi_scroll_down(td);
                }
                break;
            case '\f':
                // VT:    \f  form feed EXTENSION clears screen and homes cursor
                LOG("[FF]");
                xansi_cls(td, false);
                break;
            case '\r':
                // VT: \r   carriage return
                LOG("[CR]");
                td->cur_addr -= td->x;
                td->x = 0;
                break;
            default:           // suppress others
                return;        // fast out (no cursor change)
        }

        if (td->y >= td->rows)
        {
            td->cur_addr -= td->cols;
            td->y -= 1;
            xansi_scroll_up(td);
        }
        //        xansi_calc_cur_addr(td);
        td->lcf = 0;

#if DEBUG
        xansi_check_xy(td);
#endif
        return;
    }

    xansi_drawchar(td, cdata);
}

// starts CSI sequence or ESC sequence (if c is ESC)
static void xansi_begin_csi(xansiterm_data * td, char c)
{
    td->state             = (c == '\x1b') ? TSTATE_ESC : TSTATE_CSI;
    td->intermediate_char = 0;
    td->num_parms         = 0;
    memset(td->csi_parms, 0, sizeof(td->csi_parms));
}

// process a completed CSI sequence
static void xansi_process_csi(xansiterm_data * td, char cdata)
{
    if (td->intermediate_char)
    {
        LOGF("%c", td->intermediate_char);
    }
    for (uint16_t i = 0; i < td->num_parms; i++)
    {
        LOGF("%s%d", i ? ";" : "", td->csi_parms[i]);
    }
    LOGF("%c\n  := ", cdata);

    td->state      = TSTATE_NORMAL;            // back to NORMAL
    uint16_t num_z = td->csi_parms[0];         // for default of zero
    uint16_t num   = num_z ? num_z : 1;        // for default of  one

    switch (cdata)
    {
        case 'H':
            // VT: <CSI><row>;<col>H   cursor home / position
        case 'f':
            // VT: <CSI><row>;<col>f   cursor home / position (force)
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
        case 'h':
        case 'l':
            if (td->intermediate_char == '?')
            {
                if (num == 3)
                {
                    // VT:  <CSI>?3h    select 16:9 mode (848x480) EXTENSION: was DEC 132 column
                    // VT:  <CSI>?3l    select  4:3 mode (640x480) EXTENSION: was DEC 80 column
                    uint16_t res = (cdata == 'h') ? 848 : 640;
                    xv_prep();
                    if (xreg_getw(VID_HSIZE) != res)
                    {
                        uint16_t config = (res == 640) ? 0 : 1;
                        LOGF("<reconfig #%d>\n", config);
                        xosera_init(config);
                        xansi_vidreset(td);
                        xansi_reset(td);
                        xansi_cls(td, true);
                        LOGF("[RECONFIG %dx%d]", td->rows, td->cols);
                    }
                }
                else if (num == 7)
                {
                    // VT:  <CSI>?7h    autowrap ON (auto wrap/scroll at EOL) (default)
                    // VT:  <CSI>?7l    autowrap OFF (cursor stops at right margin)
                    if (cdata == 'l')
                    {
                        LOG("[AUTOWRAP OFF]");
                        td->flags |= TFLAG_NO_AUTOWRAP;
                        td->lcf = 0;
                    }
                    else
                    {
                        LOG("[AUTOWRAP ON]");
                        td->flags &= ~TFLAG_NO_AUTOWRAP;
                        if (td->x >= td->cols - 1)
                        {
                            td->lcf = 1;
                        }
                    }
                }
                else if (num == 25)
                {
                    // VT:  <CSI>?25h   show cursor when waiting for input (default)
                    // VT:  <CSI>?25l   no cursor
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
            {
                // VT:  <CSI>?20h   newline mode on,  LF also does CR
                // VT:  <CSI>?20l   newline mode off, LF only (default)
                if (cdata == 'l')
                {
                    LOG("[NEWLINE OFF]");
                    td->flags &= ~TFLAG_NEWLINE;
                }
                else
                {
                    LOG("[NEWLINE ON]");
                    td->flags |= TFLAG_NEWLINE;
                }
            }
            break;
        case 's':
            // VT: <CSI>s  save cursor position
            LOG("[CURSOR SAVE]");
            td->save_x   = td->x;
            td->save_y   = td->y;
            td->save_lcf = td->lcf;
            break;
        case 'u':
            // VT: <CSI>u  restore cursor position
            LOG("[CURSOR RESTORE]");
            td->x   = td->save_x;
            td->y   = td->save_y;
            td->lcf = td->save_lcf;
            break;
        case 'A':
            // VT: <CSI>A  cursor up (no scroll)
            td->y -= num;
            if (td->y >= td->rows)
            {
                td->y = 0;
            }
            LOGF("[CUP %d]", num);
            break;
        case 'B':
            // VT: <CSI>B  cursor down (no scroll)
            td->y += num;
            if (td->y >= td->rows)
            {
                td->y = td->rows - 1;
            }
            LOGF("[CDOWN %d]", num);
            break;
        case 'C':
            // VT: <CSI>C  cursor right (no scroll)
            td->x += num;
            if (td->x >= td->cols)
            {
                td->x = td->cols - 1;
            }
            LOGF("[CRIGHT %d]", num);
            break;
        case 'D':
            // VT: <CSI>D  cursor left (no scroll)
            td->x -= num;
            if (td->x >= td->cols)
            {
                td->x = 0;
            }
            LOGF("[CLEFT %d]", num);
            break;
        case 'K':
            // VT:  <CSI>K  erase from cursor to end of line
            // VT:  <CSI>1K erase from cursor to start of line
            // VT:  <CSI>2K erase from whole cursor line
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
                default:
                    break;
            }
            break;
        case 'J':
            // VT:  <CSI>J  erase down from cursor line to end of screen
            // VT:  <CSI>1J erase up from cursor line to start of screen
            // VT:  <CSI>2J erase whole screen
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
                    xansi_cls(td, false);
                    break;
                default:
                    break;
            }
            break;
        case 'm':
            // CSI Select Graphic Rendition display attribute parameter codes
            if (td->num_parms == 0)        // if no parameters
            {
                td->num_parms = 1;        // parameter zero will be 0 default
            }

            for (uint16_t i = 0; i < td->num_parms; i++)
            {
                bool def_flag = false;        // flag for default color set
                (void)def_flag;               // no warnings if unused

                uint16_t parm_code = td->csi_parms[i];        // attribute parameter
                uint8_t  col       = parm_code % 10;          // modulo ten color

                if (col < 8)        // normal color 0-7 range
                {
                    // light color ranges?
                    if (parm_code >= 90)        // if a light color range, set to normal color range
                    {
                        parm_code = (parm_code < 100) ? 30 : 40;
                    }
                }

                switch (parm_code)
                {
                    case 0:
                        // VT: SGR parm 0    reset   reset all attributes and default color
                        LOG("[RESET]");
                        td->flags &=
                            ~(TFLAG_ATTRIB_BRIGHT | TFLAG_ATTRIB_DIM | TFLAG_ATTRIB_REVERSE | TFLAG_ATTRIB_PASSTHRU);
                        td->cur_color = td->def_color;        // restore default color
                        break;
                    case 1:
                        // VT: SGR parm 1   bright  select bright colors (8-15)
                        LOG("[BRIGHT]");
                        td->flags &= ~TFLAG_ATTRIB_DIM;
                        td->flags |= TFLAG_ATTRIB_BRIGHT;
                        break;
                    case 2:
                        // VT: SGR parm 2   dim     select dim colors (0-7)
                        LOG("[DIM]");
                        td->flags &= ~TFLAG_ATTRIB_BRIGHT;
                        td->flags |= TFLAG_ATTRIB_DIM;
                        break;
                    case 7:
                        // VT: SGR parm 7   reverse swap fore/back colors
                        LOG("[REVERSE]");
                        td->flags |= TFLAG_ATTRIB_REVERSE;
                        break;
                    case 8:
                        // VT: SGR parm 8   hidden  EXTENSION: ctrl char graphic pass-through
                        LOG("[PASSTHRU]");
                        td->flags |= TFLAG_ATTRIB_PASSTHRU;
                        break;
                    case 39:
                        // VT: SGR parm 39  select default forground color
                        def_flag = true;
                        col      = td->def_color & 0xf;
                    // falls through
                    case 30:
                    case 31:
                    case 32:
                    case 33:
                    case 34:
                    case 35:
                    case 36:
                    case 37:
                        // VT: SGR parm 30-37   select forground color 0-7
                        td->cur_color = (uint8_t)(td->cur_color & 0xf0) | col;
                        LOGF("[%sFORE=%x]", def_flag ? "DEF_" : "", col);
                        break;
                    case 49:
                        // VT: SGR parm 49  select default background color
                        col      = td->def_color >> 4;
                        def_flag = true;
                    // falls through
                    case 40:
                    case 41:
                    case 42:
                    case 43:
                    case 44:
                    case 45:
                    case 46:
                    case 47:
                        // VT: SGR parm 40-47   select background color 0-7
                        td->cur_color = (uint8_t)(td->cur_color & 0x0f) | ((uint8_t)(col << 4));
                        LOGF("[%sBACK=%x]", def_flag ? "DEF_" : "", col);
                        break;
                    case 68:
                        // VT: SGR parm 68  rosco_m68k  EXTENSION: special stuff here...
                        // TODO: colormem, gfx_ctrl, tile_ctrl, vram_base, disp_base, line_len
                        // default fore/back color
                        LOG("[ROSCO_M68K=");
                        for (int rp = ++i; rp < td->num_parms; rp++)
                        {
                            LOGF("%c%d", (rp != i) ? ',' : '(', td->csi_parms[rp]);
                        }
                        LOG(")]");
                        i = td->num_parms;        // eat remaning parms
                        break;
                    default:
                        LOGF("[%d ignored]", td->csi_parms[i]);
                        break;
                }
                // calculate effective color
                if (td->flags & TFLAG_ATTRIB_REVERSE)
                {
                    td->color = ((uint8_t)(td->cur_color & 0xf0) >> 4) | (uint8_t)((td->cur_color & 0x0f) << 4);
                }
                else
                {
                    td->color = td->cur_color;
                }
                if (td->flags & TFLAG_ATTRIB_DIM)
                {
                    td->color &= ~0x08;
                }
                if (td->flags & TFLAG_ATTRIB_BRIGHT)
                {
                    td->color |= 0x08;
                }
            }
    }

    xansi_calc_cur_addr(td);
}

// external public terminal functions
void xansiterm_init()
{
    LOG("[xansiterm_init]\n");

    xansiterm_data * td = get_xansi_data();
    memset(td, 0, sizeof(*td));

    xansi_vidreset(td);
    xansi_reset(td);
    xansi_cls(td, true);
}

bool xansiterm_checkchar()
{
    xansiterm_data * td = get_xansi_data();
    xv_prep();

    xansi_check_lcf(td);        // wrap cursor if needed
    bool char_ready = checkchar();
    // blink at ~409.6ms (on half the time but only if cursor not disabled and no char ready)
    bool show_cursor = !(td->flags & TFLAG_HIDE_CURSOR) && !char_ready && (xm_getw(TIMER) & 0x800);
    xansi_draw_cursor(td, show_cursor);

    return char_ready;
}

char xansiterm_readchar()
{
    xansiterm_data * td = get_xansi_data();
    xansi_draw_cursor(td, false);        // make sure cursor not drawn

    // TODO terminal query stuff, but seems not so useful.
    return readchar();
}

void xansiterm_putchar(char cdata)
{
    xansiterm_data * td = get_xansi_data();

    // these are just used for DEBUG
    uint8_t initial_state = td->state;
    uint8_t initial_flags = td->flags;
    uint8_t initial_col   = td->color;
    uint8_t initial_x     = td->x;
    uint8_t initial_y     = td->y;
    (void)initial_flags;        // no warnings if unused
    (void)initial_col;          // no warnings if unused
    (void)initial_x;            // no warnings if unused
    (void)initial_y;            // no warnings if unused
    (void)initial_state;        // no warnings if unused

#if DEBUG
    xansi_check_xy(td);
#endif

    // ESC or 8-bit CSI received
    if ((cdata & 0x7f) == '\x1b')
    {
        // if already in CSI/ESC state and PASSTHRU set, print 2nd CSI/ESC
        if (td->state != TSTATE_NORMAL && td->flags & TFLAG_ATTRIB_PASSTHRU)
        {
            td->state = TSTATE_NORMAL;
        }
        else
        {
            // otherwise start new CSI/ESC
            xansi_begin_csi(td, cdata);
        }
    }
    else if (td->state == TSTATE_NORMAL)
    {
        xansi_processchar(td, cdata);
    }
    else if (cdata == '\x18' || cdata == '\x1A')
    {
        // VT: $18     ABORT (CAN)
        // VT: $1A     ABORT (SUB)
        LOGF("[Cancel CSI/ESC: '%c' (0x%02x)]\n", (cdata >= ' ' && cdata < 0x7f) ? cdata : ' ', cdata);
        td->state = TSTATE_NORMAL;
    }
    else if (td->state == TSTATE_ESC)        // NOTE: only one char sequences supported
    {
        td->state = TSTATE_NORMAL;
        switch (cdata)
        {
            case '\x9b':
                // VT: $9B      CSI
            case '[':
                // VT: <ESC>[   CSI
                xansi_begin_csi(td, cdata);
                break;
            case 'c':
                // VT: <ESC>c  RIS reset video mode
                LOGF("%c\n  := [TERM RESET]", cdata);
                td->flags = 0;
                xansi_vidreset(td);
                xansi_reset(td);
                xansi_cls(td, true);
                break;
            case '7':
                // VT: <ESC>7  save cursor
                LOGF("%c\n[CURSOR SAVE]", cdata);
                td->save_x   = td->x;
                td->save_y   = td->y;
                td->save_lcf = td->lcf;
                break;
            case '8':
                // VT: <ESC>8  restore cursor
                LOGF("%c\n  := [CURSOR RESTORE]\n", cdata);
                td->x   = td->save_x;
                td->y   = td->save_y;
                td->lcf = td->save_lcf;
                break;
            case '(':
                // VT: <ESC>(  1st font 8x16 (default) EXTENSION: 8x16 font size
                td->flags &= ~TFLAG_8X8_FONT;
                xansi_vidreset(td);
                xansi_reset(td);
                LOGF("%c\n  := [FONT0 8x16 %dx%d]\n", cdata, td->cols, td->rows);
                break;
            case ')':
                // VT: <ESC>)  2nd font 8x8         EXTENSION: 8x8 font size
                td->flags |= TFLAG_8X8_FONT;
                xansi_vidreset(td);
                xansi_reset(td);
                LOGF("%c\n  := [FONT1 8x8 %dx%d]\n", cdata, td->cols, td->rows);
                break;
            case 'D':
                // VT: <ESC>D  IND move cursor down
                LOGF("%c\n  := [CDOWN]", cdata);
                uint8_t x_save = td->x;        // save restore X in case NEWLINE mode
                xansi_processchar(td, '\n');
                td->x = x_save;
                break;
            case 'M':
                // VT: <ESC>M  RI move cursor up
                LOGF("%c\n  := [CUP]\n", cdata);
                xansi_processchar(td, '\v');
                break;
            case 'E':
                // VT: <ESC>E  NEL next line
                LOGF("%c\n  := [NEL]\n", cdata);
                td->y++;
                td->x   = 0;
                td->lcf = 0;
                if (td->y >= td->cols)
                {
                    td->y = td->cols - 1;
                    xansi_scroll_up(td);
                }
                break;
            default:
                LOGF("%c\n  := [unknown 0x%02x]\n", (cdata >= ' ' && cdata < 0x7f) ? cdata : ' ', cdata);
                break;
        }
        xansi_calc_cur_addr(td);
    }
    else if (td->state == TSTATE_CSI)
    {
        uint8_t cclass = cdata & 0xf0;
        if (cdata <= ' ')
        {
            // ASCII CAN or SUB aborts sequence, otherwise ignore
            if (cdata == '\x18' || cdata == '\x1A')
            {
                td->state = TSTATE_NORMAL;
            }
        }
        else if (cclass == 0x20)        // intermediate char
        {
            if (td->intermediate_char)
            {
                LOG("[2nd intermediate]");
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
                td->num_parms++;
                if (td->num_parms >= MAX_CSI_PARMS)
                {
                    LOG("[parm >16]\n");
                    td->num_parms = MAX_CSI_PARMS - 1;
                }
            }
            else
            {
                td->intermediate_char = cdata;
            }
        }
        else if (cclass >= 0x40)
        {
            xansi_process_csi(td, cdata);
        }
        else
        {
            // ASCII CAN or SUB aborts sequence, otherwise ignore
            LOGF("[Cancel '%c' (0x%02x)]", (cdata >= ' ' && cdata < 0x7f) ? cdata : ' ', cdata);
            td->state = TSTATE_NORMAL;
        }
    }

#if DEBUG
    if (initial_flags != td->flags)
    {
        LOGF("{Flags:%02x->%02x}", initial_flags, td->flags);
    }
    if (initial_col != td->color)
    {
        LOGF("{Color:%02x->%02x}", initial_col, td->color);
    }
    // position spam only on non-PASSTHRU ctrl chars or non-NORMAL state
    if (((initial_x != td->x) || (initial_y != td->y)) &&
        (td->state != TSTATE_NORMAL || (cdata < ' ' && !(td->flags & TFLAG_ATTRIB_PASSTHRU))))
    {
        LOGF("{CPos:%d,%d->%d,%d}", initial_x, initial_y, td->x, td->y);
        if (td->state == TSTATE_NORMAL)
        {
            LOG("\n");
        }
    }
    if (td->state != initial_state)
    {
        LOGF("%s", td->state == TSTATE_NORMAL ? "\n<NORM>" : td->state == TSTATE_ESC ? "<ESC>" : "<CSI>");
    }
#endif
}
