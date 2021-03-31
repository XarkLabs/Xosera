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

`default_nettype none             // mandatory for Verilog sanity
`timescale 1ns/1ns

module video_gen(
           input logic clk,                                   // video pixel clock
           input logic reset_i,                               // system reset in
           input logic enable_i,                              // enable video display
           output logic blit_cycle_o,                     // 0=video memory cycle, 1=Blit memory cycle
           output logic fontram_sel_o,                    // fontram access select
           output logic [12: 0] fontram_addr_o,          // font memory byte address out (8x4KB)
           input logic [7: 0] fontram_data_i,                 // font memory byte data in
           output logic vram_sel_o,                       // vram access select
           output logic [15: 0] vram_addr_o,              // vram word address out (16x64KB)
           input logic [15: 0] vram_data_i,                   // vram word data in
           output logic [3: 0] red_o, green_o, blue_o,    // VGA color outputs (12-bit, 4096 colors)
           output logic vsync_o, hsync_o,                 // VGA sync outputs
           output logic visible_o                         // VGA video active signal (needed for HDMI)
       );

`include "xosera_defs.svh"        // Xosera global Verilog definitions

localparam H_MEM_BEGIN = OFFSCREEN_WIDTH - 9;            // memory fetch starts 1 character early to prime output shift-logic
localparam H_MEM_END = TOTAL_WIDTH - 9;                  // memory fetch ends 1 character early (to empty shift-logic)

// bitmap generation signals
logic [15: 0] bitmap_start_addr;                          // bitmap start address
logic [15: 0] bitmap_addr;                                // current bitmap address
logic [15: 0] bitmap_data;                                // bit pattern shifting out for current bitmap word
logic [15: 0] bitmap_data_next;                           // next bitmap word to shift out

// text generation signals
logic [15: 0] text_start_addr;                            // text start address (word address)
logic [15: 0] text_addr;                                  // address to fetch character+color attribute
logic [15: 0] text_line_addr;                             // address of start of character+color attribute line
logic [3: 0] font_height;                                 // max height of font cell-1
logic [2: 0] char_x;                                      // current column of font cell (also controls memory access timing)
logic [3: 0] char_y;                                      // current line of font cell
logic [2: 0] fine_scrollx;                                // X fine scroll
logic [3: 0] fine_scrolly;                                // Y fine scroll
logic [7: 0] font_shift_out;                              // bit pattern shifting out for current font character line
logic [7: 0] text_color;                                  // background/foreground color attribute for current character
logic [7: 0] text_color_temp;                             // background/foreground color for next character

// color palette array
logic [11: 0] palette_r [0: 15];

// feature enable signals
logic tg_enable;                                          // text generation
logic bm_enable;                                          // bitmap enable

// video sync generation via state machine (Thanks tnt & drr - a much more efficient method!)
localparam STATE_PRE_SYNC = 2'b00;
localparam STATE_SYNC = 2'b01;
localparam STATE_POST_SYNC = 2'b10;
localparam STATE_VISIBLE = 2'b11;

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
logic           visible;
logic           h_last_line_pixel;
logic           v_last_frame_pixel;
logic           [1: 0] h_state_next;
logic           [1: 0] v_state_next;
logic           mem_fetch_next;
logic           mem_fetch_sync;

always_comb     hsync = (h_state == STATE_SYNC);
always_comb     vsync = (v_state == STATE_SYNC);
always_comb     visible = tg_enable && (h_state == STATE_VISIBLE) && (v_state == STATE_VISIBLE);
always_comb     h_last_line_pixel = (h_state_next == STATE_PRE_SYNC) && (h_state == STATE_VISIBLE);
always_comb     v_last_frame_pixel = (v_state_next == STATE_VISIBLE) && (v_state == STATE_POST_SYNC) && h_last_line_pixel;
always_comb     h_state_next = (h_count == h_count_next_state) ? h_state + 1 : h_state;
always_comb     v_state_next = (h_last_line_pixel && v_count == v_count_next_state) ? v_state + 1 : v_state;
always_comb     mem_fetch_next = (v_state == STATE_VISIBLE && h_count == mem_fetch_toggle) ? ~mem_fetch : mem_fetch;
always_comb     mem_fetch_sync = (~mem_fetch && mem_fetch_next);

logic [10: 0] h_count_next;
logic [10: 0] v_count_next;

// combinational block for video timing generation
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

    // set mem_fetch next toggle for video memory access
    mem_fetch_toggle = mem_fetch ? H_MEM_END : H_MEM_BEGIN;

    // scanning horizontally left to right, offscreen pixels are on left before visible pixels
    case (h_state)
        STATE_PRE_SYNC:
            h_count_next_state = H_FRONT_PORCH - 1;
        STATE_SYNC:
            h_count_next_state = H_FRONT_PORCH + H_SYNC_PULSE - 1;
        STATE_POST_SYNC:
            h_count_next_state = OFFSCREEN_WIDTH - 1;
        STATE_VISIBLE:
            h_count_next_state = TOTAL_WIDTH - 1;
    endcase

    // scanning vertically top to bottom, offscreen lines are on bottom after visible lines
    case (v_state)
        STATE_PRE_SYNC:
            v_count_next_state = VISIBLE_HEIGHT + V_FRONT_PORCH - 1;
        STATE_SYNC:
            v_count_next_state = VISIBLE_HEIGHT + V_FRONT_PORCH + V_SYNC_PULSE - 1;
        STATE_POST_SYNC:
            v_count_next_state = TOTAL_HEIGHT - 1;
        STATE_VISIBLE:
            v_count_next_state = VISIBLE_HEIGHT - 1;
    endcase
end

// logic aliases
logic font_pix;
assign font_pix = font_shift_out[7];                    // current pixel from font data shift-logic out
logic [3: 0] forecolor;
assign forecolor = text_color[3: 0];                        // current character foreground color palette index (0-15)
logic [3: 0] backcolor;
assign backcolor = text_color[7: 4];                        // current character background color palette index (0-15)

// continually form fontram address from text data from vram and char_y (avoids extra cycle for lookup)
assign fontram_addr_o = {1'b0, vram_data_i[7: 0], char_y};

always_ff @(posedge clk) begin
    if (reset_i) begin
        // TODO: Use BRAM for palette?
        // default IRGB palette colors (x"RGB")
`ifndef SYNTHESIS
        palette_r[0] <= 12'h001;                // black (with a bit of blue for debugging)
`else
        palette_r[0] <= 12'h000;                // black
`endif

        palette_r[1] <= 12'h00A;                // blue
        palette_r[2] <= 12'h0A0;                // green
        palette_r[3] <= 12'h0AA;                // cyan
        palette_r[4] <= 12'hA00;                // red
        palette_r[5] <= 12'hA0A;                // magenta
        palette_r[6] <= 12'hAA0;                // brown
        palette_r[7] <= 12'hAAA;                // light gray
        palette_r[8] <= 12'h555;                // dark gray
        palette_r[9] <= 12'h55F;                // light blue
        palette_r[10] <= 12'h5F5;                // light green
        palette_r[11] <= 12'h5FF;                // light cyan
        palette_r[12] <= 12'hF55;                // light red
        palette_r[13] <= 12'hF5F;                // light magenta
        palette_r[14] <= 12'hFF5;                // yellow
        palette_r[15] <= 12'hFFF;                // white

        tg_enable <= 1'b0;                // text generation enable
        bm_enable <= 1'b0;                // text generation enable

        h_state <= STATE_PRE_SYNC;
        v_state <= STATE_VISIBLE;
        mem_fetch <= 1'b0;
        h_count <= 11'h000;
        v_count <= 11'h000;
        fine_scrollx <= 3'b000;
        fine_scrolly <= 4'b0000;
        font_height <= 4'b1111;
        font_shift_out <= 8'h00;
        text_addr <= 16'h0000;
        text_start_addr <= 16'h0000;
        text_line_addr <= 16'h0000;
        text_color <= 8'h00;
        text_color_temp <= 8'h00;
        char_x <= 3'b0;
        char_y <= 4'b0;
        blit_cycle_o <= 1'b0;
        fontram_sel_o <= 1'b0;
        vram_sel_o <= 1'b0;
        vram_addr_o <= 16'h0000;
        red_o <= 4'b0;
        green_o <= 4'b0;
        blue_o <= 4'b0;
        hsync_o <= 1'b0;
        vsync_o <= 1'b0;
        visible_o <= 1'b0;
    end

    else begin

        // default outputs
        blit_cycle_o <= 1'b1;        // default to let bltter have any not needed (outside of data fetch area)
        vram_sel_o <= 1'b0;        // default to no VRAM access
        fontram_sel_o <= 1'b0;        // default to no font access

        red_o <= 4'h0;                    // default black color (VGA especially needs black outside of display area)
        green_o <= 4'h0;
        blue_o <= 4'h0;

        font_shift_out <= {font_shift_out[6: 0], 1'b0};        // shift font pixel data left (7 is new pixel)

        char_x <= char_x + 1;                                // increment character cell column
        if (mem_fetch_sync) begin
            char_x <= 3'b0;                            // reset on fetch sync signal
        end

        // memory read for text
        if (tg_enable && mem_fetch) begin

            // set memory cycle
            blit_cycle_o <= char_x[1];                    // blitter and video alternate every 2 cycles

            // bitmap generation
            if (bm_enable == 1'b1) begin
            end

            // text character generation
            case (char_x)                                    // do memory access based on char column
                3'b000: begin
                end

                3'b001: begin
                end

                3'b010: begin
                end

                3'b011: begin
                end

                3'b100: begin
                    vram_addr_o <= text_addr;                            // put text+color address on vram bus
                end

                3'b101: begin                                            // read vram for color + text
                    vram_sel_o <= 1'b1;                                    // select vram
                    fontram_sel_o <= 1'b1;                                    // select fontram (vram output & char y will drive fontram addr)
                end

                3'b110: begin
                    text_color_temp <= vram_data_i[15: 8];                // save color data from vram
                end

                3'b111: begin                                            // switch to new character data for next pixel
                    font_shift_out <= fontram_data_i;                        // use next font data byte
                    text_color <= text_color_temp;                        // use next font color byte (screen color if monochrome)
                    text_addr <= text_addr + 1;                            // next char+attribute
                end

            endcase
        end

        // color lookup
        if (visible) begin                                    // if this pixel is visible
            // text pixel output
            if (tg_enable && font_pix) begin                    // if text generation enabled and font pixel set
                red_o <= palette_r[forecolor][11: 8];                 // use foreground color palette entry
                green_o <= palette_r[forecolor][7: 4];
                blue_o <= palette_r[forecolor][3: 0];
            end

            else begin                                            // otherwise
                red_o <= palette_r[backcolor][11: 8];                // use background color palette entry
                green_o <= palette_r[backcolor][7: 4];
                blue_o <= palette_r[backcolor][3: 0];
            end
        end

        // end of line
        if (h_last_line_pixel) begin                        // if last pixel of scan-line
            if (char_y == font_height) begin                    // if last line of char cell
                char_y <= 4'h0;                            // reset font line
                text_line_addr <= text_line_addr + CHARS_WIDE;        // new line start address
                text_addr <= text_line_addr + CHARS_WIDE;        // new text start address
            end

            else begin                                            // else next line of char cell
                char_y <= char_y + 1;                                // next char tile line
                text_addr <= text_line_addr;                        // text addr back to line start
            end
        end

        // end of frame
        if (v_last_frame_pixel) begin                            // if last pixel of frame
            tg_enable <= enable_i;                            // enable/disable text generation
            char_y <= fine_scrolly;                        // start next frame at Y fine scroll line in char
            text_addr <= text_start_addr;                        // reset to start of text data
            text_line_addr <= text_start_addr;                        // reset to start of text data
        end

        // update registered signals from combinatorial "next" versions
        h_state <= h_state_next;
        v_state <= v_state_next;
        h_count <= h_count_next;
        v_count <= v_count_next;
        mem_fetch <= mem_fetch_next;

        // set video output signals (color already set)
        hsync_o <= hsync ? H_SYNC_POLARITY : ~H_SYNC_POLARITY;
        vsync_o <= vsync ? V_SYNC_POLARITY : ~V_SYNC_POLARITY;
        visible_o <= visible;
    end
end

endmodule
