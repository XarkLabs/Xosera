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
           input  wire logic        clk,
           input  wire logic        sel,
           input  wire logic        wr_en,
           input  wire logic  [3:0] wr_mask,
           input  wire logic [15:0] address_in,
           input  wire logic [15:0] data_in,
           output      logic [15:0] data_out
       );

`ifndef SYNTHESIS
logic [15: 0] memory[0: 65535] /* verilator public */;

// clear RAM to avoid simulation errors
initial begin
    for (integer i = 0; i < 65536; i = i + 1) begin
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
            if (wr_mask[0]) memory[address_in][ 3: 0]   <= data_in[ 3: 0];
            if (wr_mask[1]) memory[address_in][ 7: 4]   <= data_in[ 7: 4];
            if (wr_mask[2]) memory[address_in][11: 8]   <= data_in[11: 8];
            if (wr_mask[3]) memory[address_in][15:12]   <= data_in[15:12];
        end
        data_out <= memory[address_in];
    end
end

`else

logic         select0;          // bank0 selected
logic [15: 0] data0;            // data output from bank0
logic         select1;          // bank1 selected  
logic [15: 0] data1;            // data output from bank1
logic         select2;          // bank2 selected  
logic [15: 0] data2;            // data output from bank2
logic         select3;          // bank3 selected  
logic [15: 0] data3;            // data output from bank3
logic   [1:0] read_bank;      // selected bank from last access for read

assign select0   =  (address_in[15:14] == 2'b00);
assign select1   =  (address_in[15:14] == 2'b01);
assign select2   =  (address_in[15:14] == 2'b10);
assign select3   =  (address_in[15:14] == 2'b11);

always_ff @(posedge clk) begin
    read_bank <= address_in[15:14];      // save currently selected bank
end

always_comb begin
    case (read_bank)
        2'b00: data_out = data0;
        2'b01: data_out = data1;
        2'b10: data_out = data2;
        2'b11: data_out = data3;
    endcase
end

SB_SPRAM256KA umem0 (
                  .ADDRESS(address_in[13: 0]),
                  .DATAIN(data_in),
                  .MASKWREN(wr_mask),
                  .WREN(wr_en),
                  .CHIPSELECT(select0),
                  .CLOCK(clk),
                  .STANDBY(1'b0),
                  .SLEEP(1'b0),
                  .POWEROFF(1'b1),
                  .DATAOUT(data0)
              );
SB_SPRAM256KA umem1 (
                  .ADDRESS(address_in[13: 0]),
                  .DATAIN(data_in),
                  .MASKWREN(wr_mask),
                  .WREN(wr_en),
                  .CHIPSELECT(select1),
                  .CLOCK(clk),
                  .STANDBY(1'b0),
                  .SLEEP(1'b0),
                  .POWEROFF(1'b1),
                  .DATAOUT(data1)
              );
SB_SPRAM256KA umem2 (
                  .ADDRESS(address_in[13: 0]),
                  .DATAIN(data_in),
                  .MASKWREN(wr_mask),
                  .WREN(wr_en),
                  .CHIPSELECT(select2),
                  .CLOCK(clk),
                  .STANDBY(1'b0),
                  .SLEEP(1'b0),
                  .POWEROFF(1'b1),
                  .DATAOUT(data2)
              );
SB_SPRAM256KA umem3 (
                  .ADDRESS(address_in[13: 0]),
                  .DATAIN(data_in),
                  .MASKWREN(wr_mask),
                  .WREN(wr_en),
                  .CHIPSELECT(select3),
                  .CLOCK(clk),
                  .STANDBY(1'b0),
                  .SLEEP(1'b0),
                  .POWEROFF(1'b1),
                  .DATAOUT(data3)
              );
`endif

endmodule
