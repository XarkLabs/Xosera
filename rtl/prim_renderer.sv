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
    input       logic            ena_draw_i,         // enable draw

    input  wire logic [15:0]     cmd_i,              // command
    input  wire logic            cmd_valid_i,        // is command valid?

    output      logic            prim_rndr_vram_sel_o, // primitive renderer VRAM select
    output      logic            prim_rndr_wr_o,       // primitive renderer VRAM write
    output      logic [15:0]     prim_rndr_addr_o,     // primitive renderer VRAM addr
    output      logic [15:0]     prim_rndr_data_out_o, // primitive renderer bus VRAM data write
    
    input  wire logic            reset_i,
    input  wire logic            clk
    );

logic start;
logic signed [11:0] x0, y0, x1, y1;
logic signed [11:0] x, y;
logic         [7:0] color;
logic drawing;
logic done;

draw_line #(.CORDW(12)) draw_line (    // framebuffer coord width in bits
    .clk(clk),                         // clock
    .ena_draw_i(ena_draw_i),           // enable draw
    .reset_i(reset_i),                 // reset
    .start_i(start),           // start line rendering
    .x0_i(x0),                 // point 0 - horizontal position
    .y0_i(y0),                 // point 0 - vertical position
    .x1_i(x1),                 // point 1 - horizontal position
    .y1_i(y1),                 // point 1 - vertical position
    .x_o(x),                   // horizontal drawing position
    .y_o(y),                   // vertical drawing position
    .drawing_o(drawing),       // line is drawing
    .done_o(done)              // line complete (high for one tick)
    );

always_ff @(posedge clk) begin

    if (reset_i) begin
        start <= 0;
        prim_rndr_vram_sel_o <= 0;
        prim_rndr_wr_o <= 0;
    end

    if (cmd_valid_i) begin
        case(cmd_i[15:12])
            4'h0: x0    <= cmd_i[11:0];
            4'h1: y0    <= cmd_i[11:0];
            4'h2: x1    <= cmd_i[11:0];
            4'h3: y1    <= cmd_i[11:0];
            4'h4: color <= cmd_i[7:0];
            4'hF: begin
                start <= 1;
            end
            default: begin
                // Do nothing
            end
        endcase
    end

    if (drawing && ena_draw_i) begin
        if (/*x >= 0 && y >= 0 &&*/ x < xv::VISIBLE_WIDTH / 8 && y < xv::VISIBLE_HEIGHT) begin
            prim_rndr_vram_sel_o <= 1;
            prim_rndr_wr_o <= 1;
            prim_rndr_addr_o <= {4'b0, y} * (xv::VISIBLE_WIDTH / 8) + ({4'b0, x});
            prim_rndr_data_out_o <= {color[3:0], color[3:0], color[3:0], color[3:0]};
        end else begin
            prim_rndr_vram_sel_o <= 0;
            prim_rndr_wr_o <= 0;
        end
    end

    if (done) begin
        prim_rndr_vram_sel_o <= 0;
        prim_rndr_wr_o <= 0;
    end

    if (start) start <= 0;
end

endmodule
