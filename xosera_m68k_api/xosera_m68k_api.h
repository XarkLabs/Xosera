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
 * Xosera low-level C API to read/write Xosera registers
 * ------------------------------------------------------------
 */

#include <stdbool.h>
#include <stdint.h>

#if defined(delay)        // clear out mcBusywait
#undef delay
#endif
#define delay(x)      mcDelaymsec10(x / 10);
#define cpu_delay(ms) mcBusywait(ms << 4);

bool xosera_sync();                        // true if Xosera present and responding
bool xosera_init(int reconfig_num);        // wait a bit for Xosera to respond with optional reconfig (if 0 to 3)
void xv_vram_fill(uint32_t vram_addr, uint32_t numwords, uint32_t word_value);           // fill VRAM with word
void xv_copy_to_vram(uint16_t * source, uint32_t vram_dest, uint32_t numbytes);          // copy to VRAM
void xv_copy_from_vram(uint32_t vram_source, uint16_t * dest, uint32_t numbytes);        // copy from VRAM

// Low-level C API reference:
//
// set/get XM registers (main registers):
// void     xm_setw(xmreg, wval)    (omit XM_ from xmreg name)
// void     xm_setl(xmreg, lval)    (omit XM_ from xmreg name)
// void     xm_setbh(xmreg, bhval)  (omit XM_ from xmreg name)
// void     xm_setbl(xmreg, blval)  (omit XM_ from xmreg name)
// uint16_t xm_getw(xmreg)          (omit XM_ from xmreg name)
// uint32_t xm_getl(xmreg)          (omit XM_ from xmreg name)
// uint8_t  xm_getbh(xmreg)         (omit XM_ from xmreg name)
// uint8_t  xm_getbl(xmreg)         (omit XM_ from xmreg name)
//
// set/get XR registers (extended registers):
// void     xreg_setw(xreg, wval)  (omit XR_ from xreg name)
// uint16_t xreg_getw(xreg)        (omit XR_ from xreg name)
// uint8_t  xreg_getbh(xreg)       (omit XR_ from xreg name)
// uint8_t  xreg_getbl(xreg)       (omit XR_ from xreg name)
//
// set/get XR memory region address:
// void     xmem_setw(xrmem, wval)
// uint16_t xmem_getw(xrmem)
// uint8_t  xmem_getbh(xrmem)
// uint8_t  xmem_getbl(xrmem)

#include "xosera_m68k_defs.h"

// C preprocessor "stringify" to embed #define into inline asm string
#define _XM_STR(s) #s
#define XM_STR(s)  _XM_STR(s)

// NOTE: Since Xosera is using a 6800-style 8-bit bus, it uses only data lines 8-15 of each 16-bit word (i.e., only the
//       upper byte of each word) this makes the size of its register map in memory appear doubled and is the reason for
//       the pad bytes in the struct below.  Byte access is fine but for word or long access to this struct, the MOVEP
//       680x0 instruction should be used (it was designed for this purpose).  The xv-set* and xv_get* macros below make
//       it easy.
typedef struct _xreg
{
    union
    {
        struct
        {
            volatile uint8_t h;
            volatile uint8_t _h_pad;
            volatile uint8_t l;
            volatile uint8_t _l_pad;
        } b;
        const volatile uint16_t w;        // NOTE: For use as offset only with xv_setw (and MOVEP.W opcode)
        const volatile uint32_t l;        // NOTE: For use as offset only with xv_setl (and MOVEP.L opcode)
    };
} xmreg_t;

// NOTE: This seems "evil" to define in a header file, but since this is a const address gcc is smart
// enough to just use a 32-bit immediate vs loading this from a pointer variable (exactly what is desired)
static volatile xmreg_t * const xosera_ptr = (volatile xmreg_t * const)XM_BASEADDR;        // Xosera base address

// Xosera SM register base ptr
// extern volatile xmreg_t * const xosera_ptr;

// set high byte of XM_<xmreg> to 8-bit byte bh
#define xm_setbh(xmreg, bhval) (xosera_ptr[XM_##xmreg >> 2].b.h = (uint8_t)(bhval))
// set low byte of XM_<xmreg> xr to 8-bit byte bl
#define xm_setbl(xmreg, blval) (xosera_ptr[XM_##xmreg >> 2].b.l = (uint8_t)(blval))
// set XM_<xmreg> to 16-bit word wv
#define xm_setw(xmreg, wval)                                                                                           \
    __asm__ __volatile__("movep.w %[src]," XM_STR(XM_##xmreg) "(%[ptr])"                                               \
                         :                                                                                             \
                         : [src] "d"((uint16_t)(wval)), [ptr] "a"(xosera_ptr)                                          \
                         :)
// set XM_<xmreg> to 32-bit long lv (sets two consecutive 16-bit word registers)
#define xm_setl(xmreg, lval)                                                                                           \
    __asm__ __volatile__("movep.l %[src]," XM_STR(XM_##xmreg) "(%[ptr])"                                               \
                         :                                                                                             \
                         : [src] "d"((uint32_t)(lval)), [ptr] "a"(xosera_ptr)                                          \
                         :)
// set XR reg XR_<xreg> to 16-bit word wv (uses MOVEP.L if reg and value are constant)
#define xreg_setw(xreg, wval)                                                                                          \
    do                                                                                                                 \
    {                                                                                                                  \
        if (__builtin_constant_p((XR_##xreg)) && __builtin_constant_p((wval)))                                         \
        {                                                                                                              \
            __asm__ __volatile__("movep.l %[rxav]," XM_STR(XM_XR_ADDR) "(%[ptr]) ; "                                   \
                                 :                                                                                     \
                                 : [rxav] "d"(((XR_##xreg) << 16) | (uint16_t)(wval)), [ptr] "a"(xosera_ptr)           \
                                 :);                                                                                   \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            __asm__ __volatile__(                                                                                      \
                "movep.w %[rxa]," XM_STR(XM_XR_ADDR) "(%[ptr]) ; movep.w %[src]," XM_STR(XM_XR_DATA) "(%[ptr])"        \
                :                                                                                                      \
                : [rxa] "d"((XR_##xreg)), [src] "d"((uint16_t)(wval)), [ptr] "a"(xosera_ptr)                           \
                :);                                                                                                    \
        }                                                                                                              \
    } while (false)

// set XR memory address to 16-bit word wv
#define xmem_setw(xrmem, wval)                                                                                         \
    __asm__ __volatile__(                                                                                              \
        "movep.w %[xra]," XM_STR(XM_XR_ADDR) "(%[ptr]) ; movep.w %[src]," XM_STR(XM_XR_DATA) "(%[ptr])"                \
        :                                                                                                              \
        : [xra] "d"((uint16_t)(xrmem)), [src] "d"((uint16_t)(wval)), [ptr] "a"(xosera_ptr)                             \
        :)

// NOTE: Uses clang and gcc supported extension (statement expression), so we must slightly lower shields...
#pragma GCC diagnostic ignored "-Wpedantic"        // Yes, I'm slightly cheating (but ugly to have to pass in "return
                                                   // variable" - and this is the "low level" API, remember)

// return high byte from XM_<xmreg>
#define xm_getbh(xmreg) (xosera_ptr[XM_##xmreg >> 2].b.h)
// return low byte from XM_<xmreg>
#define xm_getbl(xmreg) (xosera_ptr[XM_##xmreg >> 2].b.l)
// return 16-bit word from XM_<xmreg>
#define xm_getw(xmreg)                                                                                                 \
    ({                                                                                                                 \
        uint16_t wval;                                                                                                 \
        __asm__ __volatile__("movep.w " XM_STR(XM_##xmreg) "(%[ptr]),%[dst]"                                           \
                             : [dst] "=d"(wval)                                                                        \
                             : [ptr] "a"(xosera_ptr)                                                                   \
                             :);                                                                                       \
        wval;                                                                                                          \
    })
// return 32-bit word from two consecutive 16-bit word registers (XM_<xmreg>, XM_<xmreg>+1)
#define xm_getl(xmreg)                                                                                                 \
    ({                                                                                                                 \
        uint32_t lval;                                                                                                 \
        __asm__ __volatile__("movep.l " XM_STR(XM_##xmreg) "(%[ptr]),%[dst]"                                           \
                             : [dst] "=d"(lval)                                                                        \
                             : [ptr] "a"(xosera_ptr)                                                                   \
                             :);                                                                                       \
        lval;                                                                                                          \
    })
// return high byte from AUX address xa
#define xmem_getbh(xrmem) (xm_setw(XR_ADDR, xrmem), xosera_ptr[XM_XR_DATA >> 2].b.h)
// return low byte from AUX address xa
#define xmem_getbl(xrmem) (xm_setw(XR_ADDR, xrmem), xosera_ptr[XM_XR_DATA >> 2].b.l)
// return 16-bit word from AUX address xa
#define xmem_getw(xrmem)                                                                                               \
    ({                                                                                                                 \
        uint16_t w;                                                                                                    \
        xm_setw(XR_ADDR, xrmem);                                                                                       \
        __asm__ __volatile__("movep.w " XM_STR(XM_XR_DATA) "(%[ptr]),%[dst]"                                           \
                             : [dst] "=d"(w)                                                                           \
                             : [ptr] "a"(xosera_ptr)                                                                   \
                             :);                                                                                       \
        w;                                                                                                             \
    })
// return high byte from xosera_xr_reg xr
#define xreg_getbh(xreg) xmem_getbh(XR_##xreg)
// return low byte from xosera_xr_reg xr
#define xreg_getbl(xreg) xmem_getbl(XR_##xreg)
// return 16-bit word from xosera_xr_reg xr
#define xreg_getw(xreg) xmem_getw(XR_##xreg)
