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
    xv_prep();

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

// 1bpp blit code

void blit_1bpp_to_4bpp(uint16_t vram_src_1bpp,
                       uint16_t src_startx,
                       uint16_t src_endx,
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

    // blit convert 1bpp word -> 4bpp 4 words (per column table)
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

    uint16_t src_mod = (src_width - 1) / 16;        // source modulo -1
    src_height       = src_height - 1;              // adjust source height
    dst_mod          = dst_mod - 1;                 // adjust dest modulo
    color            = ~color;                      // pre-invert color

    xv_prep();

    uint16_t                  xcol = src_startx;
    const struct blit_parms * bp   = &parms[xcol & 0xf];
    while (xcol++ <= src_endx)
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

// copies and "swizzles" TILE font into a 1bpp font in VRAM (with two characters per word x font_height)
void make_1bpp_font(uint16_t tile_addr, uint16_t font_height, uint16_t num_chars, uint16_t vram_addr)
{
    uint16_t font_words = (font_height >> 1);

    xv_prep();
    vram_setw_addr_incr(vram_addr, 1);
    // all 256 chars in pairs
    for (int c = 0; c < num_chars; c += 2)
    {
        // convert char with even/odd byte per line into two char pairs per word stored normally
        for (int wo = 0; wo < font_words; wo++)
        {
            uint16_t c1 = xmem_getw(tile_addr + (c * font_words) + wo);
            uint16_t c2 = xmem_getw(tile_addr + ((c + 1) * font_words) + wo);
            vram_setw_next((c1 & 0xff00) | (c2 >> 8));
            vram_setw_next((c1 << 8) | (c2 & 0x00ff));
        }
    }
}

// draw one tile from "column paired" 1-bpp font
void draw_1bpp_tile(uint16_t font_vaddr,
                    uint16_t font_height,
                    uint16_t bitmap_vaddr,
                    uint16_t bitmap_width,
                    uint16_t c,
                    uint16_t color)
{
    blit_1bpp_to_4bpp(font_vaddr + ((c >> 1) * font_height),
                      c & 1 ? 8 : 0,
                      c & 1 ? 15 : 7,
                      16,
                      font_height,
                      bitmap_vaddr - (c & 1 ? 2 : 0),
                      bitmap_width,
                      color);
}

void puts_1bpp(const char * str,
               uint16_t     font_vaddr,
               uint16_t     font_height,
               uint16_t     bitmap_vaddr,
               uint16_t     bitmap_width,
               uint16_t     color)
{
    char c;
    int  xw = 0;
    while ((c = *str++))
    {
        draw_1bpp_tile(font_vaddr, font_height, bitmap_vaddr + xw, bitmap_width, c, color);
        xw += 2;
    }
}

void xosera_font_test()
{
    xv_prep();

    dprintf("xosera_font_m68k\n");

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
    dprintf("%s (%dx%d)\n", success ? "succeeded" : "FAILED", xosera_vid_width(), xosera_vid_height());

    if (!success)
    {
        dprintf("Exiting.\n");
        exit(1);
    }

    while (true)
    {
        xwait_not_vblank();
        xwait_vblank();
        // center if 848x480
        xreg_setw(VID_LEFT, (xosera_vid_width() - 640) / 2);
        xreg_setw(VID_RIGHT, ((xosera_vid_width() - 640) / 2) + 640);

        const uint16_t font_vaddr   = 0xf000;
        const uint16_t bitmap_vaddr = 0x0000;
        const uint16_t bitmap_width = 640 / 2 / 4;

        xreg_setw(PA_GFX_CTRL, MAKE_GFX_CTRL(0, 0, GFX_4_BPP, 1, GFX_2X, GFX_2X));
        xreg_setw_next(/* PA_TILE_CTRL, */ MAKE_TILE_CTRL(0x0000, 0, 0, 8));
        xreg_setw_next(/* PA_DISP_ADDR, */ bitmap_vaddr);
        xreg_setw_next(/* PA_LINE_LEN,  */ bitmap_width);

        // copy/swizzle font from TILE to VRAM
        make_1bpp_font(FONT_ST_8x8_ADDR, 8, 256, font_vaddr);

        // draw whole font (in word columns, 16 tiles high)
        int      col       = 0;
        uint16_t colors[4] = {0xffff, 0x3333, 0x6666, 0xABCD};
        for (uint16_t fc = 0; fc < 256; fc += 32)
        {
            blit_1bpp_to_4bpp(font_vaddr + (fc * 4),
                              0,
                              15,
                              16,
                              8 * 16,
                              bitmap_vaddr + (col * 4) + 24 + (40 * bitmap_width),
                              bitmap_width,
                              colors[col & 3]);
            col++;
        }

        // write test message
        puts_1bpp("Hello Xosera 1-BPP to 4-BPP column blit!", font_vaddr, 8, bitmap_vaddr, bitmap_width, 0xffff);

        // write test message (with y offset to "prove" bitmap mode)
        const char * msg = "320x240x4 bitmap mode";
        char         c;
        int          xw = 18, yoff = 200;
        while ((c = *msg++))
        {
            draw_1bpp_tile(font_vaddr, 8, bitmap_vaddr + xw + (yoff * bitmap_width), bitmap_width, c, 0x1111);
            xw += 2;
            yoff += 1;
        }

        xwait_blit_done();

        delay_check(DELAY_TIME * 50);
    }

    // exit test
    reset_vid();
}
