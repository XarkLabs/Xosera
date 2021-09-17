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

logic start;
logic signed [11:0] x0, y0, x1, y1, x2, y2;
logic        [15:0] dest_addr;
logic        [11:0] dest_height;
logic signed [11:0] x, y;
logic         [7:0] color;
logic               drawing;
logic               done;

draw_triangle_fill #(.CORDW(12)) draw_triangle_fill (     // framebuffer coord width in bits
    .clk(clk),                              // clock
    .reset_i(reset_i),                      // reset
    .start_i(start),                        // start triangle rendering
    .oe_i(oe_i),                            // output enable
    .x0_i(x0),                              // point 0 - horizontal position
    .y0_i(y0),                              // point 0 - vertical position
    .x1_i(x1),                              // point 1 - horizontal position
    .y1_i(y1),                              // point 1 - vertical position
    .x2_i(x2),                              // point 2 - horizontal position
    .y2_i(y2),                              // point 2 - vertical position
    .x_o(x),                                // horizontal drawing position
    .y_o(y),                                // vertical drawing position
    .drawing_o(drawing),                    // triangle is drawing
    .busy_o(busy_o),                        // triangle drawing request in progress
    .done_o(done)                           // triangle complete (high for one tick)
    );

always_ff @(posedge clk) begin

    if (reset_i) begin
        start <= 0;
        prim_rndr_vram_sel_o <= 0;
        prim_rndr_wr_o <= 0;
        dest_addr <= 16'h0000;
        dest_height <= xv::VISIBLE_HEIGHT / 2;
    end

    if (cmd_valid_i) begin
        case(cmd_i[15:12])
            xv::PR_COORDX0      : x0    <= cmd_i[11:0];
            xv::PR_COORDY0      : y0    <= cmd_i[11:0];
            xv::PR_COORDX1      : x1    <= cmd_i[11:0];
            xv::PR_COORDY1      : y1    <= cmd_i[11:0];
            xv::PR_COORDX2      : x2    <= cmd_i[11:0];
            xv::PR_COORDY2      : y2    <= cmd_i[11:0];
            xv::PR_COLOR        : color <= cmd_i[7:0];
            xv::PR_DEST_ADDR    : dest_addr <= {cmd_i[11:0], 4'b0000};
            xv::PR_DEST_HEIGHT  : dest_height <= cmd_i[11:0];
            xv::PR_EXECUTE      : start <= 1;
            default: begin
                // Do nothing
            end
        endcase
    end

    if (drawing) begin
        if (x >= 0 && y >= 0 && x < xv::VISIBLE_WIDTH / 2 && y < dest_height) begin
            prim_rndr_vram_sel_o <= 1;
            prim_rndr_wr_o <= 1;
            prim_rndr_mask_o <= (x & 12'h001) != 12'h000 ? 4'b0011 : 4'b1100;
            prim_rndr_addr_o <= dest_addr + {4'b0, y} * (xv::VISIBLE_WIDTH / 4) + {4'b0, x} / 2;
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

    if (start) start <= 0;
end

endmodule
