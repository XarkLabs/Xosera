// video_timing.sv
//
// vim: set et ts=4 sw=4
//
// Copyright (c) 2022 Xark - https://hackaday.io/Xark
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

`define XOSERA_VERSION

`ifdef XOSERA_VERSION

module video_timing
(
    // video registers and control
    output      hres_t          h_count_o,              // horizontal video counter
    output      vres_t          v_count_o,              // vertical video counter
    output      logic           h_visible_o,            // horizontal pixel visible (not off left edge)
    output      logic           v_visible_o,            // vertical line visible (not off bottom edge)
    output      logic           end_of_line_o,          // strobe for end of line (h_count resets)
    output      logic           end_of_frame_o,         // strobe for end of frame (v_count resets)
    output      logic           end_of_visible_o,       // strobe for end of visible frame (e.g EOL 479)
    output      logic           vsync_o,                // vertical sync output (polarity depends on video mode)
    output      logic           hsync_o,                // horizontal sync output (polarity depends on video mode)
    output      logic           dv_de_o,                // display enable, true for visible pixel (needed for DV-I)
    input  wire logic           reset_i,                // system reset
    input  wire logic           clk                     // clock (video pixel clock)
);

// NOTE: Both H & V states so both can start at 0

typedef enum logic [1:0] {
    H_STATE_PRE_SYNC  = 2'b00,    // aka front porch
    H_STATE_SYNC      = 2'b01,
    H_STATE_POST_SYNC = 2'b10,    // aka back porch
    H_STATE_VISIBLE   = 2'b11
} horizontal_st;

typedef enum logic [1:0] {
    V_STATE_VISIBLE   = 2'b00,
    V_STATE_PRE_SYNC  = 2'b01,    // aka front porch
    V_STATE_SYNC      = 2'b10,
    V_STATE_POST_SYNC = 2'b11     // aka back porch
} vertical_st;

// sync generation signals (and combinatorial logic "next" versions)

hres_t          h_count;
hres_t          h_count_next;

vres_t          v_count;
vres_t          v_count_next;
vres_t          v_count_match_value;

logic [1:0]     h_state;
logic [1:0]     h_state_next;
hres_t          h_count_match_value;

logic [1:0]     v_state;
logic [1:0]     v_state_next;

logic           end_of_line;
logic           end_of_line_next;

logic           end_of_frame;
logic           end_of_frame_next;

logic           end_of_visible;
logic           end_of_visible_next;

logic           hsync;
logic           hsync_next;

logic           vsync;
logic           vsync_next;

logic           dv_de;
logic           dv_de_next;

assign h_count_o        = h_count;
assign v_count_o        = v_count;
assign end_of_line_o    = end_of_line;
assign end_of_frame_o   = end_of_frame;
assign end_of_visible_o = end_of_visible;
assign v_visible_o      = (v_state == V_STATE_VISIBLE);
assign h_visible_o      = (h_state == H_STATE_VISIBLE);
assign hsync_o          = hsync;
assign vsync_o          = vsync;
assign dv_de_o          = dv_de;

// video sync generation via state machine (Thanks tnt & drr - a much more efficient method!)
always_comb    end_of_line_next        = (h_state == H_STATE_VISIBLE) && (h_state_next == H_STATE_PRE_SYNC);
always_comb    end_of_frame_next       = (v_state == V_STATE_POST_SYNC) && (v_state_next == V_STATE_VISIBLE);
always_comb    end_of_visible_next     = (v_state == V_STATE_VISIBLE) && (v_state_next == V_STATE_PRE_SYNC);
always_comb    hsync_next              = (h_state_next == H_STATE_SYNC) ? xv::H_SYNC_POLARITY : ~xv::H_SYNC_POLARITY;
always_comb    vsync_next              = (v_state_next == V_STATE_SYNC) ? xv::V_SYNC_POLARITY : ~xv::V_SYNC_POLARITY;
always_comb    dv_de_next              = (v_state_next == V_STATE_VISIBLE) && (h_state_next == H_STATE_VISIBLE);

// combinational block for video counters
always_comb begin
    h_count_next = h_count + 1'b1;
    v_count_next = v_count;

    if (end_of_line_next) begin
        h_count_next = '0;

        if (end_of_frame_next) begin
            v_count_next = '0;
        end else begin
            v_count_next = v_count + 1'b1;
        end
    end
end

// combinational block for horizontal video state
always_comb h_state_next    = (h_count == h_count_match_value) ? h_state + 1'b1 : h_state;

always_comb begin
    (* full_case, parallel_case *)  // needed/desirable?
    // scanning horizontally left to right, offscreen pixels are on left before visible pixels
    case (h_state)
        H_STATE_PRE_SYNC:
            h_count_match_value = xv::H_FRONT_PORCH - 1;
        H_STATE_SYNC:
            h_count_match_value = xv::H_FRONT_PORCH + xv::H_SYNC_PULSE - 1;
        H_STATE_POST_SYNC:
            h_count_match_value = xv::H_FRONT_PORCH + xv::H_SYNC_PULSE + xv::H_BACK_PORCH - 1;
        H_STATE_VISIBLE:
            h_count_match_value = xv::TOTAL_WIDTH - 1;
    endcase
end

// combinational block for vertical video state
always_comb v_state_next    = end_of_line_next && (v_count == v_count_match_value) ? v_state + 1'b1 : v_state;

always_comb begin
    (* full_case, parallel_case *)  // needed/desirable?
    // scanning vertically top to bottom, offscreen lines are on bottom after visible lines
    case (v_state)
        V_STATE_VISIBLE:
            v_count_match_value = xv::VISIBLE_HEIGHT - 1;
        V_STATE_PRE_SYNC:
            v_count_match_value = xv::VISIBLE_HEIGHT + xv::V_FRONT_PORCH - 1;
        V_STATE_SYNC:
            v_count_match_value = xv::VISIBLE_HEIGHT + xv::V_FRONT_PORCH + xv::V_SYNC_PULSE - 1;
        V_STATE_POST_SYNC:
            v_count_match_value = xv::TOTAL_HEIGHT - 1;
    endcase
end

// video pixel generation
always_ff @(posedge clk) begin
    if (reset_i) begin
        h_state             <= H_STATE_PRE_SYNC;
        v_state             <= V_STATE_VISIBLE;
        h_count             <= '0;
        v_count             <= '0;

        end_of_line         <= 1'b0;
        end_of_frame        <= 1'b0;
        end_of_visible      <= 1'b0;

        hsync               <= ~xv::H_SYNC_POLARITY;
        vsync               <= ~xv::V_SYNC_POLARITY;
        dv_de               <= 1'b0;

    end else begin

        // update registered signals from combinatorial "next" versions
        end_of_line         <= end_of_line_next;
        end_of_frame        <= end_of_frame_next;
        end_of_visible      <= end_of_visible_next;

        h_state             <= h_state_next;
        v_state             <= v_state_next;
        h_count             <= h_count_next;
        v_count             <= v_count_next;

        // set other video output signals
        hsync               <= hsync_next;
        vsync               <= vsync_next;
        dv_de               <= dv_de_next;
    end
end

endmodule
`default_nettype wire               // restore default

`else

// This vdp_vga_timing.v module was adapted from the icestation-32 project:
// https://github.com/dan-rodrigues/icestation-32

// MIT License

// Copyright (c) 2020 Dan Rodrigues <danrr.gh.oss@gmail.com>

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

module video_timing #(
    parameter H_ACTIVE_WIDTH = xv::VISIBLE_WIDTH,
    parameter V_ACTIVE_HEIGHT = xv::VISIBLE_HEIGHT,

    parameter H_FRONTPORCH = xv::H_FRONT_PORCH,
    parameter H_SYNC = xv::H_SYNC_PULSE,
    parameter H_BACKPORCH = xv::H_BACK_PORCH,

    parameter V_FRONTPORCH = xv::V_FRONT_PORCH,
    parameter V_SYNC = xv::V_SYNC_PULSE,
    parameter V_BACKPORCH = xv::V_BACK_PORCH
)(
    output hres_t h_count_o,
    output vres_t v_count_o,
    output reg v_visible_o,
    output reg end_of_line_o,
    output reg end_of_frame_o,
    output reg end_of_visible_o,
//    output reg active_line_started,
    output reg hsync_o,
    output reg vsync_o,
    output reg dv_de_o,

    input wire reset_i,
    input wire clk
);
    localparam STATE_FP = 0;
    localparam STATE_SYNC = 1;
    localparam STATE_BP = 2;
    localparam STATE_ACTIVE = 3;

    localparam H_SIZE = H_ACTIVE_WIDTH + H_FRONTPORCH + H_SYNC + H_BACKPORCH;
    localparam V_SIZE = V_ACTIVE_HEIGHT + V_FRONTPORCH + V_SYNC + V_BACKPORCH;

//    localparam H_OFFSCREEN_WIDTH = H_SIZE - H_ACTIVE_WIDTH;
//    localparam V_OFFSCREEN_HEIGHT = V_SIZE - V_ACTIVE_HEIGHT;

    hres_t      h_count;
    vres_t      v_count;
    reg         hsync;
    reg         vsync;
    reg         dv_de;

    reg end_of_frame;
    reg end_of_line;
    reg end_of_visible;

    assign h_count_o        = h_count;
    assign v_count_o        = v_count;
    assign end_of_line_o    = end_of_line;
    assign end_of_frame_o   = end_of_frame;
    assign end_of_visible_o = end_of_visible;
    assign v_visible_o      = (y_fsm == STATE_ACTIVE);
    assign hsync_o          = hsync;
    assign vsync_o          = vsync;
    assign dv_de_o          = dv_de;

    wire end_of_line_nx = x_fsm_nx == STATE_FP && x_fsm == STATE_ACTIVE;
    wire end_of_frame_nx = y_fsm_nx == STATE_ACTIVE && y_fsm == STATE_BP && end_of_line_nx;
    wire end_of_visible_nx = end_of_line_nx && y_fsm == STATE_ACTIVE && y_fsm_nx == STATE_FP;

    wire hsync_b_nx = x_fsm == STATE_SYNC;
    wire vsync_b_nx = y_fsm == STATE_SYNC;

    wire dv_de_nx = y_fsm == STATE_ACTIVE && x_fsm_nx == STATE_ACTIVE;

//    wire active_line_started_nx = x_fsm == STATE_FP && x_fsm_nx == STATE_ACTIVE;

    reg [1:0] x_fsm_nx = 0;

    always @* begin
        x_fsm_nx = (h_count == x_next_count ? x_fsm + 1 : x_fsm);
    end

    reg [1:0] y_fsm_nx = 0;

    always @* begin
        y_fsm_nx = y_fsm;

        if (end_of_line_nx) begin
            y_fsm_nx = (v_count == y_next_count ? y_fsm + 1 : y_fsm);
        end
    end

    reg [1:0] x_fsm = 0;
    reg [1:0] y_fsm = STATE_ACTIVE;

    always @(posedge clk) begin
        x_fsm <= x_fsm_nx;
        y_fsm <= y_fsm_nx;
    end

    hres_t x_next_count = 0;
    vres_t y_next_count = 0;

    // target counts to advance state

    always @* begin
        (* full_case, parallel_case *)
        case (x_fsm)
            STATE_FP: x_next_count = H_FRONTPORCH - 1;
            STATE_SYNC: x_next_count = H_FRONTPORCH + H_SYNC - 1;
            STATE_BP: x_next_count = H_SYNC + H_FRONTPORCH + H_BACKPORCH - 1;
            STATE_ACTIVE: x_next_count = H_SIZE - 1;
        endcase
    end

    always @* begin
        (* full_case, parallel_case *)
        case (y_fsm)
            STATE_FP: y_next_count = V_ACTIVE_HEIGHT + V_FRONTPORCH - 1;
            STATE_SYNC: y_next_count = V_ACTIVE_HEIGHT + V_FRONTPORCH + V_SYNC - 1;
            STATE_BP: y_next_count = V_SIZE - 1;
            STATE_ACTIVE: y_next_count = V_ACTIVE_HEIGHT - 1;
        endcase
    end

    hres_t h_count_nx;
    vres_t v_count_nx;

    always @* begin
        h_count_nx = h_count + 1;
        v_count_nx = v_count;

        if (end_of_line_nx) begin
            h_count_nx = 0;
            v_count_nx = v_count + 1;

            if (end_of_frame_nx) begin
                v_count_nx = 0;
            end
        end
    end

    always @(posedge clk) begin
        if (reset_i) begin
            h_count <= '0;
            v_count <= '0;
        end else begin
            h_count <= h_count_nx;
            v_count <= v_count_nx;
        end
    end

    always @(posedge clk) begin
        if (reset_i) begin
            hsync <= !xv::H_SYNC_POLARITY;
            vsync <= !xv::V_SYNC_POLARITY;
            dv_de <= '0;
            end_of_line <= '0;
            end_of_frame <= '0;
//        active_line_started <= active_line_started_nx;
            end_of_visible <= '0;
        end else begin
            hsync <= !hsync_b_nx;
            vsync <= !vsync_b_nx;
            dv_de <= dv_de_nx;
            end_of_line <= end_of_line_nx;
            end_of_frame <= end_of_frame_nx;
//        active_line_started <= active_line_started_nx;
            end_of_visible <= end_of_visible_nx;
        end
    end

`ifdef FORMAL

    reg past_valid = 0;
    reg [1:0] past_counter = 0;

    initial begin
        restrict(reset);
    end

    always @(posedge clk) begin
        if (past_counter < 3) begin
            past_counter <= past_counter + 1;
        end else begin
            past_valid <= 1;
        end

        if (past_counter != 0) begin
            assume(!reset);
        end
    end

    always @(posedge clk) begin
        if (!$past(reset) && past_counter > 2) begin
            assert(dv_de == (h_count > H_OFFSCREEN_WIDTH && v_count < (V_SIZE - V_OFFSCREEN_HEIGHT)));

            assert(!hsync == ((h_count >= H_FRONTPORCH) && (h_count < H_FRONTPORCH + H_SYNC)));
            assert(!vsync == ((v_count >= V_ACTIVE_HEIGHT + V_FRONTPORCH) && (v_count < V_ACTIVE_HEIGHT + V_FRONTPORCH + V_SYNC)));

            assert(end_of_line_nx == (h_count == (H_SIZE - 1)));
            assert(end_of_line == (h_count == 0));

            assert(end_of_frame_nx == (v_count == (V_SIZE - 1)));
            assert(end_of_visible_nx == (v_count == V_ACTIVE_HEIGHT - 1 && end_of_line_nx));
        end
    end

`endif

endmodule

`endif
