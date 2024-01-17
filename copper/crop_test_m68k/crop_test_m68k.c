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

#include <basicio.h>
#include <machine.h>
#include <sdfat.h>


#include "xosera_m68k_api.h"

// Copper list
const uint16_t copper_list[] = {
    COP_VPOS(40),                          // Wait for line 40
    COP_MOVER(0x0065, PA_GFX_CTRL),        // Set to 8-bpp + Hx2 + Vx2
    COP_VPOS(440),                         // Wait for line 440
    COP_MOVER(0x00E5, PA_GFX_CTRL),        // Set to Blank + 8-bpp + Hx2 + Vx2
    COP_END()                              // wait for next frame
};

static void dputs(char * msg)
{
    char * s = msg;
    char   c;
    while ((c = *s++) != '\0')
    {
        if (c == '\n')
        {
            sendchar('\r');
        }
        sendchar(c);
    }
}

void xosera_crop_test()
{
    xv_prep();

    dputs("copper crop_test - set Xosera to 640x480\n\n");
    dputs("Checking for Xosera XANSI firmware...");
    if (xosera_xansi_detect(true))
    {
        dputs("detected.\n");
    }
    else
    {
        dputs(
            "\n\nXosera XANSI firmware was not detected!\n"
            "This program will likely trap without Xosera hardware.\n");
    }
    xosera_init(XINIT_CONFIG_640x480);        // 640x480
    xreg_setw(VID_CTRL, 0x0000);              // set border black

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
    xreg_setw(COPP_CTRL, 0x8000);

    dputs("640x480 cropped to 640x400 - press a key\n");

    // wait for a key (so prints don't mess up screen)
    readchar();

    xosera_init(XINIT_CONFIG_848x480);        // 848x480

    uint16_t width = xosera_vid_width();        // use read hsize (in case no 848 mode in FPGA)

    xreg_setw(VID_CTRL, 0x0000);        // set border black

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
    xreg_setw(COPP_CTRL, 0x8000);

    // wait for a key (so prints don't mess up screen)
    dputs("848x480 cropped to 848x400 (oops!) - press a key\n");
    readchar();

    xreg_setw(VID_LEFT, (width - 640) / 2);
    xreg_setw(VID_RIGHT, width - ((width - 640) / 2));

    // wait for a key (so prints don't mess up screen)
    dputs("848x480 cropped to 848x400 with vid_left & vid_right window (ahh!) - press a key\n");
    readchar();

    xm_setw(WR_INCR, 0x0001);
    xm_setw(WR_ADDR, 0x0000);

    for (int y = 0; y < 200; ++y)
        for (int x = 0; x < 320 / 2; ++x)
            xm_setw(DATA, x == 0 || y == 0 || x == (320 / 2 - 1) || y == (200 - 1) ? 0x0f0f : 0x0404);

    dputs("848x480 cropped to 848x400 hammering line_len reg glitch test - press a key\n");
    while (!checkchar())
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
    readchar();

    // disable Copper
    xreg_setw(COPP_CTRL, 0x0000);

    dputs("exit...\n");

    // restore text mode
    xosera_xansi_restore();
}
