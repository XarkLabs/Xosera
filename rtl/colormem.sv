// colormem.sv
//
// vim: set et ts=4 sw=4
//
// Copyright (c) 2020 Xark - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)
//

`default_nettype none               // mandatory for Verilog sanity
`timescale 1ns/1ps                  // mandatory to shut up Icarus Verilog

`include "xosera_pkg.sv"

module colormem#(
    parameter   AWIDTH   = 8,
    parameter   DIM      = 0
)
(
           input  wire logic                clk,
           input  wire logic                rd_en_i,
           input  wire logic  [AWIDTH-1:0]  rd_address_i,
           output      logic       [15:0]   rd_data_o,
           input  wire logic                wr_clk,
           input  wire logic                wr_en_i,
           input  wire logic  [AWIDTH-1:0]  wr_address_i,
           input  wire logic       [15:0]   wr_data_i
       );
// infer 16x256 color BRAM
logic [15: 0] bram[0 : 2**AWIDTH-1] /* verilator public */;

initial begin
    if (DIM == 0) begin
        $readmemh("default_colors.mem", bram, 0);
    end else begin
        bram[ 0] = 16'h000;
        bram[ 1] = 16'h111;
        bram[ 2] = 16'h222;
        bram[ 3] = 16'h333;
        bram[ 4] = 16'h444;
        bram[ 5] = 16'h555;
        bram[ 6] = 16'h666;
        bram[ 7] = 16'h777;
        bram[ 8] = 16'h888;
        bram[ 9] = 16'h999;
        bram[10] = 16'hAAA;
        bram[11] = 16'hBBB;
        bram[12] = 16'hCCC;
        bram[13] = 16'hDDD;
        bram[14] = 16'hEEE;
        bram[15] = 16'hFFF;
    end
end

// infer BRAM block
always_ff @(posedge wr_clk) begin
    if (wr_en_i) begin
        bram[wr_address_i] <= wr_data_i;
    end
end

always_ff @(posedge clk) begin
    if (rd_en_i) begin
        rd_data_o <= bram[rd_address_i];
    end
end

endmodule
`default_nettype wire               // restore default
