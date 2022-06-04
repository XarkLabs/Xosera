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
        parameter   AWIDTH   = 10,
        parameter   EVEN     = 1
    )
    (
           input  wire logic                clk,
//           input  wire logic                rd_en_i,
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
word_t bram[0:2**AWIDTH-1] /* verilator public*/;

localparam [31:0] githash = 32'H`GITHASH;
localparam [11:0] version = 12'H`VERSION;

initial begin
    // Fill with numbers
    for (integer i = 0; i < (2**AWIDTH) - 8; i = i + 1) begin
        bram[i] = EVEN ? 16'h0000 : 16'h0003;   // COP_END
    end

    bram[(2**AWIDTH) - 8] = EVEN ? "Xo" : "se";
    bram[(2**AWIDTH) - 7] = EVEN ? "ra" : " v";
    bram[(2**AWIDTH) - 6] = EVEN ? {"0" + 8'(version[11:8]),  "."} : {"0" + 8'(version[7:4]), "0" + 8'(version[3:0])};
    bram[(2**AWIDTH) - 5] = EVEN ? '0 : '0;
    bram[(2**AWIDTH) - 4] = EVEN ? githash[31:16] : githash[15:0];
    bram[(2**AWIDTH) - 3] = EVEN ? githash[31:16] : githash[15:0];
    bram[(2**AWIDTH) - 2] = EVEN ? githash[31:16] : githash[15:0];
    bram[(2**AWIDTH) - 1] = EVEN ? githash[31:16] : githash[15:0];
end

// infer BRAM block
always_ff @(posedge wr_clk) begin
    if (wr_en_i) begin
        bram[wr_address_i] <= wr_data_i;
    end
end

always_ff @(posedge clk) begin
// TODO: Add read write conflict "don't care" logic (when Yosys is ready)
//    if (rd_en_i) begin
        rd_data_o <= bram[rd_address_i];
//    end
end

endmodule
`default_nettype wire               // restore default
