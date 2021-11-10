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

module video_gen(
    // video registers and control
    input  wire logic            vgen_reg_wr_en_i,    // strobe to write internal config register number
    input  wire logic  [4:0]     vgen_reg_num_i,     // internal config register number (for reads)
    input  wire logic [15:0]     vgen_reg_data_i,    // data for internal config register
    output      logic [15:0]     vgen_reg_data_o,    // register/status data reads
    input wire  logic  [3:0]     intr_status_i,      // interrupt pending status
    output      logic  [3:0]     intr_signal_o,      // generate interrupt signal
`ifndef COPPER_DISABLE
    // outputs for copper
    output      logic            copp_reg_wr_o,      // COPP_CTRL write strobe
    output      logic [15:0]     copp_reg_data_o,    // copper reg data
    output      logic [10:0]     h_count_o,          // Horizontal video counter
    output      logic [10:0]     v_count_o,          // Vertical video counter
`endif
    // video memories
    output      logic            vram_sel_o,         // vram read select
    output      logic [15:0]     vram_addr_o,        // vram word address out (16x64K)
    input  wire logic [15:0]     vram_data_i,        // vram word data in
    output      logic            tilemem_sel_o,      // tile mem read select
    output      logic [xv::TILE_AWIDTH-1:0] tilemem_addr_o, // tile mem word address out (16x5K)
    input  wire logic [15:0]     tilemem_data_i,     // tile mem word data in
    // video signal outputs
    output      logic  [7:0]     color_index_o,      // color palette index output (16x256)
    output      logic            vsync_o, hsync_o,   // video sync outputs
    output      logic            dv_de_o,            // video active signal (needed for HDMI)
    // standard signals
    input  wire logic            reset_i,            // system reset in
    input  wire logic            clk                 // clock (video pixel clock)
);

localparam [31:0] githash = 32'H`GITHASH;

// video generation signals
logic [7:0]     border_color;
logic [10:0]    cursor_x;
logic [10:0]    cursor_y;
logic [10:0]    vid_top;
logic [10:0]    vid_bottom;
logic [10:0]    vid_left;
logic [10:0]    vid_right;

// playfield A generation control signals
logic           pa_blank;                           // disable plane A
logic [15:0]    pa_start_addr;                      // display data start address (word address)
logic [15:0]    pa_line_len;                        // words per disply line (added to line_addr each line)
logic  [7:0]    pa_colorbase;                       // colorbase XOR'd with pixel index (e.g. to set upper bits or alter index)
logic  [1:0]    pa_bpp;                             // bpp code (bpp_depth_t)
logic           pa_bitmap;                          // bitmap enable (else text mode)
logic  [5:0]    pa_tile_bank;                       // vram/tilemem tile bank 0-3 (0/1 with 8x16) tilemem, or 2KB/4K
logic           pa_disp_in_tile;                    // display memory 0=vram, 1=tileram
logic           pa_tile_in_vram;                    // tile memory 0=tilemem, 1=vram
logic  [3:0]    pa_tile_height;                     // max height of tile cell
logic  [1:0]    pa_h_repeat;                        // horizontal pixel repeat
logic  [1:0]    pa_v_repeat;                        // vertical pixel repeat
logic  [4:0]    pa_fine_hscroll;                    // horizontal fine scroll (8 pixel * 4 for repeat)
logic  [5:0]    pa_fine_vscroll;                    // vertical fine scroll (16 lines * 4 for repeat)
logic           pa_line_start_set;                  // true if pa_line_start changed (register write)
logic           pa_gfx_ctrl_set;                    // true if pa_gfx_ctrl changed (register write)
logic  [7:0]    pa_color_index;                     // colorbase XOR'd with pixel index (e.g. to set upper bits or alter index)

`ifdef ENABLE_PB
// playfield B generation control signals
logic           pb_blank;                           // disable plane B
logic [15:0]    pb_start_addr;                      // display data start address (word address)
logic [15:0]    pb_line_len;                        // words per disply line (added to line_addr each line)
logic  [7:0]    pb_colorbase;                       // colorbase XOR'd with pixel index (e.g. to set upper bits or alter index)
logic  [1:0]    pb_bpp;                             // bpp code (bpp_depth_t)
logic           pb_bitmap;                          // bitmap enable (else text mode)
logic  [5:0]    pb_tile_bank;                       // vram/tilemem tile bank 0-3 (0/1 with 8x16) tilemem, or 2KB/4K
logic           pa_disp_in_tile;                    // display memory 0=vram, 1=tileram
logic           pb_tile_in_vram;                    // 0=tilemem, 1=vram
logic  [3:0]    pb_tile_height;                     // max height of tile cell
logic  [1:0]    pb_h_repeat;                        // horizontal pixel repeat
logic  [1:0]    pb_v_repeat;                        // vertical pixel repeat
logic  [4:0]    pb_fine_hscroll;                    // horizontal fine scroll (8 pixel * 4 for repeat)
logic  [5:0]    pb_fine_vscroll;                    // vertical fine scroll (16 lines * 4 for repeat)
logic  [1:0]    pb_h_count;                         // current horizontal repeat countdown
logic  [1:0]    pb_v_count;                         // current vertical repeat countdown
logic  [2:0]    pb_tile_x;                          // current column of tile cell
logic  [3:0]    pb_tile_y;                          // current line of tile cell
logic           pb_line_start_set;                  // true if pb_line_start changed (register write)
logic [15:0]    pb_line_start;                      // address of next line display data start
`endif

// video sync generation via state machine (Thanks tnt & drr - a much more efficient method!)
typedef enum logic [1:0] {
    STATE_PRE_SYNC  = 2'b00,
    STATE_SYNC      = 2'b01,
    STATE_POST_SYNC = 2'b10,
    STATE_VISIBLE   = 2'b11
} video_signal_st;

// sync generation signals (and combinatorial logic "next" versions)
logic  [1:0]    h_state;
logic [10:0]    h_count;
logic [10:0]    h_count_next;
logic [10:0]    h_count_next_state;

logic  [1:0]    v_state;
logic [10:0]    v_count;
logic [10:0]    v_count_next;
logic [10:0]    v_count_next_state;

logic [15:0]    line_set_addr;                          // address for on-the-fly addr set

// sync condition indicators (combinatorial)
logic           hsync;
logic           vsync;
logic           dv_display_ena;
logic           h_line_last_pixel;
logic           last_visible_pixel;
logic           last_frame_pixel;
logic [1:0]     h_state_next;
logic [1:0]     v_state_next;

`ifndef COPPER_DISABLE
assign h_count_o    = h_count;
assign v_count_o    = v_count;
`endif

// video config registers read/write
always_ff @(posedge clk) begin
    if (reset_i) begin
        intr_signal_o       <= 4'b0;
        border_color        <= 8'h08;           // defaulting to dark grey to show operational
        cursor_x            <= 11'h180;
        cursor_y            <= 11'h100;
        vid_top             <= 11'h0;
        vid_bottom          <= xv::VISIBLE_HEIGHT[10:0];
        vid_left            <= 11'h0;
        vid_right           <= xv::VISIBLE_WIDTH[10:0];
`ifdef SYNTHESIS
        pa_blank            <= 1'b0;            // playfield A starts blanked
`else
        pa_blank            <= 1'b0;            // unless simulating
`endif
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

        pa_line_start_set   <= 1'b0;            // indicates user line address set
        pa_gfx_ctrl_set     <= 1'b0;
        line_set_addr       <= 16'h0000;        // user set display addr
`ifndef COPPER_DISABLE
        copp_reg_wr_o       <= 1'b0;
        copp_reg_data_o     <= 16'h0000;
`endif
    end else begin
        pa_line_start_set   <= 1'b0;            // indicates user line address set
        pa_gfx_ctrl_set     <= 1'b0;
        intr_signal_o       <= 4'b0;
`ifndef COPPER_DISABLE
        copp_reg_wr_o       <= 1'b0;
`endif
        // video register write
        if (vgen_reg_wr_en_i) begin
            case (vgen_reg_num_i)
                xv::XR_VID_CTRL: begin
                    border_color    <= vgen_reg_data_i[15:8];
                    intr_signal_o   <= vgen_reg_data_i[3:0];
                end
                xv::XR_COPP_CTRL: begin
`ifndef COPPER_DISABLE
                    copp_reg_wr_o   <= 1'b1;
                    copp_reg_data_o[15] <= vgen_reg_data_i[15];
                    copp_reg_data_o[xv::COPPER_AWIDTH-1:0]  <= vgen_reg_data_i[xv::COPPER_AWIDTH-1:0];
`endif
                end
                xv::XR_CURSOR_X: begin
                    cursor_x        <= vgen_reg_data_i[10:0];
                end
                xv::XR_CURSOR_Y: begin
                    cursor_y        <= vgen_reg_data_i[10:0];
                end
                xv::XR_VID_TOP: begin
                    vid_top        <= vgen_reg_data_i[10:0];
                end
                xv::XR_VID_BOTTOM: begin
                    vid_bottom     <= vgen_reg_data_i[10:0];
                end
                xv::XR_VID_LEFT: begin
                    vid_left       <= vgen_reg_data_i[10:0];
                end
                xv::XR_VID_RIGHT: begin
                    vid_right      <= vgen_reg_data_i[10:0];
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
                    pa_line_len   <= vgen_reg_data_i;
                end
                xv::XR_PA_HV_SCROLL: begin
                    pa_fine_hscroll <= vgen_reg_data_i[12:8];
                    pa_fine_vscroll <= vgen_reg_data_i[5:0];
                end
                xv::XR_PA_LINE_ADDR: begin
                    pa_line_start_set <= 1'b1;               // changed flag
                    line_set_addr   <= vgen_reg_data_i;
                end
`ifdef ENABLE_PB
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
                    pb_line_len   <= vgen_reg_data_i;
                end
                xv::XR_PB_HV_SCROLL: begin
                    pb_fine_hscroll <= vgen_reg_data_i[12:8];
                    pb_fine_vscroll <= vgen_reg_data_i[5:0];
                end
                xv::XR_PB_LINE_ADDR: begin
                    pb_line_start_set   <= 1'b1;
                    line_set_addr       <= vgen_reg_data_i;
                end
`endif
                default: begin
                end
            endcase
        end
        // vsync interrupt generation
        if (last_visible_pixel) begin
            intr_signal_o[3]  <= 1'b1;
        end
    end
end

// video registers read
always_ff @(posedge clk) begin
    case (vgen_reg_num_i)
        xv::XR_VID_CTRL:       vgen_reg_data_o <= { border_color, 4'b0, intr_status_i };
`ifndef COPPER_DISABLE
        xv::XR_COPP_CTRL:      vgen_reg_data_o <= { copp_reg_data_o[15], 5'b0000, copp_reg_data_o[xv::COPPER_AWIDTH-1:0]};
`endif
        xv::XR_CURSOR_X:       vgen_reg_data_o <= {5'b0, cursor_x };
        xv::XR_CURSOR_Y:       vgen_reg_data_o <= {5'b0, cursor_y };
        xv::XR_VID_TOP:        vgen_reg_data_o <= {5'b0, vid_top };
        xv::XR_VID_BOTTOM:     vgen_reg_data_o <= {5'b0, vid_bottom };
        xv::XR_VID_LEFT:       vgen_reg_data_o <= {5'b0, vid_left };
        xv::XR_VID_RIGHT:      vgen_reg_data_o <= {5'b0, vid_right };
        xv::XR_SCANLINE:       vgen_reg_data_o <= { (v_state != STATE_VISIBLE), (h_state != STATE_VISIBLE), 3'b000, v_count };
        xv::XR_VERSION:        vgen_reg_data_o <= { 1'b`GITCLEAN, 3'b000, 12'h`VERSION };
        xv::XR_GITHASH_H:      vgen_reg_data_o <= githash[31:16];
        xv::XR_GITHASH_L:      vgen_reg_data_o <= githash[15:0];
        xv::XR_VID_HSIZE:      vgen_reg_data_o <= { 6'h0, xv::VISIBLE_WIDTH[9:0]};
        xv::XR_VID_VSIZE:      vgen_reg_data_o <= { 6'h0, xv::VISIBLE_HEIGHT[9:0]};
        xv::XR_VID_VFREQ:      vgen_reg_data_o <= xv::REFRESH_FREQ;
        xv::XR_PA_GFX_CTRL:    vgen_reg_data_o <= { pa_colorbase, pa_blank, pa_bitmap, pa_bpp, pa_h_repeat, pa_v_repeat };
        xv::XR_PA_TILE_CTRL:   vgen_reg_data_o <= { pa_tile_bank, pa_disp_in_tile, pa_tile_in_vram, 4'b0, pa_tile_height };
        xv::XR_PA_DISP_ADDR:   vgen_reg_data_o <= pa_start_addr;
        xv::XR_PA_LINE_LEN:    vgen_reg_data_o <= pa_line_len;
        xv::XR_PA_HV_SCROLL:   vgen_reg_data_o <= { 3'b0, pa_fine_hscroll, 2'b00, pa_fine_vscroll };
        default:                    vgen_reg_data_o <= 16'h0000;
    endcase
end

// video signal generation
always_comb     hsync = (h_state_next == STATE_SYNC);
always_comb     vsync = (v_state_next == STATE_SYNC);
always_comb     dv_display_ena = (h_state_next == STATE_VISIBLE) && (v_state_next == STATE_VISIBLE);
always_comb     h_line_last_pixel = (h_state_next == STATE_PRE_SYNC) && (h_state == STATE_VISIBLE);
always_comb     last_visible_pixel = (v_state_next == STATE_PRE_SYNC) && (v_state == STATE_VISIBLE) && h_line_last_pixel;
always_comb     last_frame_pixel = (v_state_next == STATE_VISIBLE) && (v_state == STATE_POST_SYNC) && h_line_last_pixel;

// combinational block for video counters
always_comb begin
    h_count_next = h_count + 1'b1;
    v_count_next = v_count;

    if (h_line_last_pixel) begin
        h_count_next = 0;
        v_count_next = v_count + 1'b1;

        if (last_frame_pixel) begin
            v_count_next = 0;
        end
    end
end

// combinational block for horizontal video state
always_comb h_state_next = (h_count == h_count_next_state) ? h_state + 1'b1 : h_state;
always_comb begin
    // scanning horizontally left to right, offscreen pixels are on left before visible pixels
    case (h_state)
        STATE_PRE_SYNC:
            h_count_next_state = xv::H_FRONT_PORCH - 1;
        STATE_SYNC:
            h_count_next_state = xv::H_FRONT_PORCH + xv::H_SYNC_PULSE - 1;
        STATE_POST_SYNC:
            h_count_next_state = xv::OFFSCREEN_WIDTH - 1;
        STATE_VISIBLE:
            h_count_next_state = xv::TOTAL_WIDTH - 1;
    endcase
end

// combinational block for vertical video state
always_comb v_state_next = (h_line_last_pixel && v_count == v_count_next_state) ? v_state + 1'b1 : v_state;
always_comb begin
    // scanning vertically top to bottom, offscreen lines are on bottom after visible lines
    case (v_state)
        STATE_PRE_SYNC:
            v_count_next_state = xv::VISIBLE_HEIGHT + xv::V_FRONT_PORCH - 1;
        STATE_SYNC:
            v_count_next_state = xv::VISIBLE_HEIGHT + xv::V_FRONT_PORCH + xv::V_SYNC_PULSE - 1;
        STATE_POST_SYNC:
            v_count_next_state = xv::TOTAL_HEIGHT - 1;
        STATE_VISIBLE:
            v_count_next_state = xv::VISIBLE_HEIGHT - 1;
    endcase
end

// video pixel generation

// generate tile address from index, tile y, bpp and tile size (8x8 or 8x16)
function automatic [15:0] calc_tile_addr(
        input [9:0] tile_char,
        input [3:0] tile_y,
        input [5:0] tilebank,
        input [1:0] bpp,
        input       tile_8x16,
        input       vrev
    );
    begin
        case (bpp)
            xv::BPP_1_ATTR: begin
                if (!tile_8x16) begin
                    calc_tile_addr = { tilebank, 10'b0 } | { 6'b0, tile_char[7:0], tile_y[2:1] };      // 8x8 = 1Wx4 = 4W (even/odd byte) x 256 = 1024W
                end else begin
                    calc_tile_addr = { tilebank, 10'b0 } | { 5'b0, tile_char[7:0], tile_y[3:1] };      // 8x16 = 1Wx8 = 8W (even/odd byte) x 256 = 2048W
                end
            end
            xv::BPP_4: begin
                calc_tile_addr = { tilebank, 10'b0 } | { 2'b0, tile_char[9:0], vrev ? ~tile_y[2:0] : tile_y[2:0], 1'b0 };    // 8x8 = 2Wx8 = 16W x 1024 = 16384W
            end
            default: begin
                calc_tile_addr = { tilebank, 10'b0 } | { 1'b0, tile_char[9:0], vrev ? ~tile_y[2:0] : tile_y[2:0], 2'b0 };    // 8x8 = 4Wx8 = 32W x 1024 = 32768W
            end
        endcase
    end
endfunction


video_playfield video_pf_a(
    .vram_sel_o(vram_sel_o),                     // vram read select
    .vram_addr_o(vram_addr_o),                     // vram word address out (16x64K)
    .vram_data_i(vram_data_i),                     // vram word data in
    .tilemem_sel_o(tilemem_sel_o),                     // tile mem read select
    .tilemem_addr_o(tilemem_addr_o),         // tile mem word address out (16x5K)
    .tilemem_data_i(tilemem_data_i),                     // tile mem word data in
    .v_visible(v_state == STATE_VISIBLE),
    .h_count(h_count),
    .h_line_last_pixel(h_line_last_pixel),
    .last_frame_pixel(last_frame_pixel),
    .border_color(border_color),
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
    .pf_fine_hscroll_i(pa_fine_hscroll),
    .pf_fine_vscroll_i(pa_fine_vscroll),
    .pf_line_start_set_i(pa_line_start_set),
    .pf_line_start_addr_i(line_set_addr),
    .pf_gfx_ctrl_set_i(pa_gfx_ctrl_set),
    .pf_color_index_o(pa_color_index),
    .reset_i(reset_i),
    .clk(clk)
);

always_ff @(posedge clk) begin
    if (reset_i) begin
        color_index_o       <= 8'b0;
        hsync_o             <= 1'b0;
        vsync_o             <= 1'b0;
        dv_de_o             <= 1'b0;
        h_state             <= STATE_PRE_SYNC;
        v_state             <= STATE_PRE_SYNC;  // check STATE_VISIBLE
        h_count             <= 11'h000;         // horizontal counter
        v_count             <= 11'h000;         // vertical counter

    end else begin

        // set output pixel index from pixel shift-out
        color_index_o <= pa_color_index;   // XOR colorbase bits here

        // update registered signals from combinatorial "next" versions
        h_state <= h_state_next;
        v_state <= v_state_next;
        h_count <= h_count_next;
        v_count <= v_count_next;

        // set other video output signals
        hsync_o     <= hsync ? xv::H_SYNC_POLARITY : ~xv::H_SYNC_POLARITY;
        vsync_o     <= vsync ? xv::V_SYNC_POLARITY : ~xv::V_SYNC_POLARITY;
        dv_de_o     <= dv_display_ena;
    end
end

endmodule
`default_nettype wire               // restore default
