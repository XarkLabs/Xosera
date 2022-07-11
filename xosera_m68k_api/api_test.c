/*
 * vim: set et ts=4 sw=4
 *------------------------------------------------------------
 *                                  ___ ___ _
 *  ___ ___ ___ ___ ___       _____|  _| . | |_
 * |  _| . |_ -|  _| . |     |     | . | . | '_|
 * |_| |___|___|___|___|_____|_|_|_|___|___|_,_|
 *                     |_____|
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
 * Xosera rosco_m68k low-level C API test file
 * ------------------------------------------------------------
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <machine.h>

#define XV_PREP_REQUIRED
#include "xosera_m68k_api.h"

#define nop() __asm__ __volatile__("nop ; nop")

xosera_info_t info;

// This is just meant to make sure all API macros compile and pass sanity check
void kmain(void)
{
    xosera_sync();
    xosera_init(0);
    xosera_get_info(&info);
    nop();
    xv_prep();
    nop();
    xm_setbh(TIMER, 0x12);
    nop();
    xm_setbl(TIMER, 0x34);
    nop();
    xm_setw(TIMER, 0x1234);
    nop();
    xm_setl(WR_INCR, 0x87654321);
    nop();
    xreg_setw(SCANLINE, 0x1337);
    nop();
    xreg_set_addr(SCANLINE);
    nop();
    xreg_setw_next(0xC0DE);
    nop();
    vram_setw(0xD00B, 0x1ee7);
    nop();
    vram_set_addr_incr(0x0B0B, 0x0001);
    nop();
    vram_setw_next(0x3456);
    nop();
    vram_setw_wait(0xB00B, 0x4004);
    nop();
    vram_setw_next_wait(0x7777);
    nop();
    xmem_setw(XR_TILE_ADDR, 0xBEEF);
    nop();
    xmem_set_addr(XR_COPPER_ADDR);
    nop();
    xmem_setw_next(0xBABE);
    nop();
    xmem_setw_wait(XR_COLOR_ADDR, 0xDEAD);
    nop();
    xmem_setw_next_wait(0xF00F);
    nop();
    xm_getbh(TIMER);
    nop();
    xm_getbl(TIMER);
    nop();
    xm_getw(RD_XADDR);
    nop();
    xm_getl(DATA);
    nop();
    xreg_getw(SCANLINE);
    nop();
    xreg_get_addr(VID_CTRL);
    nop();
    xreg_getw_next();
    nop();
    xmem_getw(XR_COLOR_ADDR + 3);
    nop();
    xmem_get_addr(XR_TILE_ADDR + 3);
    nop();
    xmem_getw_next();
    nop();
    xmem_getw_next_wait();
    nop();
    xwait_ctrl_bit_set(HBLANK);
    nop();
    xwait_ctrl_bit_clear(VBLANK);
    nop();
    xwait_mem_ready();
    nop();
    xwait_blit_ready();
    nop();
    xwait_blit_done();
    nop();
    xwait_hblank();
    nop();
    xwait_not_hblank();
    nop();
    xwait_vblank();
    nop();
    xwait_not_vblank();
}
