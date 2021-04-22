`ifndef XOSERA_PKG
`define XOSERA_PKG

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

endpackage
`endif
