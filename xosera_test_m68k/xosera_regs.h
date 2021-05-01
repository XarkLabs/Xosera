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

#include <stdint.h>

// NOTE: Since Xosera is using a 6800-style 8-bit bus, it uses only data lines 8-15 of each 16-bit word (i.e., only the
//       upper byte of each word) this makes the size of its register map in memory appear doubled and is the reason for
//       the pad bytes in the struct below.  Byte access is fine but for word or long access to this struct, the MOVEP
//       680x0 instruction should be used (it was designed for this purpose).  The xv-set* and xv_get* macros below make
//       it easy.
typedef struct
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
} xreg_t;

// Xosera primary register map (See https://github.com/XarkLabs/Xosera/blob/develop/REFERENCE.md)
typedef struct
{
    xreg_t aux_addr;         // address for AUX read/write via AUX_DATA (see below)
    xreg_t const_val;        // set constant value (e.g. for VRAM fill) TODO
    xreg_t rd_addr;          // set read address for reading from VRAM via DATA/DATA_2
    xreg_t wr_addr;          // set write address for writing to VRAM
    xreg_t data;             // read/write VRAM word at RD/WR_ADDR then add RD/WR_INC
    xreg_t data_2;           // 2nd DATA (to allow for 32-bit read/write to VRAM)
    xreg_t aux_data;         // read/write AUX word at AUX_ADDR
    xreg_t count;            // write blitter count and start operation or read blitter status TODO
    xreg_t rd_inc;           // RD_ADDR increment per word read
    xreg_t wr_inc;           // WR_ADDR increment per word write
    xreg_t wr_mod;           // WR_ADDR increment every `WIDTH` words read (in 2D mode) TODO
    xreg_t rd_mod;           // RD_ADDR increment every `WIDTH` words read (in 2D mode) TODO
    xreg_t width;            // width in words for 2D rectangular blit
    xreg_t blit_ctrl;        // set blitter and other options (reconfigure mode)
    xreg_t unused_E;         // T.B.D. TODO
    xreg_t unused_F;         // T.B.D. TODO
} xosera_regs_t;

// Xosera register base ptr
extern xosera_regs_t * xosera_ptr;
#define xv_setbh(xr, v)                                                                                                \
    do                                                                                                                 \
    {                                                                                                                  \
        xosera_ptr->xr.b.h = (v);                                                                                      \
    } while (0)
#define xv_setbl(xr, v)                                                                                                \
    do                                                                                                                 \
    {                                                                                                                  \
        xosera_ptr->xr.b.l = (v);                                                                                      \
    } while (0)
#define xv_setw(xr, v)                                                                                                 \
    do                                                                                                                 \
    {                                                                                                                  \
        __asm__ __volatile__("movep.w %[src],%[dst]" : : [src] "d"((v)), [dst] "m"(xosera_ptr->xr) :);                 \
    } while (0)
#define xv_setl(xr, v)                                                                                                 \
    do                                                                                                                 \
    {                                                                                                                  \
        __asm__ __volatile__("movep.l %[src],%[dst]" : : [src] "d"((v)), [dst] "m"(xosera_ptr->xr) :);                 \
    } while (0)

#define xv_aux_setbh(xa, v)                                                                                            \
    do                                                                                                                 \
    {                                                                                                                  \
        xv_setw(aux_addr, (xa));                                                                                       \
        xosera_ptr->aux_data.b.h = (v);                                                                                \
    } while (0)
#define xv_aux_setbl(xa, v)                                                                                            \
    do                                                                                                                 \
    {                                                                                                                  \
        xv_setw(aux_addr, (xa));                                                                                       \
        xosera_ptr->aux_data.b.l = (v);                                                                                \
    } while (0)
#define xv_aux_setw(xa, v)                                                                                             \
    do                                                                                                                 \
    {                                                                                                                  \
        xv_setw(aux_addr, (xa));                                                                                       \
        __asm__ __volatile__("movep.w %[src],%[dst]" : : [src] "d"((v)), [dst] "m"(xosera_ptr->aux_data) :);           \
    } while (0)
#define xv_aux_setl(xa, v)                                                                                             \
    do                                                                                                                 \
    {                                                                                                                  \
        xv_setw(aux_addr, (xa));                                                                                       \
        __asm__ __volatile__("movep.l %[src],%[dst]" : : [src] "d"((v)), [dst] "m"(xosera_ptr->aux_data) :);           \
    } while (0)

// NOTE: Uses clang and gcc supported extension (statement expression), so we must slightly lower shields...
#pragma GCC diagnostic ignored "-Wpedantic"        // Yes, I'm slightly cheating (but ugly to have to pass in "return
                                                   // variable" - and this is the "low level" API, remember)

#define xv_getbh(xr) (xosera_ptr->xr.b.h)
#define xv_getbl(xr) (xosera_ptr->xr.b.l)
#define xv_getw(xr)                                                                                                    \
    ({                                                                                                                 \
        uint16_t w;                                                                                                    \
        __asm__ __volatile__("movep.w %[src],%[dst]" : [dst] "=d"(w) : [src] "m"(xosera_ptr->xr) :);                   \
        w;                                                                                                             \
    })
#define xv_getl(xr)                                                                                                    \
    ({                                                                                                                 \
        uint32_t l;                                                                                                    \
        __asm__ __volatile__("movep.l %[src],%[dst]" : [dst] "=d"(l) : [src] "m"(xosera_ptr->xr) :);                   \
        l;                                                                                                             \
    })

#define xv_aux_getbh(xa) (xv_setw(aux_addr, (xa)), xosera_ptr->aux_data.b.h)
#define xv_aux_getbl(xa) (xv_setw(aux_addr, (xa)), xosera_ptr->aux_data.b.l)
#define xv_aux_getw(xa)                                                                                                \
    ({                                                                                                                 \
        uint16_t w;                                                                                                    \
        xv_setw(aux_addr, (xa));                                                                                       \
        __asm__ __volatile__("movep.w %[src],%[dst]" : [dst] "=d"(w) : [src] "m"(xosera_ptr->aux_data) :);             \
        w;                                                                                                             \
    })
