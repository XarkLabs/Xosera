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
const uint32_t copper_list[] = {
    COP_WAIT_V(40),                        // wait  0, 40                   ; Wait for line 40, H position ignored
    COP_MOVER(0x0065, PA_GFX_CTRL),        // mover 0x0065, PA_GFX_CTRL     ; Set to 8-bpp + Hx2 + Vx2
    COP_WAIT_V(440),                       // wait  0, 440                  ; Wait for line 440, H position ignored
    COP_MOVER(0x00E5, PA_GFX_CTRL),        // mover 0x00E5, PA_GFX_CTRL     ; Set to Blank + 8-bpp + Hx2 + Vx2
    COP_END()                              // nextf
};

void xosera_test()
{
    if (checkchar())
    {
        readchar();
    }

    xosera_init(0);

    xreg_setw(VID_CTRL, 0x0000);        // set border black

    xm_setw(XR_ADDR, XR_COPPER_MEM);

    for (uint8_t i = 0; i < (sizeof(copper_list) / sizeof(uint32_t)); i++)
    {
        xm_setw(XR_DATA, copper_list[i] >> 16);
        xm_setw(XR_DATA, copper_list[i] & 0xffff);
    }

    xreg_setw(PA_LINE_LEN, 160);

    xm_setw(WR_INCR, 0x0001);
    xm_setw(WR_ADDR, 0x0000);

    for (int y = 0; y < 200; ++y)
        for (int x = 0; x < 320 / 2; ++x)
            xm_setw(DATA, x == 0 || y == 0 || x == (320 / 2 - 1) || y == (200 - 1) ? 0x0f0f : 0x0101);

    // enable Copper
    xreg_setw(COPP_CTRL, 0x8000);

    // wait for a key (so prints don't mess up screen)
    readchar();

    // disable Copper
    xreg_setw(COPP_CTRL, 0x0000);

    // restore text mode
    xosera_init(1);
    xreg_setw(PA_GFX_CTRL, 0x0000);        // unblank screen
    print("\033c");                        // reset & clear
}
