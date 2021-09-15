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
const uint8_t  copper_list_len = 10;
const uint16_t copper_list[]   = {
    0x0028,
    0x0002,        // wait  0, 40                   ; Wait for line 40, H position ignored
    0x9010,
    0x0075,        // mover 0x0075, PA_GFX_CTRL     ; Set to 8-bpp + Hx2 + Vx2
    0x01b8,
    0x0002,        // wait  0, 440                  ; Wait for line 440, H position ignored
    0x9010,
    0x0080,        // mover 0x0080, PA_GFX_CTRL     ; Blank
    0x0000,
    0x0003        // nextf
};

void xosera_test()
{
    xosera_init(0);

    xm_setw(XR_ADDR, XR_COPPER_MEM);

    for (uint8_t i = 0; i < copper_list_len; i++)
    {
        xm_setw(XR_DATA, copper_list[i]);
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
}
