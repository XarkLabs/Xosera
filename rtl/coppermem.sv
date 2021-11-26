// coppermem.sv
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

module coppermem
    #(
        parameter   AWIDTH   = 10
    )
    (
           input  wire logic                clk,
           input  wire logic                rd_en_i,
           input  wire logic [AWIDTH-1:0]   rd_address_i,
           output      word_t               rd_data_o,
           input  wire logic                wr_clk,
           input  wire logic                wr_en_i,
           input  wire logic [AWIDTH-1:0]   wr_address_i,
           input  wire word_t               wr_data_i
    );

// Note this is only half of copper mem - there are two
// of these memories (odd and even) to give 32-bit
// interface internally to the copper.
logic [15: 0] bram[0 : 2**AWIDTH-1] /* verilator public*/;

`ifndef SYNTHESIS
initial begin
    // Fill with numbers
    for (integer i = 0; i < AWIDTH; i = i + 1) begin
        bram[i] = 16'(i);
    end
end
`endif

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
