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
    // control outputs
    output      logic            fontram_sel_o,      // fontram access select
    output      logic [11:0]     fontram_addr_o,     // font memory byte address out (8x4KB)
    output      logic            vram_sel_o,         // vram access select
    output      logic [15:0]     vram_addr_o,        // vram word address out (16x64KB)
    output      logic [15:0]     vgen_reg_data_o,    // register/status data reads
    // control inputs
    input  wire logic [15:0]     vram_data_i,        // vram word data in
    input  wire logic [15:0]     fontram_data_i,     // font memory byte data in
    input  wire logic            enable_i,           // enable video (0=black output, 1=normal output)
    input  wire logic            vgen_reg_wr_i,      // strobe to write internal config register number
    input  wire logic  [3:0]     vgen_reg_num_i,     // internal config register number
    input  wire logic [15:0]     vgen_reg_data_i,    // data for internal config register
    // video signal outputs
    output      logic  [7:0]     pal_index_o,        // palette index outputs
    output      logic            vsync_intr_o,       // vsync bus interrupt pulse
    output      logic            vline_intr_o,       // scan-line interrupt pulse
    output      logic            vsync_o, hsync_o,   // VGA sync outputs
    output      logic            dv_de_o,            // VGA video active signal (needed for HDMI)
    // standard signals
    input  wire logic            reset_i,            // system reset in
    input  wire logic            clk                 // clock (video pixel clock)
);

localparam [31:0] githash = 32'H`GITHASH;

localparam H_MEM_BEGIN = xv::OFFSCREEN_WIDTH-64;    // memory prefetch starts early
localparam H_MEM_END = xv::TOTAL_WIDTH-8;           // memory fetch can end a bit early
localparam H_SCANOUT_BEGIN = xv::OFFSCREEN_WIDTH-2; // h count position to start line scanout

logic           vg_enable;                          // video generation enabled (else blanked)
logic           v_line_intr_ena;                    // enable scan line interrupt generation
logic [10:0]    v_line_intr;                        // line to generate scan line interrupt at start of

// video generation signals
logic           pa_disable;                         // disable plane A
logic [15:0]    pa_start_addr;                      // display data start address (word address)
logic [15:0]    pa_line_width;                      // words per disply line (added to line_addr each line)
logic [15:0]    pa_line_start;                      // display data start address for next line (word address)
logic  [7:0]    pa_colorbase;                       // colorbase for plane data (upper color bits)
logic  [1:0]    pa_bpp;                             // bpp code (bpp_depth_t)
logic           pa_bitmap;                          // bitmap enable (else text mode)
logic  [5:0]    pa_font_bank;                       // vram/fontmem font bank 0-3 (0/1 with 8x16) fontmem, or 2KB/4K
logic           pa_font_in_vram;                    // 0=fontmem, 1=vram
logic  [3:0]    pa_font_height;                     // max height of font cell
logic  [1:0]    pa_h_repeat;                        // horizontal pixel repeat
logic  [1:0]    pa_h_count;                         // current horizontal repeat countdown
logic  [1:0]    pa_v_repeat;                        // vertical pixel repeat
logic  [1:0]    pa_v_count;                         // current vertical repeat countdown
logic  [4:0]    pa_fine_scrollx;                    // X fine scroll (8 pixel * 4 for repeat)
logic  [5:0]    pa_fine_scrolly;                    // Y fine scroll (16 lines * 4 for repeat)
logic  [2:0]    pa_tile_x;                          // current column of font cell
logic  [3:0]    pa_tile_y;                          // current line of font cell

// internal signals
logic           pa_line_start_set;
logic [15:0]    pa_line_addr;                       // address of next line display data start
logic [15:0]    pa_addr;                            // address to fetch tile+color attribute
logic [15:0]    pa_font_addr;                       // font tile start address (VRAM or FONTRAM)
logic [15:0]    pa_font_next;                       // next font data address (VRAM or FONTRAM)

// temp tile and pixel fetch buffers
logic [15:0]    pa_attr_index;                      // tile attributes and index
logic [15:0]    pa_data_word0;                      // 1st fetched display data buffer
logic [15:0]    pa_data_word1;                      // 2nd fetched display data buffer
logic [15:0]    pa_data_word2;                      // 3rd fetched display data buffer (8 BPP)
logic [15:0]    pa_data_word3;                      // 4th fetched display data buffer (8 BPP)

logic           pa_first_buffer;                    // used to ignore first buffer to fill prefetch
logic [63:0]    pa_pixel_shiftout;                  // 8 pixels currently shifting to scan out
logic           pa_next_shiftout_hrev;              // horizontal reverse flag when copying
logic           pa_next_shiftout_ready;
logic [63:0]    pa_next_shiftout;                   // 8 pixel buffer waiting for scan out

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

logic           mem_fetch_active;                     // true when fetching display data
logic           h_start_scanout;                      // true for on pixel when "scrolled" scanline starts outputting (can be early)
logic [10:0]    h_scanout_hcount;
logic           h_scanout;                            // true when 

logic [10:0]    mem_fetch_hcount;   // horizontal count when mem_fetch_active toggles
logic  [2:0]    mem_fetch_cycle;    // current cycle state for display memory fetch

// sync condition indicators (combinatorial)
logic           hsync;
logic           vsync;
logic           dv_display_ena;
logic           h_line_first_pixel;
logic           h_line_last_pixel;
logic           last_visible_pixel;
logic           last_frame_pixel;
logic [1:0]     h_state_next;
logic [1:0]     v_state_next;
logic           mem_fetch_next;
logic           h_start_line_fetch;

// video config registers read/write
always_ff @(posedge clk) begin
    if (reset_i) begin
        v_line_intr         <= 11'b0;
        pa_disable          <= 1'b0;            // plane A starts enabled
        pa_start_addr       <= 16'h0000;
        pa_line_width       <= xv::TILES_WIDE[15:0];
        pa_line_start_set   <= 1'b0;            // indicates user line address set
        pa_line_start       <= 16'h0000;        // user start of next display line
        v_line_intr_ena     <= 1'b0;
        v_line_intr         <= 11'b0;
        pa_fine_scrollx     <= 5'b0000;
        pa_fine_scrolly     <= 6'b000000;
        pa_font_height      <= 4'b1111;
        pa_font_bank        <= 6'b000000;
        pa_font_in_vram     <= 1'b0;
        pa_bitmap           <= 1'b0;            // bitmap mode
        pa_bpp              <= xv::BPP_1_ATTR;
        pa_colorbase        <= 8'h00;
        pa_h_repeat         <= 2'b00;
        pa_v_repeat         <= 2'b00;
    end else begin
        pa_line_start_set <= 1'b0;
        // video register write
        if (vgen_reg_wr_i) begin
            case (vgen_reg_num_i[3:0])
                xv::AUX_DISPSTART[3:0]: begin
                    pa_start_addr   <= vgen_reg_data_i;
                end
                xv::AUX_DISPWIDTH[3:0]: begin
                    pa_line_width   <= vgen_reg_data_i;
                end
                xv::AUX_SCROLLXY[3:0]: begin
                    pa_fine_scrollx <= vgen_reg_data_i[12:8];
                    pa_fine_scrolly <= vgen_reg_data_i[5:0];
                end
                xv::AUX_FONTCTRL[3:0]: begin
                    pa_font_bank    <= vgen_reg_data_i[15:10];
                    pa_font_in_vram <= vgen_reg_data_i[7];
                    pa_font_height  <= vgen_reg_data_i[3:0];
                end
                xv::AUX_GFXCTRL[3:0]: begin
                    pa_colorbase    <= vgen_reg_data_i[15:8];
                    pa_disable      <= vgen_reg_data_i[7];
                    pa_bitmap       <= vgen_reg_data_i[6];
                    pa_bpp          <= vgen_reg_data_i[5:4];
                    pa_v_repeat     <= vgen_reg_data_i[3:2];
                    pa_h_repeat     <= vgen_reg_data_i[1:0];
                end
                xv::AUX_LINESTART[3:0]: begin
                    pa_line_start_set <= 1'b1;
                    pa_line_start   <= vgen_reg_data_i;
                end
                xv::AUX_LINEINTR[3:0]: begin
                    v_line_intr_ena <= vgen_reg_data_i[15];
                    v_line_intr     <= vgen_reg_data_i[10:0];
                end
                xv::AUX_UNUSED_7[3:0]: begin
                end
                default: begin
                end
            endcase
        end

        // video register read
        case (vgen_reg_num_i[3:0])
            xv::AUX_DISPSTART[3:0]:     vgen_reg_data_o <= pa_start_addr;
            xv::AUX_DISPWIDTH[3:0]:     vgen_reg_data_o <= pa_line_width;
            xv::AUX_SCROLLXY[3:0]:      vgen_reg_data_o <= { 3'b000, pa_fine_scrollx, 2'b00, pa_fine_scrolly };
            xv::AUX_FONTCTRL[3:0]:      vgen_reg_data_o <= { pa_font_bank, 2'b0, pa_font_in_vram, 3'b0, pa_font_height };
            xv::AUX_GFXCTRL[3:0]:       vgen_reg_data_o <= { pa_colorbase, pa_disable, pa_bitmap, pa_bpp, pa_v_repeat, pa_h_repeat };
            xv::AUX_LINESTART[3:0]:     vgen_reg_data_o <= 16'h0000;
            xv::AUX_LINEINTR[3:0]:      vgen_reg_data_o <= { 5'b0, v_line_intr };
            xv::AUX_UNUSED_7[3:0]:      vgen_reg_data_o <= 16'h0000;
            xv::AUX_R_WIDTH[3:0]:       vgen_reg_data_o <= {4'h0, xv::VISIBLE_WIDTH[11:0]};
            xv::AUX_R_HEIGHT[3:0]:      vgen_reg_data_o <= {4'h0, xv::VISIBLE_HEIGHT[11:0]};
            xv::AUX_R_FEATURES[3:0]:    vgen_reg_data_o <= 16'b1000000000000001;  // TODO define feature bits
            xv::AUX_R_SCANLINE[3:0]:    vgen_reg_data_o <= {(v_state != STATE_VISIBLE), (h_state != STATE_VISIBLE), 3'b000, v_count }; // negative when not vsync
            xv::AUX_R_GITHASH_H[3:0]:   vgen_reg_data_o <= githash[31:16];
            xv::AUX_R_GITHASH_L[3:0]:   vgen_reg_data_o <= githash[15:0];
            xv::AUX_R_UNUSED_E[3:0]: ;
            xv::AUX_R_UNUSED_F[3:0]: ;
        endcase
    end
end

always_comb     hsync = (h_state == STATE_SYNC);
always_comb     vsync = (v_state == STATE_SYNC);
always_comb     dv_display_ena = vg_enable && (h_state == STATE_VISIBLE) && (v_state == STATE_VISIBLE);
always_comb     h_start_scanout = (h_count == h_scanout_hcount) ? mem_fetch_active : 1'b0; 
always_comb     h_start_line_fetch = (~mem_fetch_active && mem_fetch_next);
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

// combinational block for video fetch start and stop
always_comb     mem_fetch_next = (v_state == STATE_VISIBLE && h_count == mem_fetch_hcount) ? ~mem_fetch_active : mem_fetch_active;
always_comb begin
    // set mem_fetch_active next toggle for video memory access
    if (mem_fetch_active) begin
        mem_fetch_hcount = H_MEM_END[10:0];
    end else begin
        mem_fetch_hcount = H_MEM_BEGIN[10:0];
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

// generate font address from index, tile y, bpp and tile size (8x8 or 8x16)
function [15:0] calc_font_addr(
        input [9:0] tile_char,
        input [3:0] tile_y,
        input [5:0] fontbank,
        input [1:0] bpp,
        input       tile_8x16
    );
    reg [15:0] temp_addr;
    begin
        case ({ bpp, tile_8x16})
            3'b000:  calc_font_addr = { fontbank, 10'b0 } | { 6'b0, tile_char[7: 0], tile_y[2:1] };         // 4W  1-BPP 8x8 (even/odd byte)
            3'b001:  calc_font_addr = { fontbank, 10'b0 } | { 5'b0, tile_char[7: 0], tile_y[3:1] };         // 8W  1-BPP 8x16 (even/odd byte)
            3'b010:  calc_font_addr = { fontbank, 10'b0 } | { 3'b0, tile_char[9: 0], tile_y[2:0] };         // 8W  2-BPP 8x8
            3'b011:  calc_font_addr = { fontbank, 10'b0 } | { 2'b0, tile_char[9: 0], tile_y[3:0] };         // 16W 2-BPP 8x16
            3'b100:  calc_font_addr = { fontbank, 10'b0 } | { 2'b0, tile_char[9: 0], tile_y[2:0], 1'b0 };   // 16W 4-BPP 8x8
            3'b101:  calc_font_addr = { fontbank, 10'b0 } | { 1'b0, tile_char[9: 0], tile_y[3:0], 1'b0 };   // 32W 4-BPP 8x16
            3'b110:  calc_font_addr = { fontbank, 10'b0 } | { 1'b0, tile_char[9: 0], tile_y[2:0], 2'b0 };   // 32W 8-BPP 8x8
            3'b111:  calc_font_addr = { fontbank, 10'b0 } | { tile_char[9: 0], tile_y[3:0], 2'b0 };         // 64W 8-BPP 8x16
        endcase
    end
endfunction

// up to 1024 tile glyphs per font (256 in 1-bpp mode)
assign pa_font_addr = calc_font_addr(vram_data_i[9: 0], (vram_data_i[11] && (pa_bpp != xv::BPP_1_ATTR)) ? pa_font_height - pa_tile_y : pa_tile_y,
                                     pa_font_bank, pa_bpp, pa_font_height[3]);  // NOTE: uses "hot" vram data output

always_ff @(posedge clk) begin
    if (reset_i) begin
        fontram_sel_o       <= 1'b0;
        vram_sel_o          <= 1'b0;
        vram_addr_o         <= 16'h0000;
        pal_index_o         <= 8'b0;
        vsync_intr_o        <= 1'b0;
        vline_intr_o        <= 1'b0;
        hsync_o             <= 1'b0;
        vsync_o             <= 1'b0;
        dv_de_o             <= 1'b0;
        vg_enable           <= 1'b0;            // video starts disabled (enabled after init)
        h_state             <= STATE_PRE_SYNC;
        v_state             <= STATE_PRE_SYNC;  // check STATE_VISIBLE
        h_count             <= 11'h000;         // horizontal counter
        v_count             <= 11'h000;         // vertical counter
        mem_fetch_active    <= 1'b0;            // true enables display memory fetch
        mem_fetch_cycle     <= 3'b0;            // memory fetch state
        h_scanout           <= 1'b0;
        h_scanout_hcount    <= 11'b0;
        pa_addr             <= 16'h0000;        // current display address during scan
        pa_line_addr        <= 16'h0000;
        pa_tile_x           <= 3'b0;            // tile column
        pa_tile_y           <= 4'b0;            // tile line
        pa_h_count          <= 2'b00;           // horizontal pixel repeat counter
        pa_v_count          <= 2'b00;           // vertical pixel repeat counter
        pa_attr_index       <= 16'h0000;        // word with tile index and attributes
        pa_data_word0       <= 16'h0000;        // buffers to queue one line of tile data
        pa_data_word1       <= 16'h0000;
        pa_data_word2       <= 16'h0000;
        pa_data_word3       <= 16'h0000;
        pa_first_buffer     <= 1'b0;
        pa_next_shiftout_ready <= 1'b0;
        pa_next_shiftout_hrev <= 1'b0;
        pa_pixel_shiftout   <= 64'h00000000;    // 8 4-bpp pixels to scan out
        pa_next_shiftout    <= 64'h00000000;    // 8 4-bpp pixels to scan out
        h_line_first_pixel  <= 1'b0;
    end else begin
        // default outputs
        vram_sel_o          <= 1'b0;            // default to no VRAM access
        fontram_sel_o       <= 1'b0;            // default to no font access
        vsync_intr_o        <= 1'b0;            // vsync interrupt strobe
        vline_intr_o        <= 1'b0;            // vsync interrupt strobe
        h_line_first_pixel  <= 1'b0;

        // set output pixel index from pixel shift-out
        pal_index_o <= pa_pixel_shiftout[63:56];

        if (h_scanout) begin
            // shift-in next pixel
            if (pa_h_count != 2'b00) begin
                pa_h_count              <= pa_h_count - 1'b1;
            end else begin
                pa_h_count              <= pa_h_repeat;
                pa_tile_x               <= pa_tile_x + 1'b1;

                if (pa_tile_x == 3'h7) begin
                    pa_next_shiftout_ready <= 1'b0;
                    if (pa_next_shiftout_hrev) begin
                         // next 8 pixels from buffer copied reversed
                        pa_pixel_shiftout   <= {
                            pa_next_shiftout[7:0],
                            pa_next_shiftout[15:8],
                            pa_next_shiftout[23:16],
                            pa_next_shiftout[31:24],
                            pa_next_shiftout[39:32],
                            pa_next_shiftout[47:40],
                            pa_next_shiftout[55:48],
                            pa_next_shiftout[63:56]
                        };
                    end else begin
                        pa_pixel_shiftout   <= pa_next_shiftout; // next 8 pixels from buffer
                    end
                end else begin
    `ifndef SYNTHESIS
                    pa_pixel_shiftout   <= { pa_pixel_shiftout[55:0], 8'hE3 };  // shift for next pixel
    `else
                    pa_pixel_shiftout   <= { pa_pixel_shiftout[55:0], 8'h00 };  // shift for next pixel
    `endif
                end
            end
        end

        // fetch display data if fetch active and not going to overrun existing shiftout (in state 2)
        if (mem_fetch_active && (!pa_next_shiftout_ready || mem_fetch_cycle != 3'h1)) begin
            mem_fetch_cycle <=  mem_fetch_cycle + 1'b1;
            // fetch state machine
            case (mem_fetch_cycle)
                3'h0: begin                 // [VI3/FI3] + VOp0 preload
                    if (pa_bpp == xv::BPP_8) begin
                        if (!pa_bitmap) begin
                            pa_data_word3   <= pa_font_in_vram ? vram_data_i : fontram_data_i;  // FI3: read font data
                        end else begin
                            pa_data_word3   <= vram_data_i;         // VI3: read vram data
                        end
                    end

                    vram_sel_o      <= 1'b1;                // VO0: select vram
                    vram_addr_o     <= pa_addr;             // put display address on vram bus
                    pa_addr         <= pa_addr + 1'b1;      // increment display address
                end
                3'h1: begin    // expand next 8 pixels into pixel_shift
                    pa_next_shiftout_ready <= !pa_first_buffer; // set buffer ready (unless first buffer)
                    pa_next_shiftout_hrev <= (pa_bpp == xv::BPP_1_ATTR) ? 1'b0 : pa_attr_index[11]; // horizontal reverse flag
                    case (pa_bpp)
                    xv::BPP_1_ATTR: // expand to 8-bits using attrib (defaults to colorbase when no attrib byte)
                        pa_next_shiftout  <= {
                            pa_colorbase[7:4], pa_data_word0[7] ? pa_attr_index[11:8] : pa_attr_index[15:12],
                            pa_colorbase[7:4], pa_data_word0[6] ? pa_attr_index[11:8] : pa_attr_index[15:12],
                            pa_colorbase[7:4], pa_data_word0[5] ? pa_attr_index[11:8] : pa_attr_index[15:12],
                            pa_colorbase[7:4], pa_data_word0[4] ? pa_attr_index[11:8] : pa_attr_index[15:12],
                            pa_colorbase[7:4], pa_data_word0[3] ? pa_attr_index[11:8] : pa_attr_index[15:12],
                            pa_colorbase[7:4], pa_data_word0[2] ? pa_attr_index[11:8] : pa_attr_index[15:12],
                            pa_colorbase[7:4], pa_data_word0[1] ? pa_attr_index[11:8] : pa_attr_index[15:12],
                            pa_colorbase[7:4], pa_data_word0[0] ? pa_attr_index[11:8] : pa_attr_index[15:12] };
                    xv::BPP_2:
                        pa_next_shiftout  <= {
                            pa_attr_index[15:10], pa_data_word0[15:14],
                            pa_attr_index[15:10], pa_data_word0[13:12],
                            pa_attr_index[15:10], pa_data_word0[11:10],
                            pa_attr_index[15:10], pa_data_word0[ 9: 8],
                            pa_attr_index[15:10], pa_data_word0[ 7: 6],
                            pa_attr_index[15:10], pa_data_word0[ 5: 4],
                            pa_attr_index[15:10], pa_data_word0[ 3: 2],
                            pa_attr_index[15:10], pa_data_word0[ 1: 0] };
                    xv::BPP_4:
                        pa_next_shiftout  <= {
                            pa_attr_index[15:12], pa_data_word0[15:12],
                            pa_attr_index[15:12], pa_data_word0[11: 8],
                            pa_attr_index[15:12], pa_data_word0[ 7: 4],
                            pa_attr_index[15:12], pa_data_word0[ 3: 0],
                            pa_attr_index[15:12], pa_data_word1[15:12],
                            pa_attr_index[15:12], pa_data_word1[11: 8],
                            pa_attr_index[15:12], pa_data_word1[ 7: 4],
                            pa_attr_index[15:12], pa_data_word1[ 3: 0] };
                    xv::BPP_8:
                        pa_next_shiftout  <= { pa_data_word0, pa_data_word1, pa_data_word2, pa_data_word3 };
                    endcase
                end
                3'h2: begin
                    pa_data_word0   <= vram_data_i;         // VI0: read vram data
                    pa_attr_index   <= vram_data_i;         // save for tiled use as attribute/index word

                    if (!pa_bitmap) begin
                        vram_sel_o      <= pa_font_in_vram;         // FO0: select either vram
                        fontram_sel_o   <= ~pa_font_in_vram;        // FO0: or select fontram
                        vram_addr_o     <= pa_font_addr;
                        fontram_addr_o  <= pa_font_addr[11:0];
                        pa_font_next    <= pa_font_addr + 1'b1;
                    end else begin
                        if (pa_bpp != xv::BPP_1_ATTR) begin
                            pa_attr_index[15:8] <= pa_colorbase;     // default attribute color
                        end
                        if (pa_bpp == xv::BPP_4 || pa_bpp == xv::BPP_8) begin
                            vram_sel_o      <= 1'b1;                // VO1: select vram
                            vram_addr_o     <= pa_addr;             // put display address on vram bus
                            pa_addr         <= pa_addr + 1'b1;      // increment display address
                        end
                    end
                end
                3'h3: begin    // idle cycle
                end
                3'h4: begin    // [VI1/FI0] [FO1]
                    pa_data_word1   <= vram_data_i;         // VI1: read vram data

                    if (!pa_bitmap) begin
                        pa_data_word0   <= pa_font_in_vram ? vram_data_i : fontram_data_i;  // FI0: read font data

                        if (pa_bpp == xv::BPP_4 || pa_bpp == xv::BPP_8) begin
                            vram_sel_o      <= pa_font_in_vram;    // FO1: select either vram
                            fontram_sel_o   <= ~pa_font_in_vram;   // FO1: or select fontram
                            vram_addr_o     <= pa_font_addr;
                            fontram_addr_o  <= pa_font_addr[11:0];
                            pa_font_next    <= pa_font_addr + 1'b1;
                        end
                    end else begin
                        if (pa_bpp == xv::BPP_8) begin
                            vram_sel_o      <= 1'b1;                // VO2: select vram
                            vram_addr_o     <= pa_addr;             // put display address on vram bus
                            pa_addr         <= pa_addr + 1'b1;      // increment display address
                        end
                    end
                end
                3'h5: begin    //           [FO2]
                        if (!pa_bitmap) begin
                            if (pa_bpp == xv::BPP_8) begin
                                vram_sel_o      <= pa_font_in_vram;     // FO2: select either vram
                                fontram_sel_o   <= ~pa_font_in_vram;    // FO2: or select fontram
                                vram_addr_o     <= pa_font_addr;
                                fontram_addr_o  <= pa_font_addr[11:0];
                                pa_font_next    <= pa_font_addr + 1'b1;
                            end
                            // get even/odd font byte for 1 BPP
                            if (pa_bpp == xv::BPP_1_ATTR && !pa_tile_y[0]) begin
                                pa_data_word0[7:0]  <= pa_data_word0[15:8];
                            end
                        end
                end
                3'h6: begin    // [VI2/FI1] [VO3/FO3]
                    if (pa_bpp == xv::BPP_8) begin
                        pa_data_word2   <= vram_data_i;         // VI2: read vram data
                    end

                    if (!pa_bitmap) begin
                        pa_data_word1   <= pa_font_in_vram ? vram_data_i : fontram_data_i;  // FI1: read font data

                        if (pa_bpp == xv::BPP_4 || pa_bpp == xv::BPP_8) begin
                            vram_sel_o      <= pa_font_in_vram;    // FO3: select either vram
                            fontram_sel_o   <= ~pa_font_in_vram;   // FO3: or select fontram
                            vram_addr_o     <= pa_font_addr;
                            fontram_addr_o  <= pa_font_addr[11:0];
                            pa_font_next    <= pa_font_addr + 1'b1;
                        end
                    end else begin
                        if (pa_bpp == xv::BPP_8) begin
                            vram_sel_o      <= 1'b1;                // VO2: select vram
                            vram_addr_o     <= pa_addr;             // put display address on vram bus
                            pa_addr         <= pa_addr + 1'b1;      // increment display address
                        end
                    end
                end
                3'h7: begin    // [FI2]
                    pa_first_buffer <= 1'b0;
                    if (pa_bpp == xv::BPP_8) begin
                        if (!pa_bitmap) begin
                            pa_data_word2   <= pa_font_in_vram ? vram_data_i : fontram_data_i;  // FI2: read font data
                        end
                    end
                end
            endcase
        end

        // start of line display fetch
        if (h_start_line_fetch) begin       // on line fetch start signal
            mem_fetch_cycle     <= 3'h0;    // reset fetch cycle state
            pa_first_buffer     <= 1'b1;    // set first buffer flag (used to fill prefetch)
            pa_next_shiftout_ready <= 1'b0;
            h_scanout_hcount    <= H_SCANOUT_BEGIN[10:0] - { 5'b00000, pa_fine_scrollx };

`ifndef SYNTHESIS
            // trash buffers to help spot stale data
            pa_data_word0   <= 16'h0BAD;
            pa_data_word1   <= 16'h1BAD;
            pa_data_word2   <= 16'h2BAD;
            pa_data_word3   <= 16'h3BAD;
            pa_attr_index   <= 16'hE33F;
            pa_pixel_shiftout   <= 64'he3e3e3e3e3e3e3e3;
            pa_next_shiftout    <= 64'he3e3e3e3e3e3e3e3;
`endif
        end

        // when "scrolled" scanline starts outputting (before display if scrolled)
        if (h_start_scanout) begin
            h_scanout           <= 1'b1;
            pa_tile_x           <= 3'h0;
            pa_h_count          <= pa_h_repeat;
            pa_pixel_shiftout   <= pa_next_shiftout; // next 8 pixels from buffer
            pa_next_shiftout_ready <= 1'b0;
        end

        // end of line
        if (h_line_last_pixel) begin
            h_scanout   <= 1'b0;
            pa_addr     <= pa_line_addr;                    // addr back to line start (for more tile, or v repeat)
            if (pa_v_count != 2'b00) begin                  // is line repeating
                pa_v_count  <= pa_v_count - 1'b1;               // keep decrementing
            end else begin
                pa_v_count  <= pa_v_repeat;                     // reset v repeat
                if (pa_tile_y == pa_font_height || pa_bitmap) begin     // is last line of tile cell or bitmap?
                    pa_tile_y     <= 4'h0;                              // reset tile cell line
                    pa_line_addr  <= pa_line_addr + pa_line_width;      // new line start address
                    pa_addr       <= pa_line_addr + pa_line_width;      // new text start address
                end
                else begin                                          
                    pa_tile_y <= pa_tile_y + 1;                     // next line of tile cell
                end
            end
        end

        // end of frame / before next frame
        if (last_frame_pixel) begin                   // if last pixel of frame
            vg_enable       <= enable_i;                // enable/disable video generation
            pa_addr         <= pa_start_addr;           // set start of display data
            pa_line_addr    <= pa_start_addr;           // set line to start of display data

            pa_v_count      <= pa_v_repeat - pa_fine_scrolly[1:0];    // fine scroll within scaled line (v repeat)
            pa_tile_y       <= pa_fine_scrolly[5:2];    // fine scroll tile line
        end

        // use new line start if set
        if (pa_line_start_set) begin
            pa_line_addr    <= pa_line_start;
        end

        // scan line interrupt generation TODO temp until "copper"
        if (h_line_first_pixel && (v_count == v_line_intr)) begin
            vline_intr_o    <=  v_line_intr_ena;
        end
        h_line_first_pixel <= h_line_last_pixel;    // delay last pixel to get first pixel
        
        // vsync interrupt generation
        if (last_visible_pixel) begin
            vsync_intr_o    <= 1'b1;
        end

        // update registered signals from combinatorial "next" versions
        h_state <= h_state_next;
        v_state <= v_state_next;
        h_count <= h_count_next;
        v_count <= v_count_next;
        mem_fetch_active <= mem_fetch_next & ~pa_disable;

        // set other video output signals
        hsync_o     <= hsync ? xv::H_SYNC_POLARITY : ~xv::H_SYNC_POLARITY;
        vsync_o     <= vsync ? xv::V_SYNC_POLARITY : ~xv::V_SYNC_POLARITY;
        dv_de_o     <= dv_display_ena;
    end
end

endmodule
`default_nettype wire               // restore default
