// vram.sv
//
// vim: set et ts=4 sw=4
//
// Copyright (c) 2020 Xark - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)
//

`default_nettype none             // mandatory for Verilog sanity
`timescale 1ns/1ns

module vram(
           input  logic         clk,
           input  logic         sel,
           input  logic         wr_en,
           input  logic [15: 0] address_in,
           input  logic [15: 0] data_in,
           output logic [15: 0] data_out
       );

`ifndef SYNTHESIS
integer i;
logic [15: 0] memory[0: 65535] /* verilator public */;

// clear RAM to avoid simulation errors
initial
    for (i = 0; i < 65536; i = i + 1)
        memory[i] = 16'hdead;    // "garbage"

// synchronous write (keeps memory updated for easy simulator access)
always_ff @(posedge clk) begin
    if (sel & wr_en) begin
        memory[address_in] <= data_in;
    end
    data_out <= memory[address_in];
end

`else

logic [15: 0] data0;
SB_SPRAM256KA umem0 (
                  .ADDRESS(address_in[13: 0]),
                  .DATAIN(data_in),
                  .MASKWREN(4'b1111),
                  .WREN(wr_en & sel),
                  .CHIPSELECT(address_in[15: 14] == 2'b00),
                  .CLOCK(clk),
                  .STANDBY(1'b0),
                  .SLEEP(1'b0),
                  .POWEROFF(1'b1),
                  .DATAOUT(data0)
              );
logic [15: 0] data1;
SB_SPRAM256KA umem1 (
                  .ADDRESS(address_in[13: 0]),
                  .DATAIN(data_in),
                  .MASKWREN(4'b1111),
                  .WREN(wr_en & sel),
                  .CHIPSELECT(address_in[15: 14] == 2'b01),
                  .CLOCK(clk),
                  .STANDBY(1'b0),
                  .SLEEP(1'b0),
                  .POWEROFF(1'b1),
                  .DATAOUT(data1)
              );
logic [15: 0] data2;
SB_SPRAM256KA umem2 (
                  .ADDRESS(address_in[13: 0]),
                  .DATAIN(data_in),
                  .MASKWREN(4'b1111),
                  .WREN(wr_en & sel),
                  .CHIPSELECT(address_in[15: 14] == 2'b10),
                  .CLOCK(clk),
                  .STANDBY(1'b0),
                  .SLEEP(1'b0),
                  .POWEROFF(1'b1),
                  .DATAOUT(data2)
              );
logic [15: 0] data3;
SB_SPRAM256KA umem3 (
                  .ADDRESS(address_in[13: 0]),
                  .DATAIN(data_in),
                  .MASKWREN(4'b1111),
                  .WREN(wr_en & sel),
                  .CHIPSELECT(address_in[15: 14] == 2'b11),
                  .CLOCK(clk),
                  .STANDBY(1'b0),
                  .SLEEP(1'b0),
                  .POWEROFF(1'b1),
                  .DATAOUT(data3)
              );

always_comb begin
    case (address_in[15: 14])
        2'b00:
            data_out = data0;
        2'b01:
            data_out = data1;
        2'b10:
            data_out = data2;
        2'b11:
            data_out = data3;
    endcase
end

`endif

endmodule
