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

#include "xosera_m68k_api.h"

#define nop() __asm__ __volatile__("nop ; nop")

xosera_info_t info;

volatile uint8_t  g8;
volatile uint16_t g16;
volatile uint32_t g32;

// This is just meant to make sure all API macros compile and pass sanity check
void kmain(void)
{
    xosera_sync();
    xosera_init(XINIT_CONFIG_640x480);
    xosera_get_info(&info);
    nop();
    xv_prep();
    nop();
    xm_setbh(SYS_CTRL, 0x12);
    nop();
    xm_setbl(FEATURE, 0x34);
    nop();
    xm_setw(FEATURE, 0x1234);
    nop();
    xm_setl(WR_INCR, 0x0001ABCD);
    nop();
    g8 = xm_getbh(TIMER);
    nop();
    g8 = xm_getbl(TIMER);
    nop();
    g16 = xm_getw(RD_XADDR);
    nop();
    g32 = xm_getl(DATA);
    nop();
    xreg_setw(UNUSED_08, 0x1337);
    nop();
    xreg_setw_next_addr(UNUSED_08);
    nop();
    xreg_setw_next(0xC0DE);
    nop();
    g16 = xreg_getw(SCANLINE);
    nop();
    xreg_getw_next_addr(VID_CTRL);
    nop();
    g16 = xreg_getw_next();
    nop();
    xmem_setw(XR_TILE_ADDR, 0xBEEF);
    nop();
    xmem_setw_wait(XR_TILE_ADDR, 0xBEEF);
    nop();
    xmem_setw_next_addr(XR_COPPER_ADDR);
    nop();
    xmem_setw_next(0xBABE);
    nop();
    xmem_setw_next_wait(0xDEAD);
    nop();
    xmem_setw_next_wait(0xF00F);
    nop();
    g16 = xmem_getw(XR_COLOR_ADDR + 3);
    nop();
    g16 = xmem_getw_wait(XR_COLOR_ADDR + 7);
    nop();
    xmem_getw_next_addr(XR_TILE_ADDR + 3);
    nop();
    xmem_getw_next();
    nop();
    xmem_getw_next_wait();
    nop();
    vram_setw(0xD00B, 0x1ee7);
    nop();
    vram_setw_wait(0xB00B, 0x4004);
    nop();
    vram_setw_addr_incr(0x0001, 0xABCD);
    nop();
    vram_setw_next_addr(0xABCD);
    nop();
    vram_setw_next(0x3456);
    nop();
    vram_setw_next_wait(0x7777);
    nop();
    vram_setl(0xD00B, 0x12345678);
    nop();
    vram_setl_next(0xDEADBEEF);
    nop();
    vram_setl_next_wait(0x7777AAAA);
    nop();
    g16 = vram_getw(0x2345);
    nop();
    g16 = vram_getw_wait(XR_COLOR_ADDR + 7);
    nop();
    vram_getw_next_addr(XR_TILE_ADDR + 3);
    nop();
    vram_getw_next();
    nop();
    vram_getw_next_wait();
    nop();
    g32 = vram_getl(0x4332);
    nop();
    g32 = vram_getl_next();
    nop();
    g8 = xm_getb_sys_ctrl(MEM_WAIT);
    nop();
    xwait_sys_ctrl_set(MEM_WAIT);
    nop();
    xwait_sys_ctrl_clear(MEM_WAIT);
    nop();
    g8 = xm_getb_sys_ctrl(BLIT_FULL);
    nop();
    xwait_sys_ctrl_set(BLIT_FULL);
    nop();
    xwait_sys_ctrl_clear(BLIT_FULL);
    nop();
    g8 = xm_getb_sys_ctrl(BLIT_BUSY);
    nop();
    xwait_sys_ctrl_set(BLIT_BUSY);
    nop();
    xwait_sys_ctrl_clear(BLIT_BUSY);
    nop();
    g8 = xm_getb_sys_ctrl(HBLANK);
    nop();
    xwait_sys_ctrl_set(HBLANK);
    nop();
    xwait_sys_ctrl_clear(HBLANK);
    nop();
    g8 = xm_getb_sys_ctrl(VBLANK);
    nop();
    xwait_sys_ctrl_set(VBLANK);
    nop();
    xwait_sys_ctrl_clear(VBLANK);
    nop();
    g8 = xis_mem_ready();
    nop();
    xwait_mem_ready();
    nop();
    g8 = xis_blit_ready();
    nop();
    xwait_blit_ready();
    nop();
    g8 = xis_blit_done();
    nop();
    xwait_blit_done();
    nop();
    g8 = xis_hblank();
    nop();
    xwait_hblank();
    nop();
    xwait_not_hblank();
    nop();
    g8 = xis_vblank();
    nop();
    xwait_vblank();
    nop();
    xwait_not_vblank();
    nop();
    if (xuart_is_send_ready())
    {
        xuart_send_byte(0x55);
    }
    nop();
    if (xuart_is_get_ready())
    {
        g8 = xuart_get_byte();
    }
    nop();
    g16 = xosera_vid_width();
    nop();
    g16 = xosera_vid_height();
    nop();
    g16 = xosera_max_hpos();
    nop();
    g16 = xosera_max_vpos();
    nop();
    g16 = xosera_aud_channels();
    nop();
}
