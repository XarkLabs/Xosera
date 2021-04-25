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
    output logic            fontram_sel_o,      // fontram access select
    output logic [12:0]     fontram_addr_o,     // font memory byte address out (8x4KB)
    output logic            vram_sel_o,         // vram access select
    output logic [15:0]     vram_addr_o,        // vram word address out (16x64KB)
    output logic [15:0]     vgen_reg_data_o,    // register/status data reads
    // control inputs
    input  logic [15:0]     vram_data_i,        // vram word data in
    input  logic  [7:0]     fontram_data_i,     // font memory byte data in
    input  logic            enable_i,           // enable video (0=black output, 1=normal output)
    input  logic            vgen_reg_wr_i,      // strobe to write internal config register number
    input  logic  [2:0]     vgen_reg_num_i,     // internal config register number
    input  logic [15:0]     vgen_reg_data_i,    // data for internal config register
    // video signal outputs
    output logic  [3:0]     pal_index_o,        // palette index outputs
    output logic            vsync_o, hsync_o,   // VGA sync outputs
    output logic            dv_de_o,            // VGA video active signal (needed for HDMI)
    // standard signals
    input  logic            reset_i,            // system reset in
    input  logic            clk                 // clock (video pixel clock)
);

// Emperically determined (at extremes of horizontal scroll [worst case])
// (odd numbers because 4 cycle latency through "fetch pipeline" and buffered)
localparam H_MEM_BEGIN = xv::OFFSCREEN_WIDTH-7;            // memory fetch starts over a tile early
localparam H2X_MEM_BEGIN = xv::OFFSCREEN_WIDTH-(7+8);      // and 8 pixels earlier with horizontal pixel double
localparam H_MEM_END = xv::TOTAL_WIDTH-4;                   // memory fetch can ends a bit early

// mode options
logic h_double;
logic v_double;

// bitmap generation signals
logic [15:0]    bitmap_start_addr;                        // bitmap start address
logic [15:0]    bitmap_addr;                              // current bitmap address
logic [15:0]    bitmap_data;                              // bit pattern shifting out for current bitmap word
logic [15:0]    bitmap_data_next;                         // next bitmap word to shift out

// text generation signals
logic [15:0]    text_start_addr;                          // text start address (word address)
logic [15:0]    text_line_width;
logic [15:0]    text_addr;                                // address to fetch tile+color attribute
logic [15:0]    text_line_addr;                           // address of start of tile+color attribute line
logic  [3:0]    font_height;                              // max height of font cell
logic           font_use_vram;                            // 0=fontmem, 1=vram
logic  [4:0]    font_bank;                                // vram/fontmem font bank 0-3 (0/1 with 8x16) fontmem, or 2KB/4K
logic  [3:0]    tile_x;                                   // current column of font cell (extra bit for horizontal double)
logic  [4:0]    tile_y;                                   // current line of font cell (extra bit for vertical double)
logic  [3:0]    fine_scrollx;                             // X fine scroll
logic  [4:0]    fine_scrolly;                             // Y fine scroll
logic  [7:0]    text_color;                               // bit pattern shifting out for current font tile line
logic  [7:0]    font_shift_out;                           // bit pattern shifting out for current font tile line
logic [15:0]    vram_data_save;                           // background/foreground color attribute for current tile

logic           tile_start;
assign          tile_start = mem_fetch && tile_x == 4'b000;
logic [15:0]    font_addr;

// feature enable signals
logic tg_enable;                                        // text generation
logic bm_enable;                                        // bitmap enable

// video sync generation via state machine (Thanks tnt & drr - a much more efficient method!)
typedef enum logic [1:0] {
    STATE_PRE_SYNC = 2'b00,
    STATE_SYNC = 2'b01,
    STATE_POST_SYNC = 2'b10,
    STATE_VISIBLE = 2'b11
} video_signal_st;

// sync generation signals (and combinatorial logic "next" versions)
logic [1: 0] h_state;
logic [10: 0] h_count;
logic [10: 0] h_count_next_state;

logic [1: 0] v_state;
logic [10: 0] v_count;
logic [10: 0] v_count_next_state;

logic mem_fetch;
logic [10: 0] mem_fetch_toggle;

// sync condition indicators (combinatorial)
logic           hsync;
logic           vsync;
logic           dv_display_ena;
logic           h_last_line_pixel;
logic           v_last_frame_pixel;
logic           [1: 0] h_state_next;
logic           [1: 0] v_state_next;
logic           mem_fetch_next;
logic           h_start_line_fetch;

always_comb     hsync = (h_state == STATE_SYNC);
always_comb     vsync = (v_state == STATE_SYNC);
always_comb     dv_display_ena = tg_enable && (h_state == STATE_VISIBLE) && (v_state == STATE_VISIBLE);
always_comb     h_last_line_pixel = (h_state_next == STATE_PRE_SYNC) && (h_state == STATE_VISIBLE);
always_comb     v_last_frame_pixel = (v_state_next == STATE_VISIBLE) && (v_state == STATE_POST_SYNC) && h_last_line_pixel;
always_comb     h_state_next = (h_count == h_count_next_state) ? h_state + 1 : h_state;
always_comb     v_state_next = (h_last_line_pixel && v_count == v_count_next_state) ? v_state + 1 : v_state;
always_comb     mem_fetch_next = (v_state == STATE_VISIBLE && h_count == mem_fetch_toggle) ? ~mem_fetch : mem_fetch;
always_comb     h_start_line_fetch = (~mem_fetch && mem_fetch_next);

logic [10: 0] h_count_next;
logic [10: 0] v_count_next;

// combinational block for video counters
always_comb begin
    h_count_next = h_count + 1;
    v_count_next = v_count;

    if (h_last_line_pixel) begin
        h_count_next = 0;
        v_count_next = v_count + 1;

        if (v_last_frame_pixel) begin
            v_count_next = 0;
        end
    end
end

// combinational block for video fetch start and stop
always_comb begin
    // set mem_fetch next toggle for video memory access (h_double subtracts an extra 16)
    if (mem_fetch) begin
        mem_fetch_toggle = H_MEM_END[10:0];
    end
    else begin
        mem_fetch_toggle = (h_double ? H2X_MEM_BEGIN[10:0] : H_MEM_BEGIN[10:0]) - { 7'b0, fine_scrollx };
    end
end

// combinational block for horizontal video state
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

// video config registers
always_ff @(posedge clk) begin
    if (reset_i) begin
        text_start_addr <= 16'h0000;
        text_line_width <= xv::TILES_WIDE[15:0];
        fine_scrollx    <= 4'b0000;         // low bit is for "1/2 doubled pixel" when h_double
        fine_scrolly    <= 5'b00000;        // low bit is for "1/2 doubled pixel" when v_double
        font_height     <= 4'b1111;
        font_use_vram   <= 1'b0;
        font_bank       <= 5'b00000;
        h_double        <= 1'b0;            // horizontal pixel double (repeat)
        v_double        <= 1'b0;            // vertical pixel double (repeat)
    end
    else begin
        // video register write
        if (vgen_reg_wr_i) begin
            case (vgen_reg_num_i)
                xv::AUX_VID_W_DISPSTART[2:0]: begin
                    text_start_addr <= vgen_reg_data_i;
                end
                xv::AUX_VID_W_TILEWIDTH[2:0]: begin
                    text_line_width <= vgen_reg_data_i;
                end
                xv::AUX_VID_W_SCROLLXY[2:0]: begin
                    fine_scrollx    <= vgen_reg_data_i[11:8];
                    fine_scrolly    <= vgen_reg_data_i[4:0];
                end
                xv::AUX_VID_W_FONTCTRL[2:0]: begin
                    font_height     <= vgen_reg_data_i[3:0];
                    font_use_vram   <= vgen_reg_data_i[8];
                    font_bank       <= vgen_reg_data_i[15:11];
                end
                xv::AUX_VID_W_GFXCTRL[2:0]: begin
                    h_double        <= vgen_reg_data_i[0];
                    v_double        <= vgen_reg_data_i[1];
                end
                default: begin
                end
            endcase
        end

        // video register read
        case (vgen_reg_num_i[1:0])
            2'b00:      vgen_reg_data_o <= {4'h0, xv::VISIBLE_WIDTH[11:0]};
            2'b01:      vgen_reg_data_o <= {4'h0, xv::VISIBLE_HEIGHT[11:0]};
            2'b10:      vgen_reg_data_o <= 16'b1000000000000001;  // TODO define feature bits
            2'b11:      vgen_reg_data_o <= {(v_state != STATE_VISIBLE), (h_state != STATE_VISIBLE), 3'b000, v_count }; // negative when not vsync
        endcase
    end
end

// logic aliases
logic           font_pix;                       // current pixel from font data shift-logic out
assign          font_pix = font_shift_out[7];
logic [3: 0]    forecolor;                      // current tile foreground color palette index (0-15)
assign          forecolor = text_color[3:0];
logic [3: 0]    backcolor;                      // current tile background color palette index (0-15)
assign          backcolor = text_color[7:4];
logic  [7:0]    text_tile;                      // current tile index

assign font_addr = font_height[3]   ? {font_bank[4:1], vram_data_i[7: 0], tile_y[4:1]}
                                    : {font_bank[4:0], vram_data_i[7: 0], tile_y[3:1]};

always_ff @(posedge clk) begin
    if (reset_i) begin
        tg_enable       <= 1'b0;                        // text generation enable
        bm_enable       <= 1'b0;                        // text generation enable
        h_state         <= STATE_PRE_SYNC;
        v_state         <= STATE_VISIBLE;
        mem_fetch       <= 1'b0;
        h_count         <= 11'h000;
        v_count         <= 11'h000;
        font_shift_out  <= 8'h00;
        text_color      <= 8'h00;
        text_addr       <= 16'h0000;
        text_line_addr  <= 16'h0000;
        vram_data_save  <= 16'h0000;
        tile_x          <= 4'b0;
        tile_y          <= 5'b0;
        fontram_sel_o   <= 1'b0;
        vram_sel_o      <= 1'b0;
        vram_addr_o     <= 16'h0000;
        pal_index_o     <= 4'b0;
        hsync_o         <= 1'b0;
        vsync_o         <= 1'b0;
        dv_de_o         <= 1'b0;
    end

    else begin

        // default outputs
        vram_sel_o <= 1'b0;                             // default to no VRAM access
        fontram_sel_o <= 1'b0;                          // default to no font access

        if (mem_fetch) begin
            if (~(h_double & tile_x[0])) begin                // only shift on even pixels with h_double
                font_shift_out <= {font_shift_out[6: 0], 1'b0}; // shift font line data (high bit is current pixel)
                case (tile_x[3:1])
                    3'b000: begin
                        vram_sel_o      <= tg_enable;                   // select vram
                        vram_addr_o     <= text_addr;                   // put text+color address on vram bus
                        text_addr       <= text_addr + 1;               // next tile+attribute
                    end
                    3'b001: begin
                    end
                    3'b010: begin
                        vram_data_save  <= vram_data_i;                 // then save current VRAM data (color for next tile)
                        vram_sel_o     <= font_use_vram & tg_enable;   // select vram
                        fontram_sel_o  <= ~font_use_vram & tg_enable;  // select fontram
                        vram_addr_o    <= font_addr;
                        fontram_addr_o <= font_addr[12:0];
                    end
                    3'b011: begin
                    end
                    3'b100: begin
                        font_shift_out  <= font_use_vram ? vram_data_i[7:0] : fontram_data_i;            // use font lookup data to set font line shift out
                        text_tile       <= vram_data_save[7:0];         // used previously saved tile
                        text_color      <= vram_data_save[15:8];        // used previously saved color
                    end
                    3'b101: begin
                    end
                    3'b110: begin
                    end
                    3'b111: begin
                    end
                endcase
            end
        end

        // pixel color output
        pal_index_o <= font_pix ? forecolor : backcolor;

        // next pixel
        tile_x <= tile_x + (h_double ? 1 : 2);          // increment tile cell column (by 2 normally, 1 if pixel doubled)

        // start of line
        if (h_start_line_fetch) begin                       // on line fetch start signal
            tile_x <= 4'b0000;                              // reset on tile_x cycle (to start tile line at proper pixel)
        end

        // end of line
        if (h_last_line_pixel) begin                        // if last pixel of scan-line
            text_addr <= text_line_addr;                    // text addr back to line start
            if (tile_y == { font_height, v_double }) begin  // if last line of tile cell
                tile_y <= 5'h0;                             // reset tile cell line
                text_line_addr <= text_line_addr + text_line_width; // new line start address
                text_addr <= text_line_addr + text_line_width;      // new text start address
            end
            else begin                                      // else next line of tile cell
                tile_y <= tile_y + (v_double ? 1 : 2);      // next tile tile line (by 2 normally, 1 if pixel doubled)
            end
        end

        // end of frame
        if (v_last_frame_pixel) begin                       // if last pixel of frame
            tg_enable <= enable_i;                          // enable/disable text generation
            tile_y <= v_double ? fine_scrolly : { fine_scrolly[3:0], 1'b0 }; // start next frame at Y fine scroll line
            text_addr <= text_start_addr;                   // reset to start of text data
            text_line_addr <= text_start_addr;              // reset to start of text data
        end

        // update registered signals from combinatorial "next" versions
        h_state <= h_state_next;
        v_state <= v_state_next;
        h_count <= h_count_next;
        v_count <= v_count_next;
        mem_fetch <= mem_fetch_next;

        // set video output signals (color already set)
        hsync_o <= hsync ? xv::H_SYNC_POLARITY : ~xv::H_SYNC_POLARITY;
        vsync_o <= vsync ? xv::V_SYNC_POLARITY : ~xv::V_SYNC_POLARITY;
        dv_de_o <= dv_display_ena;
    end
end

endmodule
