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
 * Xosera low-level C API to read/write Xosera registers
 * ------------------------------------------------------------
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <machine.h>

#include "xosera_m68k_api.h"

bool xosera_init(int reconfig_num)
{
    // check for Xosera presense (retry in case it is reconfiguring)
    for (int r = 0; r < 500; r++)
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
        // set reconfig bit, along with reconfig values
        xm_setw(SYS_CTRL, 0x40FF | (reconfig_num << 12));        // reboot FPGA to config_num
        if (xosera_sync())                                       // should not sync right away...
        {
            return false;
        }
        // wait for Xosera to regain consciousness (takes ~80 milliseconds)
        for (int r = 0; r < 500; r++)
        {
            cpu_delay(10);
            if (xosera_sync())
            {
                break;
            }
        }
    }

    return xosera_sync();
}

bool xosera_sync()
{
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

// NOTE: size is in bytes, but assumed to be a multiple of 2 (words)
void xv_vram_fill(uint32_t vram_addr, uint32_t numwords, uint32_t word_value)
{
    xm_setw(WR_ADDR, vram_addr);
    xm_setw(WR_INCR, 1);
    uint32_t long_value = (word_value << 16) | (word_value & 0xffff);
    if (numwords & 1)
    {
        xm_setw(WR_DATA, word_value);
    }
    int long_size = numwords >> 1;
    while (long_size--)
    {
        xm_setl(WR_DATA, long_value);
    }
}

// NOTE: numbytes is in bytes, but assumed to be a multiple of 2 (words)
void xv_copy_to_vram(uint16_t * source, uint32_t vram_dest, uint32_t numbytes)
{
    xm_setw(WR_ADDR, vram_dest);
    xm_setw(WR_INCR, 1);
    if (numbytes & 2)
    {
        xm_setw(WR_DATA, *source++);
    }
    uint32_t * long_ptr  = (uint32_t *)source;
    int        long_size = numbytes >> 2;
    while (long_size--)
    {
        xm_setl(WR_DATA, *long_ptr++);
    }
}

// NOTE: size is in bytes, but assumed to be a multiple of 2 (words)
void xv_copy_from_vram(uint32_t vram_source, uint16_t * dest, uint32_t numbytes)
{
    xm_setw(RD_ADDR, vram_source);
    xm_setw(RD_INCR, 1);
    if (numbytes & 2)
    {
        *dest++ = xm_getw(DATA);
    }
    uint32_t * long_ptr  = (uint32_t *)dest;
    int        long_size = numbytes >> 2;
    while (long_size--)
    {
        *long_ptr++ = xm_getl(DATA);
    }
}
