/*
 * vim: set et ts=4 sw=4
 *------------------------------------------------------------
 *                                  ___ ___ _
 *  ___ ___ ___ ___ ___       _____|  _| . | |_
 * |  _| . |_ -|  _| . |     |     | . | . | '_|
 * |_| |___|___|___|___|_____|_|_|_|___|___|_,_|
 *                     |_____|
 * ------------------------------------------------------------
 * Portions Copyright (c) 2021 Ross Bamford
 * Portions Copyright (c) 2021 Xark
 * MIT License
 *
 * Test and tech-demo for Xosera FPGA "graphics card"
 * Crop test with copper.
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

// Copper list
const uint16_t copper_list[] = {
    COP_VPOS(40),                                                                             // Wait for line 40
    COP_MOVER(MAKE_GFX_CTRL(0x00, GFX_VISIBLE, GFX_4_BPP, GFX_BITMAP, GFX_2X, GFX_2X),        // 4-bpp+Hx2+Vx2
              PA_GFX_CTRL),
    COP_VPOS(440),                                                                            // Wait for line 440
    COP_MOVER(MAKE_GFX_CTRL(0x00, GFX_BLANKED, GFX_4_BPP, GFX_BITMAP, GFX_2X, GFX_2X),        // Blank+4-bpp+Hx2+Vx2
              PA_GFX_CTRL),
    COP_END()        // wait for next frame
};

// xosera_crop_test
int main()
{
    mcBusywait(1000 * 500);        // wait a bit for terminal window/serial
    while (mcCheckInput())         // clear any queued input
    {
        mcInputchar();
    }
    xv_prep();

    debug_puts("copper crop_test - set Xosera to 640x480\n\n");
    debug_puts("Checking for Xosera XANSI firmware...");
    if (xosera_xansi_detect(true))
    {
        debug_puts("detected.\n");
    }
    else
    {
        debug_puts(
            "\n\nXosera XANSI firmware was not detected!\n"
            "This program will likely trap without Xosera hardware.\n");
    }
    xosera_init(XINIT_CONFIG_640x480);                  // 640x480
    xreg_setw(VID_CTRL, MAKE_VID_CTRL(0, 0x00));        // set border black

    xmem_setw_next_addr(XR_COPPER_ADDR);
    for (uint8_t i = 0; i < (sizeof(copper_list) / sizeof(copper_list[0])); i++)
    {
        xmem_setw_next(copper_list[i]);
    }

    xreg_setw(PA_LINE_LEN, 160);

    xm_setw(WR_INCR, 0x0001);
    xm_setw(WR_ADDR, 0x0000);

    for (int y = 0; y < 200; ++y)
        for (int x = 0; x < 320 / 2; ++x)
            xm_setw(DATA, x == 0 || y == 0 || x == (320 / 2 - 1) || y == (200 - 1) ? 0x0f0f : 0x0101);

    // enable Copper
    xreg_setw(COPP_CTRL, MAKE_COPP_CTRL(1));

    debug_puts("640x480 cropped to 640x400 - press a key\n");

    // wait for a key (so prints don't mess up screen)
    mcInputchar();

    xosera_init(XINIT_CONFIG_848x480);        // 848x480

    uint16_t width = xosera_vid_width();        // use read hsize (in case no 848 mode in FPGA)

    xreg_setw(VID_CTRL, MAKE_VID_CTRL(0, 0x00));        // set border black

    xmem_setw_next_addr(XR_COPPER_ADDR);
    for (uint8_t i = 0; i < (sizeof(copper_list) / sizeof(copper_list[0])); i++)
    {
        xmem_setw_next(copper_list[i]);
    }

    xreg_setw(PA_LINE_LEN, 160);

    xm_setw(WR_INCR, 0x0001);
    xm_setw(WR_ADDR, 0x0000);

    for (int y = 0; y < 200; ++y)
        for (int x = 0; x < 320 / 2; ++x)
            xm_setw(DATA, x == 0 || y == 0 || x == (320 / 2 - 1) || y == (200 - 1) ? 0x0f0f : 0x0202);

    // enable Copper
    xreg_setw(COPP_CTRL, MAKE_COPP_CTRL(1));

    // wait for a key (so prints don't mess up screen)
    debug_puts("848x480 cropped to 848x400 (oops!) - press a key\n");
    mcInputchar();

    xreg_setw(VID_LEFT, (width - 640) / 2);
    xreg_setw(VID_RIGHT, width - ((width - 640) / 2));

    // wait for a key (so prints don't mess up screen)
    debug_puts("848x480 cropped to 848x400 with vid_left & vid_right window (ahh!) - press a key\n");
    mcInputchar();

    xm_setw(WR_INCR, 0x0001);
    xm_setw(WR_ADDR, 0x0000);

    for (int y = 0; y < 200; ++y)
        for (int x = 0; x < 320 / 2; ++x)
            xm_setw(DATA, x == 0 || y == 0 || x == (320 / 2 - 1) || y == (200 - 1) ? 0x0f0f : 0x0404);

    debug_puts("848x480 cropped to 848x400 hammering line_len reg glitch test - press a key\n");
    while (!mcCheckInput())
    {
        for (int i = 0; i < 32768; i++)
        {
            xreg_setw(PA_LINE_LEN, 160);
            xreg_setw(PA_LINE_LEN, 160);
            xreg_setw(PA_LINE_LEN, 160);
            xreg_setw(PA_LINE_LEN, 160);
            xreg_setw(PA_LINE_LEN, 160);
            xreg_setw(PA_LINE_LEN, 160);
            xreg_setw(PA_LINE_LEN, 160);
            xreg_setw(PA_LINE_LEN, 160);
        }
    }
    mcInputchar();

    // disable Copper
    xreg_setw(COPP_CTRL, MAKE_COPP_CTRL(0));

    debug_puts("exit...\n");

    // restore text mode
    xosera_xansi_restore();
}
