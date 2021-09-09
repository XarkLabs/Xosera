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
 * Xosera low-level C API for copper lists.
 *
 * These macros allow copper lists to be created in a slightly more
 * readable way. To use them, they should be used within array
 * initializers for uint16_t arrays, e.g.
 *
 * uint16_t copper_list[] = {
 *     XC_SKIP(0,160,2),
 *     XC_JUMP(0x14),
 *     // etc..
 * }
 *
 * Each macro generates a complete instruction (two 16 bit words)
 * so the length of the array will be the number of macros * 2.
 * ------------------------------------------------------------
 */

#ifndef __XOSERA_API_COPPER_H
#define __XOSERA_API_COPPER_H

#define XC_WAIT(x,y,flags)          (0x0000 | (((y)) & 0x7ff)), ((((x)) & 0x7ff) << 4 | (((flags)) & 0xf))
#define XC_SKIP(x,y,flags)          (0x2000 | (((y)) & 0x7ff)), ((((x)) & 0x7ff) << 4 | (((flags)) & 0xf))
#define XC_JUMP(addr)               (0x4000 | (((addr)) & 0x7ff)), 0x0000
#define XC_MOVR(data,reg,flags)     (0x9000 | ((((flags)) & 0xF) << 8) | (((reg)) & 0xF)), (((data)) & 0xFFFF)
#define XC_MOVF(data,addr)          (0xA000 | (((addr)) & 0xFFF)), (((data)) & 0xFFFF)
#define XC_MOVP(data,pal)           (0xB000 | (((pal)) & 0xff)), (((data)) & 0xFFFF)
#define XC_MOVC(data,addr)          (0xA000 | (((addr)) & 0x7FF)), (((data)) & 0xFFFF)
#define XC_NEXT                     (0x0000), (0x0003)

#endif//__XOSERA_API_COPPER_H

