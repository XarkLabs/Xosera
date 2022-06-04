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

module video_gen #(
    parameter EN_VID_PF_B       = 1,
    parameter EN_AUDIO          = 1
)
(
    // video registers and control
    input  wire logic           vgen_reg_wr_en_i,       // strobe to write internal config register number
    input  wire logic  [4:0]    vgen_reg_num_i,         // internal config register number (for reads)
    input  wire word_t          vgen_reg_data_i,        // data for internal config register
    output      word_t          vgen_reg_data_o,        // register/status data reads
    input wire  logic  [3:0]    intr_status_i,          // interrupt pending status
    output      logic  [3:0]    intr_signal_o,          // generate interrupt signal
`ifdef ENABLE_COPP
    // outputs for copper
    output      logic           copp_reg_wr_o,          // COPP_CTRL write strobe
    output      word_t          copp_reg_data_o,        // copper reg data
    output      hres_t          h_count_o,              // Horizontal video counter
    output      vres_t          v_count_o,              // Vertical video counter
`endif
    // video memories
    output      logic           vram_sel_o,             // vram read select
    output      addr_t          vram_addr_o,            // vram word address out (16x64K)
    input  wire word_t          vram_data_i,            // vram word data in
    output      logic           tilemem_sel_o,          // tile mem read select
    output      tile_addr_t     tilemem_addr_o,         // tile mem word address out (16x5K)
    input  wire word_t          tilemem_data_i,         // tile mem word data in
    // video signal outputs
    output      color_t         colorA_index_o,         // color palette index output (16x256)
    output      color_t         colorB_index_o,         // color palette index output (16x256)
    output      logic           vsync_o, hsync_o,       // video sync outputs
    output      logic           dv_de_o,                // video active signal (needed for HDMI)
    // audio outputs
    output      logic           audio_pdm_l_o,          // audio left channel PDM output
    output      logic           audio_pdm_r_o,          // audio left channel PDM output
    // standard signals
    input  wire logic           reset_i,                // system reset in
    input  wire logic           clk                     // clock (video pixel clock)
);

// video generation signals
color_t             border_color;
hres_vis_t          vid_left;
hres_vis_t          vid_right;
addr_t              line_set_addr;                      // address for on-the-fly addr set

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

// video memories
logic               pb_stall;
logic               pb_vram_sel;                        // vram read select
addr_t              pb_vram_addr;                       // vram word address out (16x64K)
logic               pb_tile_sel;                        // tile mem read select
tile_addr_t         pb_tile_addr;                       // tile mem word address out (16x5K)

localparam H_MEM_BEGIN      = xv::OFFSCREEN_WIDTH-64;               // memory prefetch starts early
localparam H_MEM_END        = xv::TOTAL_WIDTH-8;                    // memory fetch can end a bit early

// sync generation signals (and combinatorial logic "next" versions)

logic           end_of_line;
logic           end_of_frame;
logic           end_of_visible;
logic           v_visible;
hres_t          h_count;
vres_t          v_count;
logic           hsync;
logic           vsync;
logic           dv_de;

assign hsync_o      = hsync;
assign vsync_o      = vsync;
assign dv_de_o      = dv_de;
assign h_count_o    = h_count;
assign v_count_o    = v_count;


video_timing video_timing
(
    // video registers and control
    .h_count_o(h_count),
    .v_count_o(v_count),
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

// audio
logic           audio_enable;

logic           audio_0_fetch;
word_t          audio_0_vol;                    // audio 0 L+R 8-bit volume/pan
logic           audio_0_restart;                // audio 0 force resttart (high bit of period)
logic [14:0]    audio_0_period;                 // audio 0 playback rate (TBD)
addr_t          audio_0_start;                  // audio 0 start address
logic           audio_0_tile;                   // audio 0 memory (0=VRAM, 1=TILE)
logic [14:0]    audio_0_len;                    // audio 0 length in words
addr_t          audio_0_addr;                   // audio 0 current address
word_t          audio_0_word;                   // audio 0 current output
logic           audio_0_pending;                // audio 0 reload pending (set when start/len set, but not chan not restarted)
logic           audio_0_reload;                 // audio 0 strobe when chan start/addr reloaded


assign pb_stall = (pa_vram_sel && pb_vram_sel) || (pa_tile_sel && pb_tile_sel);
assign vram_sel_o       = pa_vram_sel ? pa_vram_sel  : pb_vram_sel;
assign vram_addr_o      = pa_vram_sel ? pa_vram_addr : pb_vram_addr;
assign tilemem_sel_o    = pa_tile_sel ? pa_tile_sel  : pb_tile_sel;
assign tilemem_addr_o   = pa_tile_sel ? pa_tile_addr : pb_tile_addr;

video_playfield #(
    .EN_AUDIO(EN_AUDIO)
) video_pf_a(
    .stall_i(1'b0),                                     // playfield A never stalls
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
    .audio_0_fetch_i(audio_0_fetch),
    .audio_0_tile_i(audio_0_tile),
    .audio_0_addr_i(audio_0_addr),
    .audio_0_word_o(audio_0_word),
    .reset_i(reset_i),
    .clk(clk)
);

generate
    if (EN_VID_PF_B) begin : opt_PF_B
        logic       pb_vram_rd;                         // last cycle was PB vram read flag
        logic       pb_vram_rd_save;                    // PB vram read data saved flag
        word_t      pb_vram_rd_data;                    // PB vram read data
        logic       pb_tilemem_rd;                      // last cycle was PB tilemem read flag
        logic       pb_tilemem_rd_save;                 // PB tilemem read data saved flag
        word_t      pb_tilemem_rd_data;                 // PB tilemem read data

        word_t      audio_dummy_word;

        logic       unused_pb_audio;
        assign      unused_pb_audio = &{ 1'b0, audio_dummy_word };

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

        video_playfield #(
            .EN_AUDIO(0)
        )
        video_pf_b(
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
            .audio_0_fetch_i(1'b0),
            .audio_0_tile_i(1'b0),
            .audio_0_addr_i('0),
            .audio_0_word_o(audio_dummy_word),
            .reset_i(reset_i),
            .clk(clk)
        );
    end else begin : no_PF_B
        logic unused_pf_b;
        assign unused_pf_b = &{ 1'b0,
            pb_stall,
            pb_blank,
            pb_start_addr,
            pb_line_len,
            pb_colorbase,
            pb_bpp,
            pb_bitmap,
            pb_tile_bank,
            pb_disp_in_tile,
            pb_tile_in_vram,
            pb_tile_height,
            pb_h_repeat,
            pb_v_repeat,
            pb_fine_hscroll,
            pb_fine_vscroll,
            pb_line_start_set,
            pb_gfx_ctrl_set
        };
        assign pb_color_index   = '0;
        assign pb_vram_sel      = '0;
        assign pb_vram_addr     = '0;
        assign pb_tile_sel      = '0;
        assign pb_tile_addr     = '0;
    end
endgenerate

// video config registers read/write
always_ff @(posedge clk) begin
    if (reset_i) begin
        intr_signal_o       <= 4'b0;
        border_color        <= 8'h08;               // defaulting to dark grey to show operational
        vid_left            <= '0;
        vid_right           <= $bits(vid_right)'(xv::VISIBLE_WIDTH);

        pa_blank            <= 1'b1;                // playfield A starts blanked
        pa_start_addr       <= 16'h0000;
        pa_line_len         <= xv::TILES_WIDE[15:0];
        pa_fine_hscroll     <= 5'b0;
        pa_fine_vscroll     <= 6'b0;
        pa_tile_height      <= 4'b1111;
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

        pb_blank            <= 1'b1;            // playfield B starts blanked
        pb_start_addr       <= 16'h0000;
        pb_line_len         <= xv::TILES_WIDE[15:0];
        pb_fine_hscroll     <= 5'b0;
        pb_fine_vscroll     <= 6'b0;
        pb_tile_height      <= 4'b1111;
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

        line_set_addr       <= 16'h0000;        // user set display addr

`ifdef ENABLE_COPP
        copp_reg_wr_o       <= 1'b0;
        copp_reg_data_o     <= 16'h0000;
`endif

        audio_0_vol         <= '0;
        audio_0_restart     <= 1'b0;
        audio_0_period      <= '0;
        audio_0_tile        <= '0;
        audio_0_start       <= '0;
        audio_0_len         <= '0;
        audio_0_pending     <= '0;

`ifndef SYNTHESIS
        pa_blank            <= 1'b0;            // don't blank playfield A in simulation
`endif

    end else begin
        pa_line_start_set   <= 1'b0;            // indicates user line address set
        pa_gfx_ctrl_set     <= 1'b0;

        pb_line_start_set   <= 1'b0;            // indicates user line address set
        pb_gfx_ctrl_set     <= 1'b0;

        intr_signal_o       <= 4'b0;
`ifdef ENABLE_COPP
        copp_reg_wr_o       <= 1'b0;
`endif
        audio_0_restart     <= 1'b0;

        if (audio_0_reload) begin
            audio_0_pending <= 1'b0;
        end

        // video register write
        if (vgen_reg_wr_en_i) begin
            case ({1'b0, vgen_reg_num_i})
                xv::XR_VID_CTRL: begin
                    border_color    <= vgen_reg_data_i[15:8];
                    audio_enable    <= vgen_reg_data_i[4];
                    intr_signal_o   <= vgen_reg_data_i[3:0];
                end
                xv::XR_COPP_CTRL: begin
`ifdef ENABLE_COPP
                    copp_reg_wr_o   <= 1'b1;
                    copp_reg_data_o[15] <= vgen_reg_data_i[15];
                    copp_reg_data_o[xv::COPP_W-1:0]  <= vgen_reg_data_i[xv::COPP_W-1:0];
`endif
                end
                xv::XR_AUD0_VOL: begin
                    audio_0_vol     <= vgen_reg_data_i;
                end
                xv::XR_AUD0_PERIOD: begin
                    audio_0_restart <= vgen_reg_data_i[15];
                    audio_0_period  <= vgen_reg_data_i[14:0];
                end
                xv::XR_AUD0_START: begin
                    audio_0_start   <= vgen_reg_data_i;
                    audio_0_pending <= 1'b1;
                end
                xv::XR_AUD0_LENGTH: begin
                    audio_0_tile    <= vgen_reg_data_i[15];
                    audio_0_len     <= vgen_reg_data_i[14:0];
                    audio_0_pending <= 1'b1;
                end
                xv::XR_VID_LEFT: begin
                    vid_left        <= $bits(vid_left)'(vgen_reg_data_i);
                end
                xv::XR_VID_RIGHT: begin
                    vid_right       <= $bits(vid_right)'(vgen_reg_data_i);
                end
                // playfield A
                xv::XR_PA_GFX_CTRL: begin
                    pa_gfx_ctrl_set <= 1'b1;                // changed flag
                    pa_colorbase    <= vgen_reg_data_i[15:8];
                    pa_blank        <= vgen_reg_data_i[7];
                    pa_bitmap       <= vgen_reg_data_i[6];
                    pa_bpp          <= vgen_reg_data_i[5:4];
                    pa_h_repeat     <= vgen_reg_data_i[3:2];
                    pa_v_repeat     <= vgen_reg_data_i[1:0];
                end
                xv::XR_PA_TILE_CTRL: begin
                    pa_tile_bank    <= vgen_reg_data_i[15:10];
                    pa_disp_in_tile <= vgen_reg_data_i[9];
                    pa_tile_in_vram <= vgen_reg_data_i[8];
                    pa_tile_height  <= vgen_reg_data_i[3:0];
                end
                xv::XR_PA_DISP_ADDR: begin
                    pa_start_addr   <= vgen_reg_data_i;
                end
                xv::XR_PA_LINE_LEN: begin
                    pa_line_len     <= vgen_reg_data_i;
                end
                xv::XR_PA_HV_SCROLL: begin
                    pa_fine_hscroll <= vgen_reg_data_i[12:8];
                    pa_fine_vscroll <= vgen_reg_data_i[5:0];
                end
                xv::XR_PA_LINE_ADDR: begin
                    pa_line_start_set <= 1'b1;               // changed flag
                    line_set_addr   <= vgen_reg_data_i;
                end
                xv::XR_PA_HV_FSCALE: begin
                    pa_h_frac_repeat <= vgen_reg_data_i[6:4];
                    pa_v_frac_repeat <= vgen_reg_data_i[2:0];
                end
                // playfield B
                xv::XR_PB_GFX_CTRL: begin
                    pb_colorbase    <= vgen_reg_data_i[15:8];
                    pb_blank        <= vgen_reg_data_i[7];
                    pb_bitmap       <= vgen_reg_data_i[6];
                    pb_bpp          <= vgen_reg_data_i[5:4];
                    pb_h_repeat     <= vgen_reg_data_i[3:2];
                    pb_v_repeat     <= vgen_reg_data_i[1:0];
                end
                xv::XR_PB_TILE_CTRL: begin
                    pb_tile_bank    <= vgen_reg_data_i[15:10];
                    pb_disp_in_tile <= vgen_reg_data_i[9];
                    pb_tile_in_vram <= vgen_reg_data_i[8];
                    pb_tile_height  <= vgen_reg_data_i[3:0];
                end
                xv::XR_PB_DISP_ADDR: begin
                    pb_start_addr   <= vgen_reg_data_i;
                end
                xv::XR_PB_LINE_LEN: begin
                    pb_line_len     <= vgen_reg_data_i;
                end
                xv::XR_PB_HV_SCROLL: begin
                    pb_fine_hscroll <= vgen_reg_data_i[12:8];
                    pb_fine_vscroll <= vgen_reg_data_i[5:0];
                end
                xv::XR_PB_LINE_ADDR: begin
                    pb_line_start_set <= 1'b1;
                    line_set_addr   <= vgen_reg_data_i;
                end
                xv::XR_PB_HV_FSCALE: begin
                    pb_h_frac_repeat <= vgen_reg_data_i[6:4];
                    pb_v_frac_repeat <= vgen_reg_data_i[2:0];
                end
                default: begin
                end
            endcase
        end
        // vsync interrupt generation
        if (end_of_visible) begin
            intr_signal_o[3]  <= 1'b1;
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
    case (vgen_reg_num_i[3:0])
        xv::XR_VID_CTRL[3:0]:       rd_vid_regs = { border_color, 3'b0, audio_0_pending, intr_status_i };
`ifdef ENABLE_COPP
        xv::XR_COPP_CTRL[3:0]:      rd_vid_regs = { copp_reg_data_o[15], 5'b0000, copp_reg_data_o[xv::COPP_W-1:0]};
`endif
//        xv::XR_VID_LEFT[3:0]:       rd_vid_regs = 16'(vid_left);
//        xv::XR_VID_RIGHT[3:0]:      rd_vid_regs = 16'(vid_right);
        xv::XR_SCANLINE[3:0]:       rd_vid_regs = { ~v_visible, 15'(v_count) };
//        xv::XR_VERSION[3:0]:        rd_vid_regs = { 1'b`GITCLEAN, 3'b000, 12'h`VERSION };
//        xv::XR_GITHASH_H[3:0]:      rd_vid_regs = githash[31:16];
//        xv::XR_GITHASH_L[3:0]:      rd_vid_regs = githash[15:0];
        xv::XR_VID_HSIZE[3:0]:      rd_vid_regs = 16'(xv::VISIBLE_WIDTH);
        xv::XR_VID_VSIZE[3:0]:      rd_vid_regs = 16'(xv::VISIBLE_HEIGHT);
//        xv::XR_VID_VFREQ[3:0]:      rd_vid_regs = xv::REFRESH_FREQ;
        default:                    rd_vid_regs = 16'h0000;
    endcase
end

// video registers read
always_comb begin
    case (vgen_reg_num_i[3:0])
        xv::XR_PA_GFX_CTRL[3:0]:    rd_pf_regs = { pa_colorbase, pa_blank, pa_bitmap, pa_bpp, pa_h_repeat, pa_v_repeat };
        xv::XR_PA_TILE_CTRL[3:0]:   rd_pf_regs = { pa_tile_bank, pa_disp_in_tile, pa_tile_in_vram, 4'b0, pa_tile_height };
        xv::XR_PA_DISP_ADDR[3:0]:   rd_pf_regs = pa_start_addr;
        xv::XR_PA_LINE_LEN[3:0]:    rd_pf_regs = pa_line_len;
//        xv::XR_PA_HV_SCROLL[3:0]:   rd_pf_regs = { 8'(pa_fine_hscroll), 8'(pa_fine_vscroll) };
        xv::XR_PB_GFX_CTRL[3:0]:    rd_pf_regs = { pb_colorbase, pb_blank, pb_bitmap, pb_bpp, pb_h_repeat, pb_v_repeat };
        xv::XR_PB_TILE_CTRL[3:0]:   rd_pf_regs = { pb_tile_bank, pb_disp_in_tile, pb_tile_in_vram, 4'b0, pb_tile_height };
        xv::XR_PB_DISP_ADDR[3:0]:   rd_pf_regs = pb_start_addr;
        xv::XR_PB_LINE_LEN[3:0]:    rd_pf_regs = pb_line_len;
//        xv::XR_PB_HV_SCROLL[3:0]:   rd_pf_regs = { 8'(pb_fine_hscroll), 8'(pb_fine_vscroll) };
        default:                    rd_pf_regs = 16'h0000;
    endcase
end

// combinational block for video fetch start and stop
`ifdef OLD_WAY
hres_t          mem_fetch_hcount;                   // horizontal count when mem_fetch_active toggles
always_comb     mem_fetch_next = (v_visible && h_count_i == mem_fetch_hcount) ? ~mem_fetch_active : mem_fetch_active;
always_comb begin
    // set mem_fetch_active next toggle for video memory access
    if (mem_fetch_active) begin
        mem_fetch_hcount = $bits(mem_fetch_hcount)'(H_MEM_END);
    end else begin
        mem_fetch_hcount = $bits(mem_fetch_hcount)'(H_MEM_BEGIN);
    end
end
`else
logic           mem_fetch;                   // true when fetching display data
logic           mem_fetch_next;
logic           mem_fetch_h_start;
logic           mem_fetch_h_end;
always_comb     mem_fetch_h_start = ($bits(h_count)'(H_MEM_BEGIN) == h_count);
always_comb     mem_fetch_h_end = ($bits(h_count)'(H_MEM_END) == h_count);
always_comb     mem_fetch_next = (!mem_fetch ? mem_fetch_h_start : !mem_fetch_h_end) && v_visible;
`endif

// video pixel generation
always_ff @(posedge clk) begin
    if (reset_i) begin
        colorA_index_o      <= 8'b0;
        colorB_index_o      <= 8'b0;

        mem_fetch           <= 1'b0;

    end else begin
        // set output pixel index from pixel shift-out
        colorA_index_o      <= pa_color_index;
        colorB_index_o      <= pb_color_index;

        mem_fetch           <= mem_fetch_next;
    end
end

// audio generation
generate
    if (EN_AUDIO) begin : opt_AUDIO
        // audio channel mixer
        audio_mixer audio_mixer
        (
            .audio_enable_i(audio_enable),
            .audio_dma_start_i(end_of_line),
            .audio_0_vol_i(audio_0_vol),
            .audio_0_period_i(audio_0_period),
            .audio_0_start_i(audio_0_start),
            .audio_0_restart_i(audio_0_restart),
            .audio_0_len_i(audio_0_len),
            .audio_0_fetch_o(audio_0_fetch),
            .audio_0_addr_o(audio_0_addr),
            .audio_0_word_i(audio_0_word),
            .audio_0_reload_o(audio_0_reload),
            .pdm_l_o(audio_pdm_l_o),
            .pdm_r_o(audio_pdm_r_o),
            .reset_i(reset_i),
            .clk(clk)
        );
    end else begin
        assign  audio_pdm_l_o   = 1'b0;
        assign  audio_pdm_r_o   = 1'b0;
        assign  audio_0_fetch   = 1'b0;
        assign  audio_0_addr    = '0;

        logic   audio_unused;
        assign  audio_unused = &{1'b0, audio_enable, audio_0_vol, audio_0_period, audio_0_start, audio_0_len, audio_0_word };
    end
endgenerate

endmodule
`default_nettype wire               // restore default
