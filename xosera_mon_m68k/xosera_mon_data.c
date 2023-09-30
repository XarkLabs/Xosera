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
 * Copyright (c) 2021-2022 Xark
 * MIT License
 *
 * Xosera rosco_m68k mon register data file
 * ------------------------------------------------------------
 */

#include "xosera_mon_m68k.h"

#include "xosera_m68k_defs.h"

// Xosera XR Memory Regions (size in 16-bit words)
const addr_range_t xr_mem[] = {
    {"VID_CTRL", XR_VID_CTRL, 0x1},          // display control and border color index
    {"COPP_CTRL", XR_COPP_CTRL, 0x1},        // display synchronized coprocessor control
    {"AUD_CTRL", XR_AUD_CTRL, 0x1},          // audio channel control
    {"SCANLINE", XR_SCANLINE, 0x1},          // read scanline (incl. offscreen), write signal video interrupt
    {"VID_LEFT", XR_VID_LEFT, 0x1},          // left edge of active display window (typically 0)
    {"VID_RIGHT", XR_VID_RIGHT, 0x1},        // right edge of active display window +1 (typically 640 or 848)
    {"POINTER_H", XR_POINTER_H, 0x1},        // pointer sprite raw H position
    {"POINTER_V", XR_POINTER_V, 0x1},        // pointer sprite raw V position / pointer color select

    // Playfield A Control XR Registers
    {"PA_GFX_CTRL", XR_PA_GFX_CTRL, 0x1},          // pf A graphics control
    {"PA_TILE_CTRL", XR_PA_TILE_CTRL, 0x1},        // pf A tile control
    {"PA_DISP_ADDR", XR_PA_DISP_ADDR, 0x1},        // pf A display VRAM start address
    {"PA_LINE_LEN", XR_PA_LINE_LEN, 0x1},          // pf A display line width in words
    {"PA_V_FSCALE", XR_PA_HV_FSCALE, 0x1},         // pf A horizontal and vertical fractional scale
    {"PA_H_SCROLL", XR_PA_H_SCROLL, 0x1},          // pf A horizontal fine scroll
    {"PA_V_SCROLL", XR_PA_V_SCROLL, 0x1},          // pf A vertical fine scroll
    {"PA_LINE_ADDR", XR_PA_LINE_ADDR, 0x1},        // pf A scanline start address (loaded at start of line)

    // Playfield B Control XR Registers
    {"PB_GFX_CTRL", XR_PB_GFX_CTRL, 0x1},          // pf B graphics control
    {"PB_TILE_CTRL", XR_PB_TILE_CTRL, 0x1},        // pf B tile control
    {"PB_DISP_ADDR", XR_PB_DISP_ADDR, 0x1},        // pf B display VRAM start address
    {"PB_LINE_LEN", XR_PB_LINE_LEN, 0x1},          // pf B display line width in words
    {"PB_HV_FSCALE", XR_PB_HV_FSCALE, 0x1},        // pf B horizontal and vertical fractional scale
    {"PB_H_SCROLL", XR_PB_H_SCROLL, 0x1},          // pf B horizontal fine scroll
    {"PB_V_SCROLL", XR_PB_V_SCROLL, 0x1},          // pf B vertical fine scroll
    {"PB_LINE_ADDR", XR_PB_LINE_ADDR, 0x1},        // pf B scanline start address (loaded at start of line)

    // Audio Registers
    {"AUD0_VOL", XR_AUD0_VOL, 0x1},              // audio channel 0 8-bit L[15:8]+R[7:0] volume (0x80 = 100%)
    {"AUD0_PERIOD", XR_AUD0_PERIOD, 0x1},        // audio channel 0 15-bit period, bit [15] force restart
    {"AUD0_LENGTH", XR_AUD0_LENGTH, 0x1},        // audio channel 0 15-bit sample word length-1, bit [15] tile mem
    {"AUD0_START", XR_AUD0_START, 0x1},          // audio channel 0 sample start add (vram/tilemem), writes next pending
    {"AUD1_VOL", XR_AUD1_VOL, 0x1},              // audio channel 1 8-bit L[15:8]+R[7:0] volume (0x80 = 100%)
    {"AUD1_PERIOD", XR_AUD1_PERIOD, 0x1},        // audio channel 1 15-bit period, bit [15] force restart
    {"AUD1_LENGTH", XR_AUD1_LENGTH, 0x1},        // audio channel 1 15-bit sample word length-1, bit [15] tile mem
    {"AUD1_START ", XR_AUD1_START, 0x1},         // audio channel 1 sample start add (vram/tilemem), writes next pending
    {"AUD2_VOL", XR_AUD2_VOL, 0x1},              // audio channel 2 8-bit L[15:8]+R[7:0] volume (0x80 = 100%)
    {"AUD2_PERIOD", XR_AUD2_PERIOD, 0x1},        // audio channel 2 15-bit period, bit [15] force restart
    {"AUD2_LENGTH", XR_AUD2_LENGTH, 0x1},        // audio channel 2 15-bit sample word length-1, bit [15] tile mem
    {"AUD2_START ", XR_AUD2_START, 0x1},         // audio channel 2 sample start add (vram/tilemem), writes next pending
    {"AUD3_VOL", XR_AUD3_VOL, 0x1},              // audio channel 3 8-bit L[15:8]+R[7:0] volume (0x80 = 100%)
    {"AUD3_PERIOD", XR_AUD3_PERIOD, 0x1},        // audio channel 3 15-bit period, bit [15] force restart
    {"AUD3_LENGTH", XR_AUD3_LENGTH, 0x1},        // audio channel 3 15-bit sample word length-1, bit [15] tile mem
    {"AUD3_START", XR_AUD3_START, 0x1},          // audio channel 3 sample start add (vram/tilemem), writes next pending

    // Blitter Registers
    {"BLIT_CTRL", XR_BLIT_CTRL, 0x1},        // (R /W) blit control (transparency control, logic op and op input flags)
    {"BLIT_ANDC", XR_BLIT_ANDC, 0x1},        // (R /W) blit line modulo added to SRC_A (XOR if A const)
    {"BLIT_XOR", XR_BLIT_XOR, 0x1},          // (R /W) blit A source VRAM read address / constant value
    {"BLIT_MOD_S", XR_BLIT_MOD_S, 0x1},        // (R /W) blit line modulo added to SRC_B (XOR if B const)
    {"BLIT_SRC_S", XR_BLIT_SRC_S, 0x1},        // (R /W) blit B AND source VRAM read address / constant value
    {"BLIT_MOD_D", XR_BLIT_MOD_D, 0x1},        // (R /W) blit line XOR modifier for C_VAL const
    {"BLIT_DST_D", XR_BLIT_DST_D, 0x1},        // (R /W) blit C XOR constant value
    {"BLIT_SHIFT", XR_BLIT_SHIFT, 0x1},        // (R /W) blit modulo added to D destination after each line
    {"BLIT_LINES", XR_BLIT_LINES, 0x1},        // (R /W) blit D VRAM destination write address
    {"BLIT_WORDS", XR_BLIT_WORDS, 0x1},        // (R /W) blit first/last masks and nibble shift (0-3)
    {"UNUSED_4A", XR_UNUSED_4A, 0x1},          // (R /W) blit number of lines minus 1
    {"UNUSED_4B", XR_UNUSED_4B, 0x1},          // (R /W) blit word count minus 1 per line (write starts blit operation)
    {"UNUSED_4C", XR_UNUSED_4C, 0x1},          // (- /-) TODO: unused XR 2C
    {"UNUSED_4D", XR_UNUSED_4D, 0x1},          // (- /-) TODO: unused XR 2D
    {"UNUSED_4E", XR_UNUSED_4E, 0x1},          // (- /-) TODO: unused XR 2E
    {"UNUSED_4F", XR_UNUSED_4F, 0x1},          // (- /-) TODO: unused XR 2F

    {"XR_TILE", XR_TILE_ADDR, XR_TILE_SIZE},                 // (R/W) 0x4000-0x53FF tile glyph/tile map memory
    {"XR_COLOR_A", XR_COLOR_A_ADDR, XR_COLOR_A_SIZE},        // (R/W) 0x8000-0x80FF A 256 entry color lookup memory
    {"XR_COLOR_B", XR_COLOR_B_ADDR, XR_COLOR_B_SIZE},        // (R/W) 0x8100-0x81FF B 256 entry color lookup memory
    {"XR_POINTER", XR_POINTER_ADDR, XR_POINTER_SIZE},        // (R/W) 0x8100-0x81FF B 256 entry color lookup memory
    {"XR_COPPER", XR_COPPER_ADDR, XR_COPPER_SIZE},           // (R/W) 0xC000-0xC5FF copper program memory
    {NULL, 0, 0}};

// Xosera Main Registers (XM Registers, directly CPU accessable)
const addr_range_t xm_regs[] = {
    {"SYS_CTRL", (XM_SYS_CTRL >> 2), 0x01},        // [15:8] status bits, PIXEL_X/Y & options, [3:0] mask
    {"INT_CTRL", (XM_INT_CTRL >> 2), 0x01},        // FPGA config, interrupt status/control
    {"TIMER", (XM_TIMER >> 2), 0x01},              // read 1/10th msec timer, write 8-bit interval timer
    {"RD_XADDR", (XM_RD_XADDR >> 2), 0x01},        // XR register/address for XM_XDATA read access
    {"WR_XADDR", (XM_WR_XADDR >> 2), 0x01},        // XR register/address for XM_XDATA write access
    {"XDATA", (XM_XDATA >> 2), 0x01},              // read/write XR register/memory at XM_RD_XADDR/XM_WR_XADDR
    {"RD_INCR", (XM_RD_INCR >> 2), 0x01},          // increment value for XM_RD_ADDR read from XM_DATA/XM_DATA_2
    {"RD_ADDR", (XM_RD_ADDR >> 2), 0x01},          // VRAM address for reading from VRAM when XM_DATA/XM_DATA_2 is read
    {"WR_INCR", (XM_WR_INCR >> 2), 0x01},          // increment value for XM_WR_ADDR on write to XM_DATA/XM_DATA_2
    {"WR_ADDR", (XM_WR_ADDR >> 2), 0x01},          // VRAM address for writing to VRAM when XM_DATA/XM_DATA_2 is written
    {"DATA", (XM_DATA >> 2), 0x01},        // read/write VRAM word at XM_RD_ADDR/XM_WR_ADDR & add XM_RD_INCR/XM_WR_INCR
    {"DATA_2", (XM_DATA_2 >> 2), 0x01},          // 2nd XM_DATA(to allow for 32-bit read/write access)
    {"PIXEL_X", (XM_PIXEL_X >> 2), 0x01},        // pixel X coordinate / setup pixel base address
    {"PIXEL_Y", (XM_PIXEL_Y >> 2), 0x01},        // pixel Y coordinate / setup pixel line width
    {"UART", (XM_UART >> 2), 0x01},              // optional debug USB UART communication
    {"FEATURE", (XM_FEATURE >> 2), 0x01},        // Xosera feature flags
    {NULL, 0, 0}};

// NOTE: These are bits in high byte of SYS_CTRL word (fastest to access)
const addr_range_t sys_ctrl_status[] = {
    {"MEM_WAIT", SYS_CTRL_MEM_WAIT_B, 1},        // (R /- )  memory read/write operation pending (with contended memory)
    {"BLIT_FULL", SYS_CTRL_BLIT_FULL_B, 1},        // (R /- )  blitter queue is full
    {"BLIT_BUSY", SYS_CTRL_BLIT_BUSY_B, 1},        // (R /- )  blitter is still busy performing an operation (not done)
    {"B_12", SYS_CTRL_UNUSED_12_B, 1},             // (R /- )  unused (reads 0)
    {"HBLANK", SYS_CTRL_HBLANK_B, 1},              // (R /- )  video signal is in horizontal blank period
    {"VBLANK", SYS_CTRL_VBLANK_B, 1},              // (R /- )  video signal is in vertical blank period
    {"PIX_NO_MASK", SYS_CTRL_PIX_NO_MASK_B, 1},        // (R /- )  unused (reads 0)
    {"PIX_8B_MASK", SYS_CTRL_PIX_8B_MASK_B, 1},        // (R /- )  unused (reads 0)
    {NULL, 0, 0}};

// XR Extended Register / Region (accessed via XM_RD_XADDR/XM_WR_XADDR and XM_XDATA)
