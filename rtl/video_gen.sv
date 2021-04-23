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
`timescale 1ns/1ps

module video_gen(
            // control outputs
            output logic            blit_cycle_o,       // 0=video memory cycle, 1=Blit memory cycle
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

`include "xosera_pkg.sv"
`include "xosera_defs.svh"        // Xosera global Verilog definitions

// Emperically determined (at extremes of horizontal scroll [worst case])
// (odd numbers because 4 cycle latency through "fetch pipeline" and buffered)
localparam H_MEM_BEGIN = OFFSCREEN_WIDTH-13;            // memory fetch starts over a character early
localparam H_MEM_END = TOTAL_WIDTH-4;                   // memory fetch can ends a bit early

// mode options
logic pixel_h_dbl;
logic pixel_v_dbl;

// bitmap generation signals
logic [15:0]    bitmap_start_addr;                        // bitmap start address
logic [15:0]    bitmap_addr;                              // current bitmap address
logic [15:0]    bitmap_data;                              // bit pattern shifting out for current bitmap word
logic [15:0]    bitmap_data_next;                         // next bitmap word to shift out

// text generation signals
logic [15:0]    text_start_addr;                          // text start address (word address)
logic [15:0]    text_line_width;
logic [15:0]    text_addr;                                // address to fetch character+color attribute
logic [15:0]    text_line_addr;                           // address of start of character+color attribute line
logic  [3:0]    font_height;                              // max height of font cell
logic  [1:0]    font_bank;                                // font bank 0-3 (0/1 with 8x16)
logic  [3:0]    char_x;                                   // current column of font cell (synchronized with memory access timing)
logic  [3:0]    char_y;                                   // current line of font cell
logic  [3:0]    fine_scrollx;                             // X fine scroll
logic  [3:0]    fine_scrolly;                             // Y fine scroll
logic  [7:0]    text_color;                               // bit pattern shifting out for current font character line
logic  [7:0]    font_shift_out;                           // bit pattern shifting out for current font character line
logic [15:0]    vram_data_save;                           // background/foreground color attribute for current character

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
logic           mem_fetch_sync;

always_comb     hsync = (h_state == STATE_SYNC);
always_comb     vsync = (v_state == STATE_SYNC);
always_comb     dv_display_ena = tg_enable && (h_state == STATE_VISIBLE) && (v_state == STATE_VISIBLE);
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

    // set mem_fetch next toggle for video memory access (pixel_h_dbl subtracts an extra 16)
    mem_fetch_toggle = mem_fetch ? H_MEM_END[10:0] : (H_MEM_BEGIN[10:0] - { 6'b0, pixel_h_dbl, fine_scrollx });

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

// video config registers
always_ff @(posedge clk) begin
    if (reset_i) begin
        text_start_addr <= 16'h0000;
        text_line_width <= CHARS_WIDE[15:0];
        fine_scrollx    <= 4'b0000;
        fine_scrolly    <= 4'b0000;
        font_height     <= 4'b1111;
        font_bank       <= 2'b00;
        pixel_h_dbl     <= 1'b0;
        pixel_v_dbl     <= 1'b0;
    end
    else begin
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
                    fine_scrolly    <= vgen_reg_data_i[3:0];
                end
                xv::AUX_VID_W_FONTCTRL[2:0]: begin
                    font_height     <= vgen_reg_data_i[3:0];
                    font_bank       <= vgen_reg_data_i[9:8];
                end
                xv::AUX_VID_W_GFXCTRL[2:0]: begin
                    pixel_h_dbl     <= vgen_reg_data_i[0];
                    pixel_v_dbl     <= vgen_reg_data_i[1];
                end
                default: begin
                end
            endcase
        end

        case (vgen_reg_num_i[1:0])
            2'b00:      vgen_reg_data_o <= VISIBLE_WIDTH[15:0];
            2'b01:      vgen_reg_data_o <= VISIBLE_HEIGHT[15:0];
            2'b10:      vgen_reg_data_o <= 16'b1000_0000_0000_0001;  // TODO feature bits
            2'b11:      vgen_reg_data_o <= {(v_state != STATE_VISIBLE), (h_state != STATE_VISIBLE), 3'b000, v_count }; // negative when not vsync
        endcase
    end
end

// logic aliases
logic font_pix;
logic [3: 0] forecolor;
logic [3: 0] backcolor;
logic  [7:0]    text_tile;                              // current character
assign font_pix = font_shift_out[7];                    // current pixel from font data shift-logic out
assign forecolor = text_color[3:0];                     // current character foreground color palette index (0-15)
assign backcolor = text_color[7:4];                     // current character background color palette index (0-15)
assign text_tile = vram_data_save[7:0];                 // current character tile index

// continually form fontram address from text data from vram and char_y (avoids extra cycle for lookup) TODO: Interleave fonts?
assign fontram_addr_o = font_height[3] ? {font_bank[1], vram_data_i[7: 0], char_y[3:0]} : {font_bank[1:0], vram_data_i[7: 0], char_y[2:0]};

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
        char_x          <= 4'b0;
        char_y          <= 4'b0;
        blit_cycle_o    <= 1'b0;
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
        blit_cycle_o <= 1'b1;                           // default to let bltter have VRAM access
        vram_sel_o <= 1'b0;                             // default to no VRAM access
        fontram_sel_o <= 1'b0;                          // default to no font access

        if (vram_sel_o) begin                           // if was VRAM selected (from previous cycle)
            fontram_sel_o <= ~bm_enable;                // then select font lookup if not bitmap mode
        end

        if (fontram_sel_o) begin                        // if font was selected (from previous cycle)
            vram_data_save  <= vram_data_i;             // then save current VRAM data (color for next char)
            font_shift_out  <= fontram_data_i;          // use font lookup data to set font line shift out
            text_color      <= vram_data_save[15:8];    // used previously saved color
        end
        else begin
            if (~(pixel_h_dbl & char_x[0]))             // only shift on even pixels with pixel_h_dbl
            font_shift_out <= {font_shift_out[6: 0], 1'b0}; // shift font line data (high bit is current pixel)
        end

        char_x <= char_x + (pixel_h_dbl ? 1 : 2);       // increment character cell column (by 2 normally, 1 if pixel doubled)

        if (mem_fetch_sync) begin
            char_x <= 4'b0000;             // reset on char_x cycle on fetch sync signal
        end

        // memory read for text
        if (mem_fetch && char_x == 4'b000) begin
            blit_cycle_o <= ~tg_enable;
            vram_sel_o <= tg_enable;                // select vram
            vram_addr_o <= text_addr;               // put text+color address on vram bus
            text_addr <= text_addr + 1;             // next char+attribute
        end

        // color lookup
        // text pixel output
        pal_index_o <= font_pix ? forecolor : backcolor;

        // end of line
        if (h_last_line_pixel) begin                        // if last pixel of scan-line
            if (char_y == font_height) begin                // if last line of char cell
                char_y <= 4'h0;                             // reset font line
                text_line_addr <= text_line_addr + text_line_width; // new line start address
                text_addr <= text_line_addr + text_line_width;   // new text start address
            end
            else begin                                      // else next line of char cell
                char_y <= char_y + 1;                       // next char tile line
                text_addr <= text_line_addr;                // text addr back to line start
            end
        end

        // end of frame
        if (v_last_frame_pixel) begin                       // if last pixel of frame
            tg_enable <= enable_i;                          // enable/disable text generation
            char_y <= fine_scrolly;                         // start next frame at Y fine scroll line in char
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
        hsync_o <= hsync ? H_SYNC_POLARITY : ~H_SYNC_POLARITY;
        vsync_o <= vsync ? V_SYNC_POLARITY : ~V_SYNC_POLARITY;
        dv_de_o <= dv_display_ena;
    end
end

endmodule
