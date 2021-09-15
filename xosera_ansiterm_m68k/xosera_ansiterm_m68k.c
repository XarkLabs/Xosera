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
 * Based on info from:
 *  https://vt100.net/docs/vt100-ug/chapter3.html#S3.3.6.1
 *  https://misc.flogisoft.com/bash/tip_colors_and_formatting
 *  (and various other sources)
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

#define XV_PREP_REQUIRED        // require Xosera xv_prep() function (for speed)
#include "xosera_m68k_api.h"

#define DEFAULT_COLOR 0x02        // rosco_m68k "retro" dark green on black
#define MAX_CSI_PARMS 16          // max CSI parameters per sequence

// terminal attribute and option flags
enum e_term_flags
{
    TFLAG_NEWLINE         = 1 << 0,        // LF also does a CR
    TFLAG_NO_AUTOWRAP     = 1 << 1,        // don't wrap to next line at EOL
    TFLAG_HIDE_CURSOR     = 1 << 2,        // don't show a cursor on input
    TFLAG_8X8_FONT        = 1 << 3,        // use alternate 8x8 font
    TFLAG_ATTRIB_BRIGHT   = 1 << 4,        // make colors bright
    TFLAG_ATTRIB_DIM      = 1 << 5,        // make colors dim
    TFLAG_ATTRIB_REVERSE  = 1 << 6,        // reverse fore/back colors
    TFLAG_ATTRIB_PASSTHRU = 1 << 7,        // pass through control chars as graphic [HIDDEN attribute]
};

// current processing state of terminal
enum e_term_state
{
    TSTATE_NORMAL,
    TSTATE_ILLEGAL,
    TSTATE_ESC,
    TSTATE_CSI,
};

// all storage for terminal (must be at 16-bit memory address [< 32KB])
typedef struct xansiterm_data
{
    uint16_t cur_addr;                        // next VRAM address to draw text
    uint16_t vram_base;                       // base VRAM address for text screen
    uint16_t vram_size;                       // size of text screen in current mode (init clears to allow 8x8 font)
    uint16_t vram_end;                        // ending address for text screen in current mode
    uint16_t cursor_save;                     // word under input cursor
    uint16_t cols, rows;                      // text columns and rows in current mode (zero based)
    uint16_t x, y;                            // current x and y cursor position (zero based)
    uint16_t save_x, save_y;                  // storage to save/restore cursor postion
    uint16_t csi_parms[MAX_CSI_PARMS];        // CSI parameter storage
    uint8_t  num_parms;                       // number of parsed CSI parameters
    uint8_t  intermediate_char;               // CSI intermediate character (only one supported)
    uint8_t  def_color;                       // default terminal colors
    uint8_t  cur_color;                       // logical colors before attribute modifications (high/low nibble)
    uint8_t  state;                           // current ANSI parsing state (e_term_state)
    uint8_t  flags;                           // various terminal flags (e_term_flags)
    uint8_t  color;                           // effective current background and forground color (high/low nibble)
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
// assert x, y matches td->cur_addr VRAM address
static void xansi_assert_xy_valid(xansiterm_data * td)
{
    uint16_t calc_x;
    uint32_t divres = td->cur_addr - td->vram_base;
    // GCC is annoying me and not using perfect opcode that gives division and remainder result
    __asm__ __volatile__(
        "divu.w %[w],%[divres]\n"
        "move.l %[divres],%[calc_x]\n"
        "swap.w %[calc_x]\n"
        : [divres] "+d"(divres), [calc_x] "=d"(calc_x)
        : [w] "d"((uint16_t)td->cols));

    uint16_t calc_y = divres;

    // check for match
    if (td->x != calc_x || td->y != calc_y)
    {
        // if y is off by 1 andLCF set, this is fine (last column flag for delayed wrap)
        if (!td->lcf || (calc_y - td->y) != 1)
        {
            LOGF("ASSERT: cur_addr:0x%04x vs x, y: %u,%u (calculated %u,%u)\n",
                 td->cur_addr,
                 td->x,
                 td->y,
                 calc_x,
                 calc_y);

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
static inline void xansi_calc_cur_addr()
{
    xansiterm_data * td = get_xansi_data();
    td->cur_addr        = xansi_calc_addr(td, td->x, td->y);
}

static void        xansi_scroll_up();
static inline void xansi_check_lcf(xansiterm_data * td)
{
    if (td->lcf)
    {
        td->lcf = 0;
        if ((uint16_t)(td->cur_addr - td->vram_base) >= td->vram_size)
        {
            td->cur_addr = td->vram_base + (td->vram_size - td->cols);
            xansi_scroll_up();
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
static _NOINLINE void xansi_clear(uint16_t start, uint16_t end)
{
    xansiterm_data * td = get_xansi_data();

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

// scroll unrolled for 16-bytes per loop, so no inline please
static _NOINLINE void xansi_do_scroll()
{
    xansiterm_data * td = get_xansi_data();
    xv_prep();

    // scroll 4 longs per loop (8 words)
    uint16_t i;
    for (i = td->vram_size - td->cols; i >= 8; i -= 8)
    {
        xm_setl(DATA, xm_getl(DATA));
        xm_setl(DATA, xm_getl(DATA));
        xm_setl(DATA, xm_getl(DATA));
        xm_setl(DATA, xm_getl(DATA));
    }
    // scroll remaining longs (0-3)
    for (; i >= 2; i -= 2)
    {
        xm_setl(DATA, xm_getl(DATA));
    }

    // scroll remaining word
    if (i)
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

// draw input cursor (trying to make it visible)
static inline void xansi_draw_cursor(xansiterm_data * td)
{
    xv_prep();

    if (!td->cursor_drawn)
    {
        td->cursor_drawn = true;
        xm_setw(RW_INCR, 0x0000);
        xm_setw(RW_ADDR, td->cur_addr);
        uint16_t data   = xm_getw(RW_DATA);
        td->cursor_save = data;

        // calculate cursor color:
        // start with current forground and background color swapped
        uint16_t cursor_color = ((uint16_t)(td->color & 0x0f) << 12) | ((uint16_t)(td->color & 0xf0) << 4);

        // check for same cursor foreground and data foreground
        if ((uint16_t)((cursor_color ^ data) & 0x0f00) == 0)
        {
            cursor_color ^= 0x0800;        // if match, toggle bright/dim of foreground
        }
        // check for same cursor background and data background
        if ((uint16_t)((cursor_color ^ data) & 0xf000) == 0)
        {
            cursor_color ^= 0x8000;        // if match, toggle bright/dim of background
        }

        xm_setw(RW_DATA, (uint16_t)(cursor_color | (uint16_t)(data & 0x00ff)));        // draw char with cursor colors
    }
}

// erase input cursor (if drawn)
static inline void xansi_erase_cursor(xansiterm_data * td)
{
    xv_prep();

    if (td->cursor_drawn)
    {
        td->cursor_drawn = false;
        xm_setw(WR_ADDR, td->cur_addr);
        xm_setw(DATA, td->cursor_save);
    }
}


// functions that don't need to be so fast, so optimize for size
#pragma GCC push_options
#pragma GCC optimize("-Os")

// set first 16 colors to default VGA colors
static void set_default_colors()
{
    static const uint16_t def_colors16[16] = {0x0000,
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
                                              0x0ff5,
                                              0x0fff};
    xv_prep();

    xm_setw(XR_ADDR, XR_COLOR_MEM);
    for (uint16_t i = 0; i < 16; i++)
    {
        xm_setw(XR_DATA, def_colors16[i]);
    };
}

// reset video mode and terminal state
static void xansi_reset()
{
    xansiterm_data * td = get_xansi_data();
    xv_prep();

    // set xosera playfield A registers
    bool     alt_font    = td->flags & TFLAG_8X8_FONT;
    uint16_t rows        = ((uint16_t)(xreg_getw(VID_VSIZE) >> (uint16_t)(alt_font ? 3 : 4)));        // get text rows
    uint16_t cols        = (uint16_t)(xreg_getw(VID_HSIZE) >> 3);        // get pixel width / 8 for text columns
    uint16_t tile_addr   = alt_font ? 0x800 : 0x0000;
    uint16_t tile_height = alt_font ? 7 : 15;

    td->vram_base = 0;
    td->vram_size = cols * rows;
    td->vram_end  = td->vram_size;
    td->cols      = cols;
    td->rows      = rows;
    td->cur_color = td->def_color;
    td->color     = td->def_color;

    while ((xreg_getw(SCANLINE) & 0x8000))
        ;
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

    set_default_colors();

    if (td->x >= cols)
    {
        td->x   = cols - 1;
        td->lcf = 0;
    }
    if (td->y >= rows)
    {
        td->y = rows - 1;
    }
    xansi_calc_cur_addr();
}

// invert screen, invert again to restore unless invert flag set
static void xansi_visualbell(bool invert)
{
    xansiterm_data * td = get_xansi_data();
    xv_prep();

    xm_setw(RD_INCR, 1);
    xm_setw(WR_INCR, 1);
    for (int l = 0; l < (invert ? 1 : 2); l++)
    {
        xm_setw(RD_ADDR, td->vram_base);
        xm_setw(WR_ADDR, td->vram_base);
        for (uint16_t i = 0; i < ((td->flags & TFLAG_8X8_FONT) ? td->vram_end : td->vram_end << 1); i++)
        {
            uint16_t data = xm_getw(DATA);
            xm_setw(DATA,
                    (((uint16_t)(data & 0xf000) >> 4) | (uint16_t)((data & 0x0f00) << 4) | (uint16_t)(data & 0xff)));
        }
    }
}

// clear screen (clears the full 8x8 height)
static void xansi_cls()
{
    xansiterm_data * td = get_xansi_data();

    // if not using 8x8 font, clear double high (clear if mode switched later)
    xansi_clear(td->vram_base, (td->flags & TFLAG_8X8_FONT) ? td->vram_end : td->vram_end << 1);
    td->cur_addr = td->vram_base;
    td->x        = 0;
    td->y        = 0;
    td->lcf      = 0;
}

// setup Xosera registers for scrolling up and call scroll function
static void xansi_scroll_up()
{
    xansiterm_data * td = get_xansi_data();

    xv_prep();
    xm_setw(WR_INCR, 1);
    xm_setw(RD_INCR, 1);
    xm_setw(WR_ADDR, td->vram_base);
    xm_setw(RD_ADDR, td->vram_base + td->cols);
    xansi_do_scroll();
}

// setup Xosera registers for scrolling down and call scroll function
static inline void xansi_scroll_down(xansiterm_data * td)
{
    xv_prep();
    xm_setw(WR_INCR, -1);
    xm_setw(RD_INCR, -1);
    xm_setw(WR_ADDR, (uint16_t)(td->vram_end - 1));
    xm_setw(RD_ADDR, (uint16_t)(td->vram_end - 1 - td->cols));
    xansi_do_scroll();
}
#pragma GCC pop_options        // end -Os

// process normal character (not CSI or ESC sequence)
static void xansi_processchar(char cdata)
{
    xansiterm_data * td = get_xansi_data();

    if ((uint8_t)cdata >= ' ' || (td->flags & TFLAG_ATTRIB_PASSTHRU))
    {
        xansi_drawchar(td, cdata);
        return;
    }

    switch (cdata)
    {
        case '\a':
            // VT:  \a      BEL ^G alert (visual bell)
            LOG("[BELL]");
            xansi_visualbell(false);
            return;        // fast out (no lcf clear)
        case '\b':
            // VT:  \b      BS  ^H backspace (stops at left margin)
            LOG("[BS]");
            if (td->x > 0)
            {
                td->x -= 1;
                td->cur_addr--;
            }
            break;
        case '\t':
            // VT:    \t    HT  ^I  8 char tab EXTENSION: wraps to next line when < 8 chars
            LOG("[TAB]");
            uint16_t nx = (uint16_t)(td->x & ~0x7) + 8;
            if ((uint16_t)(td->cols - nx) >= 8)
            {
                td->cur_addr += (nx - td->x);
                td->x = nx;
            }
            else
            {
                td->cur_addr -= td->x;
                td->cur_addr += td->cols;
                td->x = 0;
                td->y++;
            }
            break;
        case '\n':
            // VT:  \n  LF  ^J  line feed (or LF+CR in NEWLINE mode)
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
            // VT:  \v  VT  ^K  vertical tab EXTENSION: reverse LF (VT100 is another LF)
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
            // VT:  \f  FF  ^L  form feed EXTENSION clear screen and home cursor (VT100 yet another LF)
            LOG("[FF]");
            xansi_cls();
            break;
        case '\r':
            // VT:  \r  CR  ^M  carriage return (move to left margin)
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
    td->lcf = 0;

#if DEBUG
    xansi_assert_xy_valid(td);
#endif
}

// parsing functions can be small
#pragma GCC push_options
#pragma GCC optimize("-Os")

// starts CSI sequence or ESC sequence (if c is ESC)
static inline void xansi_begin_csi_or_esc(xansiterm_data * td, char c)
{
    td->state             = (c == '\x1b') ? TSTATE_ESC : TSTATE_CSI;
    td->intermediate_char = 0;
    td->num_parms         = 0;
    memset(td->csi_parms, 0, sizeof(td->csi_parms));
}

// process ESC sequence (only single character supported)
static inline void xansi_process_esc(xansiterm_data * td, char cdata)
{
    td->state = TSTATE_NORMAL;
    switch (cdata)
    {
        case '\x9b':
            // VT: $9B      CSI
        case '[':
            // VT: <ESC>[   CSI
            xansi_begin_csi_or_esc(td, cdata);
            return;
        case 'c':
            // VT: <ESC>c  RIS reset initial settings
            LOGF("%c\n  := [RIS]", cdata);
            td->flags = 0;
            xansi_reset();
            xansi_cls();
            return;
        case '7':
            // VT: <ESC>7  DECSC save cursor
            LOGF("%c\n[DECSC]", cdata);
            td->save_x   = td->x;
            td->save_y   = td->y;
            td->save_lcf = td->lcf;
            return;
        case '8':
            // VT: <ESC>8  DECRC restore cursor
            LOGF("%c\n  := [DECRC]\n", cdata);
            td->x   = td->save_x;
            td->y   = td->save_y;
            td->lcf = td->save_lcf;
            break;
        case '(':
            // VT: <ESC>(  G0 character set (8x16 default) EXTENSION: Xosera 8x16 font size
            td->flags &= ~TFLAG_8X8_FONT;
            xansi_reset();
            LOGF("%c\n  := [FONT0 8x16 %dx%d]\n", cdata, td->cols, td->rows);
            return;
        case ')':
            // VT: <ESC>)  G1 character set (8x8 alternate) EXTENSION: Xosera 8x8 font size
            td->flags |= TFLAG_8X8_FONT;
            xansi_reset();
            LOGF("%c\n  := [FONT1 8x8 %dx%d]\n", cdata, td->cols, td->rows);
            break;
        case 'D':
            // VT: <ESC>D  IND move cursor down (regardless of NEWLINE mode)
            LOGF("%c\n  := [CDOWN]", cdata);
            uint8_t save_flags = td->flags;        // save flags
            td->flags &= ~TFLAG_NEWLINE;           // clear NEWLINE
            xansi_processchar('\n');
            td->flags = save_flags;        // restore flags
            break;
        case 'M':
            // VT: <ESC>M  RI move cursor up
            LOGF("%c\n  := [RI]\n", cdata);
            xansi_processchar(/* td, */ '\v');
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
        case 0x7f:        // ignore DEL and stay in ESC state
            td->state = TSTATE_ESC;
            break;
        default:
            LOGF("%c\n  := [ignore 0x%02x]\n", (cdata >= ' ' && cdata < 0x7f) ? cdata : ' ', cdata);
            return;
    }
    xansi_calc_cur_addr();
}

// process a completed CSI sequence
static inline void xansi_process_csi(xansiterm_data * td, char cdata)
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
                        xansi_reset();
                        xansi_cls();
                        LOGF("[RECONFIG %dx%d]", td->rows, td->cols);
                    }
                }
                else if (num == 5)
                {
                    // VT:  <CSI>?5h    screen mode reverse EXTENSION: this swaps current and default fore/back colors
                    // VT:  <CSI>?5l    screen mode normal EXTENSION: this swaps current and default fore/back colors
                    if (cdata == 'l' || cdata == 'h')
                    {
                        td->def_color = (uint8_t)((td->def_color & 0xf0) >> 4) | (uint8_t)((td->def_color & 0x0f) << 4);
                        td->color     = (uint8_t)((td->color & 0xf0) >> 4) | (uint8_t)((td->color & 0x0f) << 4);
                        td->cur_color = (uint8_t)((td->cur_color & 0xf0) >> 4) | (uint8_t)((td->cur_color & 0x0f) << 4);
                        xansi_visualbell(true);
                        LOG("[SCREEN REVERSE]");
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
            // VT: <CSI>s  save cursor position (ANSI)
            LOG("[CURSOR SAVE]");
            td->save_x   = td->x;
            td->save_y   = td->y;
            td->save_lcf = td->lcf;
            break;
        case 'u':
            // VT: <CSI>u  restore cursor position (ANSI)
            LOG("[CURSOR RESTORE]");
            td->x   = td->save_x;
            td->y   = td->save_y;
            td->lcf = td->save_lcf;
            break;
        case 'J':
            // VT:  <CSI>J  erase down from cursor line to end of screen
            // VT:  <CSI>1J erase up from cursor line to start of screen
            // VT:  <CSI>2J erase whole screen
            LOGF("[ERASE %s]", num_z == 0 ? "DOWN" : num_z == 1 ? "UP" : num_z == 2 ? "SCREEN" : "?");
            switch (num_z)
            {
                case 0:
                    xansi_clear(xansi_calc_addr(td, 0, td->y), td->vram_end);
                    break;
                case 1:
                    xansi_clear(td->vram_base, xansi_calc_addr(td, td->cols - 1, td->y));
                    break;
                case 2:
                    xansi_clear(td->vram_base, td->vram_end);
                    break;
                default:
                    break;
            }
            break;
        case 'K':
            // VT:  <CSI>K  erase from cursor to end of line
            // VT:  <CSI>1K erase from cursor to start of line
            // VT:  <CSI>2K erase from whole cursor line
            LOGF("[ERASE %s]", num_z == 0 ? "EOL" : num_z == 1 ? "SOL" : num_z == 2 ? "LINE" : "?");
            switch (num_z)
            {
                case 0:
                    xansi_clear(td->cur_addr, xansi_calc_addr(td, td->cols - 1, td->y));
                    break;
                case 1:
                    xansi_clear(xansi_calc_addr(td, 0, td->y), td->cur_addr);
                    break;
                case 2:
                    xansi_clear(xansi_calc_addr(td, 0, td->y), xansi_calc_addr(td, td->cols - 1, td->y));
                    break;
                default:
                    break;
            }
            break;
        case 'm':
            // VT: <CSI><parm>; ... m set character attributes
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
                        col += 8;        // make color light
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

    xansi_calc_cur_addr();
}

// parse CSI sequence
static inline void xansi_parse_csi(xansiterm_data * td, char cdata)
{
    uint8_t cclass = cdata & 0xf0;
    // ignore ctrl characters (mostly)
    if (cdata <= ' ' || cdata == 0x7f)        // NOTE: also ignores negative (high bit set)
    {
        // VT:  \x18    CAN terminate current CSI sequence, otherwise ignored
        // VT:  \x1A    SUB terminate current CSI sequence, otherwise ignored
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
                LOG("[ERR: illegal parms >16]\n");
                td->state = TSTATE_ILLEGAL;
            }
        }
        else if (cdata == ':')
        {
            LOG("[ERR: illegal colon]\n");
            td->state = TSTATE_ILLEGAL;
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
        // enter ILLEGAL state (until CAN, SUB or final character)
        LOGF("[ERR: illegal '%c' (0x%02x)]", (cdata >= ' ' && cdata < 0x7f) ? cdata : ' ', cdata);
        td->state = TSTATE_ILLEGAL;
    }
}

#pragma GCC pop_options        // end -Os

// external public terminal functions

// output character to terminal
void xansiterm_putchar(char cdata)
{
    xansiterm_data * td = get_xansi_data();

#if DEBUG
    // these are used to help DEBUG various state changes
    uint8_t  initial_state   = td->state;
    uint8_t  initial_flags   = td->flags;
    uint8_t  initial_cur_col = td->cur_color;
    uint8_t  initial_col     = td->color;
    uint16_t initial_x       = td->x;
    uint16_t initial_y       = td->y;

    xansi_assert_xy_valid(td);        //
#endif

    xansi_erase_cursor(td);

    // ESC or 8-bit CSI received
    if ((cdata & 0x7f) == '\x1b')
    {
        // if already in CSI/ESC state and PASSTHRU set, print 2nd CSI/ESC
        if (td->state >= TSTATE_ESC && (td->flags & TFLAG_ATTRIB_PASSTHRU))
        {
            td->state = TSTATE_NORMAL;
            xansi_processchar(cdata);
        }
        else
        {
            // otherwise start new CSI/ESC
            xansi_begin_csi_or_esc(td, cdata);
        }
    }
    else if (td->state == TSTATE_NORMAL)
    {
        xansi_processchar(/* td, */ cdata);
    }
    else if (cdata == '\x18' || cdata == '\x1A')
    {
        // VT: $18     ABORT (CAN)
        // VT: $1A     ABORT (SUB)
        LOGF("[CANCEL: 0x%02x]", cdata);
        td->state = TSTATE_NORMAL;        // TODO: cancel state
    }
    else if (td->state == TSTATE_ESC)        // NOTE: only one char sequences supported
    {
        xansi_process_esc(td, cdata);
    }
    else if (td->state == TSTATE_CSI)
    {
        xansi_parse_csi(td, cdata);
    }
    else if (td->state == TSTATE_ILLEGAL)
    {
        if (cdata >= 0x40)
        {
            td->state = TSTATE_NORMAL;
            LOGF("[end skip '%c' 0x%02x]", (cdata >= ' ' && cdata < 0x7f) ? cdata : ' ', cdata);
        }
        else
        {
            LOGF("[skip '%c' 0x%02x]", (cdata >= ' ' && cdata < 0x7f) ? cdata : ' ', cdata);
        }
    }

#if DEBUG
    // show altered state in log
    if (initial_flags != td->flags)
    {
        LOGF("{Flags:%02x->%02x}", initial_flags, td->flags);
    }
    if (initial_cur_col != td->cur_color || initial_col != td->color)
    {
        LOGF("{Color:%02x:%02x->%02x:%02x}", initial_cur_col, initial_col, td->cur_color, td->color);
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
        LOGF("%s",
             td->state == TSTATE_NORMAL    ? "\n<NORM>"
             : td->state == TSTATE_ILLEGAL ? "<ILLEGAL>"
             : td->state == TSTATE_ESC     ? "<ESC>"
                                           : "<CSI>");
    }
#endif
}


// support functions can be small
#pragma GCC push_options
#pragma GCC optimize("-Os")

// terminal read input character (wrapper for console readchar with cursor)
char xansiterm_readchar()
{
    xansiterm_data * td = get_xansi_data();
    xansi_erase_cursor(td);        // make sure cursor not drawn

    // TODO terminal query stuff, but seems not so useful.
    return readchar();
}

// terminal check for input character ready (wrapper console checkchar with cursor)
bool xansiterm_checkchar()
{
    xansiterm_data * td = get_xansi_data();
    xv_prep();

    xansi_check_lcf(td);        // wrap cursor if needed
    bool char_ready = checkchar();
    // blink at ~409.6ms (on half the time but only if cursor not disabled and no char ready)
    bool show_cursor = !(td->flags & TFLAG_HIDE_CURSOR) && !char_ready && (xm_getw(TIMER) & 0x800);
    if (show_cursor)
    {
        xansi_draw_cursor(td);
    }
    else
    {
        xansi_erase_cursor(td);        // make sure cursor not drawn
    }

    return char_ready;
}

// initialize terminal functions
void xansiterm_init()
{
    LOG("[xansiterm_init]\n");

    xansiterm_data * td = get_xansi_data();
    memset(td, 0, sizeof(*td));
    // set default color (that is not reset if changed) // TODO: ICP default
    td->def_color = DEFAULT_COLOR;        // default dark-green on black

    xansi_reset();
    xansi_cls();
}

#pragma GCC pop_options        // end -Os
