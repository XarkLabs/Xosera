// draw_line.sv
//
// vim: set et ts=4 sw=4
//
// Copyright (c) 2021 Xark & Contributors - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)
//

// Based on Project F - Lines and Triangles (https://projectf.io/posts/lines-and-triangles/)

`default_nettype none               // mandatory for Verilog sanity
`timescale 1ns/1ps                  // mandatory to shut up Icarus Verilog

module draw_line #(parameter CORDW=10) (                // framebuffer coord width in bits
    input  wire logic clk,                              // clock
    input  wire logic reset_i,                          // reset
    input  wire logic start_i,                          // start line rendering
    input  wire logic oe_i,                             // output enable
    input  wire logic signed [CORDW-1:0] x0_i, y0_i,    // point 0
    input  wire logic signed [CORDW-1:0] x1_i, y1_i,    // point 1
    output      logic signed [CORDW-1:0] x_o, y_o,      // drawing position
    output      logic drawing_o,                        // actively drawing
    output      logic busy_o,                           // drawing request in progress
    output      logic done_o                            // line complete (high for one tick)
    );

    // line properties
    logic swap;     // swap points to ensure y1 >= y0
    logic right;    // drawing direction;
    logic signed [CORDW-1:0] xa, ya;        // start point
    logic signed [CORDW-1:0] xb, yb;        // end point
    logic signed [CORDW-1:0] x_end, y_end;  // register end point
    always_comb begin
        swap = (y0_i > y1_i);   // swap points if y0 is below y1
        xa = swap ? x1_i : x0_i;
        xb = swap ? x0_i : x1_i;
        ya = swap ? y1_i : y0_i;
        yb = swap ? y0_i : y1_i;
    end

    // error values
    logic signed [CORDW:0] err;         // a bit wider as signed
    logic signed [CORDW:0] dx, dy;
    logic movx, movy;   // horizontal/vertical move required
    always_comb begin
        movx = (2*err >= dy);
        movy = (2*err <= dx);
    end
    
    // draw state machine
    enum {IDLE, INIT_0, INIT_1, DRAW} state;
    always_comb drawing_o = (state == DRAW && oe_i);

    always_ff @(posedge clk) begin
        case (state)
            DRAW: begin
                if (oe_i) begin
                    if (x_o == x_end && y_o == y_end) begin
                        state <= IDLE;
                        busy_o <= 0;
                        done_o <= 1;
                    end else begin
                        if (movx) begin
                            x_o <= right ? x_o + 1 : x_o - 1;
                            err <= err + dy;
                        end
                        if (movy) begin
                            y_o <= y_o + 1; // always down
                            err <= err + dx;                            
                        end
                        if (movx && movy) begin
                            x_o <= right ? x_o + 1 : x_o - 1;
                            y_o <= y_o + 1;
                            err <= err + dy + dx;                            
                        end
                    end
                end
            end
            INIT_0: begin
                state <= INIT_1;
                dx <= right ? xb - xa : xa - xb;    // dx = abs(xb - xa)
                dy <= ya - yb;                      // dy = -abs(yb - ya)
            end
            INIT_1: begin
                state <= DRAW;
                err <= dx + dy;
                x_o <= xa;
                y_o <= ya;
                x_end <= xb;
                y_end <= yb;
            end
            default: begin  // IDLE
                done_o <= 0;
                if (start_i) begin
                    state <= INIT_0;
                    right <= (xa < xb);     // draw right to left?
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
