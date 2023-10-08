/*
 * vim: set et ts=4 sw=4
 *------------------------------------------------------------
 *                                  ___ ___ _
 *  ___ ___ ___ ___ ___       _____|  _| . | |_
 * |  _| . |_ -|  _| . |     |     | . | . | '_|
 * |_| |___|___|___|___|_____|_|_|_|___|___|_,_|
 *                     |_____|
 * ------------------------------------------------------------
 * Copyright (c) 2021 Ross Bamford
 * Copyright (c) 2021 Xark
 * MIT License
 *
 * Test and tech-demo for Xosera FPGA "graphics card"
 *
 * This demo loads a copper list that divides the screen into
 * three color bands and then exits. This will cause a warm
 * reboot with the copper list still loaded.
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

#include "xosera_m68k_api.h"

#define SHOW_BARS 1

#if SHOW_BARS

#include "color_bar_table.h"

#else

const uint16_t copper_list[]   = {
    COP_MOVI(0x0F00, XR_COLOR_ADDR+0x0),
    COP_MOVI(0x0400, XR_COLOR_ADDR+0xA),
    COP_VPOS(160),
    COP_MOVI(0x00F0, XR_COLOR_ADDR+0x0),
    COP_MOVI(0x0040, XR_COLOR_ADDR+0xA),
    COP_VPOS(320),
    COP_MOVI(0x000F, XR_COLOR_ADDR+0x0),
    COP_MOVI(0x0004, XR_COLOR_ADDR+0xA),
    COP_END()
};
#endif

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

void xosera_copper_test()
{
    dprintf("Xosera_copper_test\n");
    dprintf("Checking for Xosera XANSI firmware...");
    if (xosera_xansi_detect(true))
    {
        dprintf("detected.\n");
    }
    else
    {
        dprintf(
            "\n\nXosera XANSI firmware was not detected!\n"
            "This program will likely trap without Xosera hardware.\n");
    }
    xv_prep();

#if SHOW_BARS
    xmem_setw_next_addr(color_bar_table_start);
    for (uint8_t i = 0; i < color_bar_table_size; i++)
    {
        xmem_setw_next(color_bar_table_bin[i]);
    }
#else
    xmem_setw_next_addr(XR_COPPER_ADDR);
    for (uint8_t i = 0; i < sizeof (copper_list)/sizeof (copper_list[0]); i++)
    {
        xmem_setw_next(copper_list[i]);
    }
#endif

    xreg_setw(VID_CTRL, 0x0000);        // border uses color 0
    xreg_setw(COPP_CTRL, 0x8000);

    uint8_t  feature   = xm_getbh(FEATURE);
    uint16_t monwidth  = xosera_vid_width();
    uint16_t monheight = xosera_vid_height();

    uint16_t gfxctrl  = xreg_getw(PA_GFX_CTRL);
    uint16_t tilectrl = xreg_getw(PA_TILE_CTRL);
    uint16_t dispaddr = xreg_getw(PA_DISP_ADDR);
    uint16_t linelen  = xreg_getw(PA_LINE_LEN);
    uint16_t hscroll = xreg_getw(PA_H_SCROLL);
    uint16_t vscroll = xreg_getw(PA_V_SCROLL);
    uint16_t hvfscale = xreg_getw(PA_HV_FSCALE);

    dprintf("Xosera - Features: 0x%02x\n", feature);
    dprintf("Monitor Mode: %dx%d\n", monwidth, monheight);
    dprintf("\nPlayfield A:\n");
    dprintf("PA_GFX_CTRL : 0x%04x  PA_TILE_CTRL: 0x%04x\n", gfxctrl, tilectrl);
    dprintf("PA_DISP_ADDR: 0x%04x  PA_LINE_LEN : 0x%04x\n", dispaddr, linelen);
    dprintf("PA_H_SCROLL : 0x%04x  PA_V_SCROLL : 0x%04x\n", hscroll, vscroll);
    dprintf("PA_HV_FSCALE: 0x%04x\n", hvfscale);

    delay(15000);
}
