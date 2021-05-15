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

#include "xosera_api.h"

bool xosera_init(int reconfig_num)
{
    // check for Xosera presense
    for (int r = 0; r < 100; r++)
    {
        if (xosera_sync())
        {
            break;
        }
        delay(10);
    }

    // done if configuration if not valid (0 to 3)
    if ((reconfig_num & 3) == reconfig_num)
    {
        xv_setw(const_val, 0xb007);
        // set magic "Intel" constant OR'd with config number to make Xosera faint and reconfigure
        xv_setw(blit_ctrl, 0x8080 | (reconfig_num << 8));        // reboot FPGA to config_num
        delay(20);
        if (xv_getw(const_val) == 0xb007)
        {
            printf("(reconfig failed)");
        }
        // wait for Xosera to regain consciousness (takes ~80 milliseconds)
        for (int r = 0; r < 100; r++)
        {
            delay(20);
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
    uint16_t cv = xv_getw(const_val);
    xv_setw(const_val, 0x55AA);
    if (xv_getw(const_val) != 0x55AA)
    {
        return false;        // not detected
    }
    xv_setw(const_val, 0xAA55);
    if (xv_getw(const_val) != 0xAA55)
    {
        return false;        // not detected
    }
    xv_setw(const_val, cv);
    return true;
}

void xv_vram_fill(int vram_addr, int size, int word_value)
{
    xv_setw(wr_addr, vram_addr);
    xv_setw(wr_inc, 1);
    word_value = (word_value << 16) | (word_value & 0xffff);
    if (size & 1)
    {
        xv_setw(data, word_value);
    }
    int long_size = size >> 1;
    while (long_size--)
    {
        xv_setl(data, word_value);
    }
}

// NOTE: size is in bytes, but assumed to be a multiple of 2 (words)
void xv_copy_to_vram(uint16_t * source, int vram_dest, int size)
{
    xv_setw(wr_addr, vram_dest);
    xv_setw(wr_inc, 1);
    if (size & 2)
    {
        xv_setw(data, *source++);
    }
    uint32_t * long_ptr  = (uint32_t *)source;
    int        long_size = size >> 2;
    while (long_size--)
    {
        xv_setl(data, *long_ptr++);
    }
}

// NOTE: size is in bytes, but assumed to be a multiple of 2 (words)
void xv_copy_from_vram(int vram_source, uint16_t * dest, int size)
{
    xv_setw(rd_addr, vram_source);
    xv_setw(rd_inc, 1);
    if (size & 2)
    {
        *dest++ = xv_getw(data);
    }
    uint32_t * long_ptr  = (uint32_t *)dest;
    int        long_size = size >> 2;
    while (long_size--)
    {
        *long_ptr++ = xv_getl(data);
    }
}
