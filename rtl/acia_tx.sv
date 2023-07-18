// acia_tx.sv - serial transmit submodule
// 06-02-19 E. Brombaugh
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

module acia_tx #(
    parameter BPS_RATE,                     // bps rate (baud rate)
    parameter CLK_HZ                        // clock in Hz
)(
    input wire logic [7:0]  tx_dat_i,       // transmit data byte
    input wire logic        tx_start_i,     // trigger transmission
    output logic            tx_serial_o,    // tx serial output
    output logic            tx_busy_o,      // tx is active (not ready)
    input wire logic        rst_i,          // system reset
    input wire logic        clk             // system clock
);
// calculate bit-rate from parameter
localparam BPS_COUNT    = CLK_HZ / BPS_RATE;
localparam BPS_COUNT_W  = $clog2(BPS_COUNT);

// hook up output
assign tx_serial_o = tx_sr[0];

// transmit machine
logic [8:0]             tx_sr;
logic [3:0]             tx_bcnt;
logic [BPS_COUNT_W-1:0] tx_rcnt;

always @(posedge clk) begin
    if (rst_i) begin
        tx_sr       <= '1;
        tx_bcnt     <= '0;
        tx_rcnt     <= '0;
        tx_busy_o   <= 1'b0;
    end else begin
        if (!tx_busy_o) begin
            if (tx_start_i) begin
                // start transmission
                tx_busy_o   <= 1'b1;
                tx_sr       <= { tx_dat_i, 1'b0 };
                tx_bcnt     <= 4'd9;        // 8-bits, plus start and stop
                tx_rcnt     <= BPS_COUNT_W'(BPS_COUNT);
            end
        end else begin
            if (~|tx_rcnt) begin
                // shift out next bit and restart
                tx_sr       <= { 1'b1, tx_sr[8:1] };
                tx_bcnt     <= tx_bcnt - 1'b1;
                tx_rcnt     <= BPS_COUNT_W'(BPS_COUNT);

                if (~|tx_bcnt) begin
                    // done - return to inactive state
                    tx_busy_o       <= 1'b0;
                end
            end else begin
                tx_rcnt     <= tx_rcnt - 1'b1;
            end
        end
    end
end

endmodule

`endif
`default_nettype wire               // restore default
