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

const uint8_t  copper_list_len = 26;
const uint16_t copper_list[]   = {
    /*
     * This is a convoluted way to do this, but does serve as a useful test
     * of the copper instructions...
     */

    // copperlist:
    0x20a0,
    0x0002,        //     skip  0, 160, 0b00010  ; Skip next if we've hit line 160
    0x4014,
    0x0000,        //     jmp   .gored           ; ... else, jump to set red
    0x2140,
    0x0002,        //     skip  0, 320, 0b00010  ; Skip next if we've hit line 320
    0x400e,
    0x0000,        //     jmp   .gogreen         ; ... else jump to set green
    0xa000,
    0x000f,        //     movep 0x000F, 0        ; Make background blue
    0xa00a,
    0x0004,        //     movep 0x0004, 0xA      ; Make foreground dark blue
    0x0000,
    0x0003,        //     nextf                  ; and we're done for this frame
                   // .gogreen:
    0xa000,
    0x00f0,        //     movep 0x00F0, 0        ; Make background green
    0xa00a,
    0x0040,        //     movep 0x0040, 0xA      ; Make foreground dark green
    0x4000,
    0x0000,        //     jmp   copperlist       ; and restart
                   // .gored:
    0xa000,
    0x0f00,        //     movep 0x0F00, 0        ; Make background red
    0xa00a,
    0x0400,        //     movep 0x0400, 0xA      ; Make foreground dark red
    0x4000,
    0x0000        //     jmp   copperlist       ; and restart

    /* This is a saner way to do the above!
    0xb000, 0x0f00, // movep 0, 0x0F00            ; Make background red
    0xb00a, 0x0400, // movep 0xA, 0x0F00          ; Make foreground dark red
    0x00a0, 0x0002, // wait  0, 160, 0b000010     ; Wait for line 160, ignore X position
    0xb000, 0x00f0, // movep 0, 0x00F0            ; Make background green
    0xb00a, 0x0040, // movep 0xA, 0x007           ; Make foreground dark green
    0x0140, 0x0002, // wait  0, 320, 0b000010     ; Wait for line 320, ignore X position
    0xb000, 0x000f, // movep 0, 0x000F            ; Make background blue
    0xb00a, 0x0004, // movep 0xA, 0x0007          ; Make foreground dark blue
    0x0000, 0x0003  // nextf                      ; Wait for next frame
    */
};

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

    xosera_init(0);

    xmem_set_addr(XR_COPPER_ADDR);

    for (uint8_t i = 0; i < copper_list_len; i++)
    {
        xmem_setw_next(copper_list[i]);
    }

    xreg_setw(COPP_CTRL, 0x8000);

    uint16_t features  = xreg_getw(FEATURES);
    uint16_t monwidth  = xreg_getw(VID_HSIZE);
    uint16_t monheight = xreg_getw(VID_VSIZE);

    uint16_t gfxctrl  = xreg_getw(PA_GFX_CTRL);
    uint16_t tilectrl = xreg_getw(PA_TILE_CTRL);
    uint16_t dispaddr = xreg_getw(PA_DISP_ADDR);
    uint16_t linelen  = xreg_getw(PA_LINE_LEN);
    uint16_t hvscroll = xreg_getw(PA_HV_SCROLL);
    uint16_t hvfscale = xreg_getw(PA_HV_FSCALE);

    dprintf("Xosera - Features: 0x%04x\n", features);
    dprintf("Monitor Mode: %dx%d\n", monwidth, monheight);
    dprintf("\nPlayfield A:\n");
    dprintf("PA_GFX_CTRL : 0x%04x  PA_TILE_CTRL: 0x%04x\n", gfxctrl, tilectrl);
    dprintf("PA_DISP_ADDR: 0x%04x  PA_LINE_LEN : 0x%04x\n", dispaddr, linelen);
    dprintf("PA_HV_SCROLL: 0x%04x  PA_HV_FSCALE: 0x%04x\n", hvscroll, hvfscale);
}
