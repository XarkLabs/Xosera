// prim_renderer.sv
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

module prim_renderer(
    input       logic            oe_i,                  // output enable

    input  wire logic [15:0]     cmd_i,                 // command
    input  wire logic            cmd_valid_i,           // is command valid?

    output      logic            prim_rndr_vram_sel_o,  // primitive renderer VRAM select
    output      logic            prim_rndr_wr_o,        // primitive renderer VRAM write
    output      logic  [3:0]     prim_rndr_mask_o,      // primitive renderer VRAM nibble write masks
    output      logic [15:0]     prim_rndr_addr_o,      // primitive renderer VRAM addr
    output      logic [15:0]     prim_rndr_data_out_o,  // primitive renderer bus VRAM data write

    output      logic            busy_o,                // is busy?
    
    input  wire logic            reset_i,
    input  wire logic            clk
    );

logic start_line;
logic start_filled_rectangle;
logic signed [11:0] x0, y0, x1, y1, x2, y2;
logic signed [11:0] x, y;
logic signed [11:0] x_line, y_line;
logic signed [11:0] x_filled_rectangle, y_filled_rectangle;
logic         [7:0] color;
logic drawing;
logic drawing_line;
logic drawing_filled_rectangle;
logic busy_line;
logic busy_filled_rectangle;
logic done;
logic done_line;
logic done_filled_rectangle;

always_comb busy_o = busy_line || busy_filled_rectangle;

draw_line #(.CORDW(12)) draw_line (    // framebuffer coord width in bits
    .clk(clk),                         // clock
    .reset_i(reset_i),                 // reset
    .start_i(start_line),              // start line rendering
    .oe_i(oe_i),                       // output enable
    .x0_i(x0),                         // point 0 - horizontal position
    .y0_i(y0),                         // point 0 - vertical position
    .x1_i(x1),                         // point 1 - horizontal position
    .y1_i(y1),                         // point 1 - vertical position
    .x_o(x_line),                      // horizontal drawing position
    .y_o(y_line),                      // vertical drawing position
    .drawing_o(drawing_line),          // line is drawing
    .busy_o(busy_line),                // line drawing request in progress
    .done_o(done_line)                 // line complete (high for one tick)
    );

draw_rectangle_fill #(.CORDW(12)) draw_rectangle_fill (     // framebuffer coord width in bits
    .clk(clk),                              // clock
    .reset_i(reset_i),                      // reset
    .start_i(start_filled_rectangle),       // start rectangle rendering
    .oe_i(oe_i),                            // output enable
    .x0_i(x0),                              // point 0 - horizontal position
    .y0_i(y0),                              // point 0 - vertical position
    .x1_i(x1),                              // point 1 - horizontal position
    .y1_i(y1),                              // point 1 - vertical position
    .x_o(x_filled_rectangle),               // horizontal drawing position
    .y_o(y_filled_rectangle),               // vertical drawing position
    .drawing_o(drawing_filled_rectangle),   // rectangle is drawing
    .busy_o(busy_filled_rectangle),         // rectangle drawing request in progress
    .done_o(done_filled_rectangle)          // rectangle complete (high for one tick)
    );

assign drawing = drawing_line | drawing_filled_rectangle;
assign done = done_line | done_filled_rectangle;
assign x = drawing_line ? x_line : x_filled_rectangle;
assign y = drawing_line ? y_line : y_filled_rectangle;

always_ff @(posedge clk) begin

    if (reset_i) begin
        start_line <= 0;
        start_filled_rectangle <= 0;
        prim_rndr_vram_sel_o <= 0;
        prim_rndr_wr_o <= 0;
    end

    if (cmd_valid_i) begin
        case(cmd_i[15:12])
            xv::PR_COORDX0 : x0    <= cmd_i[11:0];
            xv::PR_COORDY0 : y0    <= cmd_i[11:0];
            xv::PR_COORDX1 : x1    <= cmd_i[11:0];
            xv::PR_COORDY1 : y1    <= cmd_i[11:0];
            xv::PR_COORDX2 : x2    <= cmd_i[11:0];
            xv::PR_COORDY2 : y2    <= cmd_i[11:0];
            xv::PR_COLOR   : color <= cmd_i[7:0];
            xv::PR_EXECUTE: begin
                if (cmd_i[3:0] == xv::PR_LINE) begin
                    start_line <= 1;
                end else if (cmd_i[3:0] == xv::PR_FILLED_RECTANGLE) begin
                    start_filled_rectangle <= 1;
                end
            end
            default: begin
                // Do nothing
            end
        endcase
    end

    if (drawing && oe_i) begin
        if (x >= 0 && y >= 0 && x < xv::VISIBLE_WIDTH / 2 && y < xv::VISIBLE_HEIGHT / 2) begin
            prim_rndr_vram_sel_o <= 1;
            prim_rndr_wr_o <= 1;
            prim_rndr_mask_o <= (x & 12'h001) != 12'h000 ? 4'b0011 : 4'b1100;
            prim_rndr_addr_o <= {4'b0, y} * (xv::VISIBLE_WIDTH / 4) + {4'b0, x} / 2;
            prim_rndr_data_out_o <= {color[7:0], color[7:0]};
        end else begin
            prim_rndr_vram_sel_o <= 0;
            prim_rndr_wr_o <= 0;
        end
    end

    if (done) begin
        prim_rndr_vram_sel_o <= 0;
        prim_rndr_wr_o <= 0;
    end

    if (start_line) start_line <= 0;
    if (start_filled_rectangle) start_filled_rectangle <= 0;
end

endmodule
