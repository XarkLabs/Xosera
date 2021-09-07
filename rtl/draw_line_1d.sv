// draw_line_1d.sv
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

module draw_line_1d #(parameter CORDW=16) (        // signed coordinate width
    input  wire logic clk,                              // clock
    input  wire logic ena_draw_i,                       // enable draw
    input  wire logic reset_i,                          // reset
    input  wire logic start_i,                          // start rectangle drawing
    input  wire logic signed [CORDW-1:0] x0_i,          // point 0
    input  wire logic signed [CORDW-1:0] x1_i,          // point 1
    output      logic signed [CORDW-1:0] x_o,           // drawing position
    output      logic drawing_o,                        // actively drawing
    output      logic busy_o,                           // drawing request in progress
    output      logic done_o                            // drawing is complete (high for one tick)
    );

    // draw state machine
    enum {IDLE, DRAW} state;
    always_comb drawing_o = (state == DRAW && ena_draw_i);

    always_ff @(posedge clk) begin
        case (state)
            DRAW: begin
                if (ena_draw_i) begin
                    if (x_o == x1_i) begin
                        state <= IDLE;
                        busy_o <= 0;
                        done_o <= 1;
                    end else begin
                        x_o <= x_o + 1;
                    end
                end
            end
            default: begin  // IDLE
                done_o <= 0;
                if (start_i) begin
                    state <= DRAW;
                    x_o <= x0_i;
                    busy_o <= 1;
                end
            end
        endcase

        if (reset_i) begin
            state <= IDLE;
            busy_o <= 0;
            done_o <= 0;
        end
    end
endmodule
