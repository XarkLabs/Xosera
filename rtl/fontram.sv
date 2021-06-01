// fontram.sv
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

// Default "mem" files and bank address for font data (1 2KB banks per 8x8 font, 2 banks for 8x16 font)
`ifdef TESTPATTERN
`define FONT_FILE_0 "fonts/hexfont_8x16w.mem"
`else
`define FONT_FILE_0 "fonts/font_ST_8x16w.mem"
`endif
`define FONT_ADDR_0 0*1024
`define FONT_FILE_1 "fonts/font_ST_8x8w.mem"
`define FONT_ADDR_1 2*1024
`define FONT_FILE_2 "fonts/hexfont_8x8w.mem"
`define FONT_ADDR_2 3*1024
// `define FONT_FILE_3
// `define FONT_ADDR_3

module fontram(
    input  logic         clk,
    input  logic         rd_en_i,
    input  logic [11:0]  rd_address_i,
    output logic [15:0]  rd_data_o,
    input  logic         wr_clk,
    input  logic         wr_en_i,
    input  logic [11:0]  wr_address_i,
    input  logic [15:0]  wr_data_i
);
// infer 8x8KB font BRAM
logic [15: 0] bram[0 : 4095];
`ifndef SHOW        // yosys show command doesn't like "too long" init string
initial begin

`ifdef FONT_FILE_0
    $readmemb(`FONT_FILE_0, bram, `FONT_ADDR_0);
`else
    $readmemb("fonts/font_ST_8x16w.mem", bram, 0);
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

// Infer BRAM block
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
