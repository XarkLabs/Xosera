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
xv::playfield_t pa;
logic [15:0]    pa_line_addr;                       // display data start address for next line (word address)
logic  [1:0]    pa_h_count;                         // current horizontal repeat countdown
logic  [1:0]    pa_v_count;                         // current vertical repeat countdown
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

logic           h_start_scanout;                        // scanout start strobe
logic           h_end_scanout;                          // scanout stop strobe
logic           h_scanout;                              // scanout active
logic [10:0]    h_scanout_hcount;                       // horizontal pixel count to start scanout
logic [10:0]    h_scanout_end_hcount;                   // horizontal pixel count to stop scanout

logic           mem_fetch_active;                       // true when fetching display data
logic [10:0]    mem_fetch_hcount;                       // horizontal count when mem_fetch_active toggles

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

        pa.blank            <= 1'b0;            // plane A starts enabled
        pa.start_addr       <= 16'h0000;
        pa.line_len         <= xv::TILES_WIDE[15:0];
        pa.fine_hscroll     <= 5'b0;
        pa.fine_vscroll     <= 6'b0;
        pa.tile_height      <= 4'b1111;
        pa.tile_bank        <= 6'b0;
        pa.tile_in_vram     <= 1'b0;
        pa.bitmap           <= 1'b0;
        pa.bpp              <= xv::BPP_1_ATTR;
        pa.colorbase        <= 8'h00;
        pa.h_repeat         <= 2'b0;
        pa.v_repeat         <= 2'b0;

        pa_line_start_set   <= 1'b0;            // indicates user line address set
        pa_line_addr        <= 16'h0000;        // user start of next display line
        pa_h_count          <= 2'b0;
        pa_v_count          <= 2'b0;
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
                    pa.colorbase    <= vgen_reg_data_i[15:8];
                    pa.blank        <= vgen_reg_data_i[7];
                    pa.bitmap       <= vgen_reg_data_i[6];
                    pa.bpp          <= vgen_reg_data_i[5:4];
                    pa.h_repeat     <= vgen_reg_data_i[3:2];
                    pa.v_repeat     <= vgen_reg_data_i[1:0];
                end
                xv::XR_PA_TILE_CTRL[4:0]: begin
                    pa.tile_bank    <= vgen_reg_data_i[15:10];
                    pa.tile_in_vram <= vgen_reg_data_i[7];
                    pa.tile_height  <= vgen_reg_data_i[3:0];
                end
                xv::XR_PA_DISP_ADDR[4:0]: begin
                    pa.start_addr   <= vgen_reg_data_i;
                end
                xv::XR_PA_LINE_LEN[4:0]: begin
                    pa.line_len   <= vgen_reg_data_i;
                end
                xv::XR_PA_HV_SCROLL[4:0]: begin
                    pa.fine_hscroll <= vgen_reg_data_i[12:8];
                    pa.fine_vscroll <= vgen_reg_data_i[5:0];
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
        xv::XR_PA_GFX_CTRL[4:0]:    vgen_reg_data_o <= { pa.colorbase, pa.blank, pa.bitmap, pa.bpp, pa.v_repeat, pa.h_repeat };
        xv::XR_PA_TILE_CTRL[4:0]:   vgen_reg_data_o <= { pa.tile_bank, 2'b0, pa.tile_in_vram, 3'b0, pa.tile_height };
        xv::XR_PA_DISP_ADDR[4:0]:   vgen_reg_data_o <= pa.start_addr;
        xv::XR_PA_LINE_LEN[4:0]:    vgen_reg_data_o <= pa.line_len;
        xv::XR_PA_HV_SCROLL[4:0]:   vgen_reg_data_o <= { 3'b0, pa.fine_hscroll, 2'b00, pa.fine_vscroll };
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

// display line fetch generation FSM
typedef enum logic [3:0] {
    FETCH_IDLE          =   4'h0,   // idle, waiting for line start
    // bitmap
    FETCH_ADDR_DISP     =   4'h1,   // output bitmap VRAM address (and read tile word3 data)
    FETCH_WAIT_DISP     =   4'h2,   // wait for bitmap data
    FETCH_READ_DISP_0   =   4'h3,   // read bitmap word0/tilemap from VRAM
    FETCH_READ_DISP_1   =   4'h4,   // read bitmap word1 data from VRAM
    FETCH_READ_DISP_2   =   4'h5,   // read bitmap word2 data from VRAM
    FETCH_READ_DISP_3   =   4'h6,   // read bitmap word3 data from VRAM
    // tiled
    FETCH_ADDR_TILEMAP  =   4'h7,   // output tilemap VRAM address (and read tile word3 data)
    FETCH_WAIT_TILEMAP  =   4'h8,   // wait for tilemap data
    FETCH_READ_TILEMAP  =   4'h9,   // read tilemap from VRAM
    FETCH_ADDR_TILE     =   4'hA,   // output tile word0 VRAM/TILE address
    FETCH_WAIT_TILE     =   4'hB,   // wait for tilemap data, output word1 tile addr
    FETCH_READ_TILE_0   =   4'hC,   // read tile word0 data from bus, output word2 tile addr
    FETCH_READ_TILE_1   =   4'hD,   // read tile word1 data from bus, output word3 tile addr
    FETCH_READ_TILE_2   =   4'hE    // read tile word2 data from bus
} vgen_fetch_st;

// scanline generation (registered signals and "_next" combinatorally set signals)
logic [3:0]     pa_fetch, pa_fetch_next;            // plane A generation FSM state

// fsm outputs
logic [15:0]    pa_addr, pa_addr_next;              // address to fetch display bitmap/tilemap
logic [15:0]    pa_tile_addr, pa_tile_addr_next;    // tile start address (VRAM or TILERAM)
logic [15:0]    pa_tile_incr;                       // tile pa_tile_addr + 1

logic           vram_sel_next;                      // vram select output
logic [15:0]    vram_addr_next;                     // vram_address output
logic           tilemem_sel_next;                   // tilemem select output
logic [15:0]    tilemem_addr, tilemem_addr_next;    // tilemem address output

logic           pa_initial_buf, pa_initial_buf_next;// true on first buffer per scanline
logic           pa_words_ready, pa_words_ready_next;// true if data_words full (8-pixels)
logic [15:0]    pa_tile_attr, pa_tile_attr_next;    // tile attributes and tile index
logic [15:0]    pa_data_word0, pa_data_word0_next;  // 1st fetched display data word buffer
logic [15:0]    pa_data_word1, pa_data_word1_next;  // 2nd fetched display data word buffer
logic [15:0]    pa_data_word2, pa_data_word2_next;  // 3rd fetched display data word buffer
logic [15:0]    pa_data_word3, pa_data_word3_next;  // 4th fetched display data word buffer

logic           pa_pixels_buf_empty;                // true when pa_pixel_out needs filling
logic           pa_pixels_buf_hrev;                 // horizontal reverse flag
logic [63:0]    pa_pixels_buf;                      // 8 pixel buffer waiting for scan out

logic [63:0]    pa_pixels;                          // 8 pixels currently shifting to scan out

logic           pa_line_start_set;                  // true if pa_line_start changed (register write)
logic [15:0]    pa_line_start;                      // address of next line display data start

// fetch FSM combinational logic
always_comb begin
    // set default outputs
    pa_fetch_next       = pa_fetch;
    vram_sel_next       = 1'b0;
    vram_addr_next      = vram_addr_o;
    pa_addr_next        = pa_addr;
    pa_data_word0_next  = pa_data_word0;
    pa_data_word1_next  = pa_data_word1;
    pa_data_word2_next  = pa_data_word2;
    pa_data_word3_next  = pa_data_word3;
    pa_tile_attr_next   = pa_tile_attr;
    tilemem_sel_next    = 1'b0;
    tilemem_addr_next   = tilemem_addr;
    pa_words_ready_next = 1'b0;
    pa_initial_buf_next = pa_initial_buf;
    pa_tile_addr_next   = pa_tile_addr;

    case (pa_fetch)
        FETCH_IDLE: begin
            if (mem_fetch_active) begin                         // delay scanline until mem_fetch_active
                if (pa.bitmap) begin
                    pa_fetch_next  = FETCH_ADDR_DISP;
                end else begin
                    pa_fetch_next  = FETCH_ADDR_TILEMAP;
                end
            end
        end
        FETCH_ADDR_DISP: begin
            if (!mem_fetch_active) begin                        // stop if no longer fetching
                pa_fetch_next   = FETCH_IDLE;
            end else begin
                if (pa_pixels_buf_empty) begin                  // if room in buffer
                    vram_sel_next   = 1'b1;                     // VO0: select vram
                    vram_addr_next  = pa_addr;                          // put display address on vram bus
                    pa_addr_next    = pa_addr + 1'b1;           // increment display address
                    pa_fetch_next   = FETCH_WAIT_DISP;
                end
            end
        end
        FETCH_WAIT_DISP: begin
            if (pa.bpp != xv::BPP_1_ATTR) begin
                vram_sel_next   = 1'b1;                        // VO1: select vram
                vram_addr_next  = pa_addr;                     // put display address on vram bus
                pa_addr_next    = pa_addr + 1'b1;              // increment display address
            end
            pa_words_ready_next = !pa_initial_buf;              // set buffer ready
            pa_initial_buf_next = 1'b0;
            pa_fetch_next  = FETCH_READ_DISP_0;
        end
        FETCH_READ_DISP_0: begin
            pa_data_word0_next  = vram_data_i;                 // VI0: read vram data
            pa_tile_attr_next   = vram_data_i;                 // set attributes for 1_BPP_ATTR

            if (pa.bitmap) begin
                if (pa.bpp == xv::BPP_1_ATTR) begin
                    pa_fetch_next = FETCH_ADDR_DISP;           // done if BPP_1 bitmap
                end else begin
                    if (pa.bpp != xv::BPP_4) begin
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
            pa_tile_attr_next[15:8] = pa.colorbase;            // set attributes to colorbase
            if (pa.bpp == xv::BPP_4) begin
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

        FETCH_ADDR_TILEMAP: begin
            // read pre-loaded font word3
            if ((pa.bpp == xv::BPP_8) || (pa.bpp == xv::BPP_XX)) begin
                pa_data_word3_next  = pa.tile_in_vram ? vram_data_i : tilemem_data_i;  // TI3: read tile data
            end
            if (!mem_fetch_active) begin                        // stop if no longer fetching
                pa_fetch_next   = FETCH_IDLE;
            end else begin
                if (pa_pixels_buf_empty) begin                  // if room in buffer
                    vram_sel_next   = 1'b1;                     // VO0: select vram
                    vram_addr_next  = pa_addr;                          // put display address on vram bus
                    pa_addr_next    = pa_addr + 1'b1;           // increment display address
                    pa_fetch_next   = FETCH_WAIT_TILEMAP;
                end
            end
        end
        FETCH_WAIT_TILEMAP: begin
            pa_words_ready_next = !pa_initial_buf;              // set buffer ready
            pa_initial_buf_next = 1'b0;
            pa_fetch_next  = FETCH_READ_TILEMAP;
        end

        FETCH_READ_TILEMAP: begin
            pa_tile_attr_next   = vram_data_i;                  // save for use as tile attribute
            pa_fetch_next       = FETCH_ADDR_TILE;              // read tile bitmap words
        end
        FETCH_ADDR_TILE: begin
            vram_sel_next       = pa.tile_in_vram;              // TO0: select either vram
            vram_addr_next      = pa_tile_addr;
            tilemem_sel_next    = ~pa.tile_in_vram;             // TO0: or select tilemem
            tilemem_addr_next   = pa_tile_addr;

            pa_fetch_next       = FETCH_WAIT_TILE;
        end
        FETCH_WAIT_TILE: begin
            if (pa.bpp != xv::BPP_1_ATTR) begin
                vram_sel_next       = pa.tile_in_vram;          // TO1: select either vram
                vram_addr_next      = pa_tile_addr;
                tilemem_sel_next    = ~pa.tile_in_vram;         // TO1: or select tilemem
                tilemem_addr_next   = pa_tile_incr;
            end
            pa_fetch_next  = FETCH_READ_TILE_0;
        end
        FETCH_READ_TILE_0: begin
            pa_data_word0_next  = pa.tile_in_vram ? vram_data_i : tilemem_data_i;  // TI0: read tile data

            if (pa.bpp == xv::BPP_1_ATTR) begin                 // in BPP_1 select even/odd byte from tile word
                if (!pa_tile_y[0]) begin
                    pa_data_word0_next[7:0] = pa.tile_in_vram ? vram_data_i[15:8] : tilemem_data_i[15:8];
                end
                pa_fetch_next = FETCH_ADDR_TILEMAP;               // done if BPP_1 bitmap
            end else begin
                if (pa.bpp != xv::BPP_4) begin
                    vram_sel_next       = pa.tile_in_vram;     // TO2: select either vram
                    vram_addr_next      = pa_tile_addr;
                    tilemem_sel_next    = ~pa.tile_in_vram;    // TO2: or select tilemem
                    tilemem_addr_next   = pa_tile_incr;
                end
                pa_fetch_next = FETCH_READ_TILE_1;             // else read more bitmap words
            end
        end
        FETCH_READ_TILE_1: begin
            pa_data_word1_next  = pa.tile_in_vram ? vram_data_i : tilemem_data_i;  // TI1: read tile data

            if (pa.bpp == xv::BPP_4) begin
                pa_fetch_next = FETCH_ADDR_TILEMAP;               // done if BPP_4 bitmap
            end else begin
                vram_sel_next       = pa.tile_in_vram;         // TO3: select either vram
                vram_addr_next      = pa_tile_addr;
                tilemem_sel_next    = ~pa.tile_in_vram;        // TO3: or select tilemem
                tilemem_addr_next   = pa_tile_incr;
                pa_fetch_next       = FETCH_READ_TILE_2;       // else read more tile data words
            end
        end
        FETCH_READ_TILE_2: begin
            pa_data_word2_next  = pa.tile_in_vram ? vram_data_i : tilemem_data_i;  // TI2: read tile data
            pa_fetch_next       = FETCH_ADDR_TILEMAP;              // NOTE will read TI3 also
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
        h_scanout           <= 1'b0;
        h_scanout_hcount    <= 11'b0;
        h_scanout_end_hcount<= 11'b0;
        pa_addr             <= 16'h0000;        // current display address during scan
        pa_line_start       <= 16'h0000;        // display address for start of scan line
        pa_tile_x           <= 3'b0;            // tile column
        pa_tile_y           <= 4'b0;            // tile line
        pa_h_count          <= 2'b00;           // horizontal pixel repeat counter
        pa_v_count          <= 2'b00;           // vertical pixel repeat counter
        pa_tile_attr        <= 16'h0000;        // word with tile attributes and index
        pa_data_word0       <= 16'h0000;        // buffers for unexpanded display data
        pa_data_word1       <= 16'h0000;
        pa_data_word2       <= 16'h0000;
        pa_data_word3       <= 16'h0000;
        pa_initial_buf      <= 1'b0;
        pa_pixels_buf_empty <= 1'b0;            // flag when pa_pixels_buf is empty (continue fetching)
        pa_pixels_buf_hrev  <= 1'b0;            // flag to horizontally reverse pa_pixels_buf
        pa_pixels_buf       <= 64'h00000000;    // next 8 8-bpp pixels to scan out
        pa_pixels           <= 64'h00000000;    // 8 8-bpp pixels currently scanning out
    end else begin
    
        // fetch FSM clocked process
        // register fetch combinitorial signals
        pa_fetch        <= pa_fetch_next;
        pa_addr         <= pa_addr_next;
//        pa_tile_addr    <= pa_tile_addr_next;
        pa_tile_incr    <= pa_tile_addr_next + 1'b1;
        pa_data_word0   <= pa_data_word0_next;
        pa_data_word1   <= pa_data_word1_next;
        pa_data_word2   <= pa_data_word2_next;
        pa_data_word3   <= pa_data_word3_next;
        pa_tile_attr    <= pa_tile_attr_next;
        tilemem_addr    <= tilemem_addr_next;
        pa_initial_buf  <= pa_initial_buf_next;
        pa_words_ready  <= pa_words_ready_next;
        pa_tile_addr    <= calc_tile_addr(pa_tile_attr_next[xv::TILE_INDEX+:10], pa_tile_y, pa.tile_bank, pa.bpp, pa.tile_height[3], pa_tile_attr_next[xv::TILE_ATTR_VREV]);

        vram_sel_o      <= vram_sel_next;
        vram_addr_o     <= vram_addr_next;
        tilemem_sel_o   <= tilemem_sel_next;
        tilemem_addr_o  <= tilemem_addr_next[11:0];

        pa_fetch        <= pa_fetch_next;

        // default outputs
        spritemem_sel_o <= 1'b0;            // default to no sprite access

        // have display words been fetched?
        if (pa_words_ready) begin
            // expand display data into 8 8-bit pixels in pa_pixels_buf
            pa_pixels_buf_empty <= 1'b0;

            case (pa.bpp)
            xv::BPP_1_ATTR:
                // expand to 8-bit index using upper 4-bits of colorbase
                // register and 4-bit attribute foreground/background index
                // based on pixel bit set/clear
                pa_pixels_buf  <= {
                    pa.colorbase[7:4], pa_data_word0[7] ? pa_tile_attr[xv::TILE_ATTR_FORE+:4] : pa_tile_attr[xv::TILE_ATTR_BACK+:4],
                    pa.colorbase[7:4], pa_data_word0[6] ? pa_tile_attr[xv::TILE_ATTR_FORE+:4] : pa_tile_attr[xv::TILE_ATTR_BACK+:4],
                    pa.colorbase[7:4], pa_data_word0[5] ? pa_tile_attr[xv::TILE_ATTR_FORE+:4] : pa_tile_attr[xv::TILE_ATTR_BACK+:4],
                    pa.colorbase[7:4], pa_data_word0[4] ? pa_tile_attr[xv::TILE_ATTR_FORE+:4] : pa_tile_attr[xv::TILE_ATTR_BACK+:4],
                    pa.colorbase[7:4], pa_data_word0[3] ? pa_tile_attr[xv::TILE_ATTR_FORE+:4] : pa_tile_attr[xv::TILE_ATTR_BACK+:4],
                    pa.colorbase[7:4], pa_data_word0[2] ? pa_tile_attr[xv::TILE_ATTR_FORE+:4] : pa_tile_attr[xv::TILE_ATTR_BACK+:4],
                    pa.colorbase[7:4], pa_data_word0[1] ? pa_tile_attr[xv::TILE_ATTR_FORE+:4] : pa_tile_attr[xv::TILE_ATTR_BACK+:4],
                    pa.colorbase[7:4], pa_data_word0[0] ? pa_tile_attr[xv::TILE_ATTR_FORE+:4] : pa_tile_attr[xv::TILE_ATTR_BACK+:4] };
            xv::BPP_4:
                // expand to 8-bit index using 4-bit color extension attribute
                // and 4-bit pixel value
                pa_pixels_buf  <= {
                    pa_tile_attr[xv::TILE_ATTR_BACK+:4], pa_data_word0[15:12],
                    pa_tile_attr[xv::TILE_ATTR_BACK+:4], pa_data_word0[11: 8],
                    pa_tile_attr[xv::TILE_ATTR_BACK+:4], pa_data_word0[ 7: 4],
                    pa_tile_attr[xv::TILE_ATTR_BACK+:4], pa_data_word0[ 3: 0],
                    pa_tile_attr[xv::TILE_ATTR_BACK+:4], pa_data_word1[15:12],
                    pa_tile_attr[xv::TILE_ATTR_BACK+:4], pa_data_word1[11: 8],
                    pa_tile_attr[xv::TILE_ATTR_BACK+:4], pa_data_word1[ 7: 4],
                    pa_tile_attr[xv::TILE_ATTR_BACK+:4], pa_data_word1[ 3: 0] };
            xv::BPP_8,
            xv::BPP_XX:
                // directly copy 8-bit pixel indices
                pa_pixels_buf  <= { pa_data_word0, pa_data_word1, pa_data_word2, pa_data_word3 };
            endcase
            // keep flag with these 8 pixels for H reverse
            if (pa.bitmap || (pa.bpp == xv::BPP_1_ATTR)) begin
                pa_pixels_buf_hrev  <= 1'b0;                // no horizontal reverse in bitmap or BPP_1
            end else begin
                pa_pixels_buf_hrev  <= pa_tile_attr[xv::TILE_ATTR_HREV];    // use horizontal reverse attrib
            end
        end

        // set output pixel index from pixel shift-out
        color_index_o <= pa_pixels[63:56];

        if (h_scanout) begin
            // shift-in next pixel
            if (pa_h_count != 2'b00) begin
                pa_h_count              <= pa_h_count - 1'b1;
            end else begin
                pa_h_count              <= pa.h_repeat;
                pa_tile_x               <= pa_tile_x + 1'b1;

                if (pa_tile_x == 3'h7) begin
                    pa_pixels_buf_empty <= !pa_initial_buf;  // TODO: 1'b1
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


        // start of line display fetch
        if (h_start_line_fetch) begin       // on line fetch start signal
            pa_initial_buf          <= 1'b1;
            pa_pixels_buf_empty     <= 1'b1;
            h_scanout_hcount        <= H_SCANOUT_BEGIN[10:0] + { { 6{pa.fine_hscroll[4]} }, pa.fine_hscroll };
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
            pa_pixels[63:56] <= pa.colorbase;
        end

        // when "scrolled" scanline starts outputting (before display if scrolled)
        if (h_start_scanout) begin
            h_scanout           <= 1'b1;
            pa_tile_x           <= 3'h0;
            pa_h_count          <= pa.h_repeat;
            pa_pixels           <= pa_pixels_buf; // next 8 pixels from buffer
            pa_pixels_buf_empty <= 1'b1;
        end

        if (h_end_scanout) begin
            h_scanout           <= 1'b0;
            pa_pixels[63:56]    <= pa.colorbase;
        end

        // end of line
        if (h_line_last_pixel) begin
            h_scanout   <= 1'b0;
            pa_addr     <= pa_line_start;                   // addr back to line start (for more tile, or v repeat)
            if (pa_v_count != 2'b00) begin                  // is line repeating
                pa_v_count  <= pa_v_count - 1'b1;               // keep decrementing
            end else begin
                pa_v_count  <= pa.v_repeat;                     // reset v repeat
                if (pa_tile_y == pa.tile_height || pa.bitmap) begin // is last line of tile cell or bitmap?
                    pa_tile_y     <= 4'h0;                              // reset tile cell line
                    pa_line_start <= pa_line_start + pa.line_len;        // new line start address
                    pa_addr       <= pa_line_start + pa.line_len;        // new text start address
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
        if (pa.blank || last_frame_pixel) begin                   // if last pixel of frame
            pa_addr         <= pa.start_addr;           // set start of display data
            pa_line_start   <= pa.start_addr;           // set line to start of display data

            pa_v_count      <= pa.v_repeat - pa.fine_vscroll[1:0];    // fine scroll within scaled line (v repeat)
            pa_tile_y       <= pa.fine_vscroll[5:2];    // fine scroll tile line
        end

        // update registered signals from combinatorial "next" versions
        h_state <= h_state_next;
        v_state <= v_state_next;
        h_count <= h_count_next;
        v_count <= v_count_next;
        mem_fetch_active <= mem_fetch_next & ~pa.blank;

        // set other video output signals
        hsync_o     <= hsync ? xv::H_SYNC_POLARITY : ~xv::H_SYNC_POLARITY;
        vsync_o     <= vsync ? xv::V_SYNC_POLARITY : ~xv::V_SYNC_POLARITY;
        dv_de_o     <= dv_display_ena;
    end
end

endmodule
`default_nettype wire               // restore default
