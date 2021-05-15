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

#include "xosera_regs.h"
#include <stdbool.h>
#include <stdint.h>

#if defined(delay)        // clear out mcBusyWait
#undef delay
#endif
#define delay(x) mcDelaymsec10(x / 10);

bool xosera_init(int reconfig_num);        // true if Xosera present with optional reconfig (if 0 to 3)
bool xosera_sync();                        // true if Xosera present
void xv_vram_fill(int vram_addr, int size, int word_value);                // fill VRAM with word
void xv_copy_to_vram(uint16_t * source, int vram_dest, int size);          // copy to VRAM
void xv_copy_from_vram(int vram_source, uint16_t * dest, int size);        // copy from VRAM
