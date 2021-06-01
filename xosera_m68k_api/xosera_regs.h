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

#include <stddef.h>
#include <stdint.h>

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
} xreg_t;

// C preprocessor "stringify" to embed #define into inline asm string
#define XV_STR(s)   #s
#define XVR_XSTR(s) XV_STR(s)

// Xosera primary register map (See https://github.com/XarkLabs/Xosera/blob/develop/REFERENCE.md)
#define XVR_aux_addr  0x00        // address for AUX read/write via AUX_DATA (see below)
#define XVR_const_val 0x04        // set constant value (e.g. for VRAM fill) TODO
#define XVR_rd_addr   0x08        // set read address for reading from VRAM via DATA/DATA_2
#define XVR_wr_addr   0x0C        // set write address for writing to VRAM
#define XVR_data      0x10        // read/write VRAM word at RD/WR_ADDR then add RD/WR_INC
#define XVR_data_2    0x14        // 2nd DATA (to allow for 32-bit read/write to VRAM)
#define XVR_aux_data  0x18        // read/write AUX word at AUX_ADDR
#define XVR_count     0x1C        // write blitter count and start operation or read blitter status TODO
#define XVR_rd_inc    0x20        // RD_ADDR increment per word read
#define XVR_wr_inc    0x24        // WR_ADDR increment per word write
#define XVR_wr_mod    0x28        // WR_ADDR increment every `WIDTH` words read (in 2D mode) TODO
#define XVR_rd_mod    0x2C        // RD_ADDR increment every `WIDTH` words read (in 2D mode) TODO
#define XVR_width     0x30        // width in words for 2D rectangular blit
#define XVR_blit_ctrl 0x34        // set blitter and other options (reconfigure mode)
#define XVR_unused_e  0x38        // T.B.D. TODO
#define XVR_unused_f  0x3C        // T.B.D. TODO

// AUX memory areas
#define XV_AUX_VIDREG   0x0000        // 0x0000-0x000F 16 word video registers (see below)
#define XV_AUX_FONTMEM  0x4000        // 0x4000-0x5FFF 4K words of font memory
#define XV_AUX_COLORMEM 0x8000        // 0x8000-0x80FF 256 word color lookup table (0xXRGB)
#define XV_AUX_AUDMEM   0xC000        // 0xC000-0x??? TODO (audio registers)

// Xosera AUX video registers
#define XVA_dispstart 0x0
#define XVA_dispwidth 0x1
#define XVA_scrollxy  0x2
#define XVA_fontctrl  0x3
#define XVA_gfxctrl   0x4
#define XVA_unused_5  0x5
#define XVA_unused_6  0x6
#define XVA_unused_7  0x7
#define XVA_vidwidth  0x8
#define XVA_vidheight 0x9
#define XVA_features  0xA
#define XVA_scanline  0xB
#define XVA_githash_h 0xC
#define XVA_githash_l 0xD
#define XVA_unused_e  0xE
#define XVA_unused_f  0xF

// Xosera register base ptr
extern volatile xreg_t * const xosera_ptr;

// set high byte of xosera_reg xr to 8-bit byte bh
#define xv_setbh(xr, bh) (xosera_ptr[XVR_##xr >> 2].b.h = (bh))
// set low byte of xosera_reg xr to 8-bit byte bl
#define xv_setbl(xr, bl) (xosera_ptr[XVR_##xr >> 2].b.l = (bl))
// set xosera_reg xr to 16-bit word wv
#define xv_setw(xr, wv)                                                                                                \
    __asm__ __volatile__("movep.w %[src]," XVR_XSTR(XVR_##xr) "(%[ptr])"                                               \
                         :                                                                                             \
                         : [src] "d"((uint16_t)(wv)), [ptr] "a"(xosera_ptr)                                            \
                         :)
// set xosera_reg xr to 32-bit long lv
#define xv_setl(xr, lv)                                                                                                \
    __asm__ __volatile__("movep.l %[src]," XVR_XSTR(XVR_##xr) "(%[ptr])"                                               \
                         :                                                                                             \
                         : [src] "d"((uint32_t)(lv)), [ptr] "a"(xosera_ptr)                                            \
                         :)
// set AUX address to 16-bit word wv
#define xv_aux_setw(xa, wv)                                                                                            \
    __asm__ __volatile__("movep.w %[rxa]," XVR_XSTR(XVR_aux_addr) "(%[ptr]) ; "  \
    "movep.w %[src]," XVR_XSTR(XVR_aux_data) "(%[ptr])" : : [rxa] "d"((xa)), [src] "d"((uint16_t)(wv)), [ptr] "a"(xosera_ptr) :)
// set xosera_aux_reg xar to 16-bit word wv
#define xv_reg_setw(xar, wv)                                                                                           \
    __asm__ __volatile__("movep.w %[rxa]," XVR_XSTR(XVR_aux_addr) "(%[ptr]) ; "  \
    "movep.w %[src]," XVR_XSTR(XVR_aux_data) "(%[ptr])" : : [rxa] "d"((XVA_##xar)), [src] "d"((uint16_t)(wv)), [ptr] "a" (xosera_ptr) :)

// NOTE: Uses clang and gcc supported extension (statement expression), so we must slightly lower shields...
#pragma GCC diagnostic ignored "-Wpedantic"        // Yes, I'm slightly cheating (but ugly to have to pass in "return
                                                   // variable" - and this is the "low level" API, remember)

// return high byte from xosera_reg xr
#define xv_getbh(xr) (xosera_ptr[XVR_##xr >> 2].b.h)
// return low byte from xosera_reg xr
#define xv_getbl(xr) (xosera_ptr[XVR_##xr >> 2].b.l)
// return 16-bit word from xosera_reg xr
#define xv_getw(xr)                                                                                                    \
    ({                                                                                                                 \
        uint16_t w;                                                                                                    \
        __asm__ __volatile__("movep.w " XVR_XSTR(XVR_##xr) "(%[ptr]),%[dst]"                                           \
                             : [dst] "=d"(w)                                                                           \
                             : [ptr] "a"(xosera_ptr)                                                                   \
                             :);                                                                                       \
        w;                                                                                                             \
    })
// return 32-bit word from xosera_reg xr and xr+1
#define xv_getl(xr)                                                                                                    \
    ({                                                                                                                 \
        uint32_t l;                                                                                                    \
        __asm__ __volatile__("movep.l " XVR_XSTR(XVR_##xr) "(%[ptr]),%[dst]"                                           \
                             : [dst] "=d"(l)                                                                           \
                             : [ptr] "a"(xosera_ptr)                                                                   \
                             :);                                                                                       \
        l;                                                                                                             \
    })
// return high byte from AUX address xa
#define xv_aux_getbh(xa) (xv_setw(aux_addr, xa), xosera_ptr[XVR_aux_data >> 2].b.h)
// return low byte from AUX address xa
#define xv_aux_getbl(xa) (xv_setw(aux_addr, xa), xosera_ptr[XVR_aux_data >> 2].b.l)
// return 16-bit word from AUX address xa
#define xv_aux_getw(xa)                                                                                                \
    ({                                                                                                                 \
        uint16_t w;                                                                                                    \
        xv_setw(aux_addr, xa);                                                                                         \
        __asm__ __volatile__("movep.w " XVR_XSTR(XVR_aux_data) "(%[ptr]),%[dst]"                                       \
                             : [dst] "=d"(w)                                                                           \
                             : [ptr] "a"(xosera_ptr)                                                                   \
                             :);                                                                                       \
        w;                                                                                                             \
    })
// return high byte from xosera_aux_reg xr
#define xv_reg_getbh(xar) xv_aux_getbh(XVA_##xar)
// return low byte from xosera_aux_reg xr
#define xv_reg_getbl(xar) xv_aux_getbl(XVA_##xar)
// return 16-bit word from xosera_aux_reg xr
#define xv_reg_getw(xar) xv_aux_getw(XVA_##xar)
