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
 * Xosera cross-platform C register definition header file
 * ------------------------------------------------------------
 */

// See: https://github.com/XarkLabs/Xosera/blob/master/REFERENCE.md

// Xosera Main Registers (XM Registers, directly CPU accessable)
#define XM_xr_addr   0x0        // XR register number/address for XM_XR_DATA read/write access
#define XM_xr_data   0x1        // read/write XR register/memory at XM_XR_ADDR (XM_XR_ADDR incr. on write)
#define XM_rd_incr   0x2        // increment value for XM_RD_ADDR read from XM_DATA/XM_DATA_2
#define XM_rd_addr   0x3        // VRAM address for reading from VRAM when XM_DATA/XM_DATA_2 is read
#define XM_wr_incr   0x4        // increment value for XM_WR_ADDR on write to XM_DATA/XM_DATA_2
#define XM_wr_addr   0x5        // VRAM address for writing to VRAM when XM_DATA/XM_DATA_2 is written
#define XM_data      0x6        // read/write VRAM word at XM_RD_ADDR/XM_WR_ADDR (and add XM_RD_INCR/XM_WR_INCR)
#define XM_data_2    0x7        // 2nd XM_DATA(to allow for 32-bit read/write access)
#define XM_sys_ctrl  0x8        // busy status, FPGA reconfig, interrupt status/control, write masking
#define XM_timer     0x9        // read 1/10th millisecond timer [TODO]
#define XM_unused_a  0xA        // unused direct register 0xA [TODO]
#define XM_unused_b  0xB        // unused direct register 0xB [TODO]
#define XM_rw_incr   0xC        // XM_RW_ADDR increment value on read/write of XM_RW_DATA/XM_RW_DATA_2
#define XM_rw_addr   0xD        // read/write address for VRAM access from XM_RW_DATA/XM_RW_DATA_2
#define XM_rw_data   0xE        // read/write VRAM word at XM_RW_ADDR (and add XM_RW_INCR)
#define XM_rw_data_2 0xF        // 2nd XM_RW_DATA(to allow for 32-bit read/write access)

// XR Extended Register / Region (accessed via XM_XR_ADDR and XM_XR_DATA)

// XR Register Regions
#define XR_CONFIG_REGS   0x0000        // 0x0000-0x000F 16 16-bit config/copper registers
#define XR_VIDEO_PA_REGS 0x0010        // 0x0000-0x0017 8 16-bit playfield A video registers
#define XR_VIDEO_PB_REGS 0x0018        // 0x0000-0x000F 8 16-bit playfield B video registers
#define XR_BLIT_REGS     0x0020        // 0x0000-0x000F 16 16-bit 2D blit registers [TBD]
#define XR_POLYDRAW_REGS 0x0030        // 0x0000-0x000F 16 16-bit line/polygon draw registers [TBD]

// XR Memory Regions
#define XR_COLOR_MEM  0x8000        // 0x8000-0x80FF 256 16-bit word color lookup table (0xXRGB)
#define XR_TILE_MEM   0x9000        // 0x9000-0x9FFF 4K 16-bit words of tile/font memory
#define XR_COPPER_MEM 0xA000        // 0xA000-0xA7FF 2K 16-bit words copper program memory
#define XR_SPRITE_MEM 0xB000        // 0xB000-0xB0FF 256 16-bit word sprite/cursor memory
#define XR_UNUSED_MEM 0xC000        // 0xC000-0xFFFF (currently unused)

// Video Config / Copper XR Registers
#define XR_vid_ctrl   0x00        // display control and border color index
#define XR_vid_top    0x01        // top line of active display window (typically 0)
#define XR_vid_bottom 0x02        // bottom line of active display window (typically 479)
#define XR_vid_left   0x03        // left edge of active display window (typically 0)
#define XR_vid_right  0x04        // right edge of active display window (typically 639 or 847)
#define XR_scanline   0x05        // [15] in V blank, [14] in H blank [10:0] V scanline
#define XR_copp_ctrl  0x06        // display synchronized coprocessor control
#define XR_unused_07  0x07        // unused
#define XR_version    0x08        // Xosera optional feature bits [15:8] and version code [7:0] [TODO]
#define XR_githash_h  0x09        // [15:0] high 16-bits of 32-bit Git hash build identifier
#define XR_githash_l  0x0A        // [15:0] low 16-bits of 32-bit Git hash build identifier
#define XR_vid_hsize  0x0B        // native pixel width of monitor mode (e.g. 640/848)
#define XR_vid_vsize  0x0C        // native pixel height of monitor mode (e.g. 480)
#define XR_vid_vfreq  0x0D        // update frequency of monitor mode in BCD 1/100th Hz (0x5997 = 59.97 Hz)
#define XR_unused_0e  0x0E        // unused
#define XR_unused_0f  0x0F        // unused

// Playfield A Control XR Registers
#define XR_pa_gfx_ctrl  0x10        //  playfield A graphics control
#define XR_pa_tile_ctrl 0x11        //  playfield A tile control
#define XR_pa_disp_addr 0x12        //  playfield A display VRAM start address
#define XR_pa_line_len  0x13        //  playfield A display line width in words
#define XR_pa_hv_scroll 0x14        //  playfield A horizontal and vertical fine scroll
#define XR_pa_line_addr 0x15        //  playfield A scanline start address (loaded at start of line)
#define XR_pa_unused_16 0x16        //
#define XR_pa_unused_17 0x17        //

// Playfield B Control XR Registers
#define XR_pb_gfx_ctrl  0x18        //  playfield B graphics control
#define XR_pb_tile_ctrl 0x19        //  playfield B tile control
#define XR_pb_disp_addr 0x1A        //  playfield B display VRAM start address
#define XR_pb_line_len  0x1B        //  playfield B display line width in words
#define XR_pb_hv_scroll 0x1C        //  playfield B horizontal and vertical fine scroll
#define XR_pb_line_addr 0x1D        //  playfield B scanline start address (loaded at start of line)
#define XR_pb_unused_1e 0x1E        //
#define XR_pb_unused_1f 0x1F        //
