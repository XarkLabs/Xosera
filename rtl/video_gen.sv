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
    input  wire logic            vgen_reg_wr_i,      // strobe to write internal config register number
    input  wire logic  [4:0]     vgen_reg_num_i,     // internal config register number
    input  wire logic [15:0]     vgen_reg_data_i,    // data for internal config register
    output      logic [15:0]     vgen_reg_data_o,    // register/status data reads
    input wire  logic  [3:0]     intr_status_i,      // interrupt pending status
    output      logic  [3:0]     intr_signal_o,      // generate interrupt signal
    // video memories
    output      logic            vram_sel_o,         // vram read select
    output      logic [15:0]     vram_addr_o,        // vram word address out (16x64K)
    input  wire logic [15:0]     vram_data_i,        // vram word data in
    output      logic            tilemem_sel_o,      // tile mem read select
    output      logic [11:0]     tilemem_addr_o,     // tile mem word address out (16x4K)
    input  wire logic [15:0]     tilemem_data_i,     // tile mem word data in
    output      logic            spritemem_sel_o,    // sprite mem read select
    output      logic  [7:0]     spritemem_addr_o,   // sprite mem word address out (16x256)
    input  wire logic [15:0]     spritemem_data_i,   // sprite mem word data in
    // video signal outputs
    output      logic  [7:0]     color_index_o,      // color palette index output (16x256)
    output      logic            vsync_o, hsync_o,   // video sync outputs
    output      logic            dv_de_o,            // video active signal (needed for HDMI)
    // standard signals
    input  wire logic            reset_i,            // system reset in
    input  wire logic            clk                 // clock (video pixel clock)
);

localparam [31:0] githash = 32'H`GITHASH;

localparam H_MEM_BEGIN = xv::OFFSCREEN_WIDTH-64;    // memory prefetch starts early
localparam H_MEM_END = xv::TOTAL_WIDTH-8;           // memory fetch can end a bit early
localparam H_SCANOUT_BEGIN = xv::OFFSCREEN_WIDTH-2; // h count position to start line scanout
localparam H_SCANOUT_END = xv::TOTAL_WIDTH; // h count position to start line scanout

// video generation signals
logic [7:0]     border_color;
logic [10:0]    cursor_x;
logic [10:0]    cursor_y;
logic [10:0]    sprite_x;
logic [10:0]    sprite_y;
logic [10:0]    vid_top;
logic [10:0]    vid_bottom;
logic [10:0]    vid_left;
logic [10:0]    vid_right;

// playfield A generation signals
logic           pa_blank;                           // disable plane A
logic [15:0]    pa_start_addr;                      // display data start address (word address)
logic [15:0]    pa_line_len;                        // words per disply line (added to line_addr each line)
logic [15:0]    pa_line_addr;                       // display data start address for next line (word address)
logic  [7:0]    pa_colorbase;                       // colorbase for plane data (upper color bits)
logic  [1:0]    pa_bpp;                             // bpp code (bpp_depth_t)
logic           pa_bitmap;                          // bitmap enable (else text mode)
logic  [5:0]    pa_tile_bank;                       // vram/tilemem tile bank 0-3 (0/1 with 8x16) tilemem, or 2KB/4K
logic           pa_tile_in_vram;                    // 0=tilemem, 1=vram
logic  [3:0]    pa_tile_height;                     // max height of tile cell
logic  [1:0]    pa_h_repeat;                        // horizontal pixel repeat
logic  [1:0]    pa_h_count;                         // current horizontal repeat countdown
logic  [1:0]    pa_v_repeat;                        // vertical pixel repeat
logic  [1:0]    pa_v_count;                         // current vertical repeat countdown
logic  [4:0]    pa_fine_hscroll;                    // horizontal fine scroll (8 pixel * 4 for repeat)
logic  [5:0]    pa_fine_vscroll;                    // vertical fine scroll (16 lines * 4 for repeat)
logic  [2:0]    pa_tile_x;                          // current column of tile cell
logic  [3:0]    pa_tile_y;                          // current line of tile cell

// internal signals
logic           pa_line_start_set;
logic [15:0]    pa_line_start;                      // address of next line display data start
logic [15:0]    pa_addr;                            // address to fetch tile+color attribute
logic [15:0]    pa_tile_addr;                       // tile tile start address (VRAM or TILERAM)
logic [15:0]    pa_tile_next;                       // next tile data address (VRAM or TILERAM)

// temp tile and pixel fetch buffers
logic [7:0]     pa_attr;                      // tile attributes and index
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
logic           h_end_scanout;
logic           h_scanout;
logic [10:0]    h_scanout_hcount;
logic [10:0]    h_scanout_end_hcount;

logic [10:0]    mem_fetch_hcount;   // horizontal count when mem_fetch_active toggles
logic  [2:0]    mem_fetch_cycle;    // current cycle state for display memory fetch

// sync condition indicators (combinatorial)
logic           hsync;
logic           vsync;
logic           dv_display_ena;
logic           h_line_last_pixel;
logic           last_visible_pixel;
logic           last_frame_pixel;
logic [1:0]     h_state_next;
logic [1:0]     v_state_next;
logic           mem_fetch_next;
logic           h_start_line_fetch;

// video config registers write
always_ff @(posedge clk) begin
    if (reset_i) begin
        intr_signal_o       <= 4'b0;
        border_color        <= 8'h00;
        cursor_x            <= 11'h180;
        cursor_y            <= 11'h100;
        vid_top             <= 11'h0;
        vid_bottom          <= xv::VISIBLE_HEIGHT[10:0];
        vid_left            <= 11'h0;
        vid_right           <= xv::VISIBLE_WIDTH[10:0];
        pa_blank            <= 1'b0;            // plane A starts enabled
        pa_start_addr       <= 16'h0000;
        pa_line_len         <= xv::TILES_WIDE[15:0];
        pa_line_start_set   <= 1'b0;            // indicates user line address set
        pa_line_addr        <= 16'h0000;        // user start of next display line
        pa_fine_hscroll     <= 5'b0;
        pa_fine_vscroll     <= 6'b0;
        pa_tile_height      <= 4'b1111;
        pa_tile_bank        <= 6'b0;
        pa_tile_in_vram     <= 1'b0;
        pa_bitmap           <= 1'b0;            // bitmap mode
        pa_bpp              <= xv::BPP_1_ATTR;
        pa_colorbase        <= 8'h00;
        pa_h_repeat         <= 2'b0;
        pa_v_repeat         <= 2'b0;
    end else begin
        intr_signal_o       <= 4'b0;
        pa_line_start_set   <= 1'b0;
        // video register write
        if (vgen_reg_wr_i) begin
            case (vgen_reg_num_i[4:0])
                xv::XR_VID_CTRL[4:0]: begin
                    border_color    <= vgen_reg_data_i[15:8];
                    intr_signal_o   <= vgen_reg_data_i[3:0];
                end
                xv::XR_COPP_CTRL[4:0]: begin
                    // TODO copper
                end
                xv::XR_CURSOR_X[4:0]: begin
                    cursor_x        <= vgen_reg_data_i[10:0];
                end
                xv::XR_CURSOR_Y[4:0]: begin
                    cursor_y        <= vgen_reg_data_i[10:0];
                end
                xv::XR_VID_TOP[4:0]: begin
                    vid_top        <= vgen_reg_data_i[10:0];
                end
                xv::XR_VID_BOTTOM[4:0]: begin
                    vid_bottom     <= vgen_reg_data_i[10:0];
                end
                xv::XR_VID_LEFT[4:0]: begin
                    vid_left       <= vgen_reg_data_i[10:0];
                end
                xv::XR_VID_RIGHT[4:0]: begin
                    vid_right      <= vgen_reg_data_i[10:0];
                end
                xv::XR_PA_GFX_CTRL[4:0]: begin
                    pa_colorbase    <= vgen_reg_data_i[15:8];
                    pa_blank        <= vgen_reg_data_i[7];
                    pa_bitmap       <= vgen_reg_data_i[6];
                    pa_bpp          <= vgen_reg_data_i[5:4];
                    pa_v_repeat     <= vgen_reg_data_i[3:2];
                    pa_h_repeat     <= vgen_reg_data_i[1:0];
                end
                xv::XR_PA_TILE_CTRL[4:0]: begin
                    pa_tile_bank    <= vgen_reg_data_i[15:10];
                    pa_tile_in_vram <= vgen_reg_data_i[7];
                    pa_tile_height  <= vgen_reg_data_i[3:0];
                end
                xv::XR_PA_DISP_ADDR[4:0]: begin
                    pa_start_addr   <= vgen_reg_data_i;
                end
                xv::XR_PA_LINE_LEN[4:0]: begin
                    pa_line_len   <= vgen_reg_data_i;
                end
                xv::XR_PA_HV_SCROLL[4:0]: begin
                    pa_fine_hscroll <= vgen_reg_data_i[12:8];
                    pa_fine_vscroll <= vgen_reg_data_i[5:0];
                end
                xv::XR_PA_LINE_ADDR[4:0]: begin
                    pa_line_start_set <= 1'b1;
                    pa_line_addr   <= vgen_reg_data_i;
                end
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
    case (vgen_reg_num_i[4:0])
        xv::XR_VID_CTRL[4:0]:       vgen_reg_data_o <= {border_color, 4'bx, intr_status_i };
        xv::XR_COPP_CTRL[4:0]:      vgen_reg_data_o <= {16'h0000 }; // TODO copper
        xv::XR_CURSOR_X[4:0]:       vgen_reg_data_o <= {5'b0, cursor_x };
        xv::XR_CURSOR_Y[4:0]:       vgen_reg_data_o <= {5'b0, cursor_y };
        xv::XR_VID_TOP[4:0]:        vgen_reg_data_o <= {5'b0, vid_top };
        xv::XR_VID_BOTTOM[4:0]:     vgen_reg_data_o <= {5'b0, vid_bottom };
        xv::XR_VID_LEFT[4:0]:       vgen_reg_data_o <= {5'b0, vid_left };
        xv::XR_VID_RIGHT[4:0]:      vgen_reg_data_o <= {5'b0, vid_right };
        xv::XR_SCANLINE[4:0]:       vgen_reg_data_o <= {(v_state != STATE_VISIBLE), (h_state != STATE_VISIBLE), 3'b000, v_count };
        xv::XR_VERSION[4:0]:        vgen_reg_data_o <= { 4'b0000, 12'h`VERSION };
        xv::XR_GITHASH_H[4:0]:      vgen_reg_data_o <= githash[31:16];
        xv::XR_GITHASH_L[4:0]:      vgen_reg_data_o <= githash[15:0];
        xv::XR_VID_HSIZE[4:0]:      vgen_reg_data_o <= {6'h0, xv::VISIBLE_WIDTH[9:0]};
        xv::XR_VID_VSIZE[4:0]:      vgen_reg_data_o <= {6'h0, xv::VISIBLE_HEIGHT[9:0]};
        xv::XR_VID_VFREQ[4:0]:      vgen_reg_data_o <= xv::REFRESH_FREQ;
        xv::XR_PA_GFX_CTRL[4:0]:    vgen_reg_data_o <= { pa_colorbase, pa_blank, pa_bitmap, pa_bpp, pa_v_repeat, pa_h_repeat };
        xv::XR_PA_TILE_CTRL[4:0]:   vgen_reg_data_o <= { pa_tile_bank, 2'b0, pa_tile_in_vram, 3'b0, pa_tile_height };
        xv::XR_PA_DISP_ADDR[4:0]:   vgen_reg_data_o <= pa_start_addr;
        xv::XR_PA_LINE_LEN[4:0]:    vgen_reg_data_o <= pa_line_len;
        xv::XR_PA_HV_SCROLL[4:0]:   vgen_reg_data_o <= { 3'b0, pa_fine_hscroll, 2'b00, pa_fine_vscroll };
        default:                    vgen_reg_data_o <= 16'h0000;
    endcase
end

always_comb     hsync = (h_state == STATE_SYNC);
always_comb     vsync = (v_state == STATE_SYNC);
always_comb     dv_display_ena = (h_state == STATE_VISIBLE) && (v_state == STATE_VISIBLE);
always_comb     h_start_scanout = (h_count == h_scanout_hcount) ? mem_fetch_active : 1'b0; 
always_comb     h_end_scanout = (h_count == h_scanout_end_hcount) ? 1'b1 : 1'b0; 
always_comb     h_start_line_fetch = (~mem_fetch_active && mem_fetch_next);
always_comb     h_line_last_pixel = (h_state_next == STATE_PRE_SYNC) && (h_state == STATE_VISIBLE);
always_comb     last_visible_pixel = (v_state_next == STATE_PRE_SYNC) && (v_state == STATE_VISIBLE) && h_line_last_pixel;
always_comb     last_frame_pixel = (v_state_next == STATE_VISIBLE) && (v_state == STATE_POST_SYNC) && h_line_last_pixel;
always_comb     sprite_x = h_count - cursor_x;
always_comb     sprite_y = v_count - cursor_y;

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

// generate tile address from index, tile y, bpp and tile size (8x8 or 8x16)
function automatic [15:0] calc_tile_addr(
        input [9:0] tile_char,
        input [3:0] tile_y,
        input [5:0] tilebank,
        input [1:0] bpp,
        input       tile_8x16
    );
    begin
        case ({ bpp, tile_8x16})
            3'b000:  calc_tile_addr = { tilebank, 10'b0 } | { 6'b0, tile_char[7: 0], tile_y[2:1] };         // 4W  1-BPP 8x8 (even/odd byte)
            3'b001:  calc_tile_addr = { tilebank, 10'b0 } | { 5'b0, tile_char[7: 0], tile_y[3:1] };         // 8W  1-BPP 8x16 (even/odd byte)
            3'b010:  calc_tile_addr = { tilebank, 10'b0 } | { 3'b0, tile_char[9: 0], tile_y[2:0] };         // 8W  2-BPP 8x8
            3'b011:  calc_tile_addr = { tilebank, 10'b0 } | { 2'b0, tile_char[9: 0], tile_y[3:0] };         // 16W 2-BPP 8x16
            3'b100:  calc_tile_addr = { tilebank, 10'b0 } | { 2'b0, tile_char[9: 0], tile_y[2:0], 1'b0 };   // 16W 4-BPP 8x8
            3'b101:  calc_tile_addr = { tilebank, 10'b0 } | { 1'b0, tile_char[9: 0], tile_y[3:0], 1'b0 };   // 32W 4-BPP 8x16
            3'b110:  calc_tile_addr = { tilebank, 10'b0 } | { 1'b0, tile_char[9: 0], tile_y[2:0], 2'b0 };   // 32W 8-BPP 8x8
            3'b111:  calc_tile_addr = { tilebank, 10'b0 } | { tile_char[9: 0], tile_y[3:0], 2'b0 };         // 64W 8-BPP 8x16
        endcase
    end
endfunction

// up to 1024 tile glyphs per tile (256 in 1-bpp mode)
assign pa_tile_addr = calc_tile_addr(vram_data_i[9: 0], (vram_data_i[11] && (pa_bpp != xv::BPP_1_ATTR)) ? pa_tile_height - pa_tile_y : pa_tile_y,
                                     pa_tile_bank, pa_bpp, pa_tile_height[3]);  // NOTE: uses "hot" vram data output

always_ff @(posedge clk) begin
    if (reset_i) begin
        vram_sel_o          <= 1'b0;
        vram_addr_o         <= 16'h0000;
        tilemem_sel_o       <= 1'b0;
        tilemem_addr_o      <= 12'h000;
        spritemem_sel_o     <= 1'b0;
        spritemem_addr_o    <= 8'h00;
        color_index_o       <= 8'b0;
        hsync_o             <= 1'b0;
        vsync_o             <= 1'b0;
        dv_de_o             <= 1'b0;
        h_state             <= STATE_PRE_SYNC;
        v_state             <= STATE_PRE_SYNC;  // check STATE_VISIBLE
        h_count             <= 11'h000;         // horizontal counter
        v_count             <= 11'h000;         // vertical counter
        mem_fetch_active    <= 1'b0;            // true enables display memory fetch
        mem_fetch_cycle     <= 3'b0;            // memory fetch state
        h_scanout           <= 1'b0;
        h_scanout_hcount    <= 11'b0;
        h_scanout_end_hcount<= 11'b0;
        pa_addr             <= 16'h0000;        // current display address during scan
        pa_line_start       <= 16'h0000;
        pa_tile_x           <= 3'b0;            // tile column
        pa_tile_y           <= 4'b0;            // tile line
        pa_h_count          <= 2'b00;           // horizontal pixel repeat counter
        pa_v_count          <= 2'b00;           // vertical pixel repeat counter
        pa_attr             <= 8'h00;           // byte with tile attributes
        pa_data_word0       <= 16'h0000;        // buffers to queue one line of tile data
        pa_data_word1       <= 16'h0000;
        pa_data_word2       <= 16'h0000;
        pa_data_word3       <= 16'h0000;
        pa_first_buffer     <= 1'b0;
        pa_next_shiftout_ready <= 1'b0;
        pa_next_shiftout_hrev <= 1'b0;
        pa_pixel_shiftout   <= 64'h00000000;    // 8 4-bpp pixels to scan out
        pa_next_shiftout    <= 64'h00000000;    // 8 4-bpp pixels to scan out
    end else begin
        // default outputs
        vram_sel_o          <= 1'b0;            // default to no VRAM access
        tilemem_sel_o       <= 1'b0;            // default to no tile access
        spritemem_sel_o     <= 1'b0;            // default to no sprite access

        // set output pixel index from pixel shift-out
        color_index_o <= pa_pixel_shiftout[63:56];

`ifndef BADJUJU
        // sprite (TODO: this is pretty crappy 😅)
        spritemem_sel_o <= 1'b0;
        if (sprite_y[10:5] == 6'b0) begin
            if (sprite_x[1:0] == 2'b11) begin
/* verilator lint_off UNUSED */
                logic [10:0] sprite_inc;
/* verilator lint_on UNUSED */
                sprite_inc = sprite_x + 1'b1;
                spritemem_sel_o     <= 1'b1;
                spritemem_addr_o    <= { sprite_y[4:0], sprite_inc[4:2]};
            end
            if (sprite_x[10:5] == 6'b0) begin
                    logic [3:0] sprite_color;
                    case (sprite_x[1:0])
                        2'b01: sprite_color    <= spritemem_data_i[15:12];
                        2'b10: sprite_color    <= spritemem_data_i[11:8];
                        2'b11: sprite_color    <= spritemem_data_i[7:4];
                        2'b00: sprite_color    <= spritemem_data_i[3:0];
                    endcase
                    if (sprite_color != 4'b0000) begin
                        color_index_o <= { border_color[7:4], sprite_color };
                    end
            end
        end
`endif

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
                            pa_data_word3   <= pa_tile_in_vram ? vram_data_i : tilemem_data_i;  // FI3: read tile data
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
                    pa_next_shiftout_hrev <= (pa_bpp == xv::BPP_1_ATTR) ? 1'b0 : pa_attr[3]; // horizontal reverse flag
                    case (pa_bpp)
                    xv::BPP_1_ATTR: // expand to 8-bits using attrib (defaults to colorbase when no attrib byte)
                        pa_next_shiftout  <= {
                            pa_colorbase[7:4], pa_data_word0[7] ? pa_attr[3:0] : pa_attr[7:4],
                            pa_colorbase[7:4], pa_data_word0[6] ? pa_attr[3:0] : pa_attr[7:4],
                            pa_colorbase[7:4], pa_data_word0[5] ? pa_attr[3:0] : pa_attr[7:4],
                            pa_colorbase[7:4], pa_data_word0[4] ? pa_attr[3:0] : pa_attr[7:4],
                            pa_colorbase[7:4], pa_data_word0[3] ? pa_attr[3:0] : pa_attr[7:4],
                            pa_colorbase[7:4], pa_data_word0[2] ? pa_attr[3:0] : pa_attr[7:4],
                            pa_colorbase[7:4], pa_data_word0[1] ? pa_attr[3:0] : pa_attr[7:4],
                            pa_colorbase[7:4], pa_data_word0[0] ? pa_attr[3:0] : pa_attr[7:4] };
                    xv::BPP_2:
                        pa_next_shiftout  <= {
                            pa_attr[7:2], pa_data_word0[15:14],
                            pa_attr[7:2], pa_data_word0[13:12],
                            pa_attr[7:2], pa_data_word0[11:10],
                            pa_attr[7:2], pa_data_word0[ 9: 8],
                            pa_attr[7:2], pa_data_word0[ 7: 6],
                            pa_attr[7:2], pa_data_word0[ 5: 4],
                            pa_attr[7:2], pa_data_word0[ 3: 2],
                            pa_attr[7:2], pa_data_word0[ 1: 0] };
                    xv::BPP_4:
                        pa_next_shiftout  <= {
                            pa_attr[7:4], pa_data_word0[15:12],
                            pa_attr[7:4], pa_data_word0[11: 8],
                            pa_attr[7:4], pa_data_word0[ 7: 4],
                            pa_attr[7:4], pa_data_word0[ 3: 0],
                            pa_attr[7:4], pa_data_word1[15:12],
                            pa_attr[7:4], pa_data_word1[11: 8],
                            pa_attr[7:4], pa_data_word1[ 7: 4],
                            pa_attr[7:4], pa_data_word1[ 3: 0] };
                    xv::BPP_8:
                        pa_next_shiftout  <= { pa_data_word0, pa_data_word1, pa_data_word2, pa_data_word3 };
                    endcase
                end
                3'h2: begin
                    pa_data_word0   <= vram_data_i;         // VI0: read vram data
                    pa_attr         <= vram_data_i[15:8];   // save for use as tile attribute

                    if (!pa_bitmap) begin
                        vram_sel_o      <= pa_tile_in_vram;         // FO0: select either vram
                        tilemem_sel_o   <= ~pa_tile_in_vram;        // FO0: or select tilemem
                        vram_addr_o     <= pa_tile_addr;
                        tilemem_addr_o  <= pa_tile_addr[11:0];
                        pa_tile_next    <= pa_tile_addr + 1'b1;
                    end else begin
                        if (pa_bpp != xv::BPP_1_ATTR) begin
                            pa_attr <= pa_colorbase;          // default attribute color
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
                        pa_data_word0   <= pa_tile_in_vram ? vram_data_i : tilemem_data_i;  // FI0: read tile data

                        if (pa_bpp == xv::BPP_4 || pa_bpp == xv::BPP_8) begin
                            vram_sel_o      <= pa_tile_in_vram;    // FO1: select either vram
                            tilemem_sel_o   <= ~pa_tile_in_vram;   // FO1: or select tilemem
                            vram_addr_o     <= pa_tile_next;
                            tilemem_addr_o  <= pa_tile_next[11:0];
                            pa_tile_next    <= pa_tile_next + 1'b1;
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
                                vram_sel_o      <= pa_tile_in_vram;     // FO2: select either vram
                                tilemem_sel_o   <= ~pa_tile_in_vram;    // FO2: or select tilemem
                                vram_addr_o     <= pa_tile_next;
                                tilemem_addr_o  <= pa_tile_next[11:0];
                                pa_tile_next    <= pa_tile_next + 1'b1;
                            end
                            // get even/odd tile byte for 1 BPP
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
                        pa_data_word1   <= pa_tile_in_vram ? vram_data_i : tilemem_data_i;  // FI1: read tile data

                        if (pa_bpp == xv::BPP_4 || pa_bpp == xv::BPP_8) begin
                            vram_sel_o      <= pa_tile_in_vram;    // FO3: select either vram
                            tilemem_sel_o   <= ~pa_tile_in_vram;   // FO3: or select tilemem
                            vram_addr_o     <= pa_tile_next;
                            tilemem_addr_o  <= pa_tile_next[11:0];
                            pa_tile_next    <= pa_tile_next + 1'b1;
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
                            pa_data_word2   <= pa_tile_in_vram ? vram_data_i : tilemem_data_i;  // FI2: read tile data
                        end
                    end
                end
            endcase
        end

        // start of line display fetch
        if (h_start_line_fetch) begin       // on line fetch start signal
            mem_fetch_cycle         <= 3'h0;    // reset fetch cycle state
            pa_first_buffer         <= 1'b1;    // set first buffer flag (used to fill prefetch)
            pa_next_shiftout_ready  <= 1'b0;
            h_scanout_hcount        <= H_SCANOUT_BEGIN[10:0] + { { 6{pa_fine_hscroll[4]} }, pa_fine_hscroll };
            h_scanout_end_hcount    <= H_SCANOUT_END[10:0];   // TODO + vid_right;

`ifndef SYNTHESIS
            // trash buffers to help spot stale data
            pa_data_word0   <= 16'h0BAD;
            pa_data_word1   <= 16'h1BAD;
            pa_data_word2   <= 16'h2BAD;
            pa_data_word3   <= 16'h3BAD;
            pa_attr         <= 8'hE3;
            pa_pixel_shiftout   <= 64'he3e3e3e3e3e3e3e3;
            pa_next_shiftout    <= 64'he3e3e3e3e3e3e3e3;
`endif
            pa_pixel_shiftout[63:56]    <= pa_colorbase;
        end

        // when "scrolled" scanline starts outputting (before display if scrolled)
        if (h_start_scanout) begin
            h_scanout           <= 1'b1;
            pa_tile_x           <= 3'h0;
            pa_h_count          <= pa_h_repeat;
            pa_pixel_shiftout   <= pa_next_shiftout; // next 8 pixels from buffer
            pa_next_shiftout_ready <= 1'b0;
        end

        if (h_end_scanout) begin
            h_scanout                   <= 1'b0;
            pa_pixel_shiftout[63:56]    <= pa_colorbase;
        end

        // end of line
        if (h_line_last_pixel) begin
            h_scanout   <= 1'b0;
            pa_addr     <= pa_line_start;                   // addr back to line start (for more tile, or v repeat)
            if (pa_v_count != 2'b00) begin                  // is line repeating
                pa_v_count  <= pa_v_count - 1'b1;               // keep decrementing
            end else begin
                pa_v_count  <= pa_v_repeat;                     // reset v repeat
                if (pa_tile_y == pa_tile_height || pa_bitmap) begin // is last line of tile cell or bitmap?
                    pa_tile_y     <= 4'h0;                              // reset tile cell line
                    pa_line_start <= pa_line_start + pa_line_len;        // new line start address
                    pa_addr       <= pa_line_start + pa_line_len;        // new text start address
                end
                else begin                                          
                    pa_tile_y <= pa_tile_y + 1;                     // next line of tile cell
                end
            end
        end

        // use new line start if it has been set
        if (pa_line_start_set) begin
            pa_line_start   <= pa_line_addr;
            pa_tile_y       <= 4'b0;                    // reset tile_y to restart new text line
        end

        // end of frame / before next frame
        if (last_frame_pixel) begin                   // if last pixel of frame
            pa_addr         <= pa_start_addr;           // set start of display data
            pa_line_start   <= pa_start_addr;           // set line to start of display data

            pa_v_count      <= pa_v_repeat - pa_fine_vscroll[1:0];    // fine scroll within scaled line (v repeat)
            pa_tile_y       <= pa_fine_vscroll[5:2];    // fine scroll tile line
        end

        // update registered signals from combinatorial "next" versions
        h_state <= h_state_next;
        v_state <= v_state_next;
        h_count <= h_count_next;
        v_count <= v_count_next;
        mem_fetch_active <= mem_fetch_next & ~pa_blank;

        // set other video output signals
        hsync_o     <= hsync ? xv::H_SYNC_POLARITY : ~xv::H_SYNC_POLARITY;
        vsync_o     <= vsync ? xv::V_SYNC_POLARITY : ~xv::V_SYNC_POLARITY;
        dv_de_o     <= dv_display_ena;
    end
end

endmodule
`default_nettype wire               // restore default
