// draw_line.sv
//
// vim: set et ts=4 sw=4
//
// Copyright (c) 2021 Xark & Contributors - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)
//


`default_nettype none               // mandatory for Verilog sanity
`timescale 1ns/1ps                  // mandatory to shut up Icarus Verilog

`include "xosera_pkg.sv"

module draw_line #(parameter CORDW=10) (          // framebuffer coord width in bits
    input  wire logic clk,                        // clock
    input  wire logic ena_draw_i,                 // enable draw
    input  wire reset_i,                          // reset
    input  wire logic start_i,                    // start line rendering
    input  wire logic signed [CORDW-1:0] x0_i,    // point 0 - horizontal position
    input  wire logic signed [CORDW-1:0] y0_i,    // point 0 - vertical position
    input  wire logic signed [CORDW-1:0] x1_i,    // point 1 - horizontal position
    input  wire logic signed [CORDW-1:0] y1_i,    // point 1 - vertical position
    output      logic signed [CORDW-1:0] x_o,     // horizontal drawing position
    output      logic signed [CORDW-1:0] y_o,     // vertical drawing position
    output      logic drawing_o,                  // line is drawing
    output      logic done_o                      // line complete (high for one tick)
    );

    // line properties
    logic signed [CORDW:0] dx, dy;      // a bit wider as signed
    logic right, down;                  // drawing direction
    always_comb begin
        right = (x0_i < x1_i);
        down  = (y0_i < y1_i);
    end

    always_ff @(posedge clk) begin
        if (ena_draw_i) begin
            dx <= right ? x1_i - x0_i : x0_i - x1_i;     // dx = abs(x1 - x0)
            dy <= down  ? y0_i - y1_i : y1_i - y0_i;     // dy = -abs(y1 - y0)
        end
    end

    // error values
    logic signed [CORDW:0] err, derr;
    logic movx, movy;   // move in x and/or y required
    always_comb begin
        movx = (2*err >= dy);
        movy = (2*err <= dx);
        derr = movx ? dy : 0;
        if (movy) derr = derr + dx;
    end

    // drawing high when in_progress
    logic in_progress;  // drawing in progress
    always_comb drawing_o = in_progress;

    enum {IDLE, DRAW} state;    // we're either idle or drawing
    always_ff @(posedge clk) begin
        case (state)
            DRAW: begin
                if (ena_draw_i) begin
                    if (x_o == x1_i && y_o == y1_i) begin
                        in_progress <= 0;
                        done_o <= 1;
                        state <= IDLE;
                    end else begin
                        if (movx) x_o <= right ? x_o + 1 : x_o - 1;
                        if (movy) y_o <= down  ? y_o + 1 : y_o - 1;
                        err <= err + derr;
                    end
                end
            end
            default: begin  // IDLE
                done_o <= 0;
                if (start_i) begin
                    err <= dx + dy;
                    x_o <= x0_i;
                    y_o <= y0_i;
                    in_progress <= 1;
                    state <= DRAW;
                end
            end
        endcase

        if (reset_i) begin
            in_progress <= 0;
            done_o <= 0;
            state <= IDLE;
        end
    end
endmodule
