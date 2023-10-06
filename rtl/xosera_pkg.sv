

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

/* verilator lint_off UNUSED */

//=================================
//                               //
// Xosera configuration options  //
//                               //
//===============================//

// debug options that can be enabled (remove comment)
//
//`define USE_HEXFONT                     // use hex font instead of default fonts
//`define BUS_DEBUG_SIGNALS               // use audio outputs for debug (CS strobe etc.)

`ifndef SYNTHESIS
//`define NO_TESTPATTERN                     // don't init VRAM with test pattern and fonts in simulation
`endif

// features that can be optionally disabled (comment out to disable)
//
// set by Makefile: `define EN_PF_B                         // enable PF B (2nd overlay playfield)
// set by Makefile: `define EN_AUDIO                4       // number of channels 2/4
//`undef EN_PF_B
//`undef EN_AUDIO
`define EN_BLIT                         // enable blit unit
`define EN_POINTER                      // enable pointer sprite
`define EN_PIXEL_ADDR                   // pixel coordinate address generation
`define EN_TIMER_INTR                   // enable timer interrupt
`ifdef EN_PF_B
  `define EN_PF_B_BLND                    // enable pf B blending (otherwise overlay only)
  `define EN_BLEND_FULL                   // use full precision blending w/o FMAC (ignored with iCE40UP5K)
`endif
`define EN_COPP                         // enable copper
`ifdef EN_COPP
  `define EN_COPP_HWAITEOL                   // enable copper HPOS stop at EOL
  `ifdef EN_BLIT
    `define EN_COPP_VBLITWAIT              // VPOS #$XXX > $3FF waits for blitter
  `endif
  `define EN_COPPER_INIT                // enable copper init program at reset (optional)
`endif
//`define EN_UART                         // enable USB UART
`ifdef EN_UART
//`define EN_UART_TX                      // TX only UART (no RX if EN_UART defined)
`endif

`define VERSION 0_40                    // Xosera BCD version code (x.xx)

`ifndef GITCLEAN
`define GITCLEAN 0                      // unknown Git state (assumed dirty)
`endif
`ifndef GITHASH
`define GITHASH 00000000                // unknown Git hash (not using Git)
`endif
`ifndef BUILDDATE
`define BUILDDATE 00000000              // unknown build date
`endif

// "brief" package name (as Yosys doesn't support wildcard imports so lots of "xv::")
package xv;
`timescale 1ns/1ps                  // mandatory to shut up Icarus Verilog

`ifdef FPGA_CONFIG_NUM                 // FPGA boot config number (0-3)
localparam FPGA_CONFIG_NUM = `FPGA_CONFIG_NUM;
`else
localparam FPGA_CONFIG_NUM = 0;
`endif

`ifdef EN_AUDIO
localparam AUDIO_NCHAN  = `EN_AUDIO;    // set parameter for # audio channels
`else
localparam AUDIO_NCHAN  = 0;
`endif

localparam UART_BPS     = 230400;       // USB UART baud rate

// Xosera memory address bit widths
localparam VRAM_W   = 16;               // 64K words VRAM
localparam TILE_W   = 13;               // 4K words tile mem (and bit for extra 1K words)
localparam TILE2_W  = 10;               // 1K words extra tile mem
localparam COPP_W   = 11;               // 1K words copper mem (and bit for extra 512 words)
localparam COPP2_W  = 9;                // 512 words extra copper mem
localparam COLOR_W  = 8;                // 256 words color table mem (per playfield)
localparam POINTER_W= 8;                // 256 words pointer mem (32x32 4-bpp)
localparam AUDIO_W  = 8;                // 256 words audio parameter mem
localparam CHAN_W   = $clog2(AUDIO_NCHAN); // bits needed for AUDIO_NCHAN

// Xosera directly addressable registers (16 x 16-bit words [high/low byte])
typedef enum logic [3:0] {
    // register 16-bit read/write
    XM_SYS_CTRL     = 4'h0,             // (R /W+) status/option flags, init PIXEL_X/Y options, VRAM write masking
    XM_INT_CTRL     = 4'h1,             // (R /W+) interrupt status/control
    XM_TIMER        = 4'h2,             // (R /W+) read 1/10th millisecond timer, write 8-bit interval timer count
    XM_RD_XADDR     = 4'h3,             // (R /W+) XR register/address for XM_XDATA read access
    XM_WR_XADDR     = 4'h4,             // (R /W ) XR register/address for XM_XDATA write access
    XM_XDATA        = 4'h5,             // (R /W+) read/write XR register/memory at XM_RD_XADDR/XM_WR_XADDR
    XM_RD_INCR      = 4'h6,             // (R /W ) increment value for XM_RD_ADDR read from XM_DATA/XM_DATA_2
    XM_RD_ADDR      = 4'h7,             // (R /W+) VRAM address for reading from VRAM when XM_DATA/XM_DATA_2 is read
    XM_WR_INCR      = 4'h8,             // (R /W ) increment value for XM_WR_ADDR on write to XM_DATA/XM_DATA_2
    XM_WR_ADDR      = 4'h9,             // (R /W ) VRAM address for writing to VRAM when XM_DATA/XM_DATA_2 is written
    XM_DATA         = 4'hA,             // (R+/W+) read/write VRAM word at XM_RD_ADDR/XM_WR_ADDR & add XM_RD_INCR/XM_WR_INCR
    XM_DATA_2       = 4'hB,             // (R+/W+) 2nd XM_DATA(to allow for 32-bit read/write access)
    XM_PIXEL_X      = 4'hC,             // (- /W+) PIXEL_X (or PIXEL_BASE w/SYS_CTRL[15:8] write) pixel coord (sets WR_ADDR SYS_CTRL wrmask)
    XM_PIXEL_Y      = 4'hD,             // (- /W+) PIXEL_Y (or PIXEL_WIDTH w/SYS_CTRL[15:8] write) pixel coord (sets WR_ADDR SYS_CTRL wrmask)
    XM_UART         = 4'hE,             // (R+/W+) optional debug UART
    XM_FEATURE      = 4'hF              // (R /- ) feature info
} xm_register_t;

// NOTE: Any write to SYS_CTRL[15:8] will also set PIXEL_BASE and PIXEL_WIDTH with PIXEL_X/Y (as well as mask options)
typedef enum {
    SYS_CTRL_MEM_WAIT_B  = 15,          // memory read/write operation active (with contended memory)
    SYS_CTRL_BLIT_FULL_B = 14,          // blitter queue is full, do not write new operation to blitter registers
    SYS_CTRL_BLIT_BUSY_B = 13,          // blitter is busy (not done) performing an operation
    SYS_CTRL_UNUSED_12_B = 12,          // unused (reads 0)
    SYS_CTRL_HBLANK_B    = 11,          // video signal is in horizontal blank period
    SYS_CTRL_VBLANK_B    = 10,          // video signal is in vertical blank period
    SYS_CTRL_PIX_NO_MASK = 9,           // PIXEL_X/Y won't set WR_MASK (low two bits of PIXEL_X ignored)
    SYS_CTRL_PIX_8B_MASK = 8,           // PIXEL_X/Y 8-bit pixel mask for WR_MASK
    SYS_CTRL_MASK_B      = 0,           // leftmost bit of 4 bit nibble mask
    SYS_CTRL_MASK_W      = 4            // width  of 4 bit nibble mask
} xm_sys_ctrl_t;

typedef enum integer {
    RECONFIG        = 15,               // reset and reconfig
    BLIT_MASK_INTR  = 14,               // blitter ready mask
    TIMER_MASK_INTR = 13,               // timer interval mask
    VIDEO_MASK_INTR = 12,               // v-blank or copper mask
    AUD3_MASK_INTR  = 11,               // audio 3 ready mask
    AUD2_MASK_INTR  = 10,               // audio 2 ready mask
    AUD1_MASK_INTR  = 9,                // audio 1 ready mask
    AUD0_MASK_INTR  = 8,                // audio 0 ready mask
    UNUSED7_INTR    = 7,
    BLIT_INTR       = 6,                // blitter ready
    TIMER_INTR      = 5,                // timer interval
    VIDEO_INTR      = 4,                // v-blank or copper
    AUD3_INTR       = 3,                // audio 3 ready
    AUD2_INTR       = 2,                // audio 2 ready
    AUD1_INTR       = 1,                // audio 1 ready
    AUD0_INTR       = 0                 // audio 0 ready
} intr_bit_t;

typedef enum integer {
    FEATURE_CONFIG  = 12,
    FEATURE_AUDCHAN = 8,
    FEATURE_UART    = 7,
    FEATURE_PF_B    = 6,
    FEATURE_BLIT    = 5,
    FEATURE_COPP    = 4,
    FEATURE_MONRES  = 0
} feature_bit_t;

// XR register / memory regions
typedef enum logic [15:0] {
    // XR Register Regions
    XR_CONFIG_REGS      = 16'h0000,     // 0x0000-0x000F 16 config/video/copper registers
    XR_PA_REGS          = 16'h0010,     // 0x0010-0x0017 8 playfield A video registers
    XR_PB_REGS          = 16'h0018,     // 0x0018-0x001F 8 playfield B video registers
    XR_AUDIO_REGS       = 16'h0020,     // 0x0020-0x002F 16 audio playback registers
    XR_BLIT_REGS        = 16'h0040,     // 0x0040-0x004B 12 polygon blit registers
    // XR Memory Regions
    XR_TILE_ADDR        = 16'h4000,     // 0x4000-0x53FF 5K 16-bit words of tile memory
    XR_COLOR_ADDR       = 16'h8000,     // 0x8000-0x81FF 256 16-bit 0xXRGB color lookup playfield A & B
    XR_COPPER_ADDR      = 16'hC000      // 0xC000-0xC5FF 1.5K 16-bit words copper memory
} xr_region_t;

// XR read/write registers
typedef enum logic [6:0] {
    // Video Config / Copper XR Registers
    XR_VID_CTRL     = 7'h00,            // (R /W) border color index/color swap
    XR_COPP_CTRL    = 7'h01,            // (R /W) display synchronized coprocessor control
    XR_AUD_CTRL     = 7'h02,            // (R /W) audio channel control
    XR_SCANLINE     = 7'h03,            // (R /W) read scanline (incl. offscreen), write signal video interrupt
    XR_VID_LEFT     = 7'h04,            // (R /W) left edge of active display window (typically 0)
    XR_VID_RIGHT    = 7'h05,            // (R /W) right edge of active display window +1 (typically 640 or 848)
    XR_POINTER_H    = 7'h06,            // (- /W) pointer sprite H pos (full display h_pos -6)
    XR_POINTER_V    = 7'h07,            // (- /W) pointer sprite V pos (full display v_pos)
    XR_UNUSED_08    = 7'h08,            // (- /-) unused XR 08
    XR_UNUSED_09    = 7'h09,            // (- /-) unused XR 09
    XR_UNUSED_0A    = 7'h0A,            // (- /-) unused XR 0A
    XR_UNUSED_0B    = 7'h0B,            // (- /-) unused XR 0B
    XR_UNUSED_0C    = 7'h0C,            // (- /-) unused XR 0C
    XR_UNUSED_0D    = 7'h0D,            // (- /-) unused XR 0D
    XR_UNUSED_0E    = 7'h0E,            // (- /-) unused XR 0E
    XR_UNUSED_0F    = 7'h0F,            // (- /-) unused XR 0F
    // Playfield A Control XR Registers
    XR_PA_GFX_CTRL  = 7'h10,            // (R /W) playfield A graphics control
    XR_PA_TILE_CTRL = 7'h11,            // (R /W) playfield A tile control
    XR_PA_DISP_ADDR = 7'h12,            // (R /W) playfield A display VRAM start address
    XR_PA_LINE_LEN  = 7'h13,            // (R /W) playfield A display line width in words
    XR_PA_HV_FSCALE = 7'h14,            // (R /W) playfield A horizontal and vertical fractional scale
    XR_PA_H_SCROLL  = 7'h15,            // (R /W) playfield A horizontal fine scroll
    XR_PA_V_SCROLL  = 7'h16,            // (R /W) playfield A vertical repeat/tile scroll
    XR_PA_LINE_ADDR = 7'h17,            // (R /W) playfield A scanline start address (loaded at start of line)
    // Playfield B Control XR Registers
    XR_PB_GFX_CTRL  = 7'h18,            // (R /W) playfield B graphics control
    XR_PB_TILE_CTRL = 7'h19,            // (R /W) playfield B tile control
    XR_PB_DISP_ADDR = 7'h1A,            // (R /W) playfield B display VRAM start address
    XR_PB_LINE_LEN  = 7'h1B,            // (R /W) playfield B display line width in words
    XR_PB_HV_FSCALE = 7'h1C,            // (R /W) playfield B horizontal and vertical fractional scale
    XR_PB_H_SCROLL  = 7'h1D,            // (R /W) playfield B horizontal fine scroll
    XR_PB_V_SCROLL  = 7'h1E,            // (R /W) playfield B vertical repeat/tile scroll
    XR_PB_LINE_ADDR = 7'h1F,            // (R /W) playfield B scanline start address (loaded at start of line)
    // Audio
    XR_AUD0_VOL     = 7'h20,            // (WO) left volume [15:8] / right volume [7:0] (0x80 is 1.0)
    XR_AUD0_PERIOD  = 7'h21,            // (WO) sample period in pixel clocks, high bit RESTART flag
    XR_AUD0_LENGTH  = 7'h22,            // (WO) sample word length-1, high bit TILE mem flag
    XR_AUD0_START   = 7'h23,            // (WO) sample start address (VRAM or TILE mem as set in LENGTH)
    XR_AUD1_VOL     = 7'h24,            // (WO) left volume [15:8] / right volume [7:0] (0x80 is 1.0)
    XR_AUD1_PERIOD  = 7'h25,            // (WO) sample period in pixel clocks, high bit RESTART flag
    XR_AUD1_LENGTH  = 7'h26,            // (WO) sample word length-1, high bit TILE mem flag
    XR_AUD1_START   = 7'h27,            // (WO) sample start address (VRAM or TILE mem as set in LENGTH)
    XR_AUD2_VOL     = 7'h28,            // (WO) left volume [15:8] / right volume [7:0] (0x80 is 1.0)
    XR_AUD2_PERIOD  = 7'h29,            // (WO) sample period in pixel clocks, high bit RESTART flag
    XR_AUD2_LENGTH  = 7'h2A,            // (WO) sample word length-1, high bit TILE mem flag
    XR_AUD2_START   = 7'h2B,            // (WO) sample start address (VRAM or TILE mem as set in LENGTH)
    XR_AUD3_VOL     = 7'h2C,            // (WO) left volume [15:8] / right volume [7:0] (0x80 is 1.0)
    XR_AUD3_PERIOD  = 7'h2D,            // (WO) sample period in pixel clocks, high bit RESTART flag
    XR_AUD3_LENGTH  = 7'h2E,            // (WO) sample word length-1, high bit TILE mem flag
    XR_AUD3_START   = 7'h2F,            // (WO) sample start address (VRAM or TILE mem as set in LENGTH)
    // Blitter Registers
    XR_BLIT_CTRL    = 7'h40,            // (WO) blit control ([15:8]=transp value, [5]=8 bpp, [4]=transp on, [0]=S constant)
    XR_BLIT_ANDC    = 7'h41,            // (WO) blit AND-COMPLEMENT constant value
    XR_BLIT_XOR     = 7'h42,            // (WO) blit XOR constant value
    XR_BLIT_MOD_S   = 7'h43,            // (WO) blit line modulo added to SRC_S
    XR_BLIT_SRC_S   = 7'h44,            // (WO) blit A source VRAM read address / constant value
    XR_BLIT_MOD_D   = 7'h45,            // (WO) blit modulo added to D destination after each line
    XR_BLIT_DST_D   = 7'h46,            // (WO) blit D VRAM destination write address
    XR_BLIT_SHIFT   = 7'h47,            // (WO) blit first and last word nibble masks and nibble right shift (0-3)
    XR_BLIT_LINES   = 7'h48,            // (WO) blit number of lines minus 1, (repeats blit word count after modulo calc)
    XR_BLIT_WORDS   = 7'h49,            // (WO+) blit word count minus 1 per line (write starts blit operation)
    XR_UNUSED_4A    = 7'h4A,            // TODO: unused XR reg
    XR_UNUSED_4B    = 7'h4B,            // TODO: unused XR reg
    XR_UNUSED_4C    = 7'h4C,            // TODO: unused XR reg
    XR_UNUSED_4D    = 7'h4D,            // TODO: unused XR reg
    XR_UNUSED_4E    = 7'h4E,            // TODO: unused XR reg
    XR_UNUSED_4F    = 7'h4F,            // TODO: unused XR reg
    // dummy
    XR_none         = 7'h7F             // dummy reg for simulation
} xr_register_t;

localparam AUD_PER_RESTART_B    = 15;   // bit in AUDn_PERIOD to restart channel
localparam AUD_LEN_TILEMEM_B    = 15;   // bit in AUDn_LENGTH to indicate sample in TILEMEM

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

// Xosera init info stored in last 64 bytes of default copper memory
//
// typedef struct _xosera_info
// {
//     char          description_str[240];       // ASCII description
//     uint16_t      reserved_48[4];             // 8 reserved bytes (and force alignment)
//     unsigned char ver_bcd_major;              // major BCD version
//     unsigned char ver_bcd_minor;              // minor BCD version
//     unsigned char git_modified;               // non-zero if modified from git
//     unsigned char reserved_59;                // reserved byte
//     unsigned char githash[4];
// } xosera_info_t;

localparam XR_COPPER_SIZE       = 16'h0600;   // 1024+512 x 16-bit copper memory words
localparam XV_INFO_WORDS        = 128;        // 128 16-bit words (last 128 words in copper memory)
localparam XV_INFO_ADDR         = (XR_COPPER_ADDR + XR_COPPER_SIZE - XV_INFO_WORDS);

localparam [11:0]   version     = 12'H`VERSION;
localparam [31:0]   builddate   = 32'H`BUILDDATE;         // YYYYMMDD
localparam [8:1]    gitclean    = `GITCLEAN ? "=" : ">";    // prepend '=' if clean or '>' if modified
localparam [31:0]   githash     = 32'H`GITHASH;             // git short hash

localparam [16*8-1:0] hex_str = "FEDCBA9876543210";
/* verilator lint_off LITENDIAN */  // NOTE: This keeps the letters in forward order for humans
localparam [0:89*8-1] info_str = { "Xosera v", "0" + 8'(version[11:8]), ".", "0" + 8'(version[7:4]), "0" + 8'(version[3:0]),
                                " ",
                                hex_str[((builddate[31:28])*8)+:8], hex_str[((builddate[27:24])*8)+:8],
                                hex_str[((builddate[23:20])*8)+:8], hex_str[((builddate[19:16])*8)+:8],
                                hex_str[((builddate[15:12])*8)+:8], hex_str[((builddate[11: 8])*8)+:8],
                                hex_str[((builddate[ 7: 4])*8)+:8], hex_str[((builddate[ 3: 0])*8)+:8],
                                " ", gitclean, "#",
                                hex_str[((githash[31:28])*8)+:8], hex_str[((githash[27:24])*8)+:8],
                                hex_str[((githash[23:20])*8)+:8], hex_str[((githash[19:16])*8)+:8],
                                hex_str[((githash[15:12])*8)+:8], hex_str[((githash[11: 8])*8)+:8],
                                hex_str[((githash[ 7: 4])*8)+:8], hex_str[((githash[ 3: 0])*8)+:8],
`ifdef ICE40UP5K
                                " iCE40UP5K 128KB",
`else
                                " Unknown FPGA   ",
`endif

`ifdef EN_PF_B
                                " PF_B",
`endif
`ifdef EN_PF_B_BLND
                                " BLND",
`endif
`ifdef EN_COPP
                                " COPP",
`endif
`ifdef EN_BLIT
                                " BLIT",
`endif
`ifdef EN_PIXEL_ADDR
                                " PIXA",
`endif
`ifdef EN_POINTER
                                " PNTR",
`endif
`ifdef EN_UART
                                " UART",
`endif
`ifdef EN_AUDIO
                                " AUD", hex_str[((`EN_AUDIO)*8)+:8],
`endif

`ifndef EN_PF_B
                                8'd0,8'd0,8'd0,8'd0,8'd0,
`endif
`ifndef EN_PF_B_BLND
                                8'd0,8'd0,8'd0,8'd0,8'd0,
`endif
`ifndef EN_COPP
                                8'd0,8'd0,8'd0,8'd0,8'd0,
`endif
`ifndef EN_BLIT
                                8'd0,8'd0,8'd0,8'd0,8'd0,
`endif
`ifndef EN_PIXEL_ADDR
                                8'd0,8'd0,8'd0,8'd0,8'd0,
`endif
`ifndef EN_POINTER
                                8'd0,8'd0,8'd0,8'd0,8'd0,
`endif
`ifndef EN_UART
                                8'd0,8'd0,8'd0,8'd0,8'd0,
`endif
`ifndef EN_AUDIO
                                8'd0,8'd0,8'd0,8'd0,8'd0,
`endif
                                8'd0
                                };
/* verilator lint_on LITENDIAN */

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

`ifdef    MODE_640x480
// VGA mode 640x480 @ 60Hz (pixel clock 25.175Mhz)
`define VIDEO_MODE_NAME     "640x480@60"
localparam VIDEO_MODE_NUM    = 0;           // 0 = 640x480@60
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

`elsif    MODE_848x480
// VGA mode 848x480 @ 60Hz (pixel clock 33.750Mhz)
`define VIDEO_MODE_NAME     "848x480@60"
localparam VIDEO_MODE_NUM    = 1;           // 1 = 848x480@60
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

`elsif    MODE_640x400
// VGA mode 640x400 @ 70Hz (pixel clock 25.175Mhz)
`define VIDEO_MODE_NAME     "640x400@70"
localparam VIDEO_MODE_NUM    = 2;           // 2 = 640x400@70
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
`define VIDEO_MODE_NAME     "640x400@85"
localparam VIDEO_MODE_NUM    = 3;           // 3 = 640x400@85
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

`elsif    MODE_640x480_75
// VGA mode 640x480 @ 75Hz (pixel clock 31.500Mhz)
`define VIDEO_MODE_NAME     "640x480@75"
localparam VIDEO_MODE_NUM    = 4;           // 4 = 640x480@75
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
`define VIDEO_MODE_NAME     "640x480@85"
localparam VIDEO_MODE_NUM    = 5;           // 5 = 640x480@85
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
`define VIDEO_MODE_NAME     "720x400@70"
localparam VIDEO_MODE_NUM    = 6;           // 6 = 720x400@70
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

`elsif    MODE_800x600
// VGA mode 800x600 @ 60Hz (pixel clock 40.000Mhz)
`define VIDEO_MODE_NAME     "800x600@60"
localparam VIDEO_MODE_NUM    = 7;           // 7 = 800x600@60
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
`define VIDEO_MODE_NAME     "1024x768@60"
localparam VIDEO_MODE_NUM    = 8;           // 8 = 1024x768@60
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
`define VIDEO_MODE_NAME     "1280x720@60"
localparam VIDEO_MODE_NUM    = 9;           // 9 = 1280x720@60
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
localparam CS_ENABLED        = 1'b0;
localparam CS_DISABLED       = 1'b1;

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

/* verilator lint_on UNUSED */

endpackage

// since $cast() is not supporeted, apparently need this to convert to enum
function automatic xv::xr_register_t xr_reg_to_enum(
        input logic [6:0] r
    );
    begin
        case (r)
            7'h00:      xr_reg_to_enum = xv::XR_VID_CTRL;
            7'h01:      xr_reg_to_enum = xv::XR_COPP_CTRL;
            7'h02:      xr_reg_to_enum = xv::XR_AUD_CTRL;
            7'h03:      xr_reg_to_enum = xv::XR_SCANLINE;
            7'h04:      xr_reg_to_enum = xv::XR_VID_LEFT;
            7'h05:      xr_reg_to_enum = xv::XR_VID_RIGHT;
            7'h06:      xr_reg_to_enum = xv::XR_POINTER_H;
            7'h07:      xr_reg_to_enum = xv::XR_POINTER_V;
            7'h08:      xr_reg_to_enum = xv::XR_UNUSED_08;
            7'h09:      xr_reg_to_enum = xv::XR_UNUSED_09;
            7'h0A:      xr_reg_to_enum = xv::XR_UNUSED_0A;
            7'h0B:      xr_reg_to_enum = xv::XR_UNUSED_0B;
            7'h0C:      xr_reg_to_enum = xv::XR_UNUSED_0C;
            7'h0D:      xr_reg_to_enum = xv::XR_UNUSED_0D;
            7'h0E:      xr_reg_to_enum = xv::XR_UNUSED_0E;
            7'h0F:      xr_reg_to_enum = xv::XR_UNUSED_0F;
            7'h10:      xr_reg_to_enum = xv::XR_PA_GFX_CTRL;
            7'h11:      xr_reg_to_enum = xv::XR_PA_TILE_CTRL;
            7'h12:      xr_reg_to_enum = xv::XR_PA_DISP_ADDR;
            7'h13:      xr_reg_to_enum = xv::XR_PA_LINE_LEN;
            7'h14:      xr_reg_to_enum = xv::XR_PA_HV_FSCALE;
            7'h15:      xr_reg_to_enum = xv::XR_PA_H_SCROLL;
            7'h16:      xr_reg_to_enum = xv::XR_PA_V_SCROLL;
            7'h17:      xr_reg_to_enum = xv::XR_PA_LINE_ADDR;
            7'h18:      xr_reg_to_enum = xv::XR_PB_GFX_CTRL;
            7'h19:      xr_reg_to_enum = xv::XR_PB_TILE_CTRL;
            7'h1A:      xr_reg_to_enum = xv::XR_PB_DISP_ADDR;
            7'h1B:      xr_reg_to_enum = xv::XR_PB_LINE_LEN;
            7'h1C:      xr_reg_to_enum = xv::XR_PB_HV_FSCALE;
            7'h1D:      xr_reg_to_enum = xv::XR_PB_H_SCROLL;
            7'h1E:      xr_reg_to_enum = xv::XR_PB_V_SCROLL;
            7'h1F:      xr_reg_to_enum = xv::XR_PB_LINE_ADDR;
            7'h20:      xr_reg_to_enum = xv::XR_AUD0_VOL;
            7'h21:      xr_reg_to_enum = xv::XR_AUD0_PERIOD;
            7'h22:      xr_reg_to_enum = xv::XR_AUD0_LENGTH;
            7'h23:      xr_reg_to_enum = xv::XR_AUD0_START;
            7'h24:      xr_reg_to_enum = xv::XR_AUD1_VOL;
            7'h25:      xr_reg_to_enum = xv::XR_AUD1_PERIOD;
            7'h26:      xr_reg_to_enum = xv::XR_AUD1_LENGTH;
            7'h27:      xr_reg_to_enum = xv::XR_AUD1_START;
            7'h28:      xr_reg_to_enum = xv::XR_AUD2_VOL;
            7'h29:      xr_reg_to_enum = xv::XR_AUD2_PERIOD;
            7'h2A:      xr_reg_to_enum = xv::XR_AUD2_LENGTH;
            7'h2B:      xr_reg_to_enum = xv::XR_AUD2_START;
            7'h2C:      xr_reg_to_enum = xv::XR_AUD3_VOL;
            7'h2D:      xr_reg_to_enum = xv::XR_AUD3_PERIOD;
            7'h2E:      xr_reg_to_enum = xv::XR_AUD3_LENGTH;
            7'h2F:      xr_reg_to_enum = xv::XR_AUD3_START;
            7'h40:      xr_reg_to_enum = xv::XR_BLIT_CTRL;
            7'h41:      xr_reg_to_enum = xv::XR_BLIT_ANDC;
            7'h42:      xr_reg_to_enum = xv::XR_BLIT_XOR;
            7'h43:      xr_reg_to_enum = xv::XR_BLIT_MOD_S;
            7'h44:      xr_reg_to_enum = xv::XR_BLIT_SRC_S;
            7'h45:      xr_reg_to_enum = xv::XR_BLIT_MOD_D;
            7'h46:      xr_reg_to_enum = xv::XR_BLIT_DST_D;
            7'h47:      xr_reg_to_enum = xv::XR_BLIT_SHIFT;
            7'h48:      xr_reg_to_enum = xv::XR_BLIT_LINES;
            7'h49:      xr_reg_to_enum = xv::XR_BLIT_WORDS;
            7'h4A:      xr_reg_to_enum = xv::XR_UNUSED_4A;
            7'h4B:      xr_reg_to_enum = xv::XR_UNUSED_4B;
            7'h4C:      xr_reg_to_enum = xv::XR_UNUSED_4C;
            7'h4D:      xr_reg_to_enum = xv::XR_UNUSED_4D;
            7'h4E:      xr_reg_to_enum = xv::XR_UNUSED_4E;
            7'h4F:      xr_reg_to_enum = xv::XR_UNUSED_4F;
            default:    xr_reg_to_enum = xv::XR_none;
        endcase
    end
endfunction

// Xosera types (NOTE: in global space, due to iVerilog)
typedef logic  [7:0]                            byte_t;         // byte size (8-bit)
typedef logic signed [7:0]                      sbyte_t;        // byte size (8-bit)
typedef logic [15:0]                            word_t;         // word size (16-bit)
typedef logic signed [15:0]                     sword_t;        // word size (16-bit)
typedef logic [15:0]                            argb_t;         // ARGB color (16-bit)
typedef logic [11:0]                            rgb_t;          // RGB color (12-bit)
typedef logic [6:0]                             intr_t;         // interrupt bits

typedef logic [xv::VRAM_W-1:0]                  addr_t;         // vram or xmem address (16-bit)
typedef logic [xv::TILE_W-1:0]                  tile_addr_t;    // tile address
typedef logic [xv::COPP_W-1:0]                  copp_addr_t;    // copper address
typedef logic [xv::COLOR_W-1:0]                 color_t;        // color look up index
typedef logic [xv::POINTER_W-1:0]               pointer_t;      // pointer image mem addr

typedef logic [$clog2(xv::TOTAL_WIDTH)-1:0]     hres_t;         // horizontal coordinate types
typedef logic [$clog2(xv::TOTAL_HEIGHT)-1:0]    vres_t;         // vertical coordinate types

`endif
