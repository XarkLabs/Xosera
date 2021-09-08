// draw_rectangle.sv
//
// vim: set et ts=4 sw=4
//
// Copyright (c) 2021 Xark & Contributors - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)
//

// Based on Project F - 2D Shapes (https://projectf.io/posts/fpga-shapes/)

`default_nettype none               // mandatory for Verilog sanity
`timescale 1ns/1ps                  // mandatory to shut up Icarus Verilog

module draw_triangle_fill #(parameter CORDW=16) (      // signed coordinate width
    input  wire logic clk,                              // clock
    input  wire logic reset_i,                          // reset
    input  wire logic start_i,                          // start triangle drawing
    input  wire logic oe_i,                             // output enable
    input  wire logic signed [CORDW-1:0] x0_i, y0_i,    // vertex 0
    input  wire logic signed [CORDW-1:0] x1_i, y1_i,    // vertex 1
    input  wire logic signed [CORDW-1:0] x2_i, y2_i,    // vertex 2
    output      logic signed [CORDW-1:0] x_o, y_o,      // drawing position
    output      logic drawing_o,                        // actively drawing
    output      logic busy_o,                           // drawing request in progress
    output      logic done_o                            // drawing is complete (high for one tick)
    );

    // sorted input vertices
    logic signed [CORDW-1:0] x0s, y0s, x1s, y1s, x2s, y2s;

    // line coordinates
    logic signed [CORDW-1:0] x0a, y0a, x1a, y1a, xa, ya;
    logic signed [CORDW-1:0] x0b, y0b, x1b, y1b, xb, yb;
    logic signed [CORDW-1:0] x0h, x1h, xh;

    // previous y-value for edges
    logic signed [CORDW-1:0] prev_y;

    // previous x-values for horizontal line
    logic signed [CORDW-1:0] prev_xa;
    logic signed [CORDW-1:0] prev_xb;

    // line control signals
    logic oe_a, oe_b, oe_h;
    logic drawing_h;
    logic busy_a, busy_b, busy_h;
    logic b_edge;   // which B edge are we drawing?

    // pipeline completion signals to match coordinates
    logic busy_p1, done_p1;

    // draw state machine
    enum {IDLE, SORT_0, SORT_1, SORT_2, INIT_A, INIT_B0, INIT_B1, INIT_H,
          START_A, START_B, START_H, EDGE, H_LINE, DONE} state;

    always_ff @(posedge clk) begin
        case (state)
            SORT_0: begin
                state <= SORT_1;
                if (y0_i > y2_i) begin
                    x0s <= x2_i;
                    y0s <= y2_i;
                    x2s <= x0_i;
                    y2s <= y0_i;
                end else begin
                    x0s <= x0_i;
                    y0s <= y0_i;
                    x2s <= x2_i;
                    y2s <= y2_i;
                end
            end
            SORT_1: begin
                state <= SORT_2;
                if (y0s > y1_i) begin
                    x0s <= x1_i;
                    y0s <= y1_i;
                    x1s <= x0s;
                    y1s <= y0s;
                end else begin
                    x1s <= x1_i;
                    y1s <= y1_i;
                end
            end
            SORT_2: begin
                state <= INIT_A;
                if (y1s > y2s) begin
                    x1s <= x2_i;
                    y1s <= y2_i;
                    x2s <= x1s;
                    y2s <= y1s;
                end
            end
            INIT_A: begin
                state <= INIT_B0;
                x0a <= x0s;
                y0a <= y0s;
                x1a <= x2s;
                y1a <= y2s;
                prev_xa <= x0s;
                prev_xb <= x0s;
            end
            INIT_B0: begin
                state <= START_A;
                b_edge <= 0;
                x0b <= x0s;
                y0b <= y0s;
                x1b <= x1s;
                y1b <= y1s;
                prev_y <= y0s;
            end
            INIT_B1: begin
                state <= START_B;   // we don't need to start A again
                b_edge <= 1;
                x0b <= x1s;
                y0b <= y1s;
                x1b <= x2s;
                y1b <= y2s;
                prev_y <= y1s;
            end
            START_A: state <= START_B;
            START_B: state <= EDGE;
            EDGE: begin
                if ((ya != prev_y || !busy_a) && (yb != prev_y || !busy_b)) begin
                    state <= START_H;
                    x0h <= (prev_xa > prev_xb) ? prev_xb : prev_xa; // always draw left to right
                    x1h <= (prev_xa > prev_xb) ? prev_xa : prev_xb;
                end
            end
            START_H: state <= H_LINE;
            H_LINE: begin
                if (!busy_h) begin
                    prev_y <= yb;   // safe to update previous values once h-line done
                    prev_xa <= xa;
                    prev_xb <= xb;
                    if (!busy_b) begin
                        state <= (busy_a && b_edge == 0) ? INIT_B1 : DONE;
                    end else state <= EDGE;
                end
            end
            DONE: begin
                state <= IDLE;
                done_p1 <= 1;
                busy_p1 <= 0;
            end
            default: begin  // IDLE
                if (start_i) begin
                    //$display("Draw triangle %0d, %1d, %2d, %3d, %4d, %4d", x0_i, y0_i, x1_i, y1_i, x2_i, y2_i);
                    state <= SORT_0;
                    busy_p1 <= 1;
                end
                done_p1 <= 0;
            end
        endcase

        if (reset_i) begin
            state <= IDLE;
            busy_p1 <= 0;
            done_p1 <= 0;
            b_edge <= 0;
        end
    end

    always_comb begin
        oe_a = (state == EDGE && ya == prev_y);
        oe_b = (state == EDGE && yb == prev_y);
        oe_h = oe_i;
    end

    always_comb begin
        x_o = xh;
        y_o = prev_y;
        drawing_o = drawing_h;
        busy_o = busy_p1;
        done_o = done_p1;
    end

    draw_line #(.CORDW(CORDW)) draw_edge_a (
        .clk(clk),
        .reset_i(reset_i),
        .start_i(state == START_A),
        .oe_i(oe_a),
        .x0_i(x0a),
        .y0_i(y0a),
        .x1_i(x1a),
        .y1_i(y1a),
        .x_o(xa),
        .y_o(ya),
        .busy_o(busy_a),
        // verilator lint_save
        // verilator lint_off PINCONNECTEMPTY
        .drawing_o(),
        .done_o()
        // verilator lint_restore
    );

    draw_line #(.CORDW(CORDW)) draw_edge_b (
        .clk(clk),
        .reset_i(reset_i),
        .start_i(state == START_B),
        .oe_i(oe_b),
        .x0_i(x0b),
        .y0_i(y0b),
        .x1_i(x1b),
        .y1_i(y1b),
        .x_o(xb),
        .y_o(yb),
        .busy_o(busy_b),
        // verilator lint_save
        // verilator lint_off PINCONNECTEMPTY
        .drawing_o(),
        .done_o()
        // verilator lint_restore
    );

    draw_line_1d #(.CORDW(CORDW)) draw_h_line (
        .clk(clk),
        .reset_i(reset_i),
        .start_i(state == START_H),
        .oe_i(oe_h),
        .x0_i(x0h),
        .x1_i(x1h),
        .x_o(xh),
        .drawing_o(drawing_h),
        .busy_o(busy_h),
        // verilator lint_save
        // verilator lint_off PINCONNECTEMPTY
        .done_o()
        // verilator lint_restore
    );

endmodule
