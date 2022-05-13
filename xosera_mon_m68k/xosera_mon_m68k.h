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
 * Test and tech-demo for Xosera FPGA "graphics card"
 * ------------------------------------------------------------
 */

#include <stdint.h>
#include <stdlib.h>

#if !defined(XOSERA_MON_M68K_H)
#define XOSERA_MON_M68K_H

typedef struct _addr_range
{
    const char * name;
    uint16_t     addr;
    uint16_t     size;
} addr_range_t;


extern const addr_range_t sys_ctrl_status[];
extern const addr_range_t xm_regs[];
extern const addr_range_t xr_mem[];

#endif