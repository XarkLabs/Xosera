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

// Default "mem" files and size in banks for font data (up to 4 2KB banks, 8x16 fonts uses 2 banks)
`define FONT_FILE_0 "../fonts/font_ST_8x16.mem"
`define FONT_ADDR_0 0*2048
`define FONT_FILE_1 "../fonts/font_ST_8x8.mem"
`define FONT_ADDR_1 2*2048
`define FONT_FILE_2 "../fonts/font_v9958_8x8.mem"
`define FONT_ADDR_2 3*2048
// `define FONT_FILE_3
// `define FONT_ADDR_3

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
initial begin

`ifdef FONT_FILE_0
    $readmemb(`FONT_FILE_0, bram, `FONT_ADDR_0);
`else
    $readmemb("../fonts/font_ST_8x16.mem", bram, 0);
`endif
`ifdef FONT_FILE_1
    $readmemb(`FONT_FILE_1, bram, `FONT_ADDR_1);
`endif
`ifdef FONT_FILE_2
    $readmemb(`FONT_FILE_2, bram, `FONT_ADDR_2);
`endif
`ifdef FONT_FILE_3
    $readmemb(`FONT_FILE_3, bram, `FONT_ADDR_3);
`endif
end
`endif

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
