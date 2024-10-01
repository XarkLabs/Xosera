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

#include <rosco_m68k/machine.h>
#include <rosco_m68k/xosera.h>

#include "rosco_m68k_support.h"

#define SHOW_BARS 1

#if SHOW_BARS
#include "color_bar_table.h"
#else
const uint16_t copper_list[] = {COP_MOVI(0x0F00, XR_COLOR_ADDR + 0x0),
                                COP_MOVI(0x0400, XR_COLOR_ADDR + 0xA),
                                COP_VPOS(160),
                                COP_MOVI(0x00F0, XR_COLOR_ADDR + 0x0),
                                COP_MOVI(0x0040, XR_COLOR_ADDR + 0xA),
                                COP_VPOS(320),
                                COP_MOVI(0x000F, XR_COLOR_ADDR + 0x0),
                                COP_MOVI(0x0004, XR_COLOR_ADDR + 0xA),
                                COP_END()};
#endif

// xosera_copper_test
int main()
{
    mcBusywait(1000 * 500);        // wait a bit for terminal window/serial
    while (mcCheckInput())         // clear any queued input
    {
        mcInputchar();
    }

    debug_printf("Xosera_copper_test\n");
    debug_printf("Checking for Xosera XANSI firmware...");
    if (xosera_xansi_detect(true))
    {
        debug_printf("detected.\n");
    }
    else
    {
        debug_printf(
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
    for (uint8_t i = 0; i < sizeof(copper_list) / sizeof(copper_list[0]); i++)
    {
        xmem_setw_next(copper_list[i]);
    }
#endif

    xreg_setw(VID_CTRL, MAKE_VID_CTRL(0, 0x00));        // border uses color 0
    xreg_setw(COPP_CTRL, MAKE_COPP_CTRL(1));            // enable copper

    printf("\23348;5;0m");        // set default background color to 0
    printf("\033c");              // reset screen (and clear it)

    uint16_t feature   = xm_getw(FEATURE);
    uint16_t monwidth  = xosera_vid_width();
    uint16_t monheight = xosera_vid_height();

    uint16_t sysctrl = xm_getw(SYS_CTRL);
    uint16_t intctrl = xm_getw(INT_CTRL);

    uint16_t vidctrl  = xreg_getw(VID_CTRL);
    uint16_t coppctrl = xreg_getw(COPP_CTRL);
    uint16_t audctrl  = xreg_getw(AUD_CTRL);
    uint16_t vidleft  = xreg_getw(VID_LEFT);
    uint16_t vidright = xreg_getw(VID_RIGHT);

    uint16_t pa_gfxctrl  = xreg_getw(PA_GFX_CTRL);
    uint16_t pa_tilectrl = xreg_getw(PA_TILE_CTRL);
    uint16_t pa_dispaddr = xreg_getw(PA_DISP_ADDR);
    uint16_t pa_linelen  = xreg_getw(PA_LINE_LEN);
    uint16_t pa_hscroll  = xreg_getw(PA_H_SCROLL);
    uint16_t pa_vscroll  = xreg_getw(PA_V_SCROLL);
    uint16_t pa_hvfscale = xreg_getw(PA_HV_FSCALE);

    uint16_t pb_gfxctrl  = xreg_getw(PB_GFX_CTRL);
    uint16_t pb_tilectrl = xreg_getw(PB_TILE_CTRL);
    uint16_t pb_dispaddr = xreg_getw(PB_DISP_ADDR);
    uint16_t pb_linelen  = xreg_getw(PB_LINE_LEN);
    uint16_t pb_hscroll  = xreg_getw(PB_H_SCROLL);
    uint16_t pb_vscroll  = xreg_getw(PB_V_SCROLL);
    uint16_t pb_hvfscale = xreg_getw(PB_HV_FSCALE);

    debug_printf("FEATURE     : 0x%04x\n", feature);
    debug_printf("MONITOR RES : %dx%d\n", monwidth, monheight);
    debug_printf("\nConfig:\n");
    debug_printf("SYS_CTRL    : 0x%04x  INT_CTRL    : 0x%04x\n", sysctrl, intctrl);
    debug_printf("VID_CTRL    : 0x%04x  COPP_CTRL   : 0x%04x\n", vidctrl, coppctrl);
    debug_printf("AUD_CTRL    : 0x%04x\n", audctrl);
    debug_printf("VID_LEFT    : 0x%04x  VID_RIGHT   : 0x%04x\n", vidleft, vidright);
    debug_printf("\nPlayfield A:                                Playfield B:\n");
    debug_printf("PA_GFX_CTRL : 0x%04x  PA_TILE_CTRL: 0x%04x  PB_GFX_CTRL : 0x%04x  PB_TILE_CTRL: 0x%04x\n",
                 pa_gfxctrl,
                 pa_tilectrl,
                 pb_gfxctrl,
                 pb_tilectrl);
    debug_printf("PA_DISP_ADDR: 0x%04x  PA_LINE_LEN : 0x%04x  PB_DISP_ADDR: 0x%04x  PB_LINE_LEN : 0x%04x\n",
                 pa_dispaddr,
                 pa_linelen,
                 pb_dispaddr,
                 pb_linelen);

    debug_printf("PA_H_SCROLL : 0x%04x  PA_V_SCROLL : 0x%04x  PB_H_SCROLL : 0x%04x  PB_V_SCROLL : 0x%04x\n",
                 pa_hscroll,
                 pa_vscroll,
                 pb_hscroll,
                 pb_vscroll);

    debug_printf("PA_HV_FSCALE: 0x%04x                        PB_HV_FSCALE: 0x%04x\n", pa_hvfscale, pb_hvfscale);
    debug_printf("\n\n");


    printf("Press any key...\n");

    mcInputchar();

    mcBusywait(15000);
}
