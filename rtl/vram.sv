// vram.sv
//
// vim: set et ts=4 sw=4
//
// Copyright (c) 2020 Xark - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)
//

`default_nettype none             // mandatory for Verilog sanity
`timescale 1ns/1ps

`include "xosera_pkg.sv"

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
initial begin
    for (i = 0; i < 65536; i = i + 1) begin
        memory[i] = 16'hdead;    // "garbage"
    end

    $readmemb("fonts/font_ST_8x16w.mem", memory, 16'hf000);
    $readmemb("fonts/font_ST_8x8w.mem", memory, 16'hf800);
    $readmemb("fonts/hexfont_8x8w.mem", memory, 16'hfc00);

end

// synchronous write (keeps memory updated for easy simulator access)
always_ff @(posedge clk) begin
    if (sel) begin
        if (wr_en) begin
            memory[address_in] <= data_in;
        end
        data_out <= memory[address_in];
    end
end

`else

logic   [3:0] select;           // bit for currently selected bank
logic   [3:0] read_select;      // selected bank from last access for read
logic [15: 0] data0;            // data output from bank0
logic [15: 0] data1;            // data output from bank1
logic [15: 0] data2;            // data output from bank2
logic [15: 0] data3;            // data output from bank3

assign select = {(address_in[15:14] == 2'b00),
                 (address_in[15:14] == 2'b01),
                 (address_in[15:14] == 2'b10),
                 (address_in[15:14] == 2'b11)};

always_ff @(posedge clk) begin
    read_select <= select;      // save currently selected banks
end

always_comb begin
    data_out = data0;
    if (read_select[1]) begin
        data_out = data1;
    end
    else if (read_select[2]) begin
        data_out = data2;
    end
    else if (read_select[3]) begin
        data_out = data3;
    end
end

SB_SPRAM256KA umem0 (
                  .ADDRESS(address_in[13: 0]),
                  .DATAIN(data_in),
                  .MASKWREN(4'b1111),
                  .WREN(wr_en),
                  .CHIPSELECT(select[0]),
                  .CLOCK(clk),
                  .STANDBY(1'b0),
                  .SLEEP(1'b0),
                  .POWEROFF(1'b1),
                  .DATAOUT(data0)
              );
SB_SPRAM256KA umem1 (
                  .ADDRESS(address_in[13: 0]),
                  .DATAIN(data_in),
                  .MASKWREN(4'b1111),
                  .WREN(wr_en),
                  .CHIPSELECT(select[1]),
                  .CLOCK(clk),
                  .STANDBY(1'b0),
                  .SLEEP(1'b0),
                  .POWEROFF(1'b1),
                  .DATAOUT(data1)
              );
SB_SPRAM256KA umem2 (
                  .ADDRESS(address_in[13: 0]),
                  .DATAIN(data_in),
                  .MASKWREN(4'b1111),
                  .WREN(wr_en),
                  .CHIPSELECT(select[2]),
                  .CLOCK(clk),
                  .STANDBY(1'b0),
                  .SLEEP(1'b0),
                  .POWEROFF(1'b1),
                  .DATAOUT(data2)
              );
SB_SPRAM256KA umem3 (
                  .ADDRESS(address_in[13: 0]),
                  .DATAIN(data_in),
                  .MASKWREN(4'b1111),
                  .WREN(wr_en),
                  .CHIPSELECT(select[3]),
                  .CLOCK(clk),
                  .STANDBY(1'b0),
                  .SLEEP(1'b0),
                  .POWEROFF(1'b1),
                  .DATAOUT(data3)
              );
`endif

endmodule
