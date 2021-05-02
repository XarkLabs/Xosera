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

// Xosera primary register map (See https://github.com/XarkLabs/Xosera/blob/develop/REFERENCE.md)
typedef struct _xosera_regs
{
    xreg_t _reserved;        // dummy entry to use offset 0 (to assure an offset for MOVEP)
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

// NOTE: This struct is only used with xv_set_reg* and xv_get_reg* for AUX reg address (fake scoped enum)
typedef struct _xosera_aux_regs
{
    uint8_t dispstart;
    uint8_t dispwidth;
    uint8_t scrollxy;
    uint8_t fontctrl;
    uint8_t gfxctrl;
    uint8_t unused_5;
    uint8_t unused_6;
    uint8_t unused_7;
    uint8_t vidwidth;
    uint8_t vidheight;
    uint8_t features;
    uint8_t scanline;
    uint8_t githash_h;
    uint8_t githash_l;
    uint8_t unused_E;
    uint8_t unused_F;
} xosera_aux_regs_t;

enum xosera_aux
{
    // AUX memory areas
    AUX_VID      = 0x0000,        // 0x0000-0x000F 16 word video registers (see below)
    AUX_FONTMEM  = 0x4000,        // 0x4000-0x5FFF 8K byte font memory (even byte [15:8] ignored)
    AUX_COLORMEM = 0x8000,        // 0x8000-0x80FF 256 word color lookup table (0xXRGB)
    AUX_AUDMEM   = 0xC000,        // 0xC000-0x??? TODO (audio registers)

    // AUX_VID read-write registers (write address to AUX_ADDR first then read/write AUX_DATA)
    AUX_DISPSTART = AUX_VID | 0x0000,        // display start address
    AUX_DISPWIDTH = AUX_VID | 0x0001,        // display width in words
    AUX_SCROLLXY  = AUX_VID | 0x0002,        // [10:8] H fine scroll, [3:0] V fine scroll
    AUX_FONTCTRL  = AUX_VID | 0x0003,        // [15:11] 1KW/2KW font bank,[8] bram/vram [3:0] font height
    AUX_GFXCTRL   = AUX_VID | 0x0004,        // [0] h pix double
    AUX_UNUSED_5  = AUX_VID | 0x0005,
    AUX_UNUSED_6  = AUX_VID | 0x0006,
    AUX_UNUSED_7  = AUX_VID | 0x0007,

    // AUX_VID read-only registers (write address to AUX_ADDR first to update AUX_DATA read value)
    AUX_R_VIDWIDTH  = AUX_VID | 0x0008,        // display resolution width
    AUX_R_VIDHEIGHT = AUX_VID | 0x0009,        // display resolution height
    AUX_R_FEATURES  = AUX_VID | 0x000A,        // [15] = 1 (test)
    AUX_R_SCANLINE  = AUX_VID | 0x000B,        // [15] V blank, [14] H blank, [13:11] zero [10:0] V line
    AUX_R_GITHASHH  = AUX_VID | 0x000C,
    AUX_R_GITHASHL  = AUX_VID | 0x000D,
    AUX_R_UNUSED_E  = AUX_VID | 0x000E,
    AUX_R_UNUSED_F  = AUX_VID | 0x000F
};

// Xosera register base ptr
extern xosera_regs_t * xosera_ptr;

// set high byte of xosera_reg xr to 8-bit byte bh
#define xv_setbh(xr, bh)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        xosera_ptr->xr.b.h = (bh);                                                                                     \
    } while (0)
// set low byte of xosera_reg xr to 8-bit byte bl
#define xv_setbl(xr, bl)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        xosera_ptr->xr.b.l = (bl);                                                                                     \
    } while (0)
// set xosera_reg xr to 16-bit word wv
#define xv_setw(xr, wv)                                                                                                \
    do                                                                                                                 \
    {                                                                                                                  \
        __asm__ __volatile__("movep.w %[src],%[dst]" : : [src] "d"((wv)), [dst] "m"(xosera_ptr->xr) :);                \
    } while (0)
// set xosera_reg xr to 32-bit long lv
#define xv_setl(xr, lv)                                                                                                \
    do                                                                                                                 \
    {                                                                                                                  \
        __asm__ __volatile__("movep.l %[src],%[dst]" : : [src] "d"((lv)), [dst] "m"(xosera_ptr->xr) :);                \
    } while (0)
// set high byte of xosera_aux_reg xar to 8-bit byte bh (NOTE: byte saved until bl also set)
#define xv_reg_setbh(xar, bh)                                                                                          \
    do                                                                                                                 \
    {                                                                                                                  \
        xv_setw(aux_addr, offsetof(xosera_aux_regs_t, xar));                                                           \
        xosera_ptr->aux_data.b.h = (bh);                                                                               \
    } while (0)
// set low byte of xosera_aux_reg xar to 8-bit byte bl (NOTE: uses previously set bh or zero)
#define xv_reg_setbl(xar, bl)                                                                                          \
    do                                                                                                                 \
    {                                                                                                                  \
        xv_setw(aux_addr, offsetof(xosera_aux_regs_t, xar));                                                           \
        xosera_ptr->aux_data.b.h = (bl);                                                                               \
    } while (0)
// set xosera_aux_reg xar to 16-bit word wv
#define xv_reg_setw(xar, wv)                                                                                           \
    do                                                                                                                 \
    {                                                                                                                  \
        xv_setw(aux_addr, offsetof(xosera_aux_regs_t, xar));                                                           \
        __asm__ __volatile__("movep.w %[src],%[dst]" : : [src] "d"((wv)), [dst] "m"(xosera_ptr->aux_data) :);          \
    } while (0)
// set high byte of AUX address to 8-bit byte bh (NOTE: byte saved until bl also set)
#define xv_aux_setbh(xa, bh)                                                                                           \
    do                                                                                                                 \
    {                                                                                                                  \
        xv_setw(aux_addr, (xa));                                                                                       \
        xosera_ptr->aux_data.b.h = bh);                                                                                \
    } while (0)
// set low byte of AUX address to 8-bit byte bl (NOTE: uses previously set bh or zero)
#define xv_aux_setbl(xa, bl)                                                                                           \
    do                                                                                                                 \
    {                                                                                                                  \
        xv_setw(aux_addr, (xa));                                                                                       \
        xosera_ptr->aux_data.b.l = (bl);                                                                               \
    } while (0)
// set AUX address to 16-bit word wv
#define xv_aux_setw(xa, wv)                                                                                            \
    do                                                                                                                 \
    {                                                                                                                  \
        xv_setw(aux_addr, (xa));                                                                                       \
        __asm__ __volatile__("movep.w %[src],%[dst]" : : [src] "d"((wv)), [dst] "m"(xosera_ptr->aux_data) :);          \
    } while (0)

// NOTE: Uses clang and gcc supported extension (statement expression), so we must slightly lower shields...
#pragma GCC diagnostic ignored "-Wpedantic"        // Yes, I'm slightly cheating (but ugly to have to pass in "return
                                                   // variable" - and this is the "low level" API, remember)

// return high byte from xosera_reg xr
#define xv_getbh(xr) (xosera_ptr->xr.b.h)
// return low byte from xosera_reg xr
#define xv_getbl(xr) (xosera_ptr->xr.b.l)
// return 16-bit word from xosera_reg xr
#define xv_getw(xr)                                                                                                    \
    ({                                                                                                                 \
        uint16_t w;                                                                                                    \
        __asm__ __volatile__("movep.w %[src],%[dst]" : [dst] "=d"(w) : [src] "m"(xosera_ptr->xr) :);                   \
        w;                                                                                                             \
    })
// return 32-bit word from xosera_reg xr and xr+1
#define xv_getl(xr)                                                                                                    \
    ({                                                                                                                 \
        uint32_t l;                                                                                                    \
        __asm__ __volatile__("movep.l %[src],%[dst]" : [dst] "=d"(l) : [src] "m"(xosera_ptr->xr) :);                   \
        l;                                                                                                             \
    })
// return high byte from xosera_aux_reg xr
#define xv_reg_getbh(xar) (xv_setw(aux_addr, offsetof(xosera_aux_regs_t, xar)), xosera_ptr->aux_data.b.h)
// return low byte from xosera_aux_reg xr
#define xv_reg_getbl(xar) (xv_setw(aux_addr, offsetof(xosera_aux_regs_t, xar)), xosera_ptr->aux_data.b.l)
// return 16-bit word from xosera_aux_reg xr
#define xv_reg_getw(xar)                                                                                               \
    ({                                                                                                                 \
        uint16_t w;                                                                                                    \
        xv_setw(aux_addr, offsetof(xosera_aux_regs_t, xar));                                                           \
        __asm__ __volatile__("movep.w %[src],%[dst]" : [dst] "=d"(w) : [src] "m"(xosera_ptr->aux_data) :);             \
        w;                                                                                                             \
    })
// return high byte from AUX address xa
#define xv_aux_getbh(xa) (xv_setw(aux_addr, (xa)), xosera_ptr->aux_data.b.h)
// return low byte from AUX address xa
#define xv_aux_getbl(xa) (xv_setw(aux_addr, (xa)), xosera_ptr->aux_data.b.l)
// return 16-bit word from AUX address xa
#define xv_aux_getw(xa)                                                                                                \
    ({                                                                                                                 \
        uint16_t w;                                                                                                    \
        xv_setw(aux_addr, (xa));                                                                                       \
        __asm__ __volatile__("movep.w %[src],%[dst]" : [dst] "=d"(w) : [src] "m"(xosera_ptr->aux_data) :);             \
        w;                                                                                                             \
    })
