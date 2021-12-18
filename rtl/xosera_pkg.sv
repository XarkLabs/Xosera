

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

/* verilator lint_off UNUSED */

`ifndef GITCLEAN
`define GITCLEAN 0                      // unknown Git state (assumed dirty)
`endif
`ifndef GITHASH
`define GITHASH 00000000                // unknown Git hash (not using Git)
`endif

`define VERSION 0_27                    // BCD version code (x.xx)

//`define USE_HEXFONT                     // use hex font instead of default fonts
//`define NO_TESTPATTERN                  // don't initialize VRAM with test pattern and fonts in simulation
//`define BUS_DEBUG_SIGNALS               // use audio outputs for debug (CS strobe etc.)
//`define NO_CS_BUS_DELAY                 // set this if your 68020+ is "cranky" with Xosera (no CS & data bus cycle delay)


// features that can be optionally disabled
`define ENABLE_LFSR                     // enable XM_REG 0xA as 19-bit LFSR
`define ENABLE_TIMERLATCH               // latch timer low byte when high byte read
`define ENABLE_COPP                     // enable copper
//`define ENABLE_DRAW                     // enable draw unit

// "brief" package name (as Yosys doesn't support wildcard imports so lots of "xv::")
package xv;

// Xosera memory address bit widths
localparam VRAM_W   = 16;               // 64K words VRAM
localparam TILE_W   = 13;               // 4K words tile mem (but 8K address bits, for extra 1KW)
localparam TILE2_W  = 10;               // 1K words extra tile/sprite mem
localparam COPP_W   = 10;               // 1024 32-bit (even/odd) words copper program mem
localparam COLOR_W  = 8;                // 256 words color table mem (per playfield)

// Xosera directly addressable registers (16 x 16-bit words [high/low byte])
typedef enum logic [3:0] {
    // register 16-bit read/write (no side effects)
    XM_XR_ADDR      = 4'h0,             // (R /W+) XR register number/address for XM_XR_DATA read/write access
    XM_XR_DATA      = 4'h1,             // (R /W+) read/write XR register/memory at XM_XR_ADDR (XM_XR_ADDR incr. on write)
    XM_RD_INCR      = 4'h2,             // (R /W ) increment value for XM_RD_ADDR read from XM_DATA/XM_DATA_2
    XM_RD_ADDR      = 4'h3,             // (R /W+) VRAM address for reading from VRAM when XM_DATA/XM_DATA_2 is read
    XM_WR_INCR      = 4'h4,             // (R /W ) increment value for XM_WR_ADDR on write to XM_DATA/XM_DATA_2
    XM_WR_ADDR      = 4'h5,             // (R /W ) VRAM address for writing to VRAM when XM_DATA/XM_DATA_2 is written
    XM_DATA         = 4'h6,             // (R+/W+) read/write VRAM word at XM_RD_ADDR/XM_WR_ADDR & add XM_RD_INCR/XM_WR_INCR
    XM_DATA_2       = 4'h7,             // (R+/W+) 2nd XM_DATA(to allow for 32-bit read/write access)
    XM_SYS_CTRL     = 4'h8,             // (R /W+) busy status, FPGA reconfig, interrupt status/control, write masking
    XM_TIMER        = 4'h9,             // (RO   ) read 1/10th millisecond timer, write interrupt ack [TODO]
`ifdef ENABLE_LFSR
    XM_LFSR         = 4'hA,             // (R /W ) LFSR (random numbers)
`else
    XM_UNUSED_A     = 4'hA,             // (R /W ) unused direct register 0xA [TODO]
`endif
    XM_UNUSED_B     = 4'hB,             // (R /W ) unused direct register 0xB [TODO]
    XM_RW_INCR      = 4'hC,             // (R /W ) XM_RW_ADDR increment value on read/write of XM_RW_DATA/XM_RW_DATA_2
    XM_RW_ADDR      = 4'hD,             // (R /W+) read/write address for VRAM access from XM_RW_DATA/XM_RW_DATA_2
    XM_RW_DATA      = 4'hE,             // (R+/W+) read/write VRAM word at XM_RW_ADDR (and add XM_RW_INCR)
    XM_RW_DATA_2    = 4'hF              // (R+/W+) 2nd XM_RW_DATA(to allow for 32-bit read/write access)
} xm_register_t;

typedef enum {
    SYS_CTRL_MEM_WAIT_B = 7,
    SYS_CTRL_BLIT_BUSY_B = 6,
    SYS_CTRL_BLIT_FULL_B = 5
} xm_sys_ctrl_t;

// XR register / memory regions
typedef enum logic [15:0] {
    // XR Register Regions
    XR_CONFIG_REGS      = 16'h0000,     // 0x0000-0x000F 16 config/video/copper registers
    XR_PA_REGS          = 16'h0010,     // 0x0010-0x0017 8 playfield A video registers
    XR_PB_REGS          = 16'h0018,     // 0x0000-0x001F 8 playfield B video registers
    XR_BLIT_REGS        = 16'h0020,     // 0x0020-0x002F 16 polygon blit registers      // TODO: blit
    XR_DRAW_REGS        = 16'h0030,     // 0x0030-0x003F 16 polygon draw registers      // TODO: draw
    // XR Memory Regions
    XR_COLOR_ADDR        = 16'h8000,     // 0x8000-0x81FF 256 16-bit 0xXRGB color lookup playfield A & B
    XR_TILE_ADDR         = 16'hA000,     // 0xA000-0xB3FF 5K 16-bit words of tile memory
    XR_COPPER_ADDR       = 16'hC000,     // 0xC000-0xC7FF 2K 16-bit words copper program memory
    XR_UNUSED_MEM_E     = 16'hE000      // 0xE000-0xFFFF (unused)
} xr_region_t;

// XR read/write registers/memory regions
typedef enum logic [5:0] {
    // Video Config / Copper XR Registers
    XR_VID_CTRL     = 6'h00,            // (R /W) display control and border color index
    XR_COPP_CTRL    = 6'h01,            // (R /W) display synchronized coprocessor control
    XR_UNUSED_02    = 6'h02,            // (R /W) // TODO: replace
    XR_UNUSED_03    = 6'h03,            // (R /W) // TODO: replace
    XR_UNUSED_04    = 6'h04,            // (R /W) // TODO: replace
    XR_UNUSED_05    = 6'h05,            // (R /W) // TODO: replace
    XR_VID_LEFT     = 6'h06,            // (R /W) left edge of active display window (typically 0)
    XR_VID_RIGHT    = 6'h07,            // (R /W) right edge of active display window (typically 639 or 847)
    XR_SCANLINE     = 6'h08,            // (RO  ) [15] in V blank, [14] in H blank [10:0] V scanline
    XR_UNUSED_09    = 6'h09,            // (RO  )
    XR_VERSION      = 6'h0A,            // (RO  ) optional feature bits [15:12] and BCD version code [11:0]  // TODO: define
    XR_GITHASH_H    = 6'h0B,            // (RO  ) [15:0] high 16-bits of 32-bit Git hash build identifier
    XR_GITHASH_L    = 6'h0C,            // (RO  ) [15:0] low 16-bits of 32-bit Git hash build identifier
    XR_VID_HSIZE    = 6'h0D,            // (RO  ) native pixel width of monitor mode (e.g. 640/848)
    XR_VID_VSIZE    = 6'h0E,            // (RO  ) native pixel height of monitor mode (e.g. 480)
    XR_VID_VFREQ    = 6'h0F,            // (RO  ) update frequency of monitor mode in BCD 1/100th Hz (0x5997 = 59.97 Hz)
    // Playfield A Control XR Registers
    XR_PA_GFX_CTRL  = 6'h10,            // (R /W) playfield A graphics control
    XR_PA_TILE_CTRL = 6'h11,            // (R /W) playfield A tile control
    XR_PA_DISP_ADDR = 6'h12,            // (R /W) playfield A display VRAM start address
    XR_PA_LINE_LEN  = 6'h13,            // (R /W) playfield A display line width in words
    XR_PA_HV_SCROLL = 6'h14,            // (R /W) playfield A horizontal and vertical fine scroll
    XR_PA_LINE_ADDR = 6'h15,            // (R /W) playfield A scanline start address (loaded at start of line)
    XR_PA_UNUSED_16 = 6'h16,            //
    XR_PA_UNUSED_17 = 6'h17,            //
    // Playfield B Control XR Registers
    XR_PB_GFX_CTRL  = 6'h18,            // (R /W) playfield B graphics control
    XR_PB_TILE_CTRL = 6'h19,            // (R /W) playfield B tile control
    XR_PB_DISP_ADDR = 6'h1A,            // (R /W) playfield B display VRAM start address
    XR_PB_LINE_LEN  = 6'h1B,            // (R /W) playfield B display line width in words
    XR_PB_HV_SCROLL = 6'h1C,            // (R /W) playfield B horizontal and vertical fine scroll
    XR_PB_LINE_ADDR = 6'h1D,            // (R /W) playfield B scanline start address (loaded at start of line)
    XR_PB_UNUSED_1E = 6'h1E,            //
    XR_PB_UNUSED_1F = 6'h1F,            //
    // Blitter Registers
    XR_BLIT_CTRL    = 6'h20,            // (R /W) blit control (transparency control, logic op and op input flags)
    XR_BLIT_MOD_A   = 6'h21,            // (R /W) blit line modulo added to SRC_A (XOR if A const)
    XR_BLIT_SRC_A   = 6'h22,            // (R /W) blit A source VRAM read address / constant value
    XR_BLIT_MOD_B   = 6'h23,            // (R /W) blit line modulo added to SRC_B (XOR if B const)
    XR_BLIT_SRC_B   = 6'h24,            // (R /W) blit B AND source VRAM read address / constant value
    XR_BLIT_MOD_C   = 6'h25,            // (R /W) blit line XOR modifier for C_VAL const
    XR_BLIT_VAL_C   = 6'h26,            // (R /W) blit C XOR constant value
    XR_BLIT_MOD_D   = 6'h27,            // (R /W) blit modulo added to D destination after each line
    XR_BLIT_DST_D   = 6'h28,            // (R /W) blit D VRAM destination write address
    XR_BLIT_SHIFT   = 6'h29,            // (R /W) blit first and last word nibble masks and nibble right shift (0-3)
    XR_BLIT_LINES   = 6'h2A,            // (R /W) blit number of lines minus 1, (repeats blit word count after modulo calc)
    XR_BLIT_WORDS   = 6'h2B             // (R /W) blit word count minus 1 per line (write starts blit operation)
} xr_register_t;

typedef enum integer {
    VSYNC_INTR      = 3,
    COPP_INTR       = 2,
    DONE_INTR       = 1,
    AUDIO_INTR      = 0
} intr_bit_t;

typedef enum logic [1:0] {
    BPP_1_ATTR      = 2'b00,
    BPP_4           = 2'b01,
    BPP_8           = 2'b10,
    BPP_XX          = 2'b11             // TODO: maybe RL7 mode?
} bpp_depth_t;

typedef enum {
    TILE_INDEX      = 0,                // rightmost bit for index (8 bit in BPP_1, otherwise 10 bit)
    TILE_ATTR_VREV  = 10,               // mirror tile vertically (not in BPP_1)
    TILE_ATTR_HREV  = 11,               // mirror tile horizontally (not in BPP_1)
    TILE_ATTR_FORE  = 8,                // rightmost bit for forecolor (in BPP_1 only)
    TILE_ATTR_BACK  = 12                // rightmost bit for backcolor (in BPP_1 only)
} tile_index_attribute_bits_t;

`ifdef MODE_640x400                     // 25.175 MHz (requested), 25.125 MHz (achieved)
`elsif MODE_640x400_75                  // 31.500 MHz (requested), 31.500 MHz (achieved)
`elsif MODE_640x480                     // 25.175 MHz (requested), 25.125 MHz (achieved)
`elsif MODE_640x480_75                  // 31.500 MHz (requested), 31.500 MHz (achieved)
`elsif MODE_640x480_85                  // 36.000 MHz (requested), 36.000 MHz (achieved)
`elsif MODE_720x400                     // 28.322 MHz (requested), 28.500 MHz (achieved)
`elsif MODE_848x480                     // 33.750 MHz (requested), 33.750 MHz (achieved)
`elsif MODE_800x600                     // 40.000 MHz (requested), 39.750 MHz (achieved) [tight timing]
`elsif MODE_1024x768                    // 65.000 MHz (requested), 65.250 MHz (achieved) [fails timing]
`elsif MODE_1280x720                    // 74.176 MHz (requested), 73.500 MHz (achieved) [fails timing]
`else
`define MODE_640x480                    // default
`endif

`ifdef    MODE_640x400
// VGA mode 640x400 @ 70Hz (pixel clock 25.175Mhz)
localparam PIXEL_FREQ        = 25_175_000;  // pixel clock in Hz
localparam REFRESH_FREQ      = 16'h7000;    // vertical refresh Hz BCD
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

`elsif    MODE_640x400_85
// VGA mode 640x400 @ 85Hz (pixel clock 31.500Mhz)
localparam PIXEL_FREQ        = 31_500_000;  // pixel clock in Hz
localparam REFRESH_FREQ      = 16'h8500;    // vertical refresh Hz BCD
localparam VISIBLE_WIDTH     = 640;         // horizontal active pixels
localparam VISIBLE_HEIGHT    = 400;         // vertical active lines
localparam H_FRONT_PORCH     = 32;          // H pre-sync (front porch) pixels
localparam H_SYNC_PULSE      = 64;          // H sync pulse pixels
localparam H_BACK_PORCH      = 96;          // H post-sync (back porch) pixels
localparam V_FRONT_PORCH     = 1;           // V pre-sync (front porch) lines
localparam V_SYNC_PULSE      = 3;           // V sync pulse lines
localparam V_BACK_PORCH      = 41;          // V post-sync (back porch) lines
localparam H_SYNC_POLARITY   = 1'b0;        // H sync pulse active level
localparam V_SYNC_POLARITY   = 1'b1;        // V sync pulse active level

`elsif    MODE_640x480
// VGA mode 640x480 @ 60Hz (pixel clock 25.175Mhz)
localparam PIXEL_FREQ        = 25_175_000;  // pixel clock in Hz
localparam REFRESH_FREQ      = 16'h6000;    // vertical refresh Hz BCD
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

`elsif    MODE_640x480_75
// VGA mode 640x480 @ 75Hz (pixel clock 31.500Mhz)
localparam PIXEL_FREQ        = 31_500_000;  // pixel clock in Hz
localparam REFRESH_FREQ      = 16'h7500;    // vertical refresh Hz BCD
localparam VISIBLE_WIDTH     = 640;         // horizontal active pixels
localparam VISIBLE_HEIGHT    = 480;         // vertical active lines
localparam H_FRONT_PORCH     = 16;          // H pre-sync (front porch) pixels
localparam H_SYNC_PULSE      = 64;          // H sync pulse pixels
localparam H_BACK_PORCH      = 120;         // H post-sync (back porch) pixels
localparam V_FRONT_PORCH     = 1;           // V pre-sync (front porch) lines
localparam V_SYNC_PULSE      = 3;           // V sync pulse lines
localparam V_BACK_PORCH      = 16;          // V post-sync (back porch) lines
localparam H_SYNC_POLARITY   = 1'b0;        // H sync pulse active level
localparam V_SYNC_POLARITY   = 1'b0;        // V sync pulse active level

`elsif    MODE_640x480_85
// VGA mode 640x480 @ 85Hz (pixel clock 36.000Mhz)
localparam PIXEL_FREQ        = 36_000_000;  // pixel clock in Hz
localparam REFRESH_FREQ      = 16'h8500;    // vertical refresh Hz BCD
localparam VISIBLE_WIDTH     = 640;         // horizontal active pixels
localparam VISIBLE_HEIGHT    = 480;         // vertical active lines
localparam H_FRONT_PORCH     = 56;          // H pre-sync (front porch) pixels
localparam H_SYNC_PULSE      = 56;          // H sync pulse pixels
localparam H_BACK_PORCH      = 80;          // H post-sync (back porch) pixels
localparam V_FRONT_PORCH     = 1;           // V pre-sync (front porch) lines
localparam V_SYNC_PULSE      = 3;           // V sync pulse lines
localparam V_BACK_PORCH      = 25;          // V post-sync (back porch) lines
localparam H_SYNC_POLARITY   = 1'b0;        // H sync pulse active level
localparam V_SYNC_POLARITY   = 1'b0;        // V sync pulse active level

`elsif    MODE_720x400
// VGA mode 720x400 @ 70Hz (pixel clock 28.322Mhz)
localparam PIXEL_FREQ        = 28_322_000;  // pixel clock in Hz
localparam REFRESH_FREQ      = 16'h7000;    // vertical refresh Hz BCD
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
localparam REFRESH_FREQ      = 16'h6000;    // vertical refresh Hz BCD
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
localparam REFRESH_FREQ      = 16'h6000;    // vertical refresh Hz BCD
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
localparam REFRESH_FREQ      = 16'h6000;    // vertical refresh Hz BCD
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
localparam REFRESH_FREQ      = 16'h6000;    // vertical refresh Hz BCD
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

// tile related constants
localparam TILE_WIDTH        = 8;                               // 8 pixels wide tiles
localparam TILE_HEIGHT       = 16;                              // 8 or 16 pixels high tiles (but can be truncated)
localparam TILES_WIDE        = (VISIBLE_WIDTH/TILE_WIDTH);      // default tiled mode width
localparam TILES_HIGH        = (VISIBLE_HEIGHT/TILE_HEIGHT);    // default tiled mode height

// symbolic Xosera bus signals (to be a bit more clear)
localparam RnW_WRITE         = 1'b0;
localparam RnW_READ          = 1'b1;
localparam cs_ENABLED        = 1'b0;
localparam cs_DISABLED       = 1'b1;

`ifdef ICE40UP5K    // iCE40UltraPlus5K specific
// Lattice/SiliconBlue PLL "magic numbers" to derive pixel clock from 12Mhz oscillator (from "icepll" utility)
`ifdef    MODE_640x400  // 25.175 MHz (requested), 25.125 MHz (achieved)
localparam PCLK_HZ     =    25_125_000;
localparam PLL_DIVR    =    4'b0000;        // DIVR =  0
localparam PLL_DIVF    =    7'b1000010;     // DIVF = 66
localparam PLL_DIVQ    =    3'b101;         // DIVQ =  5
`elsif    MODE_640x400_85 // 31.500 MHz (requested), 31.500 MHz (achieved)
localparam PCLK_HZ     =    31_500_000;
localparam PLL_DIVR    =    4'b0000;        // DIVR =  0
localparam PLL_DIVF    =    7'b1010011;     // DIVF = 83
localparam PLL_DIVQ    =    3'b101;         // DIVQ =  5
`elsif    MODE_640x480  // 25.175 MHz (requested), 25.125 MHz (achieved)
localparam PCLK_HZ     =    25_125_000;
localparam PLL_DIVR    =    4'b0000;        // DIVR =  0
localparam PLL_DIVF    =    7'b1000010;     // DIVF = 66
localparam PLL_DIVQ    =    3'b101;         // DIVQ =  5
`elsif    MODE_640x480_75 // 31.500 MHz (requested), 31.500 MHz (achieved)
localparam PCLK_HZ     =    31_500_000;
localparam PLL_DIVR    =    4'b0000;        // DIVR =  0
localparam PLL_DIVF    =    7'b1010011;     // DIVF = 83
localparam PLL_DIVQ    =    3'b101;         // DIVQ =  5
`elsif    MODE_640x480_85 // 36.000 MHz (requested), 36.000 MHz (achieved)
localparam PCLK_HZ     =    36_000_000;
localparam PLL_DIVR    =    4'b0000;        // DIVR =  0
localparam PLL_DIVF    =    7'b0101111;     // DIVF = 47
localparam PLL_DIVQ    =    3'b100;         // DIVQ =  4
`elsif    MODE_720x400  // 28.322 MHz (requested), 28.500 MHz (achieved)
localparam PCLK_HZ     =    28_500_000;
localparam PLL_DIVR    =    4'b0000;        // DIVR =  0
localparam PLL_DIVF    =    7'b1001011;     // DIVF = 75
localparam PLL_DIVQ    =    3'b101;         // DIVQ =  5
`elsif    MODE_848x480  // 33.750 MHz (requested), 33.750 MHz (achieved)
localparam PCLK_HZ     =    33_750_000;
localparam PLL_DIVR    =    4'b0000;        // DIVR =  0
localparam PLL_DIVF    =    7'b0101100;     // DIVF = 44
localparam PLL_DIVQ    =    3'b100;         // DIVQ =  4
`elsif    MODE_800x600  // 40.000 MHz (requested), 39.750 MHz (achieved) [tight timing]
localparam PCLK_HZ     =    39_750_000;
localparam PLL_DIVR    =    4'b0000;        // DIVR =  0
localparam PLL_DIVF    =    7'b0110100;     // DIVF = 52
localparam PLL_DIVQ    =    3'b100;         // DIVQ =  4
`elsif MODE_1024x768    // 65.000 MHz (requested), 65.250 MHz (achieved) [fails timing]
localparam PCLK_HZ     =    65_250_000;
localparam PLL_DIVR    =    4'b0000;        // DIVR =  0
localparam PLL_DIVF    =    7'b1010110;     // DIVF = 86
localparam PLL_DIVQ    =    3'b100;         // DIVQ =  4
`elsif MODE_1280x720    // 74.176 MHz (requested), 73.500 MHz (achieved) [fails timing]
localparam PCLK_HZ     =    73_500_000;
localparam PLL_DIVR    =    4'b0000;        // DIVR =  0
localparam PLL_DIVF    =    7'b0110000;     // DIVF = 48
localparam PLL_DIVQ    =    3'b011;         // DIVQ =  3
`endif
`else
localparam PCLK_HZ     =    25_175_000;     // standard VGA
`endif

typedef logic [$clog2(TOTAL_WIDTH)-1:0]     hres_t;         // horizontal coordinate types
typedef logic [$clog2(TOTAL_HEIGHT)-1:0]    vres_t;         // vertical coordinate types

typedef logic [$clog2(VISIBLE_WIDTH)-1:0]   hres_vis_t;     // horizontal visible coordinate types
typedef logic [$clog2(VISIBLE_HEIGHT)-1:0]  vres_vis_t;     // vertical visible coordinate types


/* verilator lint_on UNUSED */

endpackage

// Xosera types (NOTE: in global space, due to iVerilog)
typedef logic  [7:0]                            byte_t;         // byte size (8-bit)
typedef logic [15:0]                            word_t;         // word size (16-bit)
typedef logic [31:0]                            long_t;         // long size (32-bit)
typedef logic [15:0]                            argb_t;         // ARGB color (16-bit)
typedef logic [11:0]                            rgb_t;          // RGB color (12-bit)

typedef logic [xv::VRAM_W-1:0]                  addr_t;         // vram or xmem address (16-bit)
typedef logic [xv::TILE_W-1:0]                  tile_addr_t;    // tile address
typedef logic [xv::COPP_W-1:0]                  copp_addr_t;    // copper address
typedef logic [xv::COLOR_W-1:0]                 color_t;        // color look up index

typedef logic [$clog2(xv::TOTAL_WIDTH)-1:0]     hres_t;         // horizontal coordinate types
typedef logic [$clog2(xv::TOTAL_HEIGHT)-1:0]    vres_t;         // vertical coordinate types

typedef logic [$clog2(xv::VISIBLE_WIDTH)-1:0]   hres_vis_t;     // horizontal visible coordinate types
typedef logic [$clog2(xv::VISIBLE_HEIGHT)-1:0]  vres_vis_t;     // vertical visible coordinate types

`endif
