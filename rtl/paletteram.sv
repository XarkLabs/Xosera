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

module paletteram(
           input  wire logic        clk,
           input  wire logic        rd_en_i,
           input  wire logic  [7:0] rd_address_i,
           output      logic [15:0] rd_data_o,
           input  wire logic        wr_clk,
           input  wire logic        wr_en_i,
           input  wire logic  [7:0] wr_address_i,
           input  wire logic [15:0] wr_data_i
       );
// infer 8x8KB font BRAM
integer i;
logic [15: 0] bram[0 : 255];

initial begin

`ifdef USE_BPP4TEST   // king tut pal
        bram[0]    = 16'h0000;
        bram[1]    = 16'h0002;
        bram[2]    = 16'h0023;
        bram[3]    = 16'h0200;
        bram[4]    = 16'h0004;
        bram[5]    = 16'h0026;
        bram[6]    = 16'h0420;
        bram[7]    = 16'h0640;
        bram[8]    = 16'h0642;
        bram[9]    = 16'h0248;
        bram[10]   = 16'h0850;
        bram[11]   = 16'h0a71;
        bram[12]   = 16'h0a72;
        bram[13]   = 16'h0c82;
        bram[14]   = 16'h0ec4;
        bram[15]   = 16'h0ec6;
`else
        bram[0]    = 16'h0000;                      // black
        bram[1]    = 16'h000A;                      // blue
        bram[2]    = 16'h00A0;                      // green
        bram[3]    = 16'h00AA;                      // cyan
        bram[4]    = 16'h0A00;                      // red
        bram[5]    = 16'h0A0A;                      // magenta
        bram[6]    = 16'h0AA0;                      // brown
        bram[7]    = 16'h0AAA;                      // light gray
        bram[8]    = 16'h0555;                      // dark gray
        bram[9]    = 16'h055F;                      // light blue
        bram[10]   = 16'h05F5;                      // light green
        bram[11]   = 16'h05FF;                      // light cyan
        bram[12]   = 16'h0F55;                      // light red
        bram[13]   = 16'h0F5F;                      // light magenta
        bram[14]   = 16'h0FF5;                      // yellow
        bram[15]   = 16'h0FFF;                      // white
`endif


        for (i = 16; i < 256; i = i + 1) begin      // TODO full default palette
            bram[i] = 16'h0555;
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
