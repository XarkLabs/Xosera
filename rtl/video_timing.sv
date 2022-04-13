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

module video_timing
(
    // video registers and control
    output      hres_t          h_count_o,              // horizontal video counter
    output      vres_t          v_count_o,              // vertical video counter
    output      logic           v_visible_o,            // vertical line visible (e.g., 0-479)
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
assign hsync_o          = hsync;
assign vsync_o          = vsync;
assign dv_de_o          = dv_de;

// video sync generation via state machine (Thanks tnt & drr - a much more efficient method!)
always_comb    end_of_line_next        = (h_state == H_STATE_VISIBLE) && (h_state_next == H_STATE_PRE_SYNC);
always_comb    end_of_frame_next       = (v_state == V_STATE_POST_SYNC) && (v_state_next == V_STATE_VISIBLE);
always_comb    end_of_visible_next     = (v_state == V_STATE_VISIBLE) && (v_state_next == V_STATE_PRE_SYNC);
always_comb    hsync_next              = (h_state == H_STATE_SYNC) ? xv::H_SYNC_POLARITY : ~xv::H_SYNC_POLARITY;
always_comb    vsync_next              = (v_state == V_STATE_SYNC) ? xv::V_SYNC_POLARITY : ~xv::V_SYNC_POLARITY;
always_comb    dv_de_next              = (v_state == V_STATE_VISIBLE) && (h_state_next == H_STATE_VISIBLE);

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
