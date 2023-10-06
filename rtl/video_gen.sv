// video_gen.sv
//
// vim: set et ts=4 sw=4
//
// Copyright (c) 2020 Xark - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)
//
// Thanks to the following inspirational and education projects:
//
// Dan "drr" Rodrigues for the amazing icestation-32 project:
//     https://github.com/dan-rodrigues/icestation-32
// Sylvain "tnt" Munaut for many amazing iCE40 projects and streams (e.g., 1920x1080 HDMI):
//     https://github.com/smunaut/ice40-playground
//
// Learning from both of these projects (and others) helped me significantly improve this design
`default_nettype none               // mandatory for Verilog sanity
`timescale 1ns/1ps                  // mandatory to shut up Icarus Verilog

`include "xosera_pkg.sv"

module video_gen (
    // video registers and control
    input  wire logic           vgen_reg_wr_en_i,       // strobe to write internal config register number
    input  wire logic  [5:0]    vgen_reg_num_i,         // internal config register number (for reads)
    input  wire word_t          vgen_reg_data_i,        // data for internal config register
    output      word_t          vgen_reg_data_o,        // register/status data reads
    output      logic           video_intr_o,           // vblank or copper interrupt signal
`ifdef EN_COPP
    // outputs for copper
    output      logic           copp_reg_wr_o,          // COPP_CTRL write strobe
    output      logic           copp_reg_enable_o,      // copper reg enable/disable
    output      hres_t          h_count_o,              // Horizontal video counter
    output      vres_t          v_count_o,              // Vertical video counter
    output      logic           end_of_line_o,
`endif
    // video memories
    output      logic           vram_sel_o,             // vram read select
    output      addr_t          vram_addr_o,            // vram word address out (16x64K)
    input  wire word_t          vram_data_i,            // vram word data in
    output      logic           tilemem_sel_o,          // tile mem read select
    output      tile_addr_t     tilemem_addr_o,         // tile mem word address out (16x5K)
    input  wire word_t          tilemem_data_i,         // tile mem word data in
    // video signal outputs
    output      logic           v_blank_o,              // line blanked (off bottom of visible)
    output      logic           h_blank_o,              // pixel blanked (off left of visible)
    output      color_t         colorA_index_o,         // color palette index output (16x256)
`ifdef EN_PF_B
    output      color_t         colorB_index_o,         // color palette index output (16x256)
`endif
`ifdef EN_POINTER
    output      pointer_t       pointer_addr_o,         // pointer image address (32x32 4-bpp)
    input  wire word_t          pointer_data_i,         // pointer image data
`endif
    output      logic           vsync_o, hsync_o,       // video sync outputs
    output      logic           dv_de_o,                // video active signal (needed for HDMI)
`ifdef EN_AUDIO
    // audio outputs
    output      logic [3:0]     audio_intr_o,           // audio START ready interrupt flags
    output      logic           audio_pdm_l_o,          // audio left channel PDM output
    output      logic           audio_pdm_r_o,          // audio left channel PDM output
`endif
    // standard signals
    input  wire logic           reset_i,                // system reset in
    input  wire logic           clk                     // clock (video pixel clock)
);

// video generation signals
color_t             border_color;
logic               vid_colorswap;                      // COLORMEM A/B swap
hres_t              vid_left;                           // left video border
hres_t              vid_right;                          // right video border
addr_t              line_set_addr;                      // address for on-the-fly addr set

`ifdef EN_POINTER
hres_t              vid_pointer_h;                      // pointer H position
vres_t              vid_pointer_v;                      // pointer V position
logic [3:0]         vid_pointer_col;                    // pointer color select (upper 4-bits of color index)
`endif

// playfield A generation control signals
logic               pa_blank;                           // disable plane A
addr_t              pa_start_addr;                      // display data start address (word address)
word_t              pa_line_len;                        // words per disply line (added to line_addr each line)
color_t             pa_colorbase;                       // colorbase XOR'd with pixel index (e.g. to set upper bits or alter index)
logic  [1:0]        pa_bpp;                             // bpp code (bpp_depth_t)
logic               pa_bitmap;                          // bitmap enable (else text mode)
logic  [5:0]        pa_tile_bank;                       // vram/tilemem tile bank 0-3 (0/1 with 8x16) tilemem, or 2KB/4K
logic               pa_disp_in_tile;                    // display memory 0=vram, 1=tileram
logic               pa_tile_in_vram;                    // tile memory 0=tilemem, 1=vram
logic  [3:0]        pa_tile_height;                     // max height of tile cell
logic  [1:0]        pa_h_repeat;                        // horizontal pixel repeat
logic  [1:0]        pa_v_repeat;                        // vertical pixel repeat
logic  [2:0]        pa_h_frac_repeat;                   // horizontal fractional pixel repeat count
logic  [2:0]        pa_v_frac_repeat;                   // vertical fractional pixel repeat count
logic  [4:0]        pa_fine_hscroll;                    // horizontal fine scroll (8 pixel * 4 for repeat)
logic  [5:0]        pa_fine_vscroll;                    // vertical fine scroll (16 lines * 4 for repeat)
logic               pa_line_start_set;                  // true if pa_line_start changed (register write)
logic               pa_gfx_ctrl_set;                    // true if pa_gfx_ctrl changed (register write)
color_t             pa_color_index;                     // colorbase XOR'd with pixel index (e.g. to set upper bits or alter index)

// video memories
logic               pa_vram_sel;                        // vram read select
addr_t              pa_vram_addr;                       // vram word address out (16x64K)
logic               pa_tile_sel;                        // tile mem read select
tile_addr_t         pa_tile_addr;                       // tile mem word address out (16x5K)

`ifdef EN_PF_B
// playfield B generation control signals
logic               pb_blank;                           // disable plane B
logic [15:0]        pb_start_addr;                      // display data start address (word address)
word_t              pb_line_len;                        // words per disply line (added to line_addr each line)
color_t             pb_colorbase;                       // colorbase XOR'd with pixel index (e.g. to set upper bits or alter index)
logic  [1:0]        pb_bpp;                             // bpp code (bpp_depth_t)
logic               pb_bitmap;                          // bitmap enable (else text mode)
logic  [5:0]        pb_tile_bank;                       // vram/tilemem tile bank 0-3 (0/1 with 8x16) tilemem, or 2KB/4K
logic               pb_disp_in_tile;                    // display memory 0=vram, 1=tileram
logic               pb_tile_in_vram;                    // 0=tilemem, 1=vram
logic  [3:0]        pb_tile_height;                     // max height of tile cell
logic  [1:0]        pb_h_repeat;                        // horizontal pixel repeat
logic  [1:0]        pb_v_repeat;                        // vertical pixel repeat
logic  [2:0]        pb_h_frac_repeat;                   // horizontal fractional pixel repeat
logic  [2:0]        pb_v_frac_repeat;                   // vertical fractional pixel repeat
logic  [4:0]        pb_fine_hscroll;                    // horizontal fine scroll (8 pixel * 4 for repeat)
logic  [5:0]        pb_fine_vscroll;                    // vertical fine scroll (16 lines * 4 for repeat)
logic               pb_line_start_set;                  // true if pa_line_start changed (register write)
logic               pb_gfx_ctrl_set;                    // true if pa_gfx_ctrl changed (register write)
color_t             pb_color_index;                     // colorbase XOR'd with pixel index (e.g. to set upper bits or alter index)

// playfield B video memories
logic               pb_stall;
logic               pb_vram_sel;                        // vram read select
addr_t              pb_vram_addr;                       // vram word address out (16x64K)
logic               pb_tile_sel;                        // tile mem read select
tile_addr_t         pb_tile_addr;                       // tile mem word address out (16x5K)
`endif

localparam H_MEM_BEGIN      = xv::OFFSCREEN_WIDTH-48;   // memory prefetch starts early
localparam H_MEM_END        = xv::TOTAL_WIDTH-8;        // memory fetch can end a bit early

// debug signals
`ifndef SYNTHESIS
/* verilator lint_off UNUSED */
xv::xr_register_t   xr_reg_enum;
always_comb begin
    if (vgen_reg_wr_en_i) begin
        xr_reg_enum = xr_reg_to_enum({ 1'b0, vgen_reg_num_i });
    end else begin
        xr_reg_enum = xv::XR_none;
    end
end
/* verilator lint_off UNUSED */
`endif

// video generation signals
logic           end_of_line;
logic           end_of_frame;
logic           end_of_visible;
logic           h_visible;
logic           v_visible;
hres_t          h_count;
vres_t          v_count;
logic           hsync;
logic           vsync;
logic           dv_de;

assign          hsync_o         = hsync;
assign          vsync_o         = vsync;
assign          dv_de_o         = dv_de;
assign          h_blank_o       = ~h_visible;
assign          v_blank_o       = ~v_visible;
`ifdef EN_COPP
assign          h_count_o       = h_count;
assign          v_count_o       = v_count;
assign          end_of_line_o   = end_of_line;
`endif

video_timing video_timing
(
    // video registers and control
    .h_count_o(h_count),
    .v_count_o(v_count),
    .h_visible_o(h_visible),
    .v_visible_o(v_visible),
    .end_of_line_o(end_of_line),
    .end_of_frame_o(end_of_frame),
    .end_of_visible_o(end_of_visible),
    .vsync_o(vsync),
    .hsync_o(hsync),
    .dv_de_o(dv_de),
    .reset_i(reset_i),
    .clk(clk)
);

`ifdef EN_AUDIO
// audio
logic                                   audio_enable;           // all channel enable
logic                                   audio_dma_vram_req;     // audio DMA request signal
logic                                   audio_dma_tile_req;     // audio DMA request signal
addr_t                                  audio_dma_addr;         // audio DMA address
logic                                   audio_dma_ack;          // audio DMA ack signal
logic                                   audio_dma_cycle;        // audio DMA request signal
logic                                   audio_reg_wr;           // audio reg write enable
logic [7*xv::AUDIO_NCHAN-1:0]           audio_vol_l_nchan;      // channel L volume/pan
logic [7*xv::AUDIO_NCHAN-1:0]           audio_vol_r_nchan;      // channel R volume/pan
logic [15*xv::AUDIO_NCHAN-1:0]          audio_period_nchan;     // channel playback rate
logic [xv::AUDIO_NCHAN-1:0]             audio_restart_nchan;    // channel force restart (period bit [15])
logic [xv::AUDIO_NCHAN-1:0]             audio_reload_nchan;     // channel sample reloaded flag
`endif

`ifdef EN_PF_B
always_comb begin
    pb_stall = (pa_vram_sel & pb_vram_sel) | (pa_tile_sel & pb_tile_sel);

    vram_sel_o      = 1'b0;
    tilemem_sel_o   = 1'b0;
    vram_addr_o     = pa_vram_addr;
    tilemem_addr_o  = pa_tile_addr;

`ifdef EN_AUDIO
    audio_dma_cycle = 1'b0;
`endif

    if (pa_vram_sel) begin
        vram_sel_o  = 1'b1;
        vram_addr_o = pa_vram_addr;
    end else if (pb_vram_sel) begin
        vram_sel_o  = 1'b1;
        vram_addr_o = pb_vram_addr;
`ifdef EN_AUDIO
    end else if (~audio_dma_ack & audio_dma_vram_req) begin
        audio_dma_cycle = 1'b1;
        vram_sel_o      = 1'b1;
        vram_addr_o     = audio_dma_addr;
`endif
    end

    if (pa_tile_sel) begin
        tilemem_sel_o  = 1'b1;
        tilemem_addr_o = pa_tile_addr;
    end else if (pb_tile_sel) begin
        tilemem_sel_o  = 1'b1;
        tilemem_addr_o = pb_tile_addr;
`ifdef EN_AUDIO
    end else if (~audio_dma_ack & audio_dma_tile_req) begin
        audio_dma_cycle = 1'b1;
        tilemem_sel_o   = 1'b1;
        tilemem_addr_o  = xv::TILE_W'(audio_dma_addr);
`endif
    end
end
`else
always_comb begin
    vram_sel_o      = 1'b0;
    tilemem_sel_o   = 1'b0;
    vram_addr_o     = pa_vram_addr;
    tilemem_addr_o  = pa_tile_addr;

`ifdef EN_AUDIO
    audio_dma_cycle = 1'b0;
`endif

    if (pa_vram_sel) begin
        vram_sel_o  = 1'b1;
        vram_addr_o = pa_vram_addr;
`ifdef EN_AUDIO
    end else if (~audio_dma_ack & audio_dma_vram_req) begin
        audio_dma_cycle = 1'b1;
        vram_sel_o      = 1'b1;
        vram_addr_o     = audio_dma_addr;
`endif
    end

    if (pa_tile_sel) begin
        tilemem_sel_o  = 1'b1;
        tilemem_addr_o = pa_tile_addr;
`ifdef EN_AUDIO
    end else if (~audio_dma_ack & audio_dma_tile_req) begin
        audio_dma_cycle = 1'b1;
        tilemem_sel_o   = 1'b1;
        tilemem_addr_o  = xv::TILE_W'(audio_dma_addr);
`endif
    end
end

`endif

`ifdef EN_AUDIO
always_ff @(posedge clk) begin
    // latch audio dma read
    audio_dma_ack       <= 1'b0;
    if (audio_dma_cycle) begin
        audio_dma_ack       <= 1'b1;
    end
end
`endif

video_playfield video_pf_a(
    .stall_i(1'b0),                                 // playfield A never stalls
    .mem_fetch_i(mem_fetch & ~pa_blank),
    .mem_fetch_start_i(mem_fetch_h_start),
    .h_count_i(h_count),
    .end_of_line_i(end_of_line),
    .end_of_frame_i(end_of_frame),
    .border_color_i(border_color ^ pa_colorbase),   // pre-XOR so colorbase doesn't affect border
    .vid_left_i(vid_left),
    .vid_right_i(vid_right),
    .vram_sel_o(pa_vram_sel),
    .vram_addr_o(pa_vram_addr),
    .vram_data_i(vram_data_i),
    .tilemem_sel_o(pa_tile_sel),
    .tilemem_addr_o(pa_tile_addr),
    .tilemem_data_i(tilemem_data_i),
    .pf_blank_i(pa_blank),
    .pf_start_addr_i(pa_start_addr),
    .pf_line_len_i(pa_line_len),
    .pf_colorbase_i(pa_colorbase),
    .pf_bpp_i(pa_bpp),
    .pf_bitmap_i(pa_bitmap),
    .pf_tile_bank_i(pa_tile_bank),
    .pf_disp_in_tile_i(pa_disp_in_tile),
    .pf_tile_in_vram_i(pa_tile_in_vram),
    .pf_tile_height_i(pa_tile_height),
    .pf_h_repeat_i(pa_h_repeat),
    .pf_v_repeat_i(pa_v_repeat),
    .pf_h_frac_repeat_i(pa_h_frac_repeat),
    .pf_v_frac_repeat_i(pa_v_frac_repeat),
    .pf_fine_hscroll_i(pa_fine_hscroll),
    .pf_fine_vscroll_i(pa_fine_vscroll),
    .pf_line_start_set_i(pa_line_start_set),
    .pf_line_start_addr_i(line_set_addr),
    .pf_gfx_ctrl_set_i(pa_gfx_ctrl_set),
    .pf_color_index_o(pa_color_index),
    .reset_i(reset_i),
    .clk(clk)
);

`ifdef EN_PF_B
logic       pb_vram_rd;                         // last cycle was PB vram read flag
logic       pb_vram_rd_save;                    // PB vram read data saved flag
word_t      pb_vram_rd_data;                    // PB vram read data
logic       pb_tilemem_rd;                      // last cycle was PB tilemem read flag
logic       pb_tilemem_rd_save;                 // PB tilemem read data saved flag
word_t      pb_tilemem_rd_data;                 // PB tilemem read data

always_ff @(posedge clk) begin
    // latch vram read data for playfield B
    if (pb_vram_rd & ~pb_vram_rd_save) begin    // if was a vram read and result not already saved
        pb_vram_rd_save <= 1'b1;                // remember vram read saved
        pb_vram_rd_data <= vram_data_i;         // save vram data
    end
    if (~pb_stall) begin                        // if not stalled, clear saved vram data
        pb_vram_rd_save <= 1'b0;
    end

    pb_vram_rd  <= pb_vram_sel;                 // remember if this cycle was reading vram

    // latch tilemem read data for playfield B
    if (pb_tilemem_rd & ~pb_tilemem_rd_save) begin // if was a tilemem read and result not already saved
        pb_tilemem_rd_save <= 1'b1;             // remember tilemem read saved
        pb_tilemem_rd_data <= tilemem_data_i;   // save tilemem data
    end
    if (~pb_stall) begin                        // if not stalled, clear saved tilemem data
        pb_tilemem_rd_save <= 1'b0;
    end

    pb_tilemem_rd  <= pb_tile_sel;              // remember if this cycle was reading tilemem
end

video_playfield video_pf_b(
    .stall_i(pb_stall),
    .mem_fetch_i(mem_fetch & ~pb_blank),
    .mem_fetch_start_i(mem_fetch_h_start),
    .h_count_i(h_count),
    .end_of_line_i(end_of_line),
    .end_of_frame_i(end_of_frame),
    .border_color_i(pb_colorbase),
    .vid_left_i(vid_left),
    .vid_right_i(vid_right),
    .vram_sel_o(pb_vram_sel),
    .vram_addr_o(pb_vram_addr),
    .vram_data_i(pb_vram_rd_save ? pb_vram_rd_data : vram_data_i),
    .tilemem_sel_o(pb_tile_sel),
    .tilemem_addr_o(pb_tile_addr),
    .tilemem_data_i(pb_tilemem_rd_save ? pb_tilemem_rd_data : tilemem_data_i),
    .pf_blank_i(pb_blank),
    .pf_start_addr_i(pb_start_addr),
    .pf_line_len_i(pb_line_len),
    .pf_colorbase_i(pb_colorbase),
    .pf_bpp_i(pb_bpp),
    .pf_bitmap_i(pb_bitmap),
    .pf_tile_bank_i(pb_tile_bank),
    .pf_disp_in_tile_i(pb_disp_in_tile),
    .pf_tile_in_vram_i(pb_tile_in_vram),
    .pf_tile_height_i(pb_tile_height),
    .pf_h_repeat_i(pb_h_repeat),
    .pf_v_repeat_i(pb_v_repeat),
    .pf_h_frac_repeat_i(pb_h_frac_repeat),
    .pf_v_frac_repeat_i(pb_v_frac_repeat),
    .pf_fine_hscroll_i(pb_fine_hscroll),
    .pf_fine_vscroll_i(pb_fine_vscroll),
    .pf_line_start_set_i(pb_line_start_set),
    .pf_line_start_addr_i(line_set_addr),
    .pf_gfx_ctrl_set_i(pb_gfx_ctrl_set),
    .pf_color_index_o(pb_color_index),
    .reset_i(reset_i),
    .clk(clk)
);
`endif

// video config registers read/write
always_ff @(posedge clk) begin
    if (reset_i) begin
        video_intr_o        <= 1'b0;
`ifdef EN_COPPER_INIT
        border_color        <= '0;
`else
        border_color        <= 8'h08;           // w/o copper init, default grey
`endif
        vid_colorswap       <= 1'b0;
        vid_left            <= '0;
`ifdef EN_COPPER_INIT
        vid_right           <= '0;
`else
        vid_right           <= xv::VISIBLE_WIDTH; // w/o copper init, default full width
`endif

`ifdef EN_POINTER
        vid_pointer_h        <= '0;
        vid_pointer_v        <= '0;
        vid_pointer_col      <= '0;
`endif

`ifdef EN_COPPER_INIT
        pa_blank            <= 1'b0;
`else
        pa_blank            <= 1'b1;            // w/o copper init, come out of reset blanked
`endif
        pa_start_addr       <= 16'h0000;
        pa_line_len         <= '0;
        pa_fine_hscroll     <= 5'b0;
        pa_fine_vscroll     <= 6'b0;
        pa_tile_height      <= '0;
        pa_tile_bank        <= 6'b0;
        pa_disp_in_tile     <= 1'b0;
        pa_tile_in_vram     <= 1'b0;
        pa_bitmap           <= 1'b0;
        pa_bpp              <= xv::BPP_1_ATTR;
        pa_colorbase        <= 8'h00;
        pa_h_repeat         <= 2'b0;
        pa_v_repeat         <= 2'b0;
        pa_h_frac_repeat    <= '0;
        pa_v_frac_repeat    <= '0;
        pa_line_start_set   <= 1'b0;            // indicates user line address set
        pa_gfx_ctrl_set     <= 1'b0;

`ifdef EN_PF_B
`ifdef EN_COPPER_INIT
        pb_blank            <= 1'b0;
`else
        pb_blank            <= 1'b1;            // w/o copper init, come out of reset blanked
`endif
        pb_start_addr       <= 16'h0000;
        pb_line_len         <= '0;
        pb_fine_hscroll     <= 5'b0;
        pb_fine_vscroll     <= 6'b0;
        pb_tile_height      <= '0;
        pb_tile_bank        <= 6'b0;
        pb_disp_in_tile     <= 1'b0;
        pb_tile_in_vram     <= 1'b0;
        pb_bitmap           <= 1'b0;
        pb_bpp              <= xv::BPP_1_ATTR;
        pb_colorbase        <= 8'h00;
        pb_h_repeat         <= 2'b0;
        pb_v_repeat         <= 2'b0;
        pb_h_frac_repeat    <= '0;
        pb_v_frac_repeat    <= '0;
        pb_line_start_set   <= 1'b0;            // indicates user line address set
        pb_gfx_ctrl_set     <= 1'b0;
`endif

        line_set_addr       <= 16'h0000;        // user set display addr

`ifdef EN_COPP
        copp_reg_wr_o       <= 1'b0;
        copp_reg_enable_o   <= 1'b1;            // copper starts at init (and will disable itself)
`endif

`ifdef EN_AUDIO
        audio_enable        <= 1'b0;
`endif

`ifdef TESTPATTERN
        vid_right           <= $bits(vid_right)'(xv::VISIBLE_WIDTH);
        pa_line_len         <= xv::TILES_WIDE[15:0];
        pa_tile_height      <= 4'b1111;
        pb_blank            <= 1'b1;
        pb_line_len         <= xv::TILES_WIDE[15:0];
        pb_tile_height      <= 4'b1111;
`endif

    end else begin
        pa_line_start_set   <= 1'b0;            // indicates user line address set
        pa_gfx_ctrl_set     <= 1'b0;

`ifdef EN_PF_B
        pb_line_start_set   <= 1'b0;            // indicates user line address set
        pb_gfx_ctrl_set     <= 1'b0;
`endif

        video_intr_o        <= end_of_visible;

`ifdef EN_COPP
        copp_reg_wr_o       <= 1'b0;
`endif
        // video register write
        if (vgen_reg_wr_en_i) begin
            case (vgen_reg_num_i)
                6'(xv::XR_VID_CTRL): begin
                    vid_colorswap                   <= vgen_reg_data_i[15];
                    border_color                    <= vgen_reg_data_i[7:0];
                end
`ifdef EN_COPP
                6'(xv::XR_COPP_CTRL): begin
                    copp_reg_wr_o                   <= 1'b1;
                    copp_reg_enable_o               <= vgen_reg_data_i[15];
                end
`endif
`ifdef EN_AUDIO
                6'(xv::XR_AUD_CTRL): begin
                    audio_enable        <= vgen_reg_data_i[0];
                end
`endif
                6'(xv::XR_SCANLINE): begin
                end
                6'(xv::XR_VID_LEFT): begin
                    vid_left        <= $bits(vid_left)'(vgen_reg_data_i);
                end
                6'(xv::XR_VID_RIGHT): begin
                    vid_right       <= $bits(vid_right)'(vgen_reg_data_i);
                end
                6'(xv::XR_POINTER_H): begin
`ifdef EN_POINTER
                    vid_pointer_h    <= $bits(vid_pointer_h)'(vgen_reg_data_i);
`endif
                end
                6'(xv::XR_POINTER_V): begin
`ifdef EN_POINTER
                    vid_pointer_v    <= $bits(vid_pointer_v)'(vgen_reg_data_i);
                    vid_pointer_col  <= vgen_reg_data_i[15:12];
`endif
                end
                6'(xv::XR_UNUSED_08): begin
                end
                6'(xv::XR_UNUSED_09): begin
                end
                6'(xv::XR_UNUSED_0A): begin
                end
                6'(xv::XR_UNUSED_0B): begin
                end
                6'(xv::XR_UNUSED_0C): begin
                end
                6'(xv::XR_UNUSED_0D): begin
                end
                6'(xv::XR_UNUSED_0E): begin
                end
                6'(xv::XR_UNUSED_0F): begin
                end
                // playfield A
                6'(xv::XR_PA_GFX_CTRL): begin
                    pa_gfx_ctrl_set <= 1'b1;                // PA _GFX_CTRLchanged strobe
                    pa_colorbase    <= vgen_reg_data_i[15:8];
                    pa_blank        <= vgen_reg_data_i[7];
                    pa_bitmap       <= vgen_reg_data_i[6];
                    pa_bpp          <= vgen_reg_data_i[5:4];
                    pa_h_repeat     <= vgen_reg_data_i[3:2];
                    pa_v_repeat     <= vgen_reg_data_i[1:0];
                end
                6'(xv::XR_PA_TILE_CTRL): begin
                    pa_tile_bank    <= vgen_reg_data_i[15:10];
                    pa_disp_in_tile <= vgen_reg_data_i[9];
                    pa_tile_in_vram <= vgen_reg_data_i[8];
                    pa_tile_height  <= vgen_reg_data_i[3:0];
                end
                6'(xv::XR_PA_DISP_ADDR): begin
                    pa_start_addr   <= vgen_reg_data_i;
                end
                6'(xv::XR_PA_LINE_LEN): begin
                    pa_line_len     <= vgen_reg_data_i;
                end
                6'(xv::XR_PA_HV_FSCALE): begin
                    pa_h_frac_repeat <= vgen_reg_data_i[6:4];
                    pa_v_frac_repeat <= vgen_reg_data_i[2:0];
                end
                6'(xv::XR_PA_H_SCROLL): begin
                    pa_fine_hscroll <= vgen_reg_data_i[4:0];
                end
                6'(xv::XR_PA_V_SCROLL): begin
                    pa_fine_vscroll[1:0] <= vgen_reg_data_i[9:8];
                    pa_fine_vscroll[5:2] <= vgen_reg_data_i[3:0];
                end
                6'(xv::XR_PA_LINE_ADDR): begin
                    pa_line_start_set <= 1'b1;               // PA_LINE_ADDR changed strobe
                    line_set_addr   <= vgen_reg_data_i;
                end
`ifdef EN_PF_B
                // playfield B
                6'(xv::XR_PB_GFX_CTRL): begin
                    pb_gfx_ctrl_set <= 1'b1;                // PB_GFX_CTRL changed strobe
                    pb_colorbase    <= vgen_reg_data_i[15:8];
                    pb_blank        <= vgen_reg_data_i[7];
                    pb_bitmap       <= vgen_reg_data_i[6];
                    pb_bpp          <= vgen_reg_data_i[5:4];
                    pb_h_repeat     <= vgen_reg_data_i[3:2];
                    pb_v_repeat     <= vgen_reg_data_i[1:0];
                end
                6'(xv::XR_PB_TILE_CTRL): begin
                    pb_tile_bank    <= vgen_reg_data_i[15:10];
                    pb_disp_in_tile <= vgen_reg_data_i[9];
                    pb_tile_in_vram <= vgen_reg_data_i[8];
                    pb_tile_height  <= vgen_reg_data_i[3:0];
                end
                6'(xv::XR_PB_DISP_ADDR): begin
                    pb_start_addr   <= vgen_reg_data_i;
                end
                6'(xv::XR_PB_LINE_LEN): begin
                    pb_line_len     <= vgen_reg_data_i;
                end
                6'(xv::XR_PB_HV_FSCALE): begin
                    pb_h_frac_repeat <= vgen_reg_data_i[6:4];
                    pb_v_frac_repeat <= vgen_reg_data_i[2:0];
                end
                6'(xv::XR_PB_H_SCROLL): begin
                    pb_fine_hscroll <= vgen_reg_data_i[4:0];
                end
                6'(xv::XR_PB_V_SCROLL): begin
                    pb_fine_vscroll[1:0] <= vgen_reg_data_i[9:8];
                    pb_fine_vscroll[5:2] <= vgen_reg_data_i[3:0];
                end
                6'(xv::XR_PB_LINE_ADDR): begin
                    pb_line_start_set <= 1'b1;               // PB_LINE_ADDR changed strobe
                    line_set_addr   <= vgen_reg_data_i;
                end
`endif
                default: begin
                end
            endcase
        end
    end
end

word_t  rd_vid_regs;
word_t  rd_pf_regs;

// video config registers read/write
always_ff @(posedge clk) begin
    vgen_reg_data_o  <= vgen_reg_num_i[4] ? rd_pf_regs : rd_vid_regs;
end

// video registers read
always_comb begin
    rd_vid_regs = 16'h0000;

    case (vgen_reg_num_i[2:0])
        3'(xv::XR_VID_CTRL):        rd_vid_regs = { vid_colorswap, 7'b0, border_color};
`ifdef EN_COPP
        3'(xv::XR_COPP_CTRL):       rd_vid_regs = { copp_reg_enable_o, 15'b0 };
`endif
`ifdef EN_AUDIO
        3'(xv::XR_AUD_CTRL):        rd_vid_regs = 16'(audio_enable);
`endif
        3'(xv::XR_SCANLINE):        rd_vid_regs = 16'(v_count);
        3'(xv::XR_VID_LEFT):        rd_vid_regs = 16'(vid_left);
        3'(xv::XR_VID_RIGHT):       rd_vid_regs = 16'(vid_right);
        default:                    ;
    endcase
end

// video registers read
always_comb begin
    rd_pf_regs = 16'h0000;

    case (vgen_reg_num_i[3:0])
        4'(xv::XR_PA_GFX_CTRL):     rd_pf_regs = { pa_colorbase, pa_blank, pa_bitmap, pa_bpp, pa_h_repeat, pa_v_repeat };
        4'(xv::XR_PA_TILE_CTRL):    rd_pf_regs = { pa_tile_bank, pa_disp_in_tile, pa_tile_in_vram, 4'b0, pa_tile_height };
        4'(xv::XR_PA_DISP_ADDR):    rd_pf_regs = pa_start_addr;
        4'(xv::XR_PA_LINE_LEN):     rd_pf_regs = pa_line_len;
        4'(xv::XR_PA_HV_FSCALE):    rd_pf_regs = { 8'h00, 4'(pa_h_frac_repeat), 4'(pa_v_frac_repeat) };
        4'(xv::XR_PA_H_SCROLL):     rd_pf_regs = { 8'h00, 8'(pa_fine_hscroll) };
        4'(xv::XR_PA_V_SCROLL):     rd_pf_regs = { 6'b000000, pa_fine_vscroll[1:0], 4'b0000, pa_fine_vscroll[5:2] };
        4'(xv::XR_PA_LINE_ADDR):    rd_pf_regs = 16'h0000;
`ifdef EN_PF_B
        4'(xv::XR_PB_GFX_CTRL):     rd_pf_regs = { pb_colorbase, pb_blank, pb_bitmap, pb_bpp, pb_h_repeat, pb_v_repeat };
        4'(xv::XR_PB_TILE_CTRL):    rd_pf_regs = { pb_tile_bank, pb_disp_in_tile, pb_tile_in_vram, 4'b0, pb_tile_height };
        4'(xv::XR_PB_DISP_ADDR):    rd_pf_regs = pb_start_addr;
        4'(xv::XR_PB_LINE_LEN):     rd_pf_regs = pb_line_len;
        4'(xv::XR_PB_HV_FSCALE):    rd_pf_regs = { 8'h00, 4'(pb_h_frac_repeat), 4'(pb_v_frac_repeat) };
        4'(xv::XR_PB_H_SCROLL):     rd_pf_regs = { 8'h00, 8'(pb_fine_hscroll) };
        4'(xv::XR_PB_V_SCROLL):     rd_pf_regs = { 6'b000000, pb_fine_vscroll[1:0], 4'b0000, pb_fine_vscroll[5:2] };
        4'(xv::XR_PB_LINE_ADDR):    rd_pf_regs = 16'h0000;
`endif
        default:                    ;
    endcase
end

`ifdef EN_POINTER
// 32x32 4-bpp pointer sprite
localparam          POINTER_SIZE    = 32;   // 32x32 4-bpp
localparam          CURSH_W         = $clog2(POINTER_SIZE);
localparam          CURSV_W         = $clog2(POINTER_SIZE);

word_t              pointer_word;        // current data word (nibble shifting left)
logic [CURSH_W:0]   pointer_h_cnt;       // extra bit for POINTER_WIDTH+3
logic [CURSV_W:0]   pointer_v_cnt;       // extra bit to detect overflow

logic               pointer_h_draw;
assign              pointer_h_draw   = (pointer_h_cnt < POINTER_SIZE+3) ? 1'b1 : 1'b0;    // +3 for memory latency delay
logic               pointer_v_draw;
assign              pointer_v_draw   = (!pointer_v_cnt[CURSV_W]) ? 1'b1 : 1'b0;
logic [3:0]         pointer_color;
assign              pointer_color    = pointer_word[15:12];

assign              pointer_addr_o   = { pointer_v_cnt[4:0], pointer_h_cnt[4:2] };

logic [CURSH_W:0]   vid_pointer_h_next;
assign              vid_pointer_h_next  = pointer_h_draw ? pointer_h_cnt + 1'b1 : pointer_h_cnt;
logic [CURSV_W:0]   vid_pointer_v_next;
assign              vid_pointer_v_next  = pointer_v_draw ? pointer_v_cnt + 1'b1 : pointer_v_cnt;
word_t              pointer_word_next;
assign              pointer_word_next   = { pointer_word[11:0], 4'b000 };  // shift pixel

// pointer sprite
always_ff @(posedge clk) begin
    if (reset_i) begin
        pointer_h_cnt    <= '0;
        pointer_v_cnt    <= '0;
        pointer_word     <= '0;
    end else begin

        pointer_word    <= pointer_word_next;
        pointer_h_cnt   <= vid_pointer_h_next;

        if (vid_pointer_h == h_count) begin
            pointer_h_cnt    <= '0;
            pointer_v_cnt    <= vid_pointer_v_next;

            if (vid_pointer_v == v_count) begin
                pointer_v_cnt    <= '0;
            end
        end

        if (pointer_h_cnt[1:0] == 2'b11) begin
            pointer_word     <= pointer_data_i;
        end

        if (!pointer_h_draw || !pointer_v_draw) begin
            pointer_word     <= '0;
        end
    end
end

`endif

// combinational block for video fetch start and stop
logic           mem_fetch;                   // true when fetching display data
logic           mem_fetch_next;
logic           mem_fetch_h_start;
logic           mem_fetch_h_end;
always_comb     mem_fetch_h_start = ($bits(h_count)'(H_MEM_BEGIN) == h_count);
always_comb     mem_fetch_h_end = ($bits(h_count)'(H_MEM_END) == h_count);
always_comb     mem_fetch_next = (!mem_fetch ? mem_fetch_h_start : !mem_fetch_h_end) && v_visible;

// video pixel generation
always_ff @(posedge clk) begin
    if (reset_i) begin
        colorA_index_o      <= 8'b0;
`ifdef EN_PF_B
        colorB_index_o      <= 8'b0;
`endif
        mem_fetch           <= 1'b0;
    end else begin
        // set output pixel index from pixel shift-out
`ifdef EN_PF_B
        colorA_index_o      <= !vid_colorswap ? pa_color_index : pb_color_index;
        colorB_index_o      <= !vid_colorswap ? pb_color_index : pa_color_index;
`else
        colorA_index_o      <= pa_color_index;
`endif
`ifdef EN_POINTER
        if (pointer_color != 0) begin
            colorA_index_o      <= { vid_pointer_col, pointer_color };
        end
`endif
        mem_fetch           <= mem_fetch_next;
    end
end

// audio generation
`ifdef EN_AUDIO
audio_mixer_slim audio_mixer(
    .audio_enable_i(audio_enable),
    .audio_reg_wr_i(audio_reg_wr),
    .audio_reg_addr_i(vgen_reg_num_i[xv::CHAN_W+2-1:0]),
    .audio_reg_data_i(vgen_reg_data_i),
    .audio_vol_l_nchan_i(audio_vol_l_nchan),
    .audio_vol_r_nchan_i(audio_vol_r_nchan),
    .audio_period_nchan_i(audio_period_nchan),
    .audio_restart_nchan_i(audio_restart_nchan),
    .audio_reload_nchan_o(audio_reload_nchan),
    .audio_dma_vram_req_o(audio_dma_vram_req),
    .audio_dma_tile_req_o(audio_dma_tile_req),
    .audio_dma_ack_i(audio_dma_ack),
    .audio_dma_addr_o(audio_dma_addr),
    .audio_dma_word_i(audio_dma_tile_req ? tilemem_data_i : vram_data_i),

    .pdm_l_o(audio_pdm_l_o),
    .pdm_r_o(audio_pdm_r_o),

    .reset_i(reset_i),
    .clk(clk)
);

// audio channel register writes
always_ff @(posedge clk) begin
    if (reset_i) begin
        audio_reg_wr            <= 1'b0;
        audio_vol_l_nchan       <= '0;
        audio_vol_r_nchan       <= '0;
        audio_period_nchan      <= '0;
        audio_restart_nchan     <= '0;
        audio_intr_o            <= '0;
    end else begin
        audio_reg_wr        <= 1'b0;

        audio_restart_nchan <= '0;
        audio_intr_o        <= audio_intr_o | 4'(audio_reload_nchan);

        for (integer i = 0; i < xv::AUDIO_NCHAN; i = i + 1) begin
            if (vgen_reg_wr_en_i) begin
                case (7'(vgen_reg_num_i))
                    7'(xv::XR_AUD0_VOL+7'(i*4)): begin
                        audio_reg_wr        <= 1'b1;                // write to audio register mem (but not used)
                        { audio_vol_l_nchan[i*7+:7], audio_vol_r_nchan[i*7+:7] }    <= { vgen_reg_data_i[15:9], vgen_reg_data_i[7:1] }; // 7-bit vol 0x80 = 1.0
                    end
                    7'(xv::XR_AUD0_PERIOD+7'(i*4)): begin
                        audio_reg_wr        <= 1'b1;                // write to audio register mem (but not used)
                        { audio_restart_nchan[i], audio_period_nchan[i*15+:15] }    <= vgen_reg_data_i;
                    end
                    7'(xv::XR_AUD0_LENGTH+7'(i*4)): begin
                        audio_reg_wr        <= 1'b1;                // write to audio register mem
                    end
                    7'(xv::XR_AUD0_START+7'(i*4)): begin
                        audio_reg_wr        <= 1'b1;                // write to audio register mem
                        audio_intr_o[i]     <= 1'b0;                // clear channel interrupt/reload flag
                    end
                    default: begin
                    end
                endcase
            end
        end
    end
end
`endif

endmodule

`default_nettype wire               // restore default
