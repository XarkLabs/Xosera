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

module draw_rectangle_fill #(parameter CORDW=16) (      // signed coordinate width
    input  wire logic clk,                              // clock
    input  wire logic reset_i,                          // reset
    input  wire logic start_i,                          // start rectangle drawing
    input  wire logic oe_i,                             // output enable
    input  wire logic signed [CORDW-1:0] x0_i, y0_i,    // vertex 0
    input  wire logic signed [CORDW-1:0] x1_i, y1_i,    // vertex 1
    output      logic signed [CORDW-1:0] x_o, y_o,      // drawing position
    output      logic drawing_o,                        // actively drawing
    output      logic busy_o,                           // drawing request in progress
    output      logic done_o                            // drawing is complete (high for one tick)
    );

    // filled rectangle has as many lines as it is tall abs(y1-y0)
    logic signed [CORDW-1:0] line_id;       // current line
    logic line_start;                       // start drawing line
    logic line_done;                        // finished drawing current line?

    // sort input Y coordinates so we always draw top-to-bottom
    logic signed [CORDW-1:0] y0s, y1s;      // vertex 0 - ordered
    always_comb begin
        y0s = (y0_i > y1_i) ? y1_i : y0_i;
        y1s = (y0_i > y1_i) ? y0_i : y1_i;          // last line
    end 

    // horizontal line coordinates
    logic signed [CORDW-1:0] lx0, lx1;

    // draw state machine
    enum {IDLE, INIT, DRAW} state;
    always_ff @(posedge clk) begin
        case (state)
            INIT: begin     // register coordinates
                state <= DRAW;
                line_start <= 1;
                // x-coordinates don't change for a given filled rectangle
                lx0 <= (x0_i > x1_i) ? x1_i : x0_i;     // draw left-to-right
                lx1 <= (x0_i > x1_i) ? x0_i : x1_i;
                y_o <= y0s + line_id;                     // vertical position
            end
            DRAW: begin
                line_start <= 0;
                if (line_done) begin
                    if (y_o == y1s) begin
                        state <= IDLE;
                        busy_o <= 0;
                        done_o <= 1;
                    end else begin
                        state <= INIT;
                        line_id <= line_id + 1;                        
                    end
                end
            end
            default: begin  // IDLE
                done_o <= 0;
                if (start_i) begin
                    state <= INIT;
                    line_id <= 0;
                    busy_o <= 1;
                end
            end
        endcase

        if (reset_i) begin
            state <= IDLE;
            line_id <= 0;
            line_start <= 0;
            busy_o <= 0;
            done_o <= 0;
        end
    end

    draw_line_1d #(.CORDW(CORDW)) draw_line_1d_inst (
        .clk(clk),
        .reset_i(reset_i),
        .start_i(line_start),
        .oe_i(oe_i),
        .x0_i(lx0),
        .x1_i(lx1),
        .x_o(x_o),
        .drawing_o(drawing_o),
        // verilator lint_save
        // verilator lint_off PINCONNECTEMPTY
        .busy_o(),
        // verilator lint_restore
        .done_o(line_done)
    );

endmodule
