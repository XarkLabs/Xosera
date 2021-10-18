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
    input  wire logic  [4:0]     vgen_reg_num_r_i,   // internal config register number (for reads)
    input  wire logic  [4:0]     vgen_reg_num_w_i,   // internal config register number (for writes)
    input  wire logic [15:0]     vgen_reg_data_i,    // data for internal config register
    output      logic [15:0]     vgen_reg_data_o,    // register/status data reads
    input wire  logic  [3:0]     intr_status_i,      // interrupt pending status
    output      logic  [3:0]     intr_signal_o,      // generate interrupt signal
    // outputs for copper
    output      logic [10:0]     h_count_o,          // Horizontal video counter
    output      logic [10:0]     v_count_o,          // Vertical video counter
    // video memories
    output      logic            vram_sel_o,         // vram read select
    output      logic [15:0]     vram_addr_o,        // vram word address out (16x64K)
    input  wire logic [15:0]     vram_data_i,        // vram word data in
    output      logic            tilemem_sel_o,      // tile mem read select
    output      logic [11:0]     tilemem_addr_o,     // tile mem word address out (16x4K)
    input  wire logic [15:0]     tilemem_data_i,     // tile mem word data in
    output      logic            spritemem_sel_o,    // sprite mem read select
    output      logic  [7:0]     spritemem_addr_o,   // sprite mem word address out (16x256)
/* verilator lint_off UNUSED */                      // HACKFAST
    input  wire logic [15:0]     spritemem_data_i,   // sprite mem word data in
/* verilator lint_on UNUSED */
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
/* verilator lint_off UNUSED */                      // HACKFAST
logic [10:0]    sprite_x;
logic [10:0]    sprite_y;
/* verilator lint_on UNUSED */
logic [10:0]    vid_top;
logic [10:0]    vid_bottom;
logic [10:0]    vid_left;
logic [10:0]    vid_right;

// playfield A generation control signals
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
// HACK: unused logic  [2:0]    mem_fetch_cycle;    // current cycle state for display memory fetch

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

assign h_count_o    = h_count;
assign v_count_o    = v_count;


// video config registers read/write
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
            case (vgen_reg_num_w_i[4:0])
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
                    pa_h_repeat     <= vgen_reg_data_i[3:2];
                    pa_v_repeat     <= vgen_reg_data_i[1:0];
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
    case (vgen_reg_num_r_i[4:0])
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

// video signal generation

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

// video pixel generation

// generate tile address from index, tile y, bpp and tile size (8x8 or 8x16)
function automatic [15:0] calc_tile_addr(
        input [9:0] tile_char,
        input [3:0] tile_y,
        input [5:0] tilebank,
        input [1:0] bpp,
        input       tile_8x16
    );
    begin
        case (bpp)
            xv::BPP_1_ATTR: begin
                if (tile_8x16) begin        
                    calc_tile_addr = { tilebank, 10'b0 } | { 5'b0, tile_char[7: 0], tile_y[3:1] };      // 8W  1-BPP 8x16 (even/odd byte)
                end else begin
                    calc_tile_addr = { tilebank, 10'b0 } | { 6'b0, tile_char[7: 0], tile_y[2:1] };      // 4W  1-BPP 8x8 (even/odd byte)
                end
            end
            xv::BPP_4: begin
                calc_tile_addr = { tilebank, 10'b0 } | { 2'b0, tile_char[9: 0], tile_y[2:0], 1'b0 };    // 16W 4-BPP 8x8
            end
            default: begin
                calc_tile_addr = { tilebank, 10'b0 } | { 1'b0, tile_char[9: 0], tile_y[2:0], 2'b0 };    // 32W 8-BPP 8x8
            end
        endcase
    end
endfunction

// display line fetch generation FSM
typedef enum logic [3:0] {
    FETCH_IDLE          =   4'h0,    // idle, waiting for line start
    FETCH_ADDR_DISP     =   4'h1,    // put bitmap/tilemap address on VRAM addr bus (and READ_TILE_3)
    FETCH_WAIT_DISP     =   4'h2,    // wait for bitmap/tilemap data
    FETCH_READ_DISP_0   =   4'h3,    // read bitmap/tilemap from VRAM data bus
    // bitmap
    FETCH_READ_DISP_1   =   4'h4,    // read bitmap data from VRAM data bus
    FETCH_READ_DISP_2   =   4'h5,    // read bitmap data from VRAM data bus
    FETCH_READ_DISP_3   =   4'h6,    // read bitmap data from VRAM data bus
    // tiled
    FETCH_ADDR_TILE     =   4'h7,    // put tile address on VRAM/TILE addr bus
    FETCH_WAIT_TILE     =   4'h8,    // wait for tilemap data
    FETCH_READ_TILE_0   =   4'h9,    // read tile data from VRAM/TILE data bus
    FETCH_READ_TILE_1   =   4'hA,    // read bitmap/tile data from VRAM/TILE data bus
    FETCH_READ_TILE_2   =   4'hB    // read bitmap/tile data from VRAM/TILE data bus
} vgen_fetch_st;

// scanline generation (registered signals and "_next" combinatorally set signals)
logic [$size(vgen_fetch_st)-1:0]     pa_fetch, pa_fetch_next;            // plane A generation FSM state

// fsm inputs
logic           pa_first_buffer;                    // used to ignore first buffer to fill prefetch
logic           pa_out_buffer_empty;

// fsm outputs
logic [15:0]    pa_addr, pa_addr_next;              // address to fetch display bitmap/tilemap
logic [15:0]    pa_tile_addr, pa_tile_addr_next;    // tile start address (VRAM or TILERAM)
logic [15:0]    pa_tile_incr;                       // tile pa_tile_addr + 1
// HACK: unused? logic           pa_pixels_buf_full, pa_pixels_buf_full_next;

logic           vram_sel, vram_sel_next;            // vram select output
logic [15:0]    vram_addr, vram_addr_next;          // vram_address output
logic           tilemem_sel, tilemem_sel_next;      // tilemem select output
logic [15:0]    tilemem_addr, tilemem_addr_next;    // tilemem address output

logic           pa_words_ready, pa_words_ready_next;// true if data_words full (8-pixels)
logic [15:0]    pa_tile_attr, pa_tile_attr_next;    // tile attributes and index
logic [15:0]    pa_data_word0, pa_data_word0_next;  // 1st fetched display data buffer
logic [15:0]    pa_data_word1, pa_data_word1_next;  // 2nd fetched display data buffer
logic [15:0]    pa_data_word2, pa_data_word2_next;  // 3rd fetched display data buffer
logic [15:0]    pa_data_word3, pa_data_word3_next;  // 4th fetched display data buffer (8 BPP)

logic           pa_pixels_buf_hrev;                 // horizontal reverse flag
// HACK: unused? logic           pa_pixels_buf_empty;                // true when pa_pixel_out needs filling
logic [63:0]    pa_pixels_buf;                      // 8 pixel buffer waiting for scan out

logic [63:0]    pa_pixels;                          // 8 pixels currently shifting to scan out

logic           pa_line_start_set;                  // true if pa_line_start changed (register write)
logic [15:0]    pa_line_start;                      // address of next line display data start

// fetch FSM state register
always_ff @(posedge clk) begin
    pa_fetch   <= pa_fetch_next;
end

// fetch FSM combinational logic
always_comb begin
    // set default outputs
    pa_fetch_next       = pa_fetch;
    vram_sel_next       = vram_sel;
    vram_addr_next      = vram_addr;
    pa_addr_next        = pa_addr;
    pa_data_word0_next  = pa_data_word0;
    pa_data_word1_next  = pa_data_word1;
    pa_data_word2_next  = pa_data_word2;
    pa_data_word3_next  = pa_data_word3;
    pa_tile_attr_next   = pa_tile_attr;
    tilemem_sel_next    = tilemem_sel;
    tilemem_addr_next   = tilemem_addr;
    pa_words_ready_next = pa_words_ready;

    // TODO: vrev pa_tile_attr[11]
    pa_tile_addr_next   = calc_tile_addr(pa_tile_attr[9: 0], pa_tile_y, pa_tile_bank, pa_bpp, pa_tile_height[3]);

    case (pa_fetch)
        FETCH_IDLE: begin
            if (mem_fetch_active) begin                         // delay scanline until mem_fetch_active
                pa_fetch_next  = FETCH_ADDR_DISP;
            end
        end
        FETCH_ADDR_DISP: begin
            // read pre-loaded last font word
            if ((pa_bpp == xv::BPP_8) | (pa_bpp == xv::BPP_XX)) begin
                pa_data_word3_next  = pa_tile_in_vram ? vram_data_i : tilemem_data_i;  // TI3: read tile data
            end
            vram_addr_next  = pa_addr;                     // put display address on vram bus
            pa_addr_next    = pa_addr + 1'b1;              // increment display address
            if (!mem_fetch_active) begin
                vram_sel_next   = 1'b1;                        // VO0: select vram
                pa_fetch_next   = FETCH_IDLE;
            end else begin
                if (pa_out_buffer_empty) begin                     // if room in buffer
                    vram_sel_next   = 1'b1;                        // VO0: select vram
                    pa_fetch_next   = FETCH_WAIT_DISP;
                end
            end
        end
        FETCH_WAIT_DISP: begin
            if (pa_bitmap && (pa_bpp != xv::BPP_1_ATTR)) begin
                vram_sel_next   = 1'b1;                        // VO1: select vram
                vram_addr_next  = pa_addr;                     // put display address on vram bus
                pa_addr_next    = pa_addr + 1'b1;              // increment display address
            end
            pa_words_ready_next = !pa_first_buffer; // set buffer ready (unless first buffer)   // HACK: ???
            pa_fetch_next  = FETCH_READ_DISP_0;
        end
        FETCH_READ_DISP_0: begin
            pa_data_word0_next  = vram_data_i;                 // VI0: read vram data
            pa_tile_attr_next    = vram_data_i;                 // also save for use as tile attribute

            if (pa_bitmap) begin
                if (pa_bpp == xv::BPP_1_ATTR) begin
                    pa_fetch_next = FETCH_ADDR_DISP;           // done if BPP_1 bitmap
                end else begin
                    if (pa_bpp != xv::BPP_4) begin
                        vram_sel_next   = 1'b1;                // VO2: select vram
                        vram_addr_next  = pa_addr;             // put display address on vram bus
                        pa_addr_next    = pa_addr + 1'b1;      // increment display address
                    end
                    pa_fetch_next = FETCH_READ_DISP_1;         // else read more bitmap words
                end
            end else begin
                pa_fetch_next = FETCH_ADDR_TILE;                // read tile bitmap words
            end
        end
        FETCH_READ_DISP_1: begin
            pa_data_word1_next  = vram_data_i;                 // VI1: read vram data

            if (pa_bpp == xv::BPP_4) begin
                pa_fetch_next = FETCH_ADDR_DISP;               // done if BPP_4 bitmap
            end else begin
                vram_sel_next   = 1'b1;                        // VO3: select vram
                vram_addr_next  = pa_addr;                     // put display address on vram bus
                pa_addr_next    = pa_addr + 1'b1;              // increment display address
                pa_fetch_next   = FETCH_READ_DISP_2;           // read more bitmap words
            end
        end
        FETCH_READ_DISP_2: begin
            pa_data_word2_next  = vram_data_i;                 // VI2: read vram data
            pa_fetch_next       = FETCH_READ_DISP_3;           // read last bitmap word
        end
        FETCH_READ_DISP_3: begin
            pa_data_word3_next  = vram_data_i;                 // VI3: read vram data
            pa_fetch_next       = FETCH_ADDR_DISP;             // done
        end
        FETCH_ADDR_TILE: begin
            vram_sel_next       = pa_tile_in_vram;             // TO0: select either vram
            vram_addr_next      = pa_tile_addr;
            tilemem_sel_next    = ~pa_tile_in_vram;            // TO0: or select tilemem
            tilemem_addr_next   = pa_tile_addr;

            pa_fetch_next       = FETCH_WAIT_TILE;
        end
        FETCH_WAIT_TILE: begin
            if (pa_bpp != xv::BPP_1_ATTR) begin
                vram_sel_next       = pa_tile_in_vram;         // TO1: select either vram
                vram_addr_next      = pa_tile_addr;
                tilemem_sel_next    = ~pa_tile_in_vram;        // TO1: or select tilemem
                tilemem_addr_next   = pa_tile_incr;
            end
            pa_fetch_next  = FETCH_READ_TILE_0;
        end
        FETCH_READ_TILE_0: begin
            pa_data_word0_next  = pa_tile_in_vram ? vram_data_i : tilemem_data_i;  // TI0: read tile data

            if (pa_bpp == xv::BPP_1_ATTR) begin
                if (!pa_tile_y[0]) begin
                    pa_data_word0_next[7:0] = pa_tile_in_vram ? vram_data_i[15:8] : tilemem_data_i[15:8];
                end
                pa_fetch_next = FETCH_ADDR_DISP;               // done if BPP_1 bitmap
            end else begin
                if (pa_bpp != xv::BPP_4) begin
                    vram_sel_next       = pa_tile_in_vram;     // TO2: select either vram
                    vram_addr_next      = pa_tile_addr;
                    tilemem_sel_next    = ~pa_tile_in_vram;    // TO2: or select tilemem
                    tilemem_addr_next   = pa_tile_incr;
                end
                pa_fetch_next = FETCH_READ_TILE_1;             // else read more bitmap words
            end
        end
        FETCH_READ_TILE_1: begin
            pa_data_word1_next  = pa_tile_in_vram ? vram_data_i : tilemem_data_i;  // TI1: read tile data

            if (pa_bpp == xv::BPP_4) begin
                pa_fetch_next = FETCH_ADDR_DISP;               // done if BPP_4 bitmap
            end else begin
                vram_sel_next       = pa_tile_in_vram;         // TO3: select either vram
                vram_addr_next      = pa_tile_addr;
                tilemem_sel_next    = ~pa_tile_in_vram;        // TO3: or select tilemem
                tilemem_addr_next   = pa_tile_incr;
                pa_fetch_next       = FETCH_READ_TILE_2;       // else read more tile data words
            end
        end
        FETCH_READ_TILE_2: begin
            pa_data_word2_next  = pa_tile_in_vram ? vram_data_i : tilemem_data_i;  // TI2: read tile data
            pa_fetch_next       = FETCH_ADDR_DISP;              // NOTE will read TI3 also
        end
        default: begin
            pa_fetch_next = FETCH_IDLE;
        end
    endcase
end

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
// HACK:unused        mem_fetch_cycle     <= 3'b0;            // memory fetch state
        h_scanout           <= 1'b0;
        h_scanout_hcount    <= 11'b0;
        h_scanout_end_hcount<= 11'b0;
        pa_addr             <= 16'h0000;        // current display address during scan
        pa_line_start       <= 16'h0000;
        pa_tile_x           <= 3'b0;            // tile column
        pa_tile_y           <= 4'b0;            // tile line
        pa_h_count          <= 2'b00;           // horizontal pixel repeat counter
        pa_v_count          <= 2'b00;           // vertical pixel repeat counter
        pa_tile_attr        <= 16'h0000;        // byte with tile attributes
        pa_data_word0       <= 16'h0000;        // buffers to queue one line of tile data
        pa_data_word1       <= 16'h0000;
        pa_data_word2       <= 16'h0000;
        pa_data_word3       <= 16'h0000;
        pa_first_buffer     <= 1'b0;
        pa_pixels_buf_hrev         <= 1'b0;
// HACK:unused        pa_pixels_buf_full  <= 1'b0;
        pa_pixels_buf       <= 64'h00000000;    // 8 8-bpp pixels to scan out
        pa_pixels           <= 64'h00000000;    // 8 8-bpp pixels to scan out
    end else begin

        // set default outputs
        pa_fetch        <= pa_fetch_next;
        vram_sel        <= vram_sel_next;
        vram_addr       <= vram_addr_next;
        pa_addr         <= pa_addr_next;
        pa_tile_addr    <= pa_tile_addr_next;
        pa_tile_incr    <= pa_tile_addr_next + 1'b1;
        pa_data_word0   <= pa_data_word0_next;
        pa_data_word1   <= pa_data_word1_next;
        pa_data_word2   <= pa_data_word2_next;
        pa_data_word3   <= pa_data_word3_next;
        pa_tile_attr    <= pa_tile_attr_next;
        tilemem_sel     <= tilemem_sel_next;
        tilemem_addr    <= tilemem_addr_next;
        pa_words_ready  <= pa_words_ready_next;

        // default outputs
        vram_sel_o          <= 1'b0;            // default to no VRAM access
        tilemem_sel_o       <= 1'b0;            // default to no tile access
        spritemem_sel_o     <= 1'b0;            // default to no sprite access

        // set output pixel index from pixel shift-out
        color_index_o <= pa_pixels[63:56];

        // have display words been fetched?
        if (pa_words_ready) begin
            // expand them into 8-bit pixels
            case (pa_bpp)
            xv::BPP_1_ATTR: // expand to 8-bits using attrib (defaults to colorbase when no attrib byte)
                pa_pixels_buf  <= {
                    pa_colorbase[7:4], pa_data_word0[7] ? pa_tile_attr[11:8] : pa_tile_attr[15:12],
                    pa_colorbase[7:4], pa_data_word0[6] ? pa_tile_attr[11:8] : pa_tile_attr[15:12],
                    pa_colorbase[7:4], pa_data_word0[5] ? pa_tile_attr[11:8] : pa_tile_attr[15:12],
                    pa_colorbase[7:4], pa_data_word0[4] ? pa_tile_attr[11:8] : pa_tile_attr[15:12],
                    pa_colorbase[7:4], pa_data_word0[3] ? pa_tile_attr[11:8] : pa_tile_attr[15:12],
                    pa_colorbase[7:4], pa_data_word0[2] ? pa_tile_attr[11:8] : pa_tile_attr[15:12],
                    pa_colorbase[7:4], pa_data_word0[1] ? pa_tile_attr[11:8] : pa_tile_attr[15:12],
                    pa_colorbase[7:4], pa_data_word0[0] ? pa_tile_attr[11:8] : pa_tile_attr[15:12] };
            xv::BPP_4:
                pa_pixels_buf  <= {
                    pa_tile_attr[15:12], pa_data_word0[15:12],
                    pa_tile_attr[15:12], pa_data_word0[11: 8],
                    pa_tile_attr[15:12], pa_data_word0[ 7: 4],
                    pa_tile_attr[15:12], pa_data_word0[ 3: 0],
                    pa_tile_attr[15:12], pa_data_word1[15:12],
                    pa_tile_attr[15:12], pa_data_word1[11: 8],
                    pa_tile_attr[15:12], pa_data_word1[ 7: 4],
                    pa_tile_attr[15:12], pa_data_word1[ 3: 0] };
            xv::BPP_8,
            xv::BPP_XX:
                pa_pixels_buf  <= { pa_data_word0, pa_data_word1, pa_data_word2, pa_data_word3 };
            endcase

            if (pa_bitmap || (pa_bpp == xv::BPP_1_ATTR)) begin
                pa_pixels_buf_hrev  <= 1'b0;                // no horizontal reverse in bitmap or BPP_1
            end else begin
                pa_pixels_buf_hrev  <= pa_tile_attr[10];    // use horizontal reverse attrib
            end
        end

        if (h_scanout) begin
            // shift-in next pixel
            if (pa_h_count != 2'b00) begin
                pa_h_count              <= pa_h_count - 1'b1;
            end else begin
                pa_h_count              <= pa_h_repeat;
                pa_tile_x               <= pa_tile_x + 1'b1;

                if (pa_tile_x == 3'h7) begin
                    pa_out_buffer_empty <= 1'b1;
                    if (pa_pixels_buf_hrev) begin
                         // next 8 pixels from buffer copied reversed
                        pa_pixels   <= {
                            pa_pixels_buf[7:0],
                            pa_pixels_buf[15:8],
                            pa_pixels_buf[23:16],
                            pa_pixels_buf[31:24],
                            pa_pixels_buf[39:32],
                            pa_pixels_buf[47:40],
                            pa_pixels_buf[55:48],
                            pa_pixels_buf[63:56]
                        };
                    end else begin
                        pa_pixels   <= pa_pixels_buf; // next 8 pixels from buffer
                    end
                end else begin
    `ifndef SYNTHESIS
                    pa_pixels   <= { pa_pixels[55:0], 8'hE3 };  // shift for next pixel
    `else
                    pa_pixels   <= { pa_pixels[55:0], 8'h00 };  // shift for next pixel
    `endif
                end
            end
        end

`ifdef NOTDEF
        // fetch display data if fetch active and not going to overrun existing out (in state 2)
        if (mem_fetch_active && (pa_pixels_buf_empty || mem_fetch_cycle != 3'h1)) begin
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
                    pa_pixels_buf_empty <= pa_first_buffer; // set buffer empty (unless first buffer)
                    pa_next_out_hrev <= (pa_bpp == xv::BPP_1_ATTR) ? 1'b0 : pa_attr[3]; // horizontal reverse flag
                    case (pa_bpp)
                    xv::BPP_1_ATTR: // expand to 8-bits using attrib (defaults to colorbase when no attrib byte)
                        pa_next_out  <= {
                            pa_colorbase[7:4], pa_data_word0[7] ? pa_attr[3:0] : pa_attr[7:4],
                            pa_colorbase[7:4], pa_data_word0[6] ? pa_attr[3:0] : pa_attr[7:4],
                            pa_colorbase[7:4], pa_data_word0[5] ? pa_attr[3:0] : pa_attr[7:4],
                            pa_colorbase[7:4], pa_data_word0[4] ? pa_attr[3:0] : pa_attr[7:4],
                            pa_colorbase[7:4], pa_data_word0[3] ? pa_attr[3:0] : pa_attr[7:4],
                            pa_colorbase[7:4], pa_data_word0[2] ? pa_attr[3:0] : pa_attr[7:4],
                            pa_colorbase[7:4], pa_data_word0[1] ? pa_attr[3:0] : pa_attr[7:4],
                            pa_colorbase[7:4], pa_data_word0[0] ? pa_attr[3:0] : pa_attr[7:4] };
                    xv::BPP_2:
                        pa_next_out  <= {
                            pa_attr[7:2], pa_data_word0[15:14],
                            pa_attr[7:2], pa_data_word0[13:12],
                            pa_attr[7:2], pa_data_word0[11:10],
                            pa_attr[7:2], pa_data_word0[ 9: 8],
                            pa_attr[7:2], pa_data_word0[ 7: 6],
                            pa_attr[7:2], pa_data_word0[ 5: 4],
                            pa_attr[7:2], pa_data_word0[ 3: 2],
                            pa_attr[7:2], pa_data_word0[ 1: 0] };
                    xv::BPP_4:
                        pa_next_out  <= {
                            pa_attr[7:4], pa_data_word0[15:12],
                            pa_attr[7:4], pa_data_word0[11: 8],
                            pa_attr[7:4], pa_data_word0[ 7: 4],
                            pa_attr[7:4], pa_data_word0[ 3: 0],
                            pa_attr[7:4], pa_data_word1[15:12],
                            pa_attr[7:4], pa_data_word1[11: 8],
                            pa_attr[7:4], pa_data_word1[ 7: 4],
                            pa_attr[7:4], pa_data_word1[ 3: 0] };
                    xv::BPP_8:
                        pa_next_out  <= { pa_data_word0, pa_data_word1, pa_data_word2, pa_data_word3 };
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
`endif

        // start of line display fetch
        if (h_start_line_fetch) begin       // on line fetch start signal
// HACK: unused            mem_fetch_cycle         <= 3'h0;    // reset fetch cycle state
            pa_first_buffer         <= 1'b1;    // set first buffer flag (used to fill prefetch)
// HACK: unused            pa_pixels_buf_empty     <= 1'b1;
            h_scanout_hcount        <= H_SCANOUT_BEGIN[10:0] + { { 6{pa_fine_hscroll[4]} }, pa_fine_hscroll };
            h_scanout_end_hcount    <= H_SCANOUT_END[10:0];   // TODO + vid_right;

`ifndef SYNTHESIS
            // trash buffers to help spot stale data
            pa_data_word0   <= 16'h0BAD;
            pa_data_word1   <= 16'h1BAD;
            pa_data_word2   <= 16'h2BAD;
            pa_data_word3   <= 16'h3BAD;
            pa_tile_attr    <= 16'hE3E3;
            pa_pixels       <= 64'he3e3e3e3e3e3e3e3;
            pa_pixels_buf   <= 64'he3e3e3e3e3e3e3e3;
`endif
            pa_pixels[63:56] <= pa_colorbase;
        end

        // when "scrolled" scanline starts outputting (before display if scrolled)
        if (h_start_scanout) begin
            h_scanout           <= 1'b1;
            pa_tile_x           <= 3'h0;
            pa_h_count          <= pa_h_repeat;
            pa_pixels           <= pa_pixels_buf; // next 8 pixels from buffer
// HACK: unused            pa_pixels_buf_empty <= 1'b1;
        end

        if (h_end_scanout) begin
            h_scanout           <= 1'b0;
            pa_pixels[63:56]    <= pa_colorbase;
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

        // end of frame or blanked, prepare for next frame
        if (pa_blank || last_frame_pixel) begin                   // if last pixel of frame
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
