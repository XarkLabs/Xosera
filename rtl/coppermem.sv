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

module coppermem(
           input  wire logic        clk,
           input  wire logic        rd_en_i,
           input  wire logic [10:0] rd_address_i,
           output      logic [15:0] rd_data_o,
           input  wire logic        wr_clk,
           input  wire logic        wr_en_i,
           input  wire logic [10:0] wr_address_i,
           input  wire logic [15:0] wr_data_i
       );

`ifndef SYNTHESIS
initial begin
    // Pathological red/green/blue banded background, for instruction
    // testing...
    //
                                               // copperlist:
    bram[0]  = 16'h20a0; bram[1]  = 16'h0002;  //     skip  0, 160, 0b00010             ; Skip next if we've hit line 160
    bram[2]  = 16'h4010; bram[3]  = 16'h0000;  //     jmp   .gored                      ; ... else, jump to set red
    bram[4]  = 16'h2140; bram[5]  = 16'h0002;  //     skip  0, 320, 0b00010             ; Skip next if we've hit line 320
    bram[6]  = 16'h400c; bram[7]  = 16'h0000;  //     jmp   .gogreen                    ; ... else jump to set blue
    bram[8]  = 16'hb000; bram[9]  = 16'h000f;  //     movep 0, 0x000F                   ; Make color 0 blue
    bram[10] = 16'h0000; bram[11] = 16'h0003;  //     nextf                             ; and we're done for this frame
                                               // .gogreen:
    bram[12] = 16'hb000; bram[13] = 16'h00f0;  //     movep 0, 0x00F0                   ; Make color 0 green
    bram[14] = 16'h4000; bram[15] = 16'h0000;  //     jmp   copperlist                  ; and restart
                                               // .gored
    bram[16] = 16'hb000; bram[17] = 16'h0f00;  //     movep 0, 0x0F00                   ; Make color 0 red
    bram[18] = 16'h4000; bram[19] = 16'h0000;  //     jmp   copperlist                  ; and restart

    // The following is the correct way to do the above :D 
/*
    bram[0]  = 16'hb000; bram[1]  = 16'h0f00; // movep 0, 0x0F00          ; Make color 0 red
    bram[2]  = 16'h00a0; bram[3]  = 16'h0002; // wait  0, 160, 0b000010   ; Wait for line 160, ignore X position
    bram[4]  = 16'hb000; bram[5]  = 16'h00f0; // movep 0, 0x00F0          ; Make color 0 green
    bram[6]  = 16'h0140; bram[7]  = 16'h0002; // wait  0, 320, 0b000010   ; Wait for line 320, ignore X position
    bram[8]  = 16'hb000; bram[9]  = 16'h000f; // movep 0, 0x000F          ; Make color 0 blue
    bram[10] = 16'h0000; bram[11] = 16'h0003; // nextf                    ; Wait for next frame
*/
    // Fill rest with incrementing addresses...
    for (integer i = 20; i < 2048; i = i + 1) begin
        bram[i] = 16'(i);
    end
end
`endif

// infer 8x4KB copper BRAM
//integer i;
logic [15: 0] bram[0 : 2047];

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
