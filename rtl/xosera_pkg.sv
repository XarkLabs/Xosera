`ifndef XOSERA_PKG
`define XOSERA_PKG
package xv;
// video reg_controller registers (16x16-bit words)
typedef enum logic [3:0] {
        // register 16-bit read/write (no side effects)
		XVID_AUX_ADDR,          // reg 0: TODO video data (as set by VID_CTRL)
		XVID_CONST,             // reg 1: TODO CPU data (instead of read from VRAM)
		XVID_RD_ADDR,           // reg 2: address to read from VRAM
		XVID_WR_ADDR,           // reg 3: address to write from VRAM

        // special registers (with side effects), odd byte write triggers effect
		XVID_DATA,              // reg 4: read/write word from/to VRAM RD/WR
		XVID_DATA_2,            // reg 5: read/write word from/to VRAM RD/WR (for 32-bit)
		XVID_AUX_DATA,          // reg 6: aux data (font/audio)
		XVID_COUNT,             // reg 7: TODO blitter "repeat" count/trigger

        // write only, 16-bit
		XVID_RD_INC,            // reg 9: read addr increment value
		XVID_WR_INC,            // reg A: write addr increment value
		XVID_WR_MOD,            // reg C: TODO write modulo width for 2D blit
		XVID_RD_MOD,            // reg B: TODO read modulo width for 2D blit
		XVID_WIDTH,             // reg 8: TODO width for 2D blit
		XVID_BLIT_CTRL,         // reg D: TODO
		XVID_UNUSED_1,          // reg E: TODO
		XVID_UNUSED_2           // reg F: TODO
} register_t;

typedef enum logic [15:0] {
    // AUX access using AUX_ADDR/AUX_DATA
    AUX_VID_W_DISPSTART = 16'h0000,        // display start address
    AUX_VID_W_TILEWIDTH = 16'h0001,        // tile line width (usually WIDTH/8)
    AUX_VID_W_SCROLLXY  = 16'h0002,        // [10:8] H fine scroll, [3:0] V fine scroll
    AUX_VID_W_FONTCTRL  = 16'h0003,        // [9:8] 2KB font bank, [3:0] font height
    AUX_VID_W_GFXCTRL   = 16'h0004,        // [0] h pix double
    AUX_VID_W_UNUSED5   = 16'h0005,
    AUX_VID_W_UNUSED6   = 16'h0006,
    AUX_VID_W_UNUSED7   = 16'h0007
} aux_r_mem_t;

typedef enum logic [15:0] {
    AUX_VID_R_WIDTH     = 16'h0000,        // display resolution width
    AUX_VID_R_HEIGHT    = 16'h0001,        // display resolution height
    AUX_VID_R_FEATURES  = 16'h0002,        // [15] = 1 (test)
    AUX_VID_R_SCANLINE  = 16'h0003,        // [15] V blank, [14:11] zero [10:0] V line
    AUX_FONT            = 16'h4000,        // 0x4000-0x5FFF 8K byte font memory (even byte [15:8] ignored)
    AUX_COLORTBL        = 16'h8000,        // 0x8000-0x80FF 256 word color lookup table (0xXRGB)
    AUX_AUD             = 16'hC000         // 0xC000-0x??? TODO (audio registers)
} aux_w_mem_t;

endpackage
`endif
