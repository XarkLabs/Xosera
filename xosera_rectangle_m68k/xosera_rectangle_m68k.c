/*
 * vim: set et ts=4 sw=4
 *------------------------------------------------------------
 *                                  ___ ___ _
 *  ___ ___ ___ ___ ___       _____|  _| . | |_
 * |  _| . |_ -|  _| . |     |     | . | . | '_|
 * |_| |___|___|___|___|_____|_|_|_|___|___|_,_|
 *                     |_____|
 * ------------------------------------------------------------
 * Copyright (c) 2024 Xark
 * MIT License
 *
 * Test and example for Xosera filled rectangle
 * ------------------------------------------------------------
 */

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <basicio.h>
#include <machine.h>

#include "xosera_m68k_api.h"

// rosco_m68k support

static void dputc(char c)
{
    __asm__ __volatile__(
        "move.w %[chr],%%d0\n"
        "move.l #2,%%d1\n"        // SENDCHAR
        "trap   #14\n"
        :
        : [chr] "d"(c)
        : "d0", "d1");
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
static void dprintf(const char * fmt, ...) __attribute__((__format__(__printf__, 1, 2)));
static void dprintf(const char * fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(dprint_buff, sizeof(dprint_buff), fmt, args);
    dprint(dprint_buff);
    va_end(args);
}

// xosera support

xosera_info_t initinfo;

static void reset_vid(void)
{
    xv_prep();

    xwait_not_vblank();
    xwait_vblank();

    xreg_setw(VID_CTRL, 0x0008);
    xreg_setw(COPP_CTRL, 0x0000);
    xreg_setw(AUD_CTRL, 0x0000);
    xreg_setw(VID_LEFT, 0);
    xreg_setw(VID_RIGHT, xosera_vid_width());
    xreg_setw(POINTER_H, 0x0000);
    xreg_setw(POINTER_V, 0x0000);

    xreg_setw(PA_GFX_CTRL, MAKE_GFX_CTRL(0x00, 0, GFX_1_BPP, 0, 0, 0));
    xreg_setw(PA_TILE_CTRL, MAKE_TILE_CTRL(XR_TILE_ADDR, 0, 0, 16));
    xreg_setw(PA_DISP_ADDR, 0x0000);
    xreg_setw(PA_LINE_LEN, xosera_vid_width() / 8);
    xreg_setw(PA_HV_FSCALE, MAKE_HV_FSCALE(0, 0));
    xreg_setw(PA_H_SCROLL, MAKE_H_SCROLL(0));
    xreg_setw(PA_V_SCROLL, MAKE_V_SCROLL(0, 0));

    xreg_setw(PB_GFX_CTRL, MAKE_GFX_CTRL(0x00, 1, GFX_1_BPP, 0, 0, 0));
    xreg_setw(PB_TILE_CTRL, MAKE_TILE_CTRL(XR_TILE_ADDR, 0, 0, 16));
    xreg_setw(PB_DISP_ADDR, 0x0000);
    xreg_setw(PB_LINE_LEN, xosera_vid_width() / 8);
    xreg_setw(PB_HV_FSCALE, MAKE_HV_FSCALE(0, 0));
    xreg_setw(PB_H_SCROLL, MAKE_H_SCROLL(0));
    xreg_setw(PB_V_SCROLL, MAKE_V_SCROLL(0, 0));

    printf("\033c");        // reset XANSI

    while (checkinput())
    {
        readchar();
    }
}

_NOINLINE bool delay_check(int ms)
{
    xv_prep();

    do
    {
        if (checkinput())
        {
            return true;
        }
        if (ms)
        {
            uint16_t tms = 10;
            do
            {
                uint16_t tv = xm_getw(TIMER);
                while (tv == xm_getw(TIMER))
                    ;
            } while (--tms);
        }
    } while (--ms);

    return false;
}

// Xosera rectangle test code

#define SCREEN_ADDR   0x0000        // VRAM address of start of bitmap
#define SCREEN_WIDTH  320           // pixel width of bitmap
#define SCREEN_HEIGHT 240           // pixel height of bitmap
#define PIXELS_8_BPP  2             // pixels per word 8-bpp
#define PIXELS_4_BPP  4             // pixels per word 4-bpp

#define RECT_SIZE 64

void fill_rect_8bpp(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c)
{
    // zero w or h ignored
    if (w < 1 || h < 1)
        return;

    uint16_t va  = SCREEN_ADDR + (y * (SCREEN_WIDTH / PIXELS_8_BPP)) + (x / PIXELS_8_BPP);        // vram address
    uint16_t ww  = ((w + 1) + (x & 1)) / PIXELS_8_BPP;        // width in words (round up, and also if right edge odd)
    uint16_t mod = (SCREEN_WIDTH / PIXELS_8_BPP) - ww;        // destination bitmap modulo
    uint16_t mask =
        ((x & 1) ? 0x3000 : 0xF000) | (((x + w) & 1) ? 0x0C00 : 0x0F00);        // fw mask even/odd | lw mask even/odd
    //    dprintf(">> va=0x%04x, ww=%d, fw[%d] lw[%d] mask=%04x\n", va, ww, x & 1, (x + w) & 1, mask);

    xv_prep();
    xwait_blit_ready();                                      // wait until blit queue empty
    xreg_setw(BLIT_CTRL, MAKE_BLIT_CTRL(0, 0, 0, 1));        // tr_val=NA, tr_8bit=NA, tr_enable=FALSE, const_S=TRUE
    xreg_setw(BLIT_ANDC, 0x0000);                            // ANDC constant (0=no effect)
    xreg_setw(BLIT_XOR, 0x0000);                             // XOR constant (0=no effect)
    xreg_setw(BLIT_MOD_S, 0x0000);                           // source modulo (constant, so not used)
    xreg_setw(BLIT_SRC_S, c);                                // word pattern (color byte repeated in word)
    xreg_setw(BLIT_MOD_D, mod);                              // dest modulo (screen width - blit width)
    xreg_setw(BLIT_DST_D, va);                               // VRAM address of upper left word
    xreg_setw(BLIT_SHIFT, mask);                             // first/last word masking (no shifting)
    xreg_setw(BLIT_LINES, h - 1);                            // lines = height-1
    xreg_setw(BLIT_WORDS, ww - 1);                           // width = blit width -1 (and go!)
}

void fill_rect_4bpp(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c)
{
    static uint8_t fw_mask[4] = {0xF0, 0x70, 0x30, 0x10};        // XXXX .XXX ..XX ...X
    static uint8_t lw_mask[4] = {0x0F, 0x08, 0x0C, 0x0E};        // XXXX X... XX.. XXX.

    // zero w or h ignored
    h -= 1;        // adjust height-1
    if (w < 1 || h < 0)
        return;

    uint16_t va   = SCREEN_ADDR + (y * (SCREEN_WIDTH / PIXELS_4_BPP)) + (x / PIXELS_4_BPP);        // vram address
    uint16_t ww   = ((w + (x & 3) + 3)) / PIXELS_4_BPP;        // round up width to words, +1 if not word aligned
    uint16_t mod  = (SCREEN_WIDTH / PIXELS_4_BPP) - ww;        // destination bitmap modulo
    uint16_t mask = (fw_mask[x & 3] | lw_mask[(x + w) & 3]) << 8;        // fw mask & lw mask
    //    dprintf(">> va=0x%04x, ww=%d, fw[%d] lw[%d] mask=%04x\n", va, ww, x & 3, (x + w) & 3, mask);

    xv_prep();
    xwait_blit_ready();                                      // wait until blit queue empty
    xreg_setw(BLIT_CTRL, MAKE_BLIT_CTRL(0, 0, 0, 1));        // tr_val=NA, tr_8bit=NA, tr_enable=FALSE, const_S=TRUE
    xreg_setw(BLIT_ANDC, 0x0000);                            // ANDC constant (0=no effect)
    xreg_setw(BLIT_XOR, 0x0000);                             // XOR constant (0=no effect)
    xreg_setw(BLIT_MOD_S, 0x0000);                           // source modulo (constant, so not used)
    xreg_setw(BLIT_SRC_S, c);                                // word pattern (color byte repeated in word)
    xreg_setw(BLIT_MOD_D, mod);                              // dest modulo (screen width - blit width)
    xreg_setw(BLIT_DST_D, va);                               // VRAM address of upper left word
    xreg_setw(BLIT_SHIFT, mask);                             // first/last word masking (no shifting)
    xreg_setw(BLIT_LINES, h);                                // lines = height-1
    xreg_setw(BLIT_WORDS, ww - 1);                           // width = blit width -1 (and go!)
}

void blit_fill(uint16_t vaddr, uint16_t words, uint16_t val)
{
    xv_prep();
    xwait_blit_ready();                                      // wait until blit queue empty
    xreg_setw(BLIT_CTRL, MAKE_BLIT_CTRL(0, 0, 0, 1));        // tr_val=NA, tr_8bit=NA, tr_enable=FALSE, const_S=TRUE
    xreg_setw(BLIT_ANDC, 0x0000);                            // ANDC constant (0=no effect)
    xreg_setw(BLIT_XOR, 0x0000);                             // XOR constant (0=no effect)
    xreg_setw(BLIT_MOD_S, 0x0000);                           // source modulo (constant, so not used)
    xreg_setw(BLIT_SRC_S, val);                              // word pattern (color byte repeated in word)
    xreg_setw(BLIT_MOD_D, 0x000);                            // dest modulo (screen width - blit width)
    xreg_setw(BLIT_DST_D, vaddr);                            // VRAM address of upper left word
    xreg_setw(BLIT_SHIFT, 0xFF00);                           // first/last word masking (no shifting)
    xreg_setw(BLIT_LINES, 0);                                // 1-D
    xreg_setw(BLIT_WORDS, words - 1);                        // width = blit width -1 (and go!)
    xwait_blit_done();                                       // wait until blit finishes fill
}

inline static uint16_t get_rand()
{
    return rand();
}

void xosera_rectangle()
{
    xv_prep();

    dprintf("Xosera_rectangle_m68k\n");

    dprintf("Checking for Xosera XANSI firmware...");
    if (xosera_xansi_detect(true))        // check for XANSI (and disable input cursor if present)
    {
        dprintf("detected.\n");
    }
    else
    {
        dprintf(
            "\n\nXosera XANSI firmware was not detected!\n"
            "This program will likely trap without Xosera hardware.\n");
    }

    dprintf("Calling xosera_init(XINIT_CONFIG_640x480)...");
    bool success = xosera_init(XINIT_CONFIG_640x480);
    dprintf("%s (%dx%d)\n\n", success ? "succeeded" : "FAILED", xosera_vid_width(), xosera_vid_height());

    if (!success)
    {
        dprintf("Exiting without Xosera init.\n");
        exit(1);
    }

    xosera_get_info(&initinfo);

    // hide mode switch
    xwait_not_vblank();
    xwait_vblank();

    xreg_setw(PA_GFX_CTRL, MAKE_GFX_CTRL(0x00, GFX_VISIBLE, GFX_8_BPP, GFX_BITMAP, GFX_2X, GFX_2X));
    xreg_setw(PA_TILE_CTRL, MAKE_TILE_CTRL(0x0C00, 0, 0, 8));
    xreg_setw(PA_DISP_ADDR, SCREEN_ADDR);
    xreg_setw(PA_LINE_LEN, SCREEN_WIDTH / PIXELS_8_BPP);
    xreg_setw(PA_H_SCROLL, MAKE_H_SCROLL(0));
    xreg_setw(PA_V_SCROLL, MAKE_V_SCROLL(0, 0));
    xreg_setw(PA_HV_FSCALE, MAKE_HV_FSCALE(HV_FSCALE_OFF, HV_FSCALE_OFF));

    xreg_setw(PB_GFX_CTRL, MAKE_GFX_CTRL(0x00, GFX_BLANKED, GFX_1_BPP, GFX_TILEMAP, GFX_1X, GFX_1X));

    blit_fill(SCREEN_ADDR, SCREEN_WIDTH * SCREEN_HEIGHT / PIXELS_8_BPP, 0x0000);

    uint16_t c = 1;
    for (int s = 0; s < (SCREEN_WIDTH - RECT_SIZE); s += 3)
    {
        fill_rect_8bpp(s, s, 13, 8, (c << 8) | c);
        c = (c + 1) & 0xf;
        if (c == 0)
            c = 1;
    }

    dprintf("(Done with 8 bpp diagonal rects, Press a key)\n");
    readchar();

    // hide mode switch
    xwait_not_vblank();
    xwait_vblank();

    xreg_setw(PA_GFX_CTRL, MAKE_GFX_CTRL(0x00, GFX_VISIBLE, GFX_4_BPP, GFX_BITMAP, GFX_2X, GFX_2X));
    xreg_setw(PA_TILE_CTRL, MAKE_TILE_CTRL(0x0C00, 0, 0, 8));
    xreg_setw(PA_DISP_ADDR, SCREEN_ADDR);
    xreg_setw(PA_LINE_LEN, SCREEN_WIDTH / PIXELS_4_BPP);
    xreg_setw(PA_H_SCROLL, MAKE_H_SCROLL(0));
    xreg_setw(PA_V_SCROLL, MAKE_V_SCROLL(0, 0));
    xreg_setw(PA_HV_FSCALE, MAKE_HV_FSCALE(HV_FSCALE_OFF, HV_FSCALE_OFF));

    blit_fill(SCREEN_ADDR, SCREEN_WIDTH * SCREEN_HEIGHT / PIXELS_4_BPP, 0x0000);

    for (int s = 0; s < (SCREEN_WIDTH - RECT_SIZE); s += 3)
    {
        fill_rect_4bpp(s, s, 13, 8, (c << 12) | (c << 8) | (c << 4) | c);
        c = (c + 1) & 0xf;
        if (c == 0)
            c = 1;
    }

    dprintf("(Done with 4 bpp diagonal rects, Press a key)\n");
    readchar();

    blit_fill(SCREEN_ADDR, SCREEN_WIDTH * SCREEN_HEIGHT / PIXELS_4_BPP, 0x0000);


    for (int r = 0; r < 100; r++)
    {
        uint16_t w = SCREEN_WIDTH;
        uint16_t h = SCREEN_HEIGHT;
        c          = 1;
        while (h)
        {
            fill_rect_4bpp((SCREEN_WIDTH - w) / 2, (SCREEN_HEIGHT - h) / 2, w, h, (c << 12) | (c << 8) | (c << 4) | c);
            c = (c + 1) & 0xf;
            if (c == 0)
                c = 1;
            w -= 2;
            h -= 2;
        }
        xwait_not_vblank();
        xwait_vblank();
        // make sure completed frame shown
        xwait_not_vblank();
        xwait_vblank();
    }
    dprintf("(Done with 4 bpp nested rects, Press a key)\n");
    readchar();

    blit_fill(SCREEN_ADDR, SCREEN_WIDTH * SCREEN_HEIGHT / PIXELS_4_BPP, 0x0000);

    srand(xm_getw(TIMER));

    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    for (int r = 0; r < 10000; r++)
    {
        c = get_rand() & 0xf;
        x = get_rand() & 0x3ff;
        y = get_rand() & 0x1ff;
        w = get_rand() & 0x03f;
        h = get_rand() & 0x03f;

        fill_rect_4bpp(x, y, w, h, (c << 12) | (c << 8) | (c << 4) | c);
        if ((get_rand() & 0xf000) == 0x0000)
        {
            srand(xm_getw(TIMER));
            get_rand();
        }
    }

    dprintf("(Done with 4 bpp random rects, Press a key)\n");
    readchar();

    dprintf("Exiting normally.\n");

    // exit test
    reset_vid();
}
