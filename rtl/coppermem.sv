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

`ifdef EN_COPP

`include "xosera_pkg.sv"
`define STRINGIFY(x) `"x`"

module coppermem
    #(
        parameter   AWIDTH  = 10,
        parameter   ODDWORD = 0
    )
    (
           input  wire logic [AWIDTH-1:0]   rd_address_i,
           output      word_t               rd_data_o,
           input  wire logic                wr_clk,
           input  wire logic                wr_en_i,
           input  wire logic [AWIDTH-1:0]   wr_address_i,
           input  wire word_t               wr_data_i,
           input  wire logic                clk
    );

// Note this is only half of copper mem - there are two
// of these memories (odd and even) to give 32-bit
// interface internally to the copper.
word_t bram[0:2**AWIDTH-1] /* verilator public*/;

initial begin
    // Fill with numbers
    bram[0] = 16'h0800;     // RA -= RA
    bram[1] = -16'h007F;
    bram[2] = 16'h1800;     // COP_RA -= 7
    bram[3] = 16'h0800;     //
    bram[4] = 16'h0800;     // COP_RA -= 1
    bram[5] = 16'h0001;     //
    bram[6] = 16'h3002;     // BGE 2
    bram[7] = 16'h2040;     // store reg 2
    bram[8] = 16'h2FFF;
    bram[9] = 16'h2FFF;
    for (integer i = 10; i < (2**AWIDTH); i = i + 1) begin
        bram[i] = 16'h2FFF; // VPOS #1023
    end
// Xosera init info stored in last 64 bytes of default copper memory (see xosera_pkg.sv)

    bram[xv::offset +  0]  = !ODDWORD ? { xv::info_str[(47*8)+1+:8], xv::info_str[(46*8)+1+:8] } : { xv::info_str[(45*8)+1+:8], xv::info_str[(44*8)+1+:8] };
    bram[xv::offset +  1]  = !ODDWORD ? { xv::info_str[(43*8)+1+:8], xv::info_str[(42*8)+1+:8] } : { xv::info_str[(41*8)+1+:8], xv::info_str[(40*8)+1+:8] };
    bram[xv::offset +  2]  = !ODDWORD ? { xv::info_str[(39*8)+1+:8], xv::info_str[(38*8)+1+:8] } : { xv::info_str[(37*8)+1+:8], xv::info_str[(36*8)+1+:8] };
    bram[xv::offset +  3]  = !ODDWORD ? { xv::info_str[(35*8)+1+:8], xv::info_str[(34*8)+1+:8] } : { xv::info_str[(33*8)+1+:8], xv::info_str[(32*8)+1+:8] };
    bram[xv::offset +  4]  = !ODDWORD ? { xv::info_str[(31*8)+1+:8], xv::info_str[(30*8)+1+:8] } : { xv::info_str[(29*8)+1+:8], xv::info_str[(28*8)+1+:8] };
    bram[xv::offset +  5]  = !ODDWORD ? { xv::info_str[(27*8)+1+:8], xv::info_str[(26*8)+1+:8] } : { xv::info_str[(25*8)+1+:8], xv::info_str[(24*8)+1+:8] };
    bram[xv::offset +  6]  = !ODDWORD ? { xv::info_str[(23*8)+1+:8], xv::info_str[(22*8)+1+:8] } : { xv::info_str[(21*8)+1+:8], xv::info_str[(20*8)+1+:8] };
    bram[xv::offset +  7]  = !ODDWORD ? { xv::info_str[(19*8)+1+:8], xv::info_str[(18*8)+1+:8] } : { xv::info_str[(17*8)+1+:8], xv::info_str[(16*8)+1+:8] };
    bram[xv::offset +  8]  = !ODDWORD ? { xv::info_str[(15*8)+1+:8], xv::info_str[(14*8)+1+:8] } : { xv::info_str[(13*8)+1+:8], xv::info_str[(12*8)+1+:8] };
    bram[xv::offset +  9]  = !ODDWORD ? { xv::info_str[(11*8)+1+:8], xv::info_str[(10*8)+1+:8] } : { xv::info_str[( 9*8)+1+:8], xv::info_str[( 8*8)+1+:8] };
    bram[xv::offset + 10]  = !ODDWORD ? { xv::info_str[( 7*8)+1+:8], xv::info_str[( 6*8)+1+:8] } : { xv::info_str[( 5*8)+1+:8], xv::info_str[( 4*8)+1+:8] };
    bram[xv::offset + 11]  = !ODDWORD ? { xv::info_str[( 3*8)+1+:8], xv::info_str[( 2*8)+1+:8] } : { xv::info_str[( 1*8)+1+:8], xv::info_str[( 0*8)+1+:8] };

    bram[xv::offset + 12]  = !ODDWORD ? 16'h0000            : 16'h0000;
    bram[xv::offset + 13]  = !ODDWORD ? 16'h0000            : 16'h0000;
    bram[xv::offset + 14]  = !ODDWORD ? 16'(xv::version)    : { `GITCLEAN ? 8'b0 : 8'b1, 8'b0 };
    bram[xv::offset + 15]  = !ODDWORD ? xv::githash[31:16]  : xv::githash[15:0];
end

// infer BRAM block
always_ff @(posedge wr_clk) begin
    if (wr_en_i) begin
        bram[wr_address_i] <= wr_data_i;
    end
end

always_ff @(posedge clk) begin
    rd_data_o <= bram[rd_address_i];
    // TODO: add read vs write "don't care"
end

endmodule

`endif
`default_nettype wire               // restore default
