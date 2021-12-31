/*
 * vim: set et ts=4 sw=4
 *------------------------------------------------------------
 *  __ __
 * |  |  |___ ___ ___ ___ ___
 * |-   -| . |_ -| -_|  _| .'|
 * |__|__|___|___|___|_| |__,|
 *
 * Xark's Open Source Enhanced Retro Adapter
 *
 * - "Not as clumsy or random as a GPU, an embedded retro
 *    adapter for a more civilized age."
 *
 * ------------------------------------------------------------
 * Copyright (c) 2021 Xark
 * MIT License
 *
 * Xosera rosco_m68k low-level C API for Xosera registers
 * ------------------------------------------------------------
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <machine.h>

#define XV_PREP_REQUIRED
#include "xosera_m68k_api.h"

void xv_delay(uint32_t ms)
{
    xv_prep();
    if (!xosera_sync())
    {
        return;
    }

    while (ms--)
    {
        uint16_t tms = 10;
        do
        {
            uint16_t tv = xm_getw(TIMER);
            while (tv == xm_getw(TIMER))
                ;
        } while (--tms);
    }
}

bool xosera_init(int reconfig_num)
{
    xv_prep();

    // check for Xosera presense (retry in case it is reconfiguring)
    for (int r = 0; r < 200; r++)
    {
        if (xosera_sync())
        {
            break;
        }
        cpu_delay(10);
    }

    // done if configuration if not valid (0 to 3)
    if ((reconfig_num & 3) == reconfig_num)
    {
        uint16_t sys_ctrl_save = xm_getw(SYS_CTRL) & 0x0F0F;
        // set reconfig bit, along with reconfig values
        xm_setw(SYS_CTRL, 0x800F | (uint16_t)(reconfig_num << 13));        // reboot FPGA to config_num

        // wait for Xosera to regain consciousness (takes ~80 milliseconds)
        for (int r = 0; r < 200; r++)
        {
            cpu_delay(10);
            if (xosera_sync())
            {
                break;
            }
        }

        xm_setw(SYS_CTRL, sys_ctrl_save);
    }

    return xosera_sync();
}

bool xosera_sync()
{
    xv_prep();

    xm_setw(XR_ADDR, 0xF5A5);
    if (xm_getw(XR_ADDR) != 0xF5A5)
    {
        return false;        // not detected
    }
    xm_setw(XR_ADDR, 0xFA5A);
    if (xm_getw(XR_ADDR) != 0xFA5A)
    {
        return false;        // not detected
    }
    return true;
}

// define xosera_ptr in a way that GCC can't see the immediate const value (causing it to keep it in a register).
__asm__(
    "               .data\n"
    "               .section    .rodata.xosera_ptr,\"a\"\n"
    "               .align      2\n"
    "               .globl      xosera_ptr\n"
    "xosera_ptr:    .long       " XM_STR(XM_BASEADDR) "\n");
