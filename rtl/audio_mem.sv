// audio_mem.sv
//
// vim: set et ts=4 sw=4
//
// Copyright (c) 2022 Xark - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)
//

`default_nettype none               // mandatory for Verilog sanity
`timescale 1ns/1ps                  // mandatory to shut up Icarus Verilog

`include "xosera_pkg.sv"

module audio_mem#(
    parameter   AWIDTH      = 8
)(
    input  wire logic  [AWIDTH-1:0]  rd_address_i,
    output      word_t               rd_data_o,
    input  wire logic                wr_clk,
    input  wire logic                wr_en_i,
    input  wire logic  [AWIDTH-1:0]  wr_address_i,
    input  wire word_t               wr_data_i,
    input  wire logic                clk
);

// infer 16x256 audio BRAM
word_t bram[0:2**AWIDTH-1] /* verilator public*/;

initial begin
    for (integer i = 0; i < 2**AWIDTH; i = i + 1) begin
        bram[i] = 16'h0000;
    end
end

// infer BRAM block
always_ff @(posedge wr_clk) begin
    if (wr_en_i) begin
        bram[wr_address_i] <= wr_data_i;
    end
end

always_ff @(posedge clk) begin
// NOTE: On iCE40UP5K hardware, a write and a read to the same location seems
// to honor new write, whereas simulation honors previous value for read.
// Add a "hack" to make simulation match observed behavior.
`ifndef SYNTHESIS
    if (wr_en_i && rd_address_i == wr_address_i)
        rd_data_o <= wr_data_i;
    else
`endif
    rd_data_o <= bram[rd_address_i];
end

endmodule
`default_nettype wire               // restore default
