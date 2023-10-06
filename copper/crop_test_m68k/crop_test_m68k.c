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

static void msg(char * msg)
{
    char * s = msg;
    char   c;
    while ((c = *s++) != '\0')
    {
        sendchar(c);
    }
    sendchar('\r');
    sendchar('\n');
}

void xosera_crop_test()
{
    printf("\033c\033[?25l");        // ANSI reset, disable input cursor

    msg("copper crop_test - set Xosera to 640x480");
    msg("");
    xosera_init(0);        // 640x480

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
            xm_setw(DATA, x == 0 || y == 0 || x == (320 / 2 - 1) || y == (200 - 1) ? 0x0f0f : 0x0101);

    // enable Copper
    xreg_setw(COPP_CTRL, 0x8000);

    msg("640x480 cropped to 640x400 - press a key");

    // wait for a key (so prints don't mess up screen)
    readchar();

    xosera_init(1);        // 848x480

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
    msg("848x480 cropped to 848x400 (oops!) - press a key");
    readchar();

    xreg_setw(VID_LEFT, (width - 640) / 2);
    xreg_setw(VID_RIGHT, width - ((width - 640) / 2));

    // wait for a key (so prints don't mess up screen)
    msg("848x480 cropped to 848x400 with vid_left & vid_right window (ahh!) - press a key");
    readchar();

    xm_setw(WR_INCR, 0x0001);
    xm_setw(WR_ADDR, 0x0000);

    for (int y = 0; y < 200; ++y)
        for (int x = 0; x < 320 / 2; ++x)
            xm_setw(DATA, x == 0 || y == 0 || x == (320 / 2 - 1) || y == (200 - 1) ? 0x0f0f : 0x0404);

    msg("848x480 cropped to 848x400 hammering line_len reg glitch test - press a key");
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

    msg("exit...");

    // restore text mode
    xreg_setw(PA_LINE_LEN, width / 8);
    xreg_setw(VID_LEFT, 0);
    xreg_setw(VID_RIGHT, width);
    xreg_setw(PA_GFX_CTRL, 0x0000);        // unblank screen
    print("\033c");                        // reset & clear
}
