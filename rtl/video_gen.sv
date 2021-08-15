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
    output      logic            bus_intr_o,         // bus interrupt output
    output      logic            vsync_o, hsync_o,   // VGA sync outputs
    output      logic            dv_de_o,            // VGA video active signal (needed for HDMI)
    // standard signals
    input  wire logic            reset_i,            // system reset in
    input  wire logic            clk                 // clock (video pixel clock)
);

localparam [31:0] githash = 32'H`GITHASH;

// Emperically determined (at extremes of horizontal scroll [worst case])
// (odd numbers because 4 cycle latency through "fetch pipeline" and buffered)
`ifdef USE_BPPTEST
// 4bpp localparam H_MEM_BEGIN = xv::OFFSCREEN_WIDTH-18;     // memory fetch starts over a tile early
localparam H_MEM_BEGIN = xv::OFFSCREEN_WIDTH-10;     // memory fetch starts over a tile early

localparam H2X_MEM_BEGIN = xv::OFFSCREEN_WIDTH-12;  // and 8 pixels earlier with horizontal pixel double
localparam H_MEM_END = xv::TOTAL_WIDTH-1;           // memory fetch can ends a bit early
`else
localparam H_MEM_BEGIN = xv::OFFSCREEN_WIDTH-7;     // memory fetch starts over a tile early
localparam H2X_MEM_BEGIN = xv::OFFSCREEN_WIDTH-12;  // and 8 pixels earlier with horizontal pixel double
localparam H_MEM_END = xv::TOTAL_WIDTH-1;           // memory fetch can ends a bit early
`endif

logic vg_enable;                                    // video generation enabled (else black/blank)

// video generation signals
logic           pa_enable;                          // enable plane A
logic [15:0]    pa_start_addr;                      // text start address (word address)
logic [15:0]    pa_line_width;                      // words per disply line
logic  [3:0]    pa_fine_scrollx;                    // X fine scroll
logic  [4:0]    pa_fine_scrolly;                    // Y fine scroll
logic           pa_font_in_vram;                    // 0=fontmem, 1=vram
logic           pa_bm_enable;                       // bitmap enable (else text mode)
logic  [5:0]    pa_font_bank;                       // vram/fontmem font bank 0-3 (0/1 with 8x16) fontmem, or 2KB/4K
logic  [3:0]    pa_font_height;                     // max height of font cell

`ifdef USE_BPPTEST
logic  [1:0]    pa_bpp;                             // bpp code (bpp_depth_t)
logic  [7:0]    pa_colorbase;
logic  [1:0]    pa_h_repeat;                        // horizontal pixel repeat
logic  [1:0]    pa_v_repeat;                        // vertical pixel repeat
logic  [2:0]    pa_tile_x;                          // current column of font cell
logic  [3:0]    pa_tile_y;                          // current line of font cell
`else
logic  [3:0]    pa_tile_x;                          // current column of font cell (extra bit for horizontal double)
logic  [4:0]    pa_tile_y;                          // current line of font cell (extra bit for vertical double)
`endif

// obsolete?
logic           pa_h_double;                        // horizontal pixel double
logic           pa_v_double;                        // vertical pixel double
logic  [7:0]    pa_text_attrib;                     // bit pattern shifting out for current font tile line
logic  [7:0]    pa_text_tile;                       // current tile index
logic  [7:0]    pa_shift_out;                       // bit pattern shifting out for current font tile line

// temp signals
logic [15:0]    pa_addr;                            // address to fetch tile+color attribute
logic [15:0]    pa_data_save;                       // 1st fetched display data
logic [15:0]    pa_line_addr;                       // address of start of tile+color attribute line
logic [15:0]    pa_font_addr;                       // font data address (VRAM or FONTRAM)

`ifdef USE_BPPTEST
logic [15:0]    pa_data_save1;                      // 1st fetched display data
logic [15:0]    pa_data_save2;                      // 2nd fetched display data
logic [15:0]    pa_pixel_shift;
logic  [1:0]    pa_h_count;
logic  [1:0]    pa_v_count;
`endif

// video sync generation via state machine (Thanks tnt & drr - a much more efficient method!)
typedef enum logic [1:0] {
    STATE_PRE_SYNC  = 2'b00,
    STATE_SYNC      = 2'b01,
    STATE_POST_SYNC = 2'b10,
    STATE_VISIBLE   = 2'b11
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

// video config registers read/write
always_ff @(posedge clk) begin
    if (reset_i) begin
        pa_start_addr       <= 16'h0000;
`ifdef USE_BPPTEST
        pa_line_width       <= 160;
`else
        pa_line_width       <= xv::TILES_WIDE[15:0];
`endif
        pa_fine_scrollx     <= 4'b0000;         // low bit is for "1/2 doubled pixel" when pa_h_double
        pa_fine_scrolly     <= 5'b00000;        // low bit is for "1/2 doubled pixel" when pa_v_double
        pa_font_height      <= 4'b1111;
        pa_font_in_vram     <= 1'b0;
        pa_font_bank        <= 6'b00000;
        pa_h_double         <= 1'b0;            // horizontal pixel double (repeat)
        pa_v_double         <= 1'b0;            // vertical pixel double (repeat)
        pa_bm_enable        <= 1'b0;            // bitmap mode
`ifdef USE_BPPTEST
        pa_bpp              <= xv::BPP_8;
        pa_colorbase        <= 8'h00;
        pa_h_repeat         <= 2'b01;           // TODO init to 1 temp for Tut
        pa_v_repeat         <= 2'b01;
`endif
    end else begin
        // video register write
        if (vgen_reg_wr_i) begin
            case (vgen_reg_num_i[3:0])
                xv::AUX_DISPSTART[3:0]: begin
                    pa_start_addr <= vgen_reg_data_i;
                end
                xv::AUX_DISPWIDTH[3:0]: begin
                    pa_line_width <= vgen_reg_data_i;
                end
                xv::AUX_SCROLLXY[3:0]: begin
                    pa_fine_scrollx    <= vgen_reg_data_i[11:8];
                    pa_fine_scrolly    <= vgen_reg_data_i[4:0];
                end
                xv::AUX_FONTCTRL[3:0]: begin
                    pa_font_bank       <= vgen_reg_data_i[15:10];
                    pa_font_in_vram   <= vgen_reg_data_i[8];
                    pa_font_height     <= vgen_reg_data_i[3:0];
                end
                xv::AUX_GFXCTRL[3:0]: begin
                    pa_bm_enable       <= vgen_reg_data_i[15];
                    pa_v_double        <= vgen_reg_data_i[1];
                    pa_h_double        <= vgen_reg_data_i[0];
                end
                xv::AUX_UNUSED_5[3:0]: begin
                end
                xv::AUX_UNUSED_6[3:0]: begin
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
            xv::AUX_SCROLLXY[3:0]:      vgen_reg_data_o <= { 4'b0000, pa_fine_scrollx, 3'b000, pa_fine_scrolly };
            xv::AUX_FONTCTRL[3:0]:      vgen_reg_data_o <= { pa_font_bank, 1'b0, pa_font_in_vram, 4'b0000, pa_font_height  };
            xv::AUX_GFXCTRL[3:0]:       vgen_reg_data_o <= { pa_bm_enable, 13'b0000000000000, pa_v_double, pa_h_double };
            xv::AUX_UNUSED_5[3:0]:      vgen_reg_data_o <= 16'h0000;
            xv::AUX_UNUSED_6[3:0]:      vgen_reg_data_o <= 16'h0000;
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
always_comb     h_last_line_pixel = (h_state_next == STATE_PRE_SYNC) && (h_state == STATE_VISIBLE);
always_comb     v_last_frame_pixel = (v_state_next == STATE_VISIBLE) && (v_state == STATE_POST_SYNC) && h_last_line_pixel;
always_comb     h_state_next = (h_count == h_count_next_state) ? h_state + 1'b1 : h_state;
always_comb     v_state_next = (h_last_line_pixel && v_count == v_count_next_state) ? v_state + 1'b1 : v_state;
always_comb     mem_fetch_next = (v_state == STATE_VISIBLE && h_count == mem_fetch_toggle) ? ~mem_fetch : mem_fetch;
always_comb     h_start_line_fetch = (~mem_fetch && mem_fetch_next);

logic [10: 0] h_count_next;
logic [10: 0] v_count_next;

// combinational block for video counters
always_comb begin
    h_count_next = h_count + 1'b1;
    v_count_next = v_count;

    if (h_last_line_pixel) begin
        h_count_next = 0;
        v_count_next = v_count + 1'b1;

        if (v_last_frame_pixel) begin
            v_count_next = 0;
        end
    end
end

// combinational block for video fetch start and stop
always_comb begin
    // set mem_fetch next toggle for video memory access (pa_h_double subtracts an extra 16)
    if (mem_fetch) begin
        mem_fetch_toggle = H_MEM_END[10:0];
    end else begin
        mem_fetch_toggle = (pa_h_double ? H2X_MEM_BEGIN[10:0] : H_MEM_BEGIN[10:0]) - { 7'b0, pa_fine_scrollx[3] & pa_h_double, pa_fine_scrollx[2:0] };
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

// logic aliases
logic           font_pix;                       // current pixel from font data shift-logic out
assign          font_pix = pa_shift_out[7];
logic [3: 0]    forecolor;                      // current tile foreground color palette index (0-15)
assign          forecolor = pa_text_attrib[3:0];
logic [3: 0]    backcolor;                      // current tile background color palette index (0-15)
assign          backcolor = pa_text_attrib[7:4];

`ifndef USE_BPPTEST
// generate font address from vram_data_i (assumed to be tile tile to lookup) and pa_tile_y
assign pa_font_addr = pa_font_height[3] ? {pa_font_bank[5:1], vram_data_i[7: 0], pa_tile_y[4:2]}
                                        : {pa_font_bank[5:0], vram_data_i[7: 0], pa_tile_y[3:2]};
`endif

always_ff @(posedge clk) begin
    if (reset_i) begin
        h_state         <= STATE_PRE_SYNC;
        v_state         <= STATE_VISIBLE;
        mem_fetch       <= 1'b0;
        h_count         <= 11'h000;
        v_count         <= 11'h000;
        pa_shift_out    <= 8'h00;
        pa_text_attrib  <= 8'h00;
        pa_addr         <= 16'h0000;
        pa_data_save    <= 16'h0000;
        pa_line_addr    <= 16'h0000;
`ifdef USE_BPPTEST
        pa_tile_x       <= 3'b0;
        pa_tile_y       <= 4'b0;
`else
        pa_tile_x       <= 4'b0;
        pa_tile_y       <= 5'b0;
`endif
        fontram_sel_o   <= 1'b0;
        vram_sel_o      <= 1'b0;
        vram_addr_o     <= 16'h0000;
        pal_index_o     <= 8'b0;
        bus_intr_o      <= 1'b0;
        hsync_o         <= 1'b0;
        vsync_o         <= 1'b0;
        dv_de_o         <= 1'b0;
        pa_enable       <= 1'b1;            // plane A starts enabled
        vg_enable       <= 1'b1;            // video starts disabled
`ifdef USE_BPPTEST
        pa_data_save1   <= 16'h0000;
        pa_data_save2   <= 16'h0000;
        pa_h_count      <= 2'b00;
        pa_v_count      <= 2'b00;
`endif
    end else begin
        // default outputs
        vram_sel_o      <= 1'b0;                            // default to no VRAM access
        fontram_sel_o   <= 1'b0;                            // default to no font access

`ifdef USE_BPPTEST
        if (mem_fetch) begin
            fontram_addr_o  <= 12'b0;
            if (pa_h_count == 2'b00) begin
                case (pa_tile_x[1:0])
                    2'b00: begin
                        vram_sel_o      <= pa_enable;           // select vram
                        vram_addr_o     <= pa_addr;             // put text+color address on vram bus
                        pa_addr         <= pa_addr + 1'b1;      // next tile+attribute

                        if (pa_bpp == xv::BPP_8) begin
                            pa_data_save2   <= pa_data_save1;       // transfer last word read
                            pa_data_save1   <= vram_data_i;         // then save current VRAM data
                        end
                    end
                    2'b01: begin
                    end
                    2'b10: begin
                        if (pa_bpp == xv::BPP_4) begin
                            pa_data_save2   <= pa_data_save1;       // transfer last word read
                            pa_data_save1   <= vram_data_i;         // then save current VRAM data
                        end
                    end
                    2'b11: begin
                    end
                endcase
            end
            
`else
        if (mem_fetch) begin
            if (~pa_h_double) begin
                pa_shift_out <= {pa_shift_out[6: 0], 1'b0}; // shift font line data (high bit is current pixel)
                case (pa_tile_x[3:1])
                    3'b000: begin
                        vram_sel_o      <= pa_enable;           // select vram
                        vram_addr_o     <= pa_addr;             // put text+color address on vram bus
                        pa_addr         <= pa_addr + 1'b1;      // next tile+attribute
                    end
                    3'b001: begin
                    end
                    3'b010: begin
                        pa_data_save    <= vram_data_i;     // then save current VRAM data (color for next tile)
                        vram_sel_o      <= pa_font_in_vram & ~pa_bm_enable & pa_enable;    // select vram
                        fontram_sel_o   <= ~pa_font_in_vram & ~pa_bm_enable & pa_enable;   // select fontram
                        vram_addr_o     <= pa_font_addr;
                        fontram_addr_o  <= pa_font_addr[11:0];
                    end
                    3'b011: begin
                    end
                    3'b100: begin
                        if (pa_bm_enable) begin
                            pa_shift_out <= pa_data_save[7:0];
                        end else begin
                            if (pa_tile_y[1]) begin    // use even or odd byte from font word
                                pa_shift_out <= pa_font_in_vram ? vram_data_i[7:0] : fontram_data_i[7:0];  // use font lookup data to set font line shift out
                            end else begin
                                pa_shift_out <= pa_font_in_vram ? vram_data_i[15:8] : fontram_data_i[15:8]; // use font lookup data to set font line shift out
                            end
                        end
                        pa_text_tile    <= pa_data_save[7:0];         // used previously saved tile
                        pa_text_attrib  <= pa_data_save[15:8];        // used previously saved color
                    end
                    3'b101: begin
                    end
                    3'b110: begin
                    end
                    3'b111: begin
                    end
                    default: begin
                    end
                endcase
            end else begin
                if (pa_tile_x[0]) begin
                    pa_shift_out <= {pa_shift_out[6: 0], 1'b0}; // shift font line data (high bit is current pixel)
                end
                case (pa_tile_x)
                    4'b0101: begin
                        vram_sel_o      <= vg_enable;                   // select vram
                        vram_addr_o     <= pa_addr;                     // put text+color address on vram bus
                        pa_addr         <= pa_addr + 1'b1;              // next tile+attribute
                    end
                    4'b0110: begin
                    end
                    4'b0111: begin
                        pa_data_save    <= vram_data_i;                 // then save current VRAM data (color for next tile)
                        vram_sel_o      <= pa_font_in_vram & ~pa_bm_enable & vg_enable;    // select vram
                        fontram_sel_o   <= ~pa_font_in_vram & ~pa_bm_enable & vg_enable;   // select fontram
                        vram_addr_o     <= pa_font_addr;
                        fontram_addr_o  <= pa_font_addr[11:0];
                    end
                    4'b1000: begin
                    end
                    4'b1001: begin
                        if (pa_bm_enable) begin
                            pa_shift_out <= pa_data_save[7:0];
                        end else begin
                            if (pa_tile_y[1]) begin
                                pa_shift_out <= pa_font_in_vram ? vram_data_i[7:0] : fontram_data_i[7:0]; // use font lookup data to set font line shift out
                            end else begin
                                pa_shift_out <= pa_font_in_vram ? vram_data_i[15:8] : fontram_data_i[15:8]; // use font lookup data to set font line shift out
                            end
                        end
                        pa_text_tile    <= pa_data_save[7:0];         // used previously saved tile
                        pa_text_attrib  <= pa_data_save[15:8];        // used previously saved color
                    end
                    default: begin
                    end
                endcase
            end
`endif
        end

`ifdef USE_BPPTEST
        case (pa_bpp)
            xv::BPP_1_ATTR,
            xv::BPP_2:      pal_index_o <= { pa_colorbase[7:2], pa_pixel_shift[15:14] };
            xv::BPP_4:      pal_index_o <= { pa_colorbase[7:4], pa_pixel_shift[15:12] };
            xv::BPP_8:      pal_index_o <= pa_pixel_shift[15:8];
        endcase

        if (pa_h_count == 2'b00) begin
            pa_h_count      <= pa_h_repeat;

            pa_tile_x       <= pa_tile_x + 1'b1;
            case (pa_bpp)
                xv::BPP_1_ATTR,
                xv::BPP_2: begin
                    pa_pixel_shift  <= { pa_pixel_shift[13:0], 2'h0 };
                    if (pa_tile_x[2:0] == 3'b111) begin
                        pa_pixel_shift  <= pa_data_save2;
                        pa_tile_x   <= 3'b000;
                    end
                end
                xv::BPP_4: begin
                    pa_pixel_shift  <= { pa_pixel_shift[11:0], 4'h0 };
                    if (pa_tile_x[1:0] == 2'b11) begin
                        pa_pixel_shift  <= pa_data_save2;
                        pa_tile_x   <= 3'b000;
                    end
                end
                xv::BPP_8: begin
                    pa_pixel_shift  <= { pa_pixel_shift[7:0], 8'h0 };
                    if (pa_tile_x[0] == 1'b1) begin
                        pa_pixel_shift  <= pa_data_save2;
                        pa_tile_x   <= 3'b000;
                    end
                end
            endcase

        end else begin
            pa_h_count  <= pa_h_count - 1'b1;
        end
`else
        // pixel color output
        pal_index_o <= { 4'h0, font_pix ? forecolor : backcolor };

        // next pixel
        pa_tile_x <= pa_tile_x + (pa_h_double ? 4'd1 : 4'd2);     // increment tile cell column (by 2 normally, 1 if pixel doubled)
`endif

        // start of line
        if (h_start_line_fetch) begin                   // on line fetch start signal
`ifdef USE_BPPTEST
            pa_tile_x   <= 3'b000;                      // reset on pa_tile_x cycle (to start tile line at proper pixel)
            vram_sel_o      <= pa_enable;           // select vram
            vram_addr_o     <= pa_addr;             // put text+color address on vram bus
            pa_addr         <= pa_addr + 1'b1;      // next tile+attribute
`else
            pa_tile_x   <= 4'b0000;                     // reset on pa_tile_x cycle (to start tile line at proper pixel)
`endif
        end

        // end of line
        if (h_last_line_pixel) begin                    // if last pixel of scan-line
`ifdef USE_BPPTEST
            if (pa_v_count == 2'b00) begin
                pa_v_count      <= pa_v_repeat;
                pa_line_addr    <= pa_line_addr + pa_line_width;    // new line start address
                pa_addr         <= pa_line_addr + pa_line_width;    // new text start address
            end else begin
                pa_addr     <= pa_line_addr;                    // text addr back to line start
                pa_v_count  <= pa_v_count - 1'b1;
            end
`else
            pa_addr <= pa_line_addr;                    // text addr back to line start
            if (pa_tile_y == { pa_font_height, pa_v_double } || pa_bm_enable) begin  // if last line of tile cell
                pa_tile_y     <= 5'h0;                            // reset tile cell line
                pa_line_addr  <= pa_line_addr + pa_line_width;    // new line start address
                pa_addr       <= pa_line_addr + pa_line_width;    // new text start address
            end
            else begin                                      // else next line of tile cell
                pa_tile_y <= pa_tile_y + (pa_v_double ? 5'd1 : 5'd2);      // next tile tile line (by 2 normally, 1 if pixel doubled)
            end
`endif            
        end

        // end of frame
        if (v_last_frame_pixel) begin                   // if last pixel of frame
            vg_enable       <= enable_i;                // enable/disable text generation
`ifdef USE_BPPTEST
            pa_v_count      <= pa_v_repeat;
//            pa_tile_y       <= pa_fine_scrolly;       // TODO fine scroll
`else
            pa_tile_y       <= pa_v_double ? pa_fine_scrolly : { pa_fine_scrolly[3:0], 1'b0 }; // start next frame at Y fine scroll line
`endif
            pa_addr         <= pa_start_addr;           // reset to start of text data
            pa_line_addr    <= pa_start_addr;           // reset to start of text data
        end

        // update registered signals from combinatorial "next" versions
        h_state <= h_state_next;
        v_state <= v_state_next;
        h_count <= h_count_next;
        v_count <= v_count_next;
        mem_fetch <= mem_fetch_next;

        // set video output signals (color already set)
        bus_intr_o  <= vsync;   // TODO general purpose interrupt
        hsync_o     <= hsync ? xv::H_SYNC_POLARITY : ~xv::H_SYNC_POLARITY;
        vsync_o     <= vsync ? xv::V_SYNC_POLARITY : ~xv::V_SYNC_POLARITY;
        dv_de_o     <= dv_display_ena;
    end
end

endmodule
`default_nettype wire               // restore default
