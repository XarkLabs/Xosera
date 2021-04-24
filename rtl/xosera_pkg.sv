

// xosera_pkg.sv - Common definitions for Xosera
//
// vim: set et ts=4 sw=4
//
// Copyright (c) 2020 Xark - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)
//

`ifndef XOSERA_PKG
`define XOSERA_PKG

`default_nettype none               // mandatory for Verilog sanity
`timescale 1ns/1ps                  // mandatory to shut up Icarus Verilog

// "brief" package name (as Yosys doesn't support wildcard imports so lots of "xv::")
package xv;
// Xosera directly addressable registers (16 x 16-bit word)
typedef enum logic [3:0]{
    // register 16-bit read/write (no side effects)
    XVID_AUX_ADDR = 4'h0,        // reg 0: set AUX bus read/write address (see below)
    XVID_CONST    = 4'h1,        // reg 1: TODO CPU data (instead of read from VRAM)
    XVID_RD_ADDR  = 4'h2,        // reg 2: address to read from VRAM
    XVID_WR_ADDR  = 4'h3,        // reg 3: address to write from VRAM

    // special registers (special read value, odd byte write triggers effect)
    XVID_DATA     = 4'h4,        // reg 4: read/write word from/to VRAM RD/WR
    XVID_DATA_2   = 4'h5,        // reg 5: read/write word from/to VRAM RD/WR (for 32-bit)
    XVID_AUX_DATA = 4'h6,        // reg 6: aux data (font/audio)
    XVID_COUNT    = 4'h7,        // reg 7: TODO blitter "repeat" count/trigger

    // write only, 16-bit
    XVID_RD_INC    = 4'h8,        // reg 9: read addr increment value
    XVID_WR_INC    = 4'h9,        // reg A: write addr increment value
    XVID_WR_MOD    = 4'hA,        // reg C: TODO write modulo width for 2D blit
    XVID_RD_MOD    = 4'hB,        // reg B: TODO read modulo width for 2D blit
    XVID_WIDTH     = 4'hC,        // reg 8: TODO width for 2D blit
    XVID_BLIT_CTRL = 4'hD,        // reg D: TODO
    XVID_UNUSED_1  = 4'hE,        // reg E: TODO
    XVID_UNUSED_2  = 4'hF         // reg F: TODO
} register_t;

// AUX memory areas
typedef enum logic [15:0]{
    AUX_VID      = 16'h0000,        // 0x0000-0x000F 16 word video registers (see below)
    AUX_FONT     = 16'h4000,        // 0x4000-0x5FFF 8K byte font memory (even byte [15:8] ignored)
    AUX_COLORTBL = 16'h8000,        // 0x8000-0x80FF 256 word color lookup table (0xXRGB)
    AUX_AUD      = 16'hC000         // 0xC000-0x??? TODO (audio registers)
} aux_mem_area_t;

// AUX_VID write-only registers (write address to AUX_ADDR first)
typedef enum logic [15:0]{
    AUX_VID_W_DISPSTART = AUX_VID | 16'h0000,        // display start address
    AUX_VID_W_TILEWIDTH = AUX_VID | 16'h0001,        // tile line width (normally WIDTH/8)
    AUX_VID_W_SCROLLXY  = AUX_VID | 16'h0002,        // [10:8] H fine scroll, [3:0] V fine scroll
    AUX_VID_W_FONTCTRL  = AUX_VID | 16'h0003,        // [9:8] 2KB font bank, [3:0] font height
    AUX_VID_W_GFXCTRL   = AUX_VID | 16'h0004,        // [0] h pix double
    AUX_VID_W_UNUSED5   = AUX_VID | 16'h0005,
    AUX_VID_W_UNUSED6   = AUX_VID | 16'h0006,
    AUX_VID_W_UNUSED7   = AUX_VID | 16'h0007
} aux_vid_w_t;

// AUX_VID read-only registers (write address to AUX_ADDR first to update value read)
typedef enum logic [15:0]{
    AUX_VID_R_WIDTH    = AUX_VID | 16'h0000,        // display resolution width
    AUX_VID_R_HEIGHT   = AUX_VID | 16'h0001,        // display resolution height
    AUX_VID_R_FEATURES = AUX_VID | 16'h0002,        // [15] = 1 (test)
    AUX_VID_R_SCANLINE = AUX_VID | 16'h0003,        // [15] V blank, [14] H blank, [13:11] zero [10:0] V line
    AUX_VID_R_UNUSED4  = AUX_VID | 16'h0004,
    AUX_VID_R_UNUSED5  = AUX_VID | 16'h0005,
    AUX_VID_R_UNUSED6  = AUX_VID | 16'h0006,
    AUX_VID_R_UNUSED7  = AUX_VID | 16'h0007
} aux_vid_r_t;

`ifdef MODE_640x400     // 25.175 MHz (requested), 25.125 MHz (achieved)
`elsif MODE_640x480     // 25.175 MHz (requested), 25.125 MHz (achieved)
`elsif MODE_720x400     // 28.322 MHz (requested), 28.500 MHz (achieved)
`elsif MODE_848x480     // 33.750 MHz (requested), 33.750 MHz (achieved)
`elsif MODE_800x600     // 40.000 MHz (requested), 39.750 MHz (achieved) [tight timing]
`elsif MODE_1024x768    // 65.000 MHz (requested), 65.250 MHz (achieved) [fails timing]
`elsif MODE_1280x720    // 74.176 MHz (requested), 73.500 MHz (achieved) [fails timing]
`else
`define MODE_640x480    // default
`endif

`ifdef    MODE_640x400
// VGA mode 640x480 @ 60Hz (pixel clock 25.175Mhz)
localparam PIXEL_FREQ        = 25_175_000;  // pixel clock in Hz
localparam VISIBLE_WIDTH     = 640;         // horizontal active pixels
localparam VISIBLE_HEIGHT    = 400;         // vertical active lines
localparam H_FRONT_PORCH     = 16;          // H pre-sync (front porch) pixels
localparam H_SYNC_PULSE      = 96;          // H sync pulse pixels
localparam H_BACK_PORCH      = 48;          // H post-sync (back porch) pixels
localparam V_FRONT_PORCH     = 12;          // V pre-sync (front porch) lines
localparam V_SYNC_PULSE      = 2;           // V sync pulse lines
localparam V_BACK_PORCH      = 35;          // V post-sync (back porch) lines
localparam H_SYNC_POLARITY   = 1'b0;        // H sync pulse active level
localparam V_SYNC_POLARITY   = 1'b1;        // V sync pulse active level

`elsif    MODE_640x480
// VGA mode 640x480 @ 60Hz (pixel clock 25.175Mhz)
localparam PIXEL_FREQ        = 25_175_000;  // pixel clock in Hz
localparam VISIBLE_WIDTH     = 640;         // horizontal active pixels
localparam VISIBLE_HEIGHT    = 480;         // vertical active lines
localparam H_FRONT_PORCH     = 16;          // H pre-sync (front porch) pixels
localparam H_SYNC_PULSE      = 96;          // H sync pulse pixels
localparam H_BACK_PORCH      = 48;          // H post-sync (back porch) pixels
localparam V_FRONT_PORCH     = 10;          // V pre-sync (front porch) lines
localparam V_SYNC_PULSE      = 2;           // V sync pulse lines
localparam V_BACK_PORCH      = 33;          // V post-sync (back porch) lines
localparam H_SYNC_POLARITY   = 1'b0;        // H sync pulse active level
localparam V_SYNC_POLARITY   = 1'b0;        // V sync pulse active level

`elsif    MODE_720x400
// VGA mode 720x400 @ 70Hz (pixel clock 28.322Mhz)
localparam PIXEL_FREQ        = 28_322_000;  // pixel clock in Hz
localparam VISIBLE_WIDTH     = 720;         // horizontal active pixels
localparam VISIBLE_HEIGHT    = 400;         // vertical active lines
localparam H_FRONT_PORCH     = 18;          // H pre-sync (front porch) pixels
localparam H_SYNC_PULSE      = 108;         // H sync pulse pixels
localparam H_BACK_PORCH      = 54;          // H post-sync (back porch) pixels
localparam V_FRONT_PORCH     = 12;          // V pre-sync (front porch) lines
localparam V_SYNC_PULSE      = 2;           // V sync pulse lines
localparam V_BACK_PORCH      = 35;          // V post-sync (back porch) lines
localparam H_SYNC_POLARITY   = 1'b0;        // H sync pulse active level
localparam V_SYNC_POLARITY   = 1'b1;        // V sync pulse active level

`elsif    MODE_848x480
// VGA mode 848x480 @ 60Hz (pixel clock 33.750Mhz)
localparam PIXEL_FREQ        = 33_750_000;  // pixel clock in Hz
localparam VISIBLE_WIDTH     = 848;         // horizontal active pixels
localparam VISIBLE_HEIGHT    = 480;         // vertical active lines
localparam H_FRONT_PORCH     = 16;          // H pre-sync (front porch) pixels
localparam H_SYNC_PULSE      = 112;         // H sync pulse pixels
localparam H_BACK_PORCH      = 112;         // H post-sync (back porch) pixels
localparam V_FRONT_PORCH     = 6;           // V pre-sync (front porch) lines
localparam V_SYNC_PULSE      = 8;           // V sync pulse lines
localparam V_BACK_PORCH      = 23;          // V post-sync (back porch) lines
localparam H_SYNC_POLARITY   = 1'b1;        // H sync pulse active level
localparam V_SYNC_POLARITY   = 1'b1;        // V sync pulse active level

`elsif    MODE_800x600
// VGA mode 800x600 @ 60Hz (pixel clock 40.000Mhz)
localparam PIXEL_FREQ        = 40_000_000;  // pixel clock in Hz
localparam VISIBLE_WIDTH     = 800;         // horizontal active pixels
localparam VISIBLE_HEIGHT    = 600;         // vertical active lines
localparam H_FRONT_PORCH     = 40;          // H pre-sync (front porch) pixels
localparam H_SYNC_PULSE      = 128;         // H sync pulse pixels
localparam H_BACK_PORCH      = 88;          // H post-sync (back porch) pixels
localparam V_FRONT_PORCH     = 1;           // V pre-sync (front porch) lines
localparam V_SYNC_PULSE      = 4;           // V sync pulse lines
localparam V_BACK_PORCH      = 23;          // V post-sync (back porch) lines
localparam H_SYNC_POLARITY   = 1'b1;        // H sync pulse active level
localparam V_SYNC_POLARITY   = 1'b1;        // V sync pulse active level

`elsif    MODE_1024x768
// VGA mode 1024x768 @ 60Hz (pixel clock 65.000Mhz)
localparam PIXEL_FREQ        = 65_000_000;  // pixel clock in Hz
localparam VISIBLE_WIDTH     = 1024;        // horizontal active pixels
localparam VISIBLE_HEIGHT    = 768;         // vertical active lines
localparam H_FRONT_PORCH     = 24;          // H pre-sync (front porch) pixels
localparam H_SYNC_PULSE      = 136;         // H sync pulse pixels
localparam H_BACK_PORCH      = 160;         // H post-sync (back porch) pixels
localparam V_FRONT_PORCH     = 3;           // V pre-sync (front porch) lines
localparam V_SYNC_PULSE      = 6;           // V sync pulse lines
localparam V_BACK_PORCH      = 29;          // V post-sync (back porch) lines
localparam H_SYNC_POLARITY   = 1'b0;        // H sync pulse active level
localparam V_SYNC_POLARITY   = 1'b0;        // V sync pulse active level

`elsif    MODE_1280x720
// VGA mode 1280x720 @ 60Hz (pixel clock 74.250Mhz)
localparam PIXEL_FREQ        = 74_250_000;  // pixel clock in Hz
localparam VISIBLE_WIDTH     = 1280;        // horizontal active pixels
localparam VISIBLE_HEIGHT    = 720;         // vertical active lines
localparam H_FRONT_PORCH     = 110;         // H pre-sync (front porch) pixels
localparam H_SYNC_PULSE      = 40;          // H sync pulse pixels
localparam H_BACK_PORCH      = 220;         // H post-sync (back porch) pixels
localparam V_FRONT_PORCH     = 5;           // V pre-sync (front porch) lines
localparam V_SYNC_PULSE      = 5;           // V sync pulse lines
localparam V_BACK_PORCH      = 20;          // V post-sync (back porch) lines
localparam H_SYNC_POLARITY   = 1'b1;        // H sync pulse active level
localparam V_SYNC_POLARITY   = 1'b1;        // V sync pulse active level
`endif

// calculated video mode parametereters
localparam TOTAL_WIDTH       = H_FRONT_PORCH + H_SYNC_PULSE + H_BACK_PORCH + VISIBLE_WIDTH;
localparam TOTAL_HEIGHT      = V_FRONT_PORCH + V_SYNC_PULSE + V_BACK_PORCH + VISIBLE_HEIGHT;
localparam OFFSCREEN_WIDTH   = TOTAL_WIDTH - VISIBLE_WIDTH;
localparam OFFSCREEN_HEIGHT  = TOTAL_HEIGHT - VISIBLE_HEIGHT;

// character font related constants
localparam FONT_WIDTH        = 8;                               // 8 pixels wide character tiles
localparam FONT_HEIGHT       = 16;                              // up to 16 pixels high character tiles
localparam CHARS_WIDE        = (VISIBLE_WIDTH/FONT_WIDTH);      // default tiled mode width
localparam CHARS_HIGH        = (VISIBLE_HEIGHT/FONT_HEIGHT);    // default tiled mode height

// symbolic Xosera bus signals (to be a bit more clear)
localparam RnW_WRITE         = 1'b0;
localparam RnW_READ          = 1'b1;
localparam cs_ENABLED        = 1'b0;
localparam cs_DISABLED       = 1'b1;

`ifdef ICE40UP5K    // iCE40UltraPlus5K specific
// Lattice/SiliconBlue PLL "magic numbers" to derive pixel clock from 12Mhz oscillator (from "icepll" utility)
`ifdef    MODE_640x400  // 25.175 MHz (requested), 25.125 MHz (achieved)
localparam PLL_DIVR    =    4'b0000;        // DIVR =  0
localparam PLL_DIVF    =    7'b1000010;     // DIVF = 66
localparam PLL_DIVQ    =    3'b101;         // DIVQ =  5
`elsif    MODE_640x480  // 25.175 MHz (requested), 25.125 MHz (achieved)
localparam PLL_DIVR    =    4'b0000;        // DIVR =  0
localparam PLL_DIVF    =    7'b1000010;     // DIVF = 66
localparam PLL_DIVQ    =    3'b101;         // DIVQ =  5
`elsif    MODE_720x400  // 28.322 MHz (requested), 28.500 MHz (achieved)
localparam PLL_DIVR    =    4'b0000;        // DIVR =  0
localparam PLL_DIVF    =    7'b1001011;     // DIVF = 75
localparam PLL_DIVQ    =    3'b101;         // DIVQ =  5
`elsif    MODE_848x480  // 33.750 MHz (requested), 33.750 MHz (achieved)
localparam PLL_DIVR    =    4'b0000;        // DIVR =  0
localparam PLL_DIVF    =    7'b0101100;     // DIVF = 44
localparam PLL_DIVQ    =    3'b100;         // DIVQ =  4
`elsif    MODE_800x600  // 40.000 MHz (requested), 39.750 MHz (achieved) [tight timing]
localparam PLL_DIVR    =    4'b0000;        // DIVR =  0
localparam PLL_DIVF    =    7'b0110100;     // DIVF = 52
localparam PLL_DIVQ    =    3'b100;         // DIVQ =  4
`elsif MODE_1024x768    // 65.000 MHz (requested), 65.250 MHz (achieved) [fails timing]
localparam PLL_DIVR    =    4'b0000;        // DIVR =  0
localparam PLL_DIVF    =    7'b1010110;     // DIVF = 86
localparam PLL_DIVQ    =    3'b100;         // DIVQ =  4
`elsif MODE_1280x720    // 74.176 MHz (requested), 73.500 MHz (achieved) [fails timing]
localparam PLL_DIVR    =    4'b0000;        // DIVR =  0
localparam PLL_DIVF    =    7'b0110000;     // DIVF = 48
localparam PLL_DIVQ    =    3'b011;         // DIVQ =  3
`endif
`endif

//`define TESTPATTERN     // init with "test pattern" instead of clear VRAM

endpackage
`endif
