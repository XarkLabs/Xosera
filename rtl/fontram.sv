// fontram.sv
//
// vim: set et ts=4 sw=4
//
// Copyright (c) 2020 Xark - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)
//

`default_nettype none             // mandatory for Verilog sanity
`timescale 1ns/1ps

// "hack" to allow quoted filename from define
`define STRINGIFY(x) `"x`"

module fontram(
           input logic clk,
           input logic rd_en_i,
           input logic [12: 0] rd_address_i,
           output logic [7: 0] rd_data_o,
           input logic wr_clk,
           input logic wr_en_i,
           input logic [12: 0] wr_address_i,
           input logic [7: 0] wr_data_i
       );
// infer 8x8KB font BRAM
logic [7: 0] bram[8191: 0];
`ifndef SHOW        // yosys show command doesn't like "too long" init string
initial
`ifndef FONT_MEM
    $readmemb("../fonts/font_8x16.mem", bram, 0, 4095);
`else
    $readmemb(`STRINGIFY(`FONT_MEM), bram, 0, 4095);
`endif
`endif

always_ff @(posedge clk) begin
    if (rd_en_i) begin
        rd_data_o <= bram[rd_address_i];
    end
end

always_ff @(posedge wr_clk) begin
    if (wr_en_i) begin
        bram[rd_address_i] <= wr_data_i;
    end
end
endmodule
