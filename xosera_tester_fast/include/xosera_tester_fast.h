#ifndef __XOSERA_TESTER_FAST_H
#define __XOSERA_TESTER_FAST_H

// Xosera is operated via 16 16-bit registers the basics of which are outlined below.
//
// NOTE: TODO registers below are planned but not yet "wired up" in Xosera design
//
// Xosera uses 128 KB of embedded SPRAM (inside iCE40UP5K FPGA) for VRAM.
// This VRAM is arranged as 65536 x 16-bits so all Xosera addresses are 16-bit
// and all data transfers to/from VRAM are in 16-bit words.  Since Xosera uses an
// an 8-bit data bus, it uses big-endian (68K-style) byte transfers with MSB in even
// bytes and LSB in odd bytes (indicated via BUS_BYTESEL signal).
//
// When XVID_DATA or XVID_DATA2 is read, a 16-bit word is read from VRAM[XVID_RD_ADDR] and
// XVID_RD_ADDR += XVID_WR_INC (twos-complement, overflow ignored).
// Similarly, when the LSB of XVID_DATA or XVID_DATA2 is written to, a 16-bit value is
// written to VRAM[XVID_WR_ADDR] and XVID_WR_ADDR += XVID_WR_INC (twos-complement, overflow
// ignored).  The MSB of the word written will be the MSB previously written to XVID_DATA
// or XVID_DATA2 or zero if the last register write was to a different register.
// This allows faster output if only the LSB changes (e.g., text output with constant
// attribute byte).  Also both XVID_DATA or XVID_DATA2 exist to allow m68K to benefit
// from 32-bit data transfers using MOVEP.L instruction (using 4 8-bit transfers).

// Registers are currently write-only except XVID_DATA and XVID_DATA_2 (only upper two
// register number bits are used to decode register reads).


enum
{
    // register 16-bit read/write (no side effects)
    XVID_AUX_ADDR,        // reg 0: TODO video data (as set by VID_CTRL)
    XVID_CONST,           // reg 1: TODO CPU data (instead of read from VRAM)
    XVID_RD_ADDR,         // reg 2: address to read from VRAM
    XVID_WR_ADDR,         // reg 3: address to write from VRAM

    // special, odd byte write triggers
    XVID_DATA,            // reg 4: read/write word from/to VRAM RD/WR
    XVID_DATA_2,          // reg 5: read/write word from/to VRAM RD/WR (for 32-bit)
    XVID_AUX_DATA,        // reg 6: aux data (font/audio)
    XVID_COUNT,           // reg 7: TODO blitter "repeat" count/trigger

    // write only, 16-bit
    XVID_RD_INC,           // reg 9: read addr increment value
    XVID_WR_INC,           // reg A: write addr increment value
    XVID_WR_MOD,           // reg C: TODO write modulo width for 2D blit
    XVID_RD_MOD,           // reg B: TODO read modulo width for 2D blit
    XVID_WIDTH,            // reg 8: TODO width for 2D blit
    XVID_BLIT_CTRL,        // reg D: TODO
    XVID_UNUSED_E,         // reg E: TODO
    XVID_UNUSED_F,         // reg F: TODO

    // AUX read-only setting AUX_ADDR, reading AUX_DATA
    AUX_VID             = 0x0000,        // 0-8191 8-bit address (bits 15:8 ignored writing)
    AUX_VID_W_DISPSTART = 0x0000,        // display start address
    AUX_VID_W_TILEWIDTH = 0x0001,        // tile line width (usually WIDTH/8)
    AUX_VID_W_SCROLLXY  = 0x0002,        // [10:8] H fine scroll, [3:0] V fine scroll
    AUX_VID_W_FONTCTRL  = 0x0003,        // [9:8] 2KB font bank, [3:0] font height
    AUX_VID_W_GFXCTRL   = 0x0004,        // [1] v double TODO, [0] h double

    // AUX write-only setting AUX_ADDR, writing AUX_DATA
    AUX_VID_R_WIDTH    = 0x0000,        // display resolution width
    AUX_VID_R_HEIGHT   = 0x0001,        // display resolution height
    AUX_VID_R_FEATURES = 0x0002,        // [15] = 1 (test)
    AUX_VID_R_SCANLINE = 0x0003,        // [15] V blank, [14:11] zero [10:0] V line
    AUX_W_FONT         = 0x4000,        // 0x4000-0x5FFF 8K byte font memory (even byte [15:8] ignored)
    AUX_W_COLORTBL     = 0x8000,        // 0x8000-0x80FF 256 word color lookup table (0xXRGB)
    AUX_W_AUD          = 0xc000         // 0xC000-0x??? TODO (audio registers)
};

#ifdef __cplusplus
extern "C" {
#endif

void setup(void);
void loop(void);

#ifdef __cplusplus
}
#endif

#endif//__XOSERA_TESTER_FAST_H
