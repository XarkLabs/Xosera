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
 * Xosera rosco_m68k low-level C API for Xosera registers
 * ------------------------------------------------------------
 */

#if !defined(XOSERA_M68K_API_H)
#define XOSERA_M68K_API_H

#include <stdbool.h>
#include <stdint.h>

#if defined(delay)        // clear out mcBusywait
#undef delay
#endif
#define delay(ms)     xv_delay(ms)
#define cpu_delay(ms) mcBusywait(ms << 9);        // ~500 == 1ms @ 10MHz 68K

bool xosera_sync();                        // true if Xosera present and responding
bool xosera_init(int reconfig_num);        // wait a bit for Xosera to respond and optional reconfig (if 0 to 3)
void xv_delay(uint32_t ms);                // delay milliseconds using Xosera TIMER
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

#ifdef ROSCO_M68K_PRO
#define MOVEW "move.w"
#define MOVEL "move.l"
#else
#define MOVEW "movep.w"
#define MOVEL "movep.l"
#endif

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

// Xosera XM register base ptr
#if !defined(XV_PREP_REQUIRED)
extern volatile xmreg_t * const xosera_ptr;
#endif

// Extra-credit function that saves 8 cycles per function that calls xosera API functions (call once at top).
// (NOTE: This works by "shadowing" the global xosera_ptr and using asm to load the constant value more efficiently.  If
// GCC "sees" the constant pointer value, it seems to want to load it over and over as needed.  This method gets GCC to
// load the pointer once (using more efficient immediate addressing mode) and keep it loaded in an address register.)
#define xv_prep()                                                                                                      \
    volatile xmreg_t * const xosera_ptr = ({                                                                           \
        xmreg_t * ptr;                                                                                                 \
        __asm__ __volatile__("lea.l " XM_STR(XM_BASEADDR) ",%[ptr]" : [ptr] "=a"(ptr) : :);                            \
        ptr;                                                                                                           \
    })

// set high byte (even address) of XM register XM_<xmreg> to 8-bit high_byte
#define xm_setbh(xmreg, high_byte) (xosera_ptr[(XM_##xmreg) >> 2].b.h = (high_byte))
// set low byte (odd address) of XM register XM_<xmreg> xr to 8-bit low_byte
#define xm_setbl(xmreg, low_byte) (xosera_ptr[(XM_##xmreg) >> 2].b.l = (low_byte))
// set XM register XM_<xmreg> to 16-bit word word_value
#define xm_setw(xmreg, word_value)                                                                                     \
    do                                                                                                                 \
    {                                                                                                                  \
        (void)(XM_##xmreg);                                                                                            \
        uint16_t uval16 = (word_value);                                                                                \
        __asm__ __volatile__(MOVEW " %[src]," XM_STR(XM_##xmreg) "(%[ptr])"                                            \
                             :                                                                                         \
                             : [src] "d"(uval16), [ptr] "a"(xosera_ptr)                                                \
                             :);                                                                                       \
    } while (false)

// set XM register XM_<xmreg> to 32-bit long long_value (sets two consecutive 16-bit word registers)
#define xm_setl(xmreg, long_value)                                                                                     \
    do                                                                                                                 \
    {                                                                                                                  \
        (void)(XM_##xmreg);                                                                                            \
        uint32_t uval32 = (long_value);                                                                                \
        __asm__ __volatile__(MOVEL " %[src]," XM_STR(XM_##xmreg) "(%[ptr])"                                            \
                             :                                                                                         \
                             : [src] "d"(uval32), [ptr] "a"(xosera_ptr)                                                \
                             :);                                                                                       \
    } while (false)
// set XR register XR_<xreg> to 16-bit word word_value (uses MOVEP.L if reg and value are constant)
#define xreg_setw(xreg, word_value)                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        (void)(XR_##xreg);                                                                                             \
        uint16_t uval16 = (word_value);                                                                                \
        if (__builtin_constant_p((XR_##xreg)) && __builtin_constant_p((word_value)))                                   \
        {                                                                                                              \
            __asm__ __volatile__(MOVEL " %[rxav]," XM_STR(XM_XR_ADDR) "(%[ptr]) ; "                                    \
                                 :                                                                                     \
                                 : [rxav] "d"(((XR_##xreg) << 16) | (uint16_t)((word_value))), [ptr] "a"(xosera_ptr)   \
                                 :);                                                                                   \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            __asm__ __volatile__(                                                                                      \
                MOVEW " %[rxa]," XM_STR(XM_XR_ADDR) "(%[ptr]) ; " MOVEW " %[src]," XM_STR(XM_XR_DATA) "(%[ptr])"       \
                :                                                                                                      \
                : [rxa] "d"((XR_##xreg)), [src] "d"(uval16), [ptr] "a"(xosera_ptr)                                     \
                :);                                                                                                    \
        }                                                                                                              \
    } while (false)

// set XR memory address xrmem to 16-bit word word_value
#define xmem_setw(xrmem, word_value)                                                                                   \
    do                                                                                                                 \
    {                                                                                                                  \
        uint16_t umem16 = (xrmem);                                                                                     \
        uint16_t uval16 = (word_value);                                                                                \
        __asm__ __volatile__(                                                                                          \
            MOVEW " %[xra]," XM_STR(XM_XR_ADDR) "(%[ptr]) ; " MOVEW " %[src]," XM_STR(XM_XR_DATA) "(%[ptr])"           \
            :                                                                                                          \
            : [xra] "d"(umem16), [src] "d"(uval16), [ptr] "a"(xosera_ptr)                                              \
            :);                                                                                                        \
    } while (false)

// NOTE: Uses clang and gcc supported extension (statement expression), so we must slightly lower shields...
#pragma GCC diagnostic ignored "-Wpedantic"        // Yes, I'm slightly cheating (but ugly to have to pass in "return
                                                   // variable" - and this is the "low level" API, remember)

// return high byte (even address) from XM register XM_<xmreg>
#define xm_getbh(xmreg) (xosera_ptr[XM_##xmreg >> 2].b.h)
// return low byte (odd address) from XM register XM_<xmreg>
#define xm_getbl(xmreg) (xosera_ptr[XM_##xmreg >> 2].b.l)
// return 16-bit word from XM register XM_<xmreg>
#define xm_getw(xmreg)                                                                                                 \
    ({                                                                                                                 \
        (void)(XM_##xmreg);                                                                                            \
        uint16_t word_value;                                                                                           \
        __asm__ __volatile__(MOVEW " " XM_STR(XM_##xmreg) "(%[ptr]),%[dst]"                                            \
                             : [dst] "=d"(word_value)                                                                  \
                             : [ptr] "a"(xosera_ptr)                                                                   \
                             :);                                                                                       \
        word_value;                                                                                                    \
    })
// return 32-bit word from two consecutive 16-bit word XM registers XM_<xmreg> & XM_<xmreg>+1
#define xm_getl(xmreg)                                                                                                 \
    ({                                                                                                                 \
        (void)(XM_##xmreg);                                                                                            \
        uint32_t long_value;                                                                                           \
        __asm__ __volatile__(MOVEL " " XM_STR(XM_##xmreg) "(%[ptr]),%[dst]"                                            \
                             : [dst] "=d"(long_value)                                                                  \
                             : [ptr] "a"(xosera_ptr)                                                                   \
                             :);                                                                                       \
        long_value;                                                                                                    \
    })

// return high byte (even address) from XR memory address xrmem
#define xmem_getbh(xrmem) (xm_setw(XR_ADDR, xrmem), xosera_ptr[XM_XR_DATA >> 2].b.h)
// return low byte (odd address) from XR memory address xrmem
#define xmem_getbl(xrmem) (xm_setw(XR_ADDR, xrmem), xosera_ptr[XM_XR_DATA >> 2].b.l)
// return 16-bit word from XR memory address xrmem
#define xmem_getw(xrmem)                                                                                               \
    ({                                                                                                                 \
        uint16_t word_value;                                                                                           \
        xm_setw(XR_ADDR, xrmem);                                                                                       \
        __asm__ __volatile__(MOVEW " " XM_STR(XM_XR_DATA) "(%[ptr]),%[dst]"                                            \
                             : [dst] "=d"(word_value)                                                                  \
                             : [ptr] "a"(xosera_ptr)                                                                   \
                             :);                                                                                       \
        word_value;                                                                                                    \
    })
// return high byte (even address) from XR register XR_<xreg>
#define xreg_getbh(xreg)                                                                                               \
    ({                                                                                                                 \
        (void)(XR_##xreg);                                                                                             \
        uint8_t byte_value;                                                                                            \
        xm_setw(XR_ADDR, (XR_##xreg));                                                                                 \
        byte_value = xosera_ptr[XM_XR_DATA >> 2].b.h;                                                                  \
        byte_value;                                                                                                    \
    })

// return low byte (odd address) from XR register XR_<xreg>
#define xreg_getbl(xreg)                                                                                               \
    ({                                                                                                                 \
        (void)(XR_##xreg);                                                                                             \
        uint8_t byte_value;                                                                                            \
        xm_setw(XR_ADDR, (XR_##xreg));                                                                                 \
        byte_value = xosera_ptr[XM_XR_DATA >> 2].b.l;                                                                  \
        byte_value;                                                                                                    \
    })
// return 16-bit word from XR register XR_<xreg>
#define xreg_getw(xreg)                                                                                                \
    ({                                                                                                                 \
        (void)(XR_##xreg);                                                                                             \
        uint16_t word_value;                                                                                           \
        xm_setw(XR_ADDR, (XR_##xreg));                                                                                 \
        __asm__ __volatile__(MOVEW " " XM_STR(XM_XR_DATA) "(%[ptr]),%[dst]"                                            \
                             : [dst] "=d"(word_value)                                                                  \
                             : [ptr] "a"(xosera_ptr)                                                                   \
                             :);                                                                                       \
        word_value;                                                                                                    \
    })

// wait for bit in SYS_CTRL // TODO: check proper byte
#define xm_wait_SYS_CTRL(bit_num)                                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
    } while (xm_getbl(SYS_CTRL) & (1 << (bit_num)))

// wait for slow memory or xreg to complete read/write (under bus contention)
#define xwait_mem_busy() xm_wait_SYS_CTRL(SYS_CTRL_MEMWAIT_B)
// wait for blit unit to become idle
#define xwait_blit_busy()                                                                                              \
    do                                                                                                                 \
    {                                                                                                                  \
        uint8_t bitval = 1 << SYS_CTRL_BLITBUSY_B;                                                                     \
        __asm__ __volatile__("1: and.b " XM_STR(XM_SYS_CTRL) "+2(%[ptr]),%[bitval] ; bne.s  1b\n"                      \
                                                                                                                       \
                             :                                                                                         \
                             : [ptr] "a"(xosera_ptr), [bitval] "d"(bitval)                                             \
                             :);                                                                                       \
    } while (false)
// wait for blit unit to have room in xreg queue
#define xwait_blit_full()                                                                                              \
    do                                                                                                                 \
    {                                                                                                                  \
        uint8_t bitval = 1 << SYS_CTRL_BLITFULL_B;                                                                     \
        __asm__ __volatile__("1: and.b " XM_STR(XM_SYS_CTRL) "+2(%[ptr]),%[bitval] ; bne.s  1b\n"                      \
                                                                                                                       \
                             :                                                                                         \
                             : [ptr] "a"(xosera_ptr), [bitval] "d"(bitval)                                             \
                             :);                                                                                       \
    } while (false)

// return high byte (even address) from XR memory address xrmem
#define xmem_getbh_wait(xrmem)                                                                                         \
    ({                                                                                                                 \
        (void)(XR_##xreg);                                                                                             \
        uint8_t byte_value;                                                                                            \
        xm_setw(XR_ADDR, (xrmem));                                                                                     \
        xwait_mem_busy();                                                                                              \
        byte_value = xosera_ptr[XM_XR_DATA >> 2].b.h;                                                                  \
        byte_value;                                                                                                    \
    })
// return low byte (odd address) from XR memory address xrmem
#define xmem_getbl_wait(xrmem)                                                                                         \
    ({                                                                                                                 \
        (void)(XR_##xreg);                                                                                             \
        uint8_t byte_value;                                                                                            \
        xm_setw(XR_ADDR, (XR_##xreg));                                                                                 \
        xwait_mem_busy();                                                                                              \
        byte_value = xosera_ptr[XM_XR_DATA >> 2].b.l;                                                                  \
        byte_value;                                                                                                    \
    })
// return 16-bit word from XR memory address xrmem
#define xmem_getw_wait(xrmem)                                                                                          \
    ({                                                                                                                 \
        uint16_t word_value;                                                                                           \
        xm_setw(XR_ADDR, xrmem);                                                                                       \
        xwait_mem_busy();                                                                                              \
        __asm__ __volatile__(MOVEW " " XM_STR(XM_XR_DATA) "(%[ptr]),%[dst]"                                            \
                             : [dst] "=d"(word_value)                                                                  \
                             : [ptr] "a"(xosera_ptr)                                                                   \
                             :);                                                                                       \
        word_value;                                                                                                    \
    })
// return high byte (even address) from XR register XR_<xreg>
#define xreg_getbh_wait(xreg)                                                                                          \
    ({                                                                                                                 \
        (void)(XR_##xreg);                                                                                             \
        uint8_t byte_value;                                                                                            \
        xm_setw(XR_ADDR, (XR_##xreg));                                                                                 \
        xwait_mem_busy();                                                                                              \
        byte_value = xosera_ptr[XM_XR_DATA >> 2].b.h;                                                                  \
        byte_value;                                                                                                    \
    })
// return low byte (odd address) from XR register XR_<xreg>
#define xreg_getbl_wait(xreg)                                                                                          \
    ({                                                                                                                 \
        (void)(XR_##xreg);                                                                                             \
        uint8_t byte_value;                                                                                            \
        xm_setw(XR_ADDR, (XR_##xreg));                                                                                 \
        xwait_mem_busy();                                                                                              \
        byte_value = xosera_ptr[XM_XR_DATA >> 2].b.l;                                                                  \
        byte_value;                                                                                                    \
    })
// return 16-bit word from XR register XR_<xreg>
#define xreg_getw_wait(xreg)                                                                                           \
    ({                                                                                                                 \
        (void)(XR_##xreg);                                                                                             \
        uint16_t word_value;                                                                                           \
        xm_setw(XR_ADDR, (XR_##xreg));                                                                                 \
        xwait_mem_busy();                                                                                              \
        __asm__ __volatile__(MOVEW " " XM_STR(XM_XR_DATA) "(%[ptr]),%[dst]"                                            \
                             : [dst] "=d"(word_value)                                                                  \
                             : [ptr] "a"(xosera_ptr)                                                                   \
                             :);                                                                                       \
        word_value;                                                                                                    \
    })

#endif        // XOSERA_M68K_API_H
