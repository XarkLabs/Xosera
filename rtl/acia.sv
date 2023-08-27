// acia.sv - strippped-down version of MC6850 ACIA with home-made TX/RX
// 03-02-19 E. Brombaugh
// 07-16-23 Xark            - modified for Xosera and SystemVerilog
//
// MIT License
//
// Copyright (c) 2019 Eric Brombaugh
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

`default_nettype none               // mandatory for Verilog sanity
`timescale 1ns/1ps                  // mandatory to shut up Icarus Verilog

`include "xosera_pkg.sv"

`ifdef EN_UART

module acia #(
    parameter BPS_RATE,                     // bps rate (baud rate)
    parameter CLK_HZ                        // clock in Hz
)(
`ifndef EN_UART_TX
    input  wire logic           rd_i,       // read strobe
`endif
    input  wire logic           wr_i,       // write strobe
    input  wire logic           rs_i,       // register select
`ifndef EN_UART_TX
    input  wire logic           rx_i,       // serial receive
`endif
    output logic                tx_o,       // serial transmit
    input  wire logic  [7:0]    din_i,      // data bus input
`ifndef EN_UART_TX
    output logic       [7:0]    dout_o,     // data bus output
`endif
    output logic                txf_o,      // transmit empty (ready to send char)
`ifndef EN_UART_TX
    output logic                rxf_o,      // receive full (char ready to read)
`endif
    input  wire logic           rst_i,      // system reset
    input  wire logic           clk         // system clock
);

logic           tx_start;
`ifndef EN_UART_TX
logic [7:0]     rx_dat;
`endif

// generate tx_start signal on write to register 1
assign tx_start = rs_i & wr_i;

// load dout_o with rx data
always_ff @(posedge clk) begin
    if (rst_i) begin
        txf_o   <= 1'b0;
`ifndef EN_UART_TX
        rxf_o   <= 1'b0;
        dout_o  <= '0;
`endif
    end else begin
        txf_o   <= txf;
`ifndef EN_UART_TX
        rxf_o   <= rxf;
        dout_o  <= rx_dat;
`endif
    end
end

// tx_o empty is cleared when tx_start starts, cleared when tx_busy deasserts
logic           txf;
logic           tx_busy;
logic           prev_tx_busy;
always_ff @(posedge clk) begin
    if (rst_i) begin
        txf             <= 1'b0;
        prev_tx_busy    <= 1'b0;
    end else begin
        prev_tx_busy    <= tx_busy;

        if (tx_start) begin
            txf             <= 1'b1;
        end else if (prev_tx_busy & ~tx_busy) begin
            txf             <= 1'b0;
        end
    end
end

`ifndef EN_UART_TX
// rx_i full is set when rx_stb pulses, cleared when data logic read
logic           rx_stb;
logic           rxf;
always_ff @(posedge clk) begin
    if (rst_i) begin
        rxf     <= 1'b0;
    end else begin
        if (rx_stb) begin
            rxf     <= 1'b1;
        end else if (rs_i & rd_i) begin
            rxf     <= 1'b0;
        end
    end
end

// Async Receiver
acia_rx #(
    .BPS_RATE(BPS_RATE),        // bps rate
    .CLK_HZ(CLK_HZ)             // clock rate
) acia_rx (
    .rx_serial_i(rx_i),         // raw serial input
    .rx_dat_o(rx_dat),          // received byte
    .rx_stb_o(rx_stb),          // received data available
    .rst_i(rst_i),              // system reset
    .clk(clk)                   // system clock
);

`endif

// Transmitter
acia_tx #(
    .BPS_RATE(BPS_RATE),        // bps rate
    .CLK_HZ(CLK_HZ)             // clock rate
) acia_tx (
    .tx_dat_i(din_i),           // transmit data byte
    .tx_start_i(tx_start),      // trigger transmission
    .tx_serial_o(tx_o),         // tx_o serial output
    .tx_busy_o(tx_busy),        // tx_o is active (not ready)
    .rst_i(rst_i),              // system reset
    .clk(clk)                   // system clock
);

endmodule

`endif
`default_nettype wire               // restore default
