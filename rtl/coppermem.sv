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

// Xosera init info stored in last 64 bytes of default copper memory
//
// typedef struct _xosera_info
// {
//     char          description_str[48];        // ASCII description
//     uint16_t      reserved_48[4];             // 8 reserved bytes (and force alignment)
//     unsigned char ver_bcd_major;              // major BCD version
//     unsigned char ver_bcd_minor;              // minor BCD version
//     unsigned char git_modified;               // non-zero if modified from git
//     unsigned char reserved_59;                // reserved byte
//     unsigned char githash[4];
// } xosera_info_t;

localparam          offset  = (2**AWIDTH) - (64/4);
localparam [11:0]   version = 12'H`VERSION;
localparam [8:1]    clean   = `GITCLEAN ? " " : "+";    // '+' appended to version if non-clean
localparam [31:0]   githash = 32'H`GITHASH;             // git short hash

localparam [16*8:1] hex_str = "FEDCBA9876543210";
localparam [48*8:1] description_str = { "Xosera v", "0" + 8'(version[11:8]), ".", "0" + 8'(version[7:4]), "0" + 8'(version[3:0]),
                                        " [#",
                                        hex_str[((githash[31:28])*8)+1+:8], hex_str[((githash[27:24])*8)+1+:8],
                                        hex_str[((githash[23:20])*8)+1+:8], hex_str[((githash[19:16])*8)+1+:8],
                                        hex_str[((githash[15:12])*8)+1+:8], hex_str[((githash[11: 8])*8)+1+:8],
                                        hex_str[((githash[ 7: 4])*8)+1+:8], hex_str[((githash[ 3: 0])*8)+1+:8], clean,
                                        "] iCE40UP5K w/128KB VRAM" };
initial begin
    // Fill with numbers
    for (integer i = 0; i < (2**AWIDTH); i = i + 1) begin
        bram[i] = !ODDWORD ? 16'h0000 : 16'h0003;   // COP_END
    end

    bram[offset +  0]  = !ODDWORD ? { description_str[(47*8)+1+:8], description_str[(46*8)+1+:8] } : { description_str[(45*8)+1+:8], description_str[(44*8)+1+:8] };
    bram[offset +  1]  = !ODDWORD ? { description_str[(43*8)+1+:8], description_str[(42*8)+1+:8] } : { description_str[(41*8)+1+:8], description_str[(40*8)+1+:8] };
    bram[offset +  2]  = !ODDWORD ? { description_str[(39*8)+1+:8], description_str[(38*8)+1+:8] } : { description_str[(37*8)+1+:8], description_str[(36*8)+1+:8] };
    bram[offset +  3]  = !ODDWORD ? { description_str[(35*8)+1+:8], description_str[(34*8)+1+:8] } : { description_str[(33*8)+1+:8], description_str[(32*8)+1+:8] };
    bram[offset +  4]  = !ODDWORD ? { description_str[(31*8)+1+:8], description_str[(30*8)+1+:8] } : { description_str[(29*8)+1+:8], description_str[(28*8)+1+:8] };
    bram[offset +  5]  = !ODDWORD ? { description_str[(27*8)+1+:8], description_str[(26*8)+1+:8] } : { description_str[(25*8)+1+:8], description_str[(24*8)+1+:8] };
    bram[offset +  6]  = !ODDWORD ? { description_str[(23*8)+1+:8], description_str[(22*8)+1+:8] } : { description_str[(21*8)+1+:8], description_str[(20*8)+1+:8] };
    bram[offset +  7]  = !ODDWORD ? { description_str[(19*8)+1+:8], description_str[(18*8)+1+:8] } : { description_str[(17*8)+1+:8], description_str[(16*8)+1+:8] };
    bram[offset +  8]  = !ODDWORD ? { description_str[(15*8)+1+:8], description_str[(14*8)+1+:8] } : { description_str[(13*8)+1+:8], description_str[(12*8)+1+:8] };
    bram[offset +  9]  = !ODDWORD ? { description_str[(11*8)+1+:8], description_str[(10*8)+1+:8] } : { description_str[( 9*8)+1+:8], description_str[( 8*8)+1+:8] };
    bram[offset + 10]  = !ODDWORD ? { description_str[( 7*8)+1+:8], description_str[( 6*8)+1+:8] } : { description_str[( 5*8)+1+:8], description_str[( 4*8)+1+:8] };
    bram[offset + 11]  = !ODDWORD ? { description_str[( 3*8)+1+:8], description_str[( 2*8)+1+:8] } : { description_str[( 1*8)+1+:8], description_str[( 0*8)+1+:8] };

    bram[offset + 12]  = !ODDWORD ? 16'h0000         : 16'h0000;
    bram[offset + 13]  = !ODDWORD ? 16'h0000         : 16'h0000;
    bram[offset + 14]  = !ODDWORD ? 16'(version)     : { `GITCLEAN ? 8'b0 : 8'b1, 8'b0 };
    bram[offset + 15]  = !ODDWORD ? githash[31:16]   : githash[15:0];

    if (!ODDWORD) begin
        $display("XOSERA INFO: \"%s\" (%dx%d)", description_str, xv::VISIBLE_WIDTH, xv::VISIBLE_HEIGHT);
    end
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
`default_nettype wire               // restore default
