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
        parameter   ADDINFO = "N"
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

// Xosera init info stored in last 256 bytes of default copper memory
//
// typedef struct _xosera_info
// {
//     char          description_str[240];        // ASCII description
//     uint16_t      reserved_48[4];             // 8 reserved bytes (and force alignment)
//     unsigned char ver_bcd_major;              // major BCD version
//     unsigned char ver_bcd_minor;              // minor BCD version
//     unsigned char git_modified;               // non-zero if modified from git
//     unsigned char reserved_59;                // reserved byte
//     unsigned char githash[4];
// } xosera_info_t;

word_t bram[0:2**AWIDTH-1] /* verilator public*/;

integer x;

initial begin
    // is this the 2nd memory block (with xosera_info)?
    if (ADDINFO == "Y") begin
        for (integer i = 0; i < (2**AWIDTH)-128; i = i + 1) begin
            bram[i]    = 16'h2BFF;          // wait EOF
        end
        // Xosera init info stored in last 256 bytes of default copper memory (see xosera_pkg.sv)
        x = 0;
        for (integer i = (2**AWIDTH)-128; i < (2**AWIDTH)-4; i = i + 1) begin
            if ((x*8)+8 < $bits(xv::info_str)) begin
                bram[i]    = { xv::info_str[(x*8)+:8], xv::info_str[(x*8)+8+:8] };
            end else begin
                bram[i]    = 16'h0000;
            end
            x = x + 2;
        end

        bram[(2**AWIDTH)-4]  = 16'(xv::version);
        bram[(2**AWIDTH)-3]  = { `GITCLEAN ? 8'b0 : 8'b1, 8'b0 };
        bram[(2**AWIDTH)-2]  = xv::githash[31:16];
        bram[(2**AWIDTH)-1]  = xv::githash[15:0];
    end else begin
`ifdef EN_COPPER_INIT
        // use default copper program to init Xosera
`ifdef MODE_640x480
        $readmemh("default_copper_640.mem", bram, 0);
`else
        $readmemh("default_copper_848.mem", bram, 0);
`endif
`else
        bram[0] = 16'(xv::XR_COPP_CTRL);        // turn off copper
        bram[1] = 16'h0000;
        for (integer i = 2; i < (2**AWIDTH); i = i + 1) begin
            bram[i]    = 16'h2BFF;              // wait EOF
        end
`endif
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

`endif
`default_nettype wire               // restore default
