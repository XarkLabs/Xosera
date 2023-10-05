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
#include <sdfat.h>

#define DELAY_TIME 1000        // impatient human speed

#if !defined(NUM_ELEMENTS)
#define NUM_ELEMENTS(a) (sizeof(a) / sizeof(a[0]))
#endif

#include "xosera_m68k_api.h"

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
static void dprintf(const char * fmt, ...) __attribute__((__format__(__printf__, 1, 2)));
static void dprintf(const char * fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(dprint_buff, sizeof(dprint_buff), fmt, args);
    dprint(dprint_buff);
    va_end(args);
}

static void reset_vid(void)
{
    xwait_not_vblank();
    xwait_vblank();

    xreg_setw(VID_CTRL, 0x0008);
    xreg_setw(COPP_CTRL, 0x0000);
    xreg_setw(AUD_CTRL, 0x0000);
    xreg_setw(VID_LEFT, 0);
    xreg_setw(VID_RIGHT, xosera_vid_width());
    xreg_setw(POINTER_H, 0x0000);
    xreg_setw(POINTER_V, 0x0000);

    xreg_setw(PA_GFX_CTRL, MAKE_GFX_CTRL(0x00, 0, GFX_BPP_1, 0, 0, 0));
    xreg_setw(PA_TILE_CTRL, MAKE_TILE_CTRL(XR_TILE_ADDR, 0, 0, 16));
    xreg_setw(PA_DISP_ADDR, 0x0000);
    xreg_setw(PA_LINE_LEN, xosera_vid_width() / 8);
    xreg_setw(PA_HV_FSCALE, MAKE_HV_FSCALE(0, 0));
    xreg_setw(PA_H_SCROLL, MAKE_H_SCROLL(0));
    xreg_setw(PA_V_SCROLL, MAKE_V_SCROLL(0, 0));

    xreg_setw(PB_GFX_CTRL, MAKE_GFX_CTRL(0x00, 1, GFX_BPP_1, 0, 0, 0));
    xreg_setw(PB_TILE_CTRL, MAKE_TILE_CTRL(XR_TILE_ADDR, 0, 0, 16));
    xreg_setw(PB_DISP_ADDR, 0x0000);
    xreg_setw(PB_LINE_LEN, xosera_vid_width() / 8);
    xreg_setw(PB_HV_FSCALE, MAKE_HV_FSCALE(0, 0));
    xreg_setw(PB_H_SCROLL, MAKE_H_SCROLL(0));
    xreg_setw(PB_V_SCROLL, MAKE_V_SCROLL(0, 0));

    printf("\033c");        // reset XANSI

    while (checkchar())
    {
        readchar();
    }
}

static inline void checkbail()
{
    if (checkchar())
    {
        reset_vid();
        _WARM_BOOT();
    }
}

_NOINLINE void delay_check(int ms)
{
    while (ms--)
    {
        checkbail();
        uint16_t tms = 10;
        do
        {
            uint16_t tv = xm_getw(TIMER);
            while (tv == xm_getw(TIMER))
                ;
        } while (--tms);
    }
}

void blit_1bpp_to_4bpp(uint16_t vram_src_1bpp,
                       uint16_t src_width,
                       uint16_t src_height,
                       uint16_t vram_dst_4bpp,
                       uint16_t dst_mod,
                       uint16_t color)
{
    struct blit_parms
    {
        uint16_t mask;         // blit_andc mask
        uint16_t shift;        // blit_shift reg value
        uint16_t doff;         // destination word offset
        uint16_t width;        // width minus one for blit
    };

    static const struct blit_parms parms[16] = {{
                                                    // 8000
                                                    // 8--- ---- ---- ---- >> 0, +0
                                                    // 8000
                                                    0x8000 ^ 0xffff,        // mask
                                                    0x8800,                 // shift
                                                    0,                      // doff
                                                    1 - 1,                  // width-1
                                                },
                                                {
                                                    // 4000
                                                    // -4-- ---- ---- ---- >> 1, +0
                                                    // 0400
                                                    0x0400 ^ 0xffff,        // mask
                                                    0x4401,                 // shift
                                                    0,                      // doff
                                                    1 - 1,                  // width-1
                                                },
                                                {
                                                    // 2000
                                                    // --2- ---- ---- ---- >> 2, +0
                                                    // 0020
                                                    0x0020 ^ 0xffff,        // mask
                                                    0x2202,                 // shift
                                                    0,                      // doff
                                                    1 - 1,                  // width-1
                                                },
                                                {
                                                    // 1000
                                                    // ---1 ---- ---- ---- >> 3, +0
                                                    // 0001
                                                    0x0001 ^ 0xffff,        // mask
                                                    0x1103,                 // shift
                                                    0,                      // doff
                                                    1 - 1,                  // width-1
                                                },
                                                {
                                                    // 0800 Xxxx
                                                    // ---- 8--- ---- ---- >> 3, +0
                                                    // xxxx 8000           w 2
                                                    0x8000 ^ 0xffff,        // mask
                                                    0x0803,                 // shift
                                                    0,                      // doff
                                                    2 - 1,                  // width-1
                                                },
                                                {
                                                    //      0400
                                                    // ---- -4-- ---- ---- >> 0, +1
                                                    //      0400
                                                    0x0400 ^ 0xffff,        // mask
                                                    0x4400,                 // shift
                                                    1,                      // doff
                                                    1 - 1,                  // width-1
                                                },
                                                {
                                                    //      0200
                                                    // ---- --2- ---- ---- >> 1, +1
                                                    //      0020
                                                    0x0020 ^ 0xffff,        // mask
                                                    0x2201,                 // shift
                                                    1,                      // doff
                                                    1 - 1,                  // width-1
                                                },
                                                {
                                                    //      0100
                                                    // ---- ---1 ---- ---- >> 2, +1
                                                    //      0001
                                                    0x0001 ^ 0xffff,        // mask
                                                    0x1102,                 // shift
                                                    1,                      // doff
                                                    1 - 1,                  // width-1
                                                },
                                                {
                                                    //      0080 Xxxx
                                                    // ---- ---- 8--- ---- >> 2, +1
                                                    //      xxxx 8000      w 2
                                                    0x8000 ^ 0xffff,        // mask
                                                    0x0802,                 // shift
                                                    1,                      // doff
                                                    2 - 1,                  // width-1
                                                },
                                                {
                                                    //      0040 xXxx
                                                    // ---- ---- -4-- ---- >> 3, +1
                                                    //           0400      w 2
                                                    0x0400 ^ 0xffff,        // mask
                                                    0x0403,                 // shift
                                                    1,                      // doff
                                                    2 - 1,                  // width-1
                                                },
                                                {
                                                    //           0020
                                                    // ---- ---- --2- ---- >> 0, +2
                                                    //           0020
                                                    0x0020 ^ 0xffff,        // mask
                                                    0x2200,                 // shift
                                                    2,                      // doff
                                                    1 - 1,                  // width-1
                                                },
                                                {
                                                    //           0010
                                                    // ---- ---- ---1 ---- >> 1, +2
                                                    //           0001
                                                    0x0001 ^ 0xffff,        // mask
                                                    0x1101,                 // shift
                                                    2,                      // doff
                                                    1 - 1,                  // width-1
                                                },
                                                {
                                                    //           0008 Xxxx
                                                    // ---- ---- ---- 8--- >> 1, +2
                                                    //                8000
                                                    0x8000 ^ 0xffff,        // mask
                                                    0x0801,                 // shift
                                                    2,                      // doff
                                                    2 - 1,                  // width-1
                                                },
                                                {
                                                    //           0004 xXxx
                                                    // ---- ---- ---- -4-- >> 2, +2
                                                    //           xxxx 0400 w 2
                                                    0x0400 ^ 0xffff,        // mask
                                                    0x0402,                 // shift
                                                    2,                      // doff
                                                    2 - 1,                  // width-1
                                                },
                                                {
                                                    //           0002 xxXx
                                                    // ---- ---- ---- --2- >> 3, +2
                                                    //           xxxx 0020 w 2
                                                    0x0020 ^ 0xffff,        // mask
                                                    0x0203,                 // shift
                                                    2,                      // doff
                                                    2 - 1,                  // width-1
                                                },
                                                {
                                                    //                0001
                                                    // ---- ---- ---- ---1 >> 0, +3
                                                    //                0001
                                                    0x0001 ^ 0xffff,        // mask
                                                    0x1100,                 // shift
                                                    3,                      // doff
                                                    1 - 1,                  // width-1
                                                }};

    uint16_t src_mod             = (src_width - 1) / 16;        // source modulo -1
    src_height                   = src_height - 1;              // adjust source height
    dst_mod                      = dst_mod - 1;                 // adjust dest modulo
    color                        = ~color;                      // pre-invert color
    const struct blit_parms * bp = parms;

    xv_prep();

    while (src_width--)
    {
        xwait_blit_ready();
        xreg_setw(BLIT_CTRL, MAKE_BLIT_CTRL(0x00, 0, 1, 0));             // transp=0x00, 4-bpp, transp_en, no s_const
        xreg_setw_next(/*BLIT_ANDC, */ bp->mask);                        // ANDC constant
        xreg_setw_next(/*BLIT_XOR,  */ bp->mask ^ color);                // XOR constant
        xreg_setw_next(/*BLIT_MOD_S,*/ src_mod - bp->width);             // source modulo
        xreg_setw_next(/*BLIT_SRC_S,*/ vram_src_1bpp);                   // source addr/value
        xreg_setw_next(/*BLIT_MOD_D,*/ dst_mod - bp->width);             // dest modulo
        xreg_setw_next(/*BLIT_DST_D,*/ vram_dst_4bpp + bp->doff);        // dest address
        xreg_setw_next(/*BLIT_SHIFT,*/ bp->shift);                       // edge masking and shifting
        xreg_setw_next(/*BLIT_LINES,*/ src_height);                      // line count-1
        xreg_setw_next(/*BLIT_WORDS,*/ bp->width);                       // word count-1

        bp++;
        if (bp >= &parms[16])
        {
            bp = parms;
            vram_src_1bpp += 1;
            vram_dst_4bpp += 4;
        }
    }
}

void test_1bpp_font_blit()
{
    const uint16_t bm_width_w = (xosera_vid_width() / 2) / 4;
    const uint16_t bm_addr    = 0x0000;
    const uint16_t fm_addr    = 0xC000;
    //    const uint16_t fm_height  = (15 * 8) - 1;

    uint16_t copsave = xreg_getw(COPP_CTRL);
    xwait_not_vblank();
    xwait_vblank();
    xreg_setw(COPP_CTRL, 0x0000);

    xreg_setw(PA_GFX_CTRL, MAKE_GFX_CTRL(0, 0, GFX_BPP_4, 1, GFX_2X, GFX_2X));
    xreg_setw(PA_TILE_CTRL, 0x0007);
    xreg_setw(PA_DISP_ADDR, 0x0000);
    xreg_setw(PA_LINE_LEN, bm_width_w);
    xreg_setw(PA_H_SCROLL, 0x0000);
    xreg_setw(PA_V_SCROLL, 0x0000);
    xreg_setw(PA_HV_FSCALE, 0x0000);
    xreg_setw(PB_GFX_CTRL, 0x0080);

    xwait_blit_ready();
    // fill vram with 0x0000
    xreg_setw(BLIT_CTRL, 0x0001);              // no transp, constS
    xreg_setw(BLIT_ANDC, 0x0000);              // ANDC constant
    xreg_setw(BLIT_XOR, 0x0000);               // XOR constant
    xreg_setw(BLIT_MOD_S, 0x0000);             // no modulo S
    xreg_setw(BLIT_SRC_S, 0x8888);             // A = fill pattern
    xreg_setw(BLIT_MOD_D, 0x0000);             // no modulo D
    xreg_setw(BLIT_DST_D, 0x0000);             // VRAM display end address
    xreg_setw(BLIT_SHIFT, 0xFF00);             // no edge masking or shifting
    xreg_setw(BLIT_LINES, 0x0000);             // 1D
    xreg_setw(BLIT_WORDS, 0x10000 - 1);        // 64KW VRAM
    xwait_blit_done();

    vram_setw_addr_incr(fm_addr, 1);

    for (int c = 0; c < 256; c += 2)
    {
        for (int v = 0; v < 4; v++)
        {
            uint16_t w  = xmem_getw(FONT_ST_8x8_ADDR + ((c) * 4) + v);
            uint16_t w2 = xmem_getw(FONT_ST_8x8_ADDR + ((c + 1) * 4) + v);
            vram_setw_next((w & 0xff00) | (w2 >> 8));
            vram_setw_next((w << 8) | (w2 & 0x00ff));
        }
    }
    blit_1bpp_to_4bpp(fm_addr, 16, 8 * 16, bm_addr, bm_width_w, 0xffff);


    xwait_blit_done();
    delay_check(DELAY_TIME * 50);

    xreg_setw(COPP_CTRL, copsave);
}

void xosera_font_test()
{
    printf("\033c\033[?25l");        // ANSI reset, disable input cursor

    dprintf("Xosera_test_m68k\n");

    cpu_delay(1000);

    dprintf("Calling xosera_sync()...");
    bool success = xosera_sync();
    dprintf("%s\n", success ? "detected" : "not-detected");

    if (success && xosera_vid_width() != 640)
    {
        dprintf("Calling xosera_init(0)...");
        success = xosera_init(0);
        dprintf("%s (%dx%d)\n\n", success ? "succeeded" : "FAILED", xosera_vid_width(), xosera_vid_height());
    }

    if (!success)
    {
        dprintf("Exiting without Xosera init.\n");
        exit(1);
    }

    while (true)
    {
        test_1bpp_font_blit();
    }

    // exit test
    reset_vid();
}
