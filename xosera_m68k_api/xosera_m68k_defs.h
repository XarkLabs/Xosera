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
 * Xosera rosco_m68k C register definition header file
 * ------------------------------------------------------------
 */

// See: https://github.com/XarkLabs/Xosera/blob/master/REFERENCE.md

#if !defined(XOSERA_M68K_DEFS_H)
#define XOSERA_M68K_DEFS_H

#define XM_BASEADDR 0xf80060        // Xosera rosco_m68k register base address

// Xosera XR Memory Regions (size in 16-bit words)
#define XR_COLOR_ADDR   0x8000        // (R/W) 0x8000-0x81FF 2 x A & B color lookup memory
#define XR_COLOR_SIZE   0x0200        //                      2 x 256 x 16-bit words  (0xARGB)
#define XR_COLOR_A_ADDR 0x8000        // (R/W) 0x8000-0x80FF A 256 entry color lookup memory
#define XR_COLOR_A_SIZE 0x0100        //                      256 x 16-bit words (0xARGB)
#define XR_COLOR_B_ADDR 0x8100        // (R/W) 0x8100-0x81FF B 256 entry color lookup memory
#define XR_COLOR_B_SIZE 0x0100        //                      256 x 16-bit words (0xARGB)
#define XR_TILE_ADDR    0xA000        // (R/W) 0xA000-0xB3FF tile glyph/tile map memory
#define XR_TILE_SIZE    0x1400        //                      5120 x 16-bit tile glyph/tile map memory
#define XR_COPPER_ADDR  0xC000        // (R/W) 0xC000-0xC7FF copper program memory (32-bit instructions)
#define XR_COPPER_SIZE  0x0800        //                      2048 x 16-bit copper program memory addresses
#define XR_UNUSED_ADDR  0xE000        // (-/-) 0xE000-0xFFFF unused

// Macros to make bit-fields easier (works similar to Verilog "+:" operator, e.g., word[RIGHTMOST_BIT +: BIT_WIDTH])
#define XB_(v, right_bit, bit_width) (((v) & ((1 << (bit_width)) - 1)) << (right_bit))

// Xosera Main Registers (XM Registers, directly CPU accessable)
// NOTE: Main register numbers are multiplied by 4 for rosco_m68k, because of even byte 6800 8-bit addressing plus
// 16-bit registers
#define XM_XR_ADDR   0x0         // (R /W+) XR register number/address for XM_XR_DATA read/write access
#define XM_XR_DATA   0x4         // (R /W+) read/write XR register/memory at XM_XR_ADDR (XM_XR_ADDR incr. on write)
#define XM_RD_INCR   0x8         // (R /W ) increment value for XM_RD_ADDR read from XM_DATA/XM_DATA_2
#define XM_RD_ADDR   0xC         // (R /W+) VRAM address for reading from VRAM when XM_DATA/XM_DATA_2 is read
#define XM_WR_INCR   0x10        // (R /W ) increment value for XM_WR_ADDR on write to XM_DATA/XM_DATA_2
#define XM_WR_ADDR   0x14        // (R /W ) VRAM address for writing to VRAM when XM_DATA/XM_DATA_2 is written
#define XM_DATA      0x18        // (R+/W+) read/write VRAM word at XM_RD_ADDR/XM_WR_ADDR (and add XM_RD_INCR/XM_WR_INCR)
#define XM_DATA_2    0x1C        // (R+/W+) 2nd XM_DATA(to allow for 32-bit read/write access)
#define XM_SYS_CTRL  0x20        // (R /W+) busy status, FPGA reconfig, interrupt status/control, write masking
#define XM_TIMER     0x24        // (R /W+) read 1/10th millisecond timer, write clear interrupt signal
#define XM_LFSR      0x28        // (RO)    LFSR pseudo-random number // TODO: keep this?
#define XM_UNUSED_B  0x2C        // (R /W ) unused direct register 0xB // TODO: slated for XR_DATA_2 after reorg
#define XM_RW_INCR   0x30        // (R /W ) XM_RW_ADDR increment value on read/write of XM_RW_DATA/XM_RW_DATA_2
#define XM_RW_ADDR   0x34        // (R /W+) read/write address for VRAM access from XM_RW_DATA/XM_RW_DATA_2
#define XM_RW_DATA   0x38        // (R+/W+) read/write VRAM word at XM_RW_ADDR (and add XM_RW_INCR)
#define XM_RW_DATA_2 0x3C        // (R+/W+) 2nd XM_RW_DATA(to allow for 32-bit read/write access)

#define SYS_CTRL_MEMWAIT_B  7
#define SYS_CTRL_BLITBUSY_B 6
#define SYS_CTRL_BLITFULL_B 5

#define MK_SYS_CTRL(reboot, bootcfg, intena, wrmask)                                                                   \
    (XB_(reboot, 15, 1) | XB_(bootcfg, 14, 2) | XB_(intena, 11, 4) | XB_(wrmask, 3, 0))

// XR Extended Register / Region (accessed via XM_XR_ADDR and XM_XR_DATA)

//  Video Config and Copper XR Registers
#define XR_VID_CTRL   0x00        // (R /W) display control and border color index
#define XR_COPP_CTRL  0x01        // (R /W) display synchronized coprocessor control
#define XR_CURSOR_X   0x02        // (R /W) sprite cursor X position // TODO: to be refactored
#define XR_CURSOR_Y   0x03        // (R /W) sprite cursor Y position // TODO: to be refactored
#define XR_VID_TOP    0x04        // (R /W) // TODO: to be refactored
#define XR_VID_BOTTOM 0x05        // (R /W) // TODO: to be refactored
#define XR_VID_LEFT   0x06        // (R /W) left edge of active display window (typically 0)
#define XR_VID_RIGHT  0x07        // (R /W) right edge +1 of active display window (typically 640 or 848)
#define XR_SCANLINE   0x08        // (RO  ) [15] in V blank, [14] in H blank [10:0] V scanline
#define XR_UNUSED_09  0x09        // (RO  )
#define XR_VERSION    0x0A        // (RO  ) Xosera optional feature bits [15:8] and version code [7:0] [TODO]
#define XR_GITHASH_H  0x0B        // (RO  ) [15:0] high 16-bits of 32-bit Git hash build identifier
#define XR_GITHASH_L  0x0C        // (RO  ) [15:0] low 16-bits of 32-bit Git hash build identifier
#define XR_VID_HSIZE  0x0D        // (RO  ) native pixel width of monitor mode (e.g. 640/848)
#define XR_VID_VSIZE  0x0E        // (RO  ) native pixel height of monitor mode (e.g. 480)
#define XR_VID_VFREQ  0x0F        // (RO  ) update frequency of monitor mode in BCD 1/100th Hz (0x5997 = 59.97 Hz)

#define MAKE_VID_CTRL(borcol, intmask) (XB_(borcol, 8, 8) | XB_(intmask, 0, 4))

// Playfield A Control XR Registers
#define XR_PA_GFX_CTRL  0x10        // (R /W) playfield A graphics control
#define XR_PA_TILE_CTRL 0x11        // (R /W) playfield A tile control
#define XR_PA_DISP_ADDR 0x12        // (R /W) playfield A display VRAM start address
#define XR_PA_LINE_LEN  0x13        // (R /W) playfield A display line width in words
#define XR_PA_HV_SCROLL 0x14        // (R /W) playfield A horizontal and vertical fine scroll
#define XR_PA_LINE_ADDR 0x15        // (R /W) playfield A scanline start address (loaded at start of line)
#define XR_PA_UNUSED_16 0x16        //
#define XR_PA_UNUSED_17 0x17        //

// Playfield B Control XR Registers
#define XR_PB_GFX_CTRL  0x18        // (R /W) playfield B graphics control
#define XR_PB_TILE_CTRL 0x19        // (R /W) playfield B tile control
#define XR_PB_DISP_ADDR 0x1A        // (R /W) playfield B display VRAM start address
#define XR_PB_LINE_LEN  0x1B        // (R /W) playfield B display line width in words
#define XR_PB_HV_SCROLL 0x1C        // (R /W) playfield B horizontal and vertical fine scroll
#define XR_PB_LINE_ADDR 0x1D        // (R /W) playfield B scanline start address (loaded at start of line)
#define XR_PB_UNUSED_1E 0x1E        //
#define XR_PB_UNUSED_1F 0x1F        //

#define XR_GFX_BPP_1 0        // Px_GFX_CTRL.bpp
#define XR_GFX_BPP_4 1        // Px_GFX_CTRL.bpp
#define XR_GFX_BPP_8 2        // Px_GFX_CTRL.bpp
#define XR_GFX_BPP_X 3        // Px_GFX_CTRL.bpp

#define MAKE_GFX_CTRL(colbase, blank, bpp, bm, hx, vx)                                                                 \
    (XB_(colbase, 8, 8) | XB_(blank, 7, 1) | XB_(bm, 6, 1) | XB_(bpp, 4, 2) | XB_(hx, 2, 2) | XB_(vx, 0, 2))
#define MAKE_TILE_CTRL(tilebase, map_in_tile, glyph_in_vram, tileheight)                                               \
    (((tilebase)&0xFC00) | XB_(map_in_tile, 9, 1) | XB_(glyph_in_vram, 8, 1) | XB_(((tileheight)-1), 0, 3))
#define MAKE_HV_SCROLL(h_scrl, v_scrl) (XB_(h_scrl, 8, 8) | XB_(v_scrl, 0, 8))

// Blitter Registers
#define XR_BLIT_CTRL  0x20        // (R /W) blit control bits (transparency control, logic op and op input flags)
#define XR_BLIT_MOD_C 0x21        // (R /W) blit value XOR'd to C const after each line
#define XR_BLIT_VAL_C 0x22        // (R /W) blit C constant value
#define XR_BLIT_MOD_B 0x23        // (R /W) blit modulo added to B addr after each line, or XOR'd if B const
#define XR_BLIT_SRC_B 0x24        // (R /W) blit B source VRAM read address / constant value
#define XR_BLIT_MOD_D 0x25        // (R /W) blit modulo added to D destination after each line
#define XR_BLIT_MOD_A 0x26        // (R /W) blit modulo added to A addr after each line, or XOR'd if A const
#define XR_BLIT_SRC_A 0x27        // (R /W) blit A source VRAM read address / constant value
#define XR_BLIT_SHIFT 0x28        // (R /W) blit first and last word nibble masks and nibble right shift (0-3)
#define XR_BLIT_DST_D 0x29        // (R /W) blit D VRAM destination write address
#define XR_BLIT_LINES 0x2A        // (R /W) blit number of lines minus 1, (repeats blit word count after modulo calc)
#define XR_BLIT_WORDS 0x2B        // (R /W) blit word count minus 1 per line (write starts blit operation)

// Copper instruction helper macros
#define COP_WAIT_HV(h_pos, v_pos)   (0x00000000 | XB_((uint32_t)(v_pos), 16, 12) | XB_((uint32_t)(h_pos), 4, 12))
#define COP_WAIT_H(h_pos)           (0x00000001 | XB_((uint32_t)(h_pos), 4, 12))
#define COP_WAIT_V(v_pos)           (0x00000002 | XB_((uint32_t)(v_pos), 16, 12))
#define COP_WAIT_F()                (0x00000003)
#define COP_END()                   (0x00000003)
#define COP_SKIP_HV(h_pos, v_pos)   (0x20000000 | XB_((uint32_t)(v_pos), 16, 12) | XB_((uint32_t)(h_pos), 4, 12))
#define COP_SKIP_H(h_pos)           (0x20000001 | XB_((uint32_t)(h_pos), 4, 12))
#define COP_SKIP_V(v_pos)           (0x20000002 | XB_((uint32_t)(v_pos), 16, 12))
#define COP_SKIP_F()                (0x20000003)
#define COP_JUMP(cop_addr)          (0x40000000 | XB_((uint32_t)(cop_addr), 16, 13))
#define COP_MOVER(val16, xreg)      (0x60000000 | XB_((uint32_t)(XR_##xreg), 16, 13) | ((uint16_t)(val16)))
#define COP_MOVEF(val16, tile_addr) (0x80000000 | XB_((uint32_t)(tile_addr), 16, 13) | ((uint16_t)(val16)))
#define COP_MOVEP(rgb16, color_num) (0xA0000000 | XB_((uint32_t)(color_num), 16, 13) | ((uint16_t)(rgb16)))
#define COP_MOVEC(val16, cop_addr)  (0xC0000000 | XB_((uint32_t)(cop_addr), 16, 13) | ((uint16_t)(val16)))

// TODO: repace more magic constants with defines for bit positions


#endif        // XOSERA_M68K_DEFS_H
