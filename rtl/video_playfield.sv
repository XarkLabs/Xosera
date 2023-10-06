// video_playfield.sv
//
// vim: set et ts=4 sw=4
//
// Copyright (c) 2021 Xark - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)
//

`default_nettype none               // mandatory for Verilog sanity
`timescale 1ns/1ps                  // mandatory to shut up Icarus Verilog

`include "xosera_pkg.sv"

module video_playfield (
    // video control signals
    input  wire logic           stall_i,                    // stall this cycle (memory contention)
    input  wire hres_t          h_count_i,                  // horizontal video timing counter
    input  wire logic           mem_fetch_i,                // playfield memory fetch enable
    input  wire logic           mem_fetch_start_i,          // strobe when memory fetch starts
    input  wire logic           end_of_line_i,              // strobe at end of scanline
    input  wire logic           end_of_frame_i,             // strobe at end of frame
    input  wire color_t         border_color_i,             // color index for border/blanked pixels
    input  wire hres_t          vid_left_i,                 // left edge of screen (leftmost visible pixel is 0)
    input  wire hres_t          vid_right_i,                // right edge of screen +1 (aka 640/848 for full width)
    // video memories
    output      logic           vram_sel_o,                 // vram read select
    output      addr_t          vram_addr_o,                // vram word address out (16x64K)
    input  wire word_t          vram_data_i,                // vram word data in
    output      logic           tilemem_sel_o,              // tile mem read select
    output      tile_addr_t     tilemem_addr_o,             // tile mem word address out (16x5K)
    input  wire word_t          tilemem_data_i,             // tile mem word data in
    // playfield generation control signals
    input  wire logic           pf_blank_i,                 // disable plane
    input  wire addr_t          pf_start_addr_i,            // display data start address (word address)
    input  wire word_t          pf_line_len_i,              // words per disply line (added to line_addr each line)
    input  wire color_t         pf_colorbase_i,             // colorbase XOR'd with pixel index (e.g. to set upper bits or alter index)
    input  wire logic  [1:0]    pf_bpp_i,                   // bpp code (bpp_depth_t)
    input  wire logic           pf_bitmap_i,                // bitmap enable (else text mode)
    input  wire logic  [5:0]    pf_tile_bank_i,             // vram/tilemem tile bank 0-3 (0/1 with 8x16) tilemem, or 2KB/4K
    input  wire logic           pf_disp_in_tile_i,          // display memory 0=vram, 1=tileram
    input  wire logic           pf_tile_in_vram_i,          // tile memory 0=tilemem, 1=vram
    input  wire logic  [3:0]    pf_tile_height_i,           // max height of tile cell
    input  wire logic  [1:0]    pf_h_repeat_i,              // horizontal pixel repeat
    input  wire logic  [1:0]    pf_v_repeat_i,              // vertical pixel repeat
    input  wire logic  [2:0]    pf_h_frac_repeat_i,         // horizontal fractional pixel repeat
    input  wire logic  [2:0]    pf_v_frac_repeat_i,         // vertical fractional pixel repeat
    input  wire logic  [4:0]    pf_fine_hscroll_i,          // horizontal fine scroll (8 pixel * 4 for repeat)
    input  wire logic  [5:0]    pf_fine_vscroll_i,          // vertical fine scroll (16 lines * 4 for repeat)
    input  wire logic           pf_gfx_ctrl_set_i,          // true if pf_gfx_ctrl_i changed (register write)
    input  wire logic           pf_line_start_set_i,        // true if pf_line_start_i changed (register write)
    input  wire addr_t          pf_line_start_addr_i,       // address of next line display data start
    // video color index output
    output      color_t         pf_color_index_o,           // output color index
    // standard signals
    input  wire logic           reset_i,                    // system reset
    input  wire                 clk                         // pixel clock
);

// display line fetch generation FSM
typedef enum logic [4:0] {
    FETCH_IDLE          =   5'h00,                  // wait for dma or memfetch
    // bitmap
    FETCH_BITMAP        =   5'h03,                  // output bitmap VRAM address
    FETCH_ADDR_BITMAP   =   5'h04,                  // output bitmap VRAM address
    FETCH_WAIT_BITMAP   =   5'h05,                  // wait for bitmap data
    FETCH_READ_BITMAP_0 =   5'h06,                  // read bitmap word0/tilemap from VRAM
    FETCH_READ_BITMAP_1 =   5'h07,                  // read bitmap word1 data from VRAM
    FETCH_READ_BITMAP_2 =   5'h08,                  // read bitmap word2 data from VRAM
    FETCH_READ_BITMAP_3 =   5'h09,                  // read bitmap word3 data from VRAM
    // tiled
    FETCH_ADDR_TILEMAP  =   5'h0A,                  // output tilemap VRAM address (and read tile word3 data)
    FETCH_WAIT_TILEMAP  =   5'h0B,                  // wait for tilemap data
    FETCH_READ_TILEMAP  =   5'h0C,                  // read tilemap from VRAM
    FETCH_ADDR_TILE     =   5'h0D,                  // output tile word0 VRAM/TILE address
    FETCH_WAIT_TILE     =   5'h0E,                  // wait for tilemap data, output word1 tile addr
    FETCH_READ_TILE_0   =   5'h0F,                  // read tile word0 data from bus, output word2 tile addr
    FETCH_READ_TILE_1   =   5'h10,                  // read tile word1 data from bus, output word3 tile addr
    FETCH_READ_TILE_2   =   5'h11,                  // read tile word2 data from bus
    FETCH_ADDR_TILEMAP_2=   5'h12                   // read tile word3 and output next tilemap VRAM address
} vgen_fetch_st;

logic           scanout;                            // scanout active
logic           scanout_start;                      // scanout start strobe
logic           scanout_end;                        // scanout stop strobe
hres_t          scanout_start_hcount;               // horizontal pixel count to start scanout
hres_t          scanout_end_hcount;                 // horizontal pixel count to stop scanout

logic  [1:0]    pf_h_count;                         // current horizontal repeat countdown
logic  [1:0]    pf_v_count;                         // current vertical repeat countdown
logic  [2:0]    pf_h_frac_count;                    // current horizontal skip countdown
logic  [2:0]    pf_v_frac_count;                    // current vertical skip countdown
logic  [2:0]    pf_tile_x;                          // current column of tile cell
logic  [3:0]    pf_tile_y;                          // current line of tile cell

addr_t          pf_line_start;                      // current line starting address

// fetch fsm outputs
// scanline generation (registered signals and "_next" combinatorally set signals)
vgen_fetch_st   pf_fetch, pf_fetch_next;            // playfield A generation FSM state

addr_t          pf_addr, pf_addr_next;              // address to fetch display bitmap/tilemap
addr_t          pf_tile_addr;                       // tile start address (VRAM or TILERAM)

logic           vram_sel_next;                      // vram select output
logic           tilemem_sel_next;                   // tilemem select output
addr_t          fetch_addr, fetch_addr_next;        // VRAM or TILERAM address output

logic           pf_initial_buf, pf_initial_buf_next;// true on first buffer per scanline
logic           pf_words_ready, pf_words_ready_next;// true if data_words full (8-pixels)
word_t          pf_attr_word, pf_attr_word_next;    // attributes (and tile index)
word_t          pf_data_word0, pf_data_word0_next;  // 1st fetched display data word buffer
word_t          pf_data_word1, pf_data_word1_next;  // 2nd fetched display data word buffer
word_t          pf_data_word2, pf_data_word2_next;  // 3rd fetched display data word buffer
word_t          pf_data_word3, pf_data_word3_next;  // 4th fetched display data word buffer

logic           pf_pixels_buf_full;                 // true when pf_pixel_out needs filling
logic           pf_pixels_buf_hrev;                 // horizontal reverse flag
logic [63:0]    pf_pixels_buf;                      // 8 pixel buffer waiting for scan out
logic [63:0]    pf_pixels;                          // 8 pixels currently shifting to scan out

always_comb     scanout_start = (h_count_i == scanout_start_hcount) ? mem_fetch_i : 1'b0;
always_comb     scanout_end = (h_count_i >= scanout_end_hcount) ? 1'b1 : 1'b0;

// generate tile address from index, tile y, bpp and tile size (8x8 or 8x16)
function automatic addr_t calc_tile_addr(
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

// fetch FSM combinational logic
always_comb begin
    // set default outputs
    pf_words_ready_next = pf_words_ready;
    pf_initial_buf_next = pf_initial_buf;
    pf_fetch_next       = pf_fetch;
    pf_addr_next        = pf_addr;

    pf_data_word0_next  = pf_data_word0;
    pf_data_word1_next  = pf_data_word1;
    pf_data_word2_next  = pf_data_word2;
    pf_data_word3_next  = pf_data_word3;
    pf_attr_word_next   = pf_attr_word;

    fetch_addr_next     = fetch_addr;

    pf_tile_addr        = calc_tile_addr(pf_attr_word_next[xv::TILE_INDEX+:10], pf_tile_y, pf_tile_bank_i, pf_bpp_i, pf_tile_height_i[3], pf_attr_word_next[xv::TILE_ATTR_VREV]);

    pf_words_ready_next = 1'b0;
    vram_sel_next       = 1'b0;
    tilemem_sel_next    = 1'b0;

    case (pf_fetch)
        FETCH_IDLE: begin
            if (pf_bitmap_i) begin
                pf_fetch_next   = FETCH_ADDR_BITMAP;
            end else begin
                pf_fetch_next   = FETCH_ADDR_TILEMAP;
            end
        end
        // bitmap mode
        FETCH_ADDR_BITMAP: begin
            if (!pf_pixels_buf_full) begin                  // if room in buffer
                vram_sel_next   = 1'b1;                     // VO0: select vram
                fetch_addr_next = pf_addr;                  // put display address on vram bus
                pf_addr_next    = pf_addr + 1'b1;           // increment display address
                pf_fetch_next   = FETCH_WAIT_BITMAP;
            end
        end
        FETCH_WAIT_BITMAP: begin
            if (pf_bpp_i != xv::BPP_1_ATTR) begin
                vram_sel_next   = 1'b1;                     // VO1: select vram
                fetch_addr_next = pf_addr;                  // put display address on vram bus
                pf_addr_next    = pf_addr + 1'b1;           // increment display address
            end
            pf_words_ready_next = !pf_initial_buf;          // set buffer ready
            pf_initial_buf_next = 1'b0;
            pf_fetch_next   = FETCH_READ_BITMAP_0;
        end
        FETCH_READ_BITMAP_0: begin
            pf_data_word0_next  = vram_data_i;              // VI0: read vram data
            pf_attr_word_next   = vram_data_i;              // set attributes for 1_BPP_ATTR

            if (pf_bpp_i == xv::BPP_1_ATTR) begin
                pf_fetch_next   = FETCH_ADDR_BITMAP;        // done if BPP_1 bitmap
            end else begin
                if (pf_bpp_i != xv::BPP_4) begin
                    vram_sel_next   = 1'b1;                 // VO2: select vram
                    fetch_addr_next = pf_addr;              // put display address on vram bus
                    pf_addr_next    = pf_addr + 1'b1;       // increment display address
                end
                pf_fetch_next   = FETCH_READ_BITMAP_1;      // else read more bitmap words
            end
        end
        FETCH_READ_BITMAP_1: begin
            pf_data_word1_next  = vram_data_i;              // VI1: read vram data
            pf_attr_word_next[15:11] = 5'b00000;            // clear color and hrev attributes (vrev ignored)

            if (pf_bpp_i == xv::BPP_4) begin
                pf_fetch_next   = FETCH_ADDR_BITMAP;        // done if BPP_4 bitmap
            end else begin
                vram_sel_next   = 1'b1;                     // VO3: select vram
                fetch_addr_next = pf_addr;                  // put display address on vram bus
                pf_addr_next    = pf_addr + 1'b1;           // increment display address
                pf_fetch_next   = FETCH_READ_BITMAP_2;      // read more bitmap words
            end
        end
        FETCH_READ_BITMAP_2: begin
            pf_data_word2_next  = vram_data_i;              // VI2: read vram data
            pf_fetch_next       = FETCH_READ_BITMAP_3;      // read last bitmap word
        end
        FETCH_READ_BITMAP_3: begin
            pf_data_word3_next  = vram_data_i;              // VI3: read vram data
            pf_fetch_next       = FETCH_ADDR_BITMAP;        // done
        end
        // tilemap mode
        FETCH_ADDR_TILEMAP: begin
            if (!pf_pixels_buf_full) begin                  // if room in buffer
                vram_sel_next   = ~pf_disp_in_tile_i;       // VO0: select either vram
                tilemem_sel_next= pf_disp_in_tile_i;        // VO0: or select tilemem
                fetch_addr_next = pf_addr;                  // put display address on vram bus
                pf_addr_next    = pf_addr + 1'b1;           // increment display address
                pf_fetch_next   = FETCH_WAIT_TILEMAP;
            end
        end
        FETCH_WAIT_TILEMAP: begin
            pf_words_ready_next = !pf_initial_buf;          // set buffer ready
            pf_initial_buf_next = 1'b0;
            pf_fetch_next   = FETCH_READ_TILEMAP;
        end

        FETCH_READ_TILEMAP: begin
            pf_attr_word_next   = pf_disp_in_tile_i ? tilemem_data_i : vram_data_i;   // save attribute+tile
            pf_fetch_next       = FETCH_ADDR_TILE;          // read tile bitmap words
        end
        FETCH_ADDR_TILE: begin
            vram_sel_next       = pf_tile_in_vram_i;        // TO0: select either vram
            tilemem_sel_next    = ~pf_tile_in_vram_i;       // TO0: or select tilemem
            fetch_addr_next     = pf_tile_addr;             // will have been calculated from pf_attr_word_next
            pf_fetch_next       = FETCH_WAIT_TILE;
        end
        FETCH_WAIT_TILE: begin
            if (pf_bpp_i != xv::BPP_1_ATTR) begin
                vram_sel_next       = pf_tile_in_vram_i;    // TO1: select either vram
                tilemem_sel_next    = ~pf_tile_in_vram_i;   // TO1: or select tilemem
                fetch_addr_next     = { fetch_addr[15:1], 1'b1 };
            end
            pf_fetch_next   = FETCH_READ_TILE_0;
        end
        FETCH_READ_TILE_0: begin
            pf_data_word0_next  = pf_tile_in_vram_i ? vram_data_i : tilemem_data_i;  // TI0: read tile data

            if (pf_bpp_i == xv::BPP_1_ATTR) begin           // in BPP_1 select even/odd byte from tile word
                if (!pf_tile_y[0]) begin
                    pf_data_word0_next[7:0] = pf_tile_in_vram_i ? vram_data_i[15:8] : tilemem_data_i[15:8];
                end
                pf_fetch_next = FETCH_ADDR_TILEMAP;         // done if BPP_1 mode
            end else begin
                if (pf_bpp_i != xv::BPP_4) begin
                    vram_sel_next       = pf_tile_in_vram_i;// TO2: select either vram
                    tilemem_sel_next    = ~pf_tile_in_vram_i; // TO2: or select tilemem
                    fetch_addr_next     = { fetch_addr[15:2], 2'b10 };
                end
                pf_fetch_next = FETCH_READ_TILE_1;          // else read more tile words
            end
        end
        FETCH_READ_TILE_1: begin
            pf_data_word1_next  = pf_tile_in_vram_i ? vram_data_i : tilemem_data_i;  // TI1: read tile data

            if (pf_bpp_i == xv::BPP_4) begin
                pf_fetch_next = FETCH_ADDR_TILEMAP;         // done if BPP_4 mode
            end else begin
                vram_sel_next       = pf_tile_in_vram_i;    // TO3: select either vram
                tilemem_sel_next    = ~pf_tile_in_vram_i;   // TO3: or select tilemem
                fetch_addr_next     = { fetch_addr[15:2], 2'b11 };
                pf_fetch_next       = FETCH_READ_TILE_2;    // else read more tile data words
            end
        end
        FETCH_READ_TILE_2: begin
            pf_data_word2_next  = pf_tile_in_vram_i ? vram_data_i : tilemem_data_i;  // TI2: read tile data
            pf_fetch_next       = FETCH_ADDR_TILEMAP_2;     // NOTE will read TI3 also
        end
        FETCH_ADDR_TILEMAP_2: begin
            // read pre-loaded font word3
            pf_data_word3_next  = pf_tile_in_vram_i ? vram_data_i : tilemem_data_i;  // TI3: read tile data
            vram_sel_next   = ~pf_disp_in_tile_i;           // VO0: select either vram
            tilemem_sel_next= pf_disp_in_tile_i;            // VO0: or select tilemem
            fetch_addr_next = pf_addr;                      // put display address on vram bus
            pf_addr_next    = pf_addr + 1'b1;               // increment display address
            pf_fetch_next   = FETCH_WAIT_TILEMAP;
        end
        default: begin
            pf_fetch_next = FETCH_IDLE;
        end
    endcase
end

assign  pf_color_index_o    = pf_pixels[63:56] ^ pf_colorbase_i;   // XOR colorbase bits here

always_ff @(posedge clk) begin
    if (reset_i) begin
        vram_sel_o          <= 1'b0;
        vram_addr_o         <= 16'h0000;
        tilemem_sel_o       <= 1'b0;
        tilemem_addr_o      <= '0;

        scanout             <= 1'b0;
        scanout_start_hcount<= '0;
        scanout_end_hcount  <= '0;

        pf_fetch            <= FETCH_IDLE;
        fetch_addr          <= 16'h0000;
        pf_addr             <= 16'h0000;        // current display address during scan
        pf_attr_word        <= 16'h0000;        // word with tile attributes and index
        pf_data_word0       <= 16'h0000;        // buffers for unexpanded display data
        pf_data_word1       <= 16'h0000;
        pf_data_word2       <= 16'h0000;
        pf_data_word3       <= 16'h0000;
        pf_initial_buf      <= 1'b0;
        pf_words_ready      <= 1'b0;

        pf_initial_buf      <= 1'b0;
        pf_pixels_buf_full  <= 1'b0;            // flag when pf_pixels_buf is empty (continue fetching)
        pf_pixels_buf_hrev  <= 1'b0;            // flag to horizontally reverse pf_pixels_buf
        pf_pixels_buf       <= 64'h00000000;    // next 8 8-bpp pixels to scan out
        pf_pixels           <= 64'h00000000;    // 8 8-bpp pixels currently scanning out

        pf_h_frac_count     <= '0;
        pf_v_frac_count     <= '0;
        pf_h_count          <= '0;
        pf_v_count          <= '0;
        pf_tile_x           <= '0;
        pf_tile_y           <= '0;
        pf_line_start       <= '0;

    end else begin

        // fetch FSM clocked process
        // register fetch combinitorial signals
        if (!stall_i) begin
            pf_fetch        <= pf_fetch_next;

            fetch_addr      <= fetch_addr_next;
            pf_addr         <= pf_addr_next;
            pf_attr_word    <= pf_attr_word_next;
            pf_data_word0   <= pf_data_word0_next;
            pf_data_word1   <= pf_data_word1_next;
            pf_data_word2   <= pf_data_word2_next;
            pf_data_word3   <= pf_data_word3_next;
            pf_initial_buf  <= pf_initial_buf_next;
            pf_words_ready  <= pf_words_ready_next;

            vram_sel_o      <= vram_sel_next;
            vram_addr_o     <= fetch_addr_next;
            tilemem_sel_o   <= tilemem_sel_next;
            tilemem_addr_o  <= $bits(tilemem_addr_o)'(fetch_addr_next);
        end

        // have display words been fetched?
        if (pf_words_ready) begin
            pf_pixels_buf_full  <= 1'b1;     // mark buffer full
            // keep flag with these 8 pixels for H reverse attribute (if applicable)
            if (pf_bitmap_i || (pf_bpp_i == xv::BPP_1_ATTR)) begin
                pf_pixels_buf_hrev  <= 1'b0;                // no horizontal reverse in bitmap or BPP_1
            end else begin
                pf_pixels_buf_hrev  <= pf_attr_word[xv::TILE_ATTR_HREV];    // use horizontal reverse attrib
            end

            // expand display data into pf_pixels_buf depending on mode
            case (pf_bpp_i)
            xv::BPP_1_ATTR:
                // expand to 8-bit index with upper 4-bits zero
                // and 4-bit attribute foreground/background index
                // based on pixel bit set/clear
                pf_pixels_buf  <= {
                    4'h0, pf_data_word0[7] ? pf_attr_word[xv::TILE_ATTR_FORE+:4] : pf_attr_word[xv::TILE_ATTR_BACK+:4],
                    4'h0, pf_data_word0[6] ? pf_attr_word[xv::TILE_ATTR_FORE+:4] : pf_attr_word[xv::TILE_ATTR_BACK+:4],
                    4'h0, pf_data_word0[5] ? pf_attr_word[xv::TILE_ATTR_FORE+:4] : pf_attr_word[xv::TILE_ATTR_BACK+:4],
                    4'h0, pf_data_word0[4] ? pf_attr_word[xv::TILE_ATTR_FORE+:4] : pf_attr_word[xv::TILE_ATTR_BACK+:4],
                    4'h0, pf_data_word0[3] ? pf_attr_word[xv::TILE_ATTR_FORE+:4] : pf_attr_word[xv::TILE_ATTR_BACK+:4],
                    4'h0, pf_data_word0[2] ? pf_attr_word[xv::TILE_ATTR_FORE+:4] : pf_attr_word[xv::TILE_ATTR_BACK+:4],
                    4'h0, pf_data_word0[1] ? pf_attr_word[xv::TILE_ATTR_FORE+:4] : pf_attr_word[xv::TILE_ATTR_BACK+:4],
                    4'h0, pf_data_word0[0] ? pf_attr_word[xv::TILE_ATTR_FORE+:4] : pf_attr_word[xv::TILE_ATTR_BACK+:4] };
            xv::BPP_4:
                // expand to 8-bit index using 4-bit color extension attribute
                // and 4-bit pixel value
                pf_pixels_buf  <= {
                    pf_attr_word[xv::TILE_ATTR_BACK+:4], pf_data_word0[15:12],
                    pf_attr_word[xv::TILE_ATTR_BACK+:4], pf_data_word0[11: 8],
                    pf_attr_word[xv::TILE_ATTR_BACK+:4], pf_data_word0[ 7: 4],
                    pf_attr_word[xv::TILE_ATTR_BACK+:4], pf_data_word0[ 3: 0],
                    pf_attr_word[xv::TILE_ATTR_BACK+:4], pf_data_word1[15:12],
                    pf_attr_word[xv::TILE_ATTR_BACK+:4], pf_data_word1[11: 8],
                    pf_attr_word[xv::TILE_ATTR_BACK+:4], pf_data_word1[ 7: 4],
                    pf_attr_word[xv::TILE_ATTR_BACK+:4], pf_data_word1[ 3: 0] };
            xv::BPP_8,
            xv::BPP_XX:
                // copy 8-bit pixel indices XORing the upper 4-bit color extension attribute
                pf_pixels_buf  <= {
                    pf_attr_word[xv::TILE_ATTR_BACK+:4] ^ pf_data_word0[15:12], pf_data_word0[11: 8],
                    pf_attr_word[xv::TILE_ATTR_BACK+:4] ^ pf_data_word0[ 7: 4], pf_data_word0[ 3: 0],
                    pf_attr_word[xv::TILE_ATTR_BACK+:4] ^ pf_data_word1[15:12], pf_data_word1[11: 8],
                    pf_attr_word[xv::TILE_ATTR_BACK+:4] ^ pf_data_word1[ 7: 4], pf_data_word1[ 3: 0],
                    pf_attr_word[xv::TILE_ATTR_BACK+:4] ^ pf_data_word2[15:12], pf_data_word2[11: 8],
                    pf_attr_word[xv::TILE_ATTR_BACK+:4] ^ pf_data_word2[ 7: 4], pf_data_word2[ 3: 0],
                    pf_attr_word[xv::TILE_ATTR_BACK+:4] ^ pf_data_word3[15:12], pf_data_word3[11: 8],
                    pf_attr_word[xv::TILE_ATTR_BACK+:4] ^ pf_data_word3[ 7: 4], pf_data_word3[ 3: 0]  };
            endcase
        end

        if (scanout) begin
            // shift-in next pixel
            pf_h_frac_count <= pf_h_frac_count - 1'b1;                      // update fractional pixel repeat counter
            if (pf_h_frac_repeat_i != '0 && pf_h_frac_count == '0) begin    // repeat last pixel?
                pf_h_frac_count <= pf_h_frac_repeat_i;                      // reset repeat counter, repeat pixel
            end else if (pf_h_count != '0) begin
                pf_h_count              <= pf_h_count - 1'b1;
            end else begin
                pf_h_count              <= pf_h_repeat_i;
                pf_tile_x               <= pf_tile_x + 1'b1;

                if (pf_tile_x == 3'h7) begin
                    pf_pixels_buf_full <= 1'b0;
                    if (pf_pixels_buf_hrev) begin
                         // next 8 pixels from buffer copied reversed
                        pf_pixels   <= {
                            pf_pixels_buf[7:0],
                            pf_pixels_buf[15:8],
                            pf_pixels_buf[23:16],
                            pf_pixels_buf[31:24],
                            pf_pixels_buf[39:32],
                            pf_pixels_buf[47:40],
                            pf_pixels_buf[55:48],
                            pf_pixels_buf[63:56]
                        };
                    end else begin
                        pf_pixels   <= pf_pixels_buf;                       // next 8 pixels from buffer
                    end
                end else begin
                    pf_pixels   <= { pf_pixels[55:0], border_color_i };     // shift for next pixel
                end
            end
        end

        // start of line display fetch
        if (mem_fetch_start_i) begin       // on line fetch start signal
            pf_initial_buf          <= 1'b1;
            pf_pixels_buf_full      <= 1'b0;
            scanout_start_hcount    <= (xv::OFFSCREEN_WIDTH-2 + vid_left_i) - $bits(scanout_start_hcount)'(pf_fine_hscroll_i);
            scanout_end_hcount      <= xv::OFFSCREEN_WIDTH-2 + vid_right_i;

            pf_addr                 <= pf_line_start;       // set start address for this line

`ifndef SYNTHESIS
            pf_data_word0           <= 16'h0BAD;            // poison buffers in simulation
            pf_data_word1           <= 16'h1BAD;
            pf_data_word2           <= 16'h2BAD;
            pf_data_word3           <= 16'h3BAD;
            pf_attr_word            <= 16'hE3E3;
            pf_pixels               <= 64'he3e3e3e3e3e3e3e3;
            pf_pixels_buf           <= 64'he3e3e3e3e3e3e3e3;
`endif
        end

        // when "scrolled" scanline starts outputting (before display if scrolled)
        if (scanout_start) begin
            scanout             <= 1'b1;
            pf_tile_x           <= 3'h0;
            pf_h_count          <= pf_h_repeat_i;
            pf_pixels           <= pf_pixels_buf;       // get initial 8 pixels from buffer
            pf_pixels_buf_full  <= 1'b0;
        end

        // when scanline stops outputting
        if (scanout_end) begin
            scanout             <= 1'b0;
            pf_pixels[63:56]    <= border_color_i;
        end

        // use new line start immediately when line_start altered
        if (pf_line_start_set_i) begin
            pf_line_start       <= pf_line_start_addr_i;    // set new line start address
        end

        // use new v_repeat count immediately when gfx_ctrl altered
        if (pf_gfx_ctrl_set_i) begin
            pf_v_count          <= pf_v_repeat_i;           // set new v repeat count
        end

        // end of line
        if (end_of_line_i) begin
            scanout             <= 1'b0;                                        // force scanout off
            pf_pixels[63:56]    <= border_color_i;                              // output border_color_i
            pf_fetch            <= FETCH_IDLE;                                  // reset fetch state
            pf_addr             <= pf_line_start;                               // addr back to line start (for tile lines, or v repeat)
            pf_h_frac_count     <= '0;                                          // reset horizontal fractional pixel repeat counter
            pf_v_frac_count     <= pf_v_frac_count - 1'b1;                      // update vertical fractional line repeat counter
            if (pf_v_frac_repeat_i != '0 && pf_v_frac_count == '0) begin    // repeat last line?
                pf_v_frac_count <= pf_v_frac_repeat_i;                      // reset repeat count, repeat line
            end else if (pf_v_count != 2'b00) begin                         // is line repeating?
                pf_v_count      <= pf_v_count - 1'b1;                       // keep decrementing
            end else begin
                pf_v_count      <= pf_v_repeat_i;                           // reset v repeat
                if (pf_bitmap_i || (pf_tile_y >= pf_tile_height_i)) begin   // is bitmap mode or >= last line of tile cell?
                    pf_tile_y       <= 4'h0;                                // reset tile cell line
                    pf_line_start   <= pf_line_start + pf_line_len_i;       // calculate next line start address
                end else begin
                    pf_tile_y       <= pf_tile_y + 1;                       // next line of tile cell
                end
            end
        end

        // end of frame or blanked, prepare for next frame
        if (pf_blank_i || end_of_frame_i) begin         // is last pixel of frame?
            pf_addr             <= pf_start_addr_i;         // set start of display data
            pf_line_start       <= pf_start_addr_i;         // set line to start of display data

            pf_v_count          <= pf_v_repeat_i - pf_fine_vscroll_i[1:0];  // fine scroll within scaled line (v repeat)
            pf_tile_y           <= pf_fine_vscroll_i[5:2];  // fine scroll tile line
            pf_v_frac_count     <= '0;
            pf_pixels[63:56]    <= border_color_i;          // set border_color_i (in case blanked)
        end
    end
end

endmodule
`default_nettype wire               // restore default
