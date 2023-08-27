// acia_rx.sv - asynchronous serial receive submodule
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
`ifndef EN_UART_TX

module acia_rx #(
    parameter BPS_RATE,                     // bps rate (baud rate)
    parameter CLK_HZ                        // clock in Hz
)(
    input  wire logic       rx_serial_i,    // raw serial input
    output logic [7:0]      rx_dat_o,       // received byte
    output logic            rx_stb_o,       // received data available
    input  wire logic       rst_i,          // system or acia reset
    input  wire logic       clk             // system clock
);
// calculate bit-rate from parameter
localparam BPS_COUNT    = CLK_HZ / BPS_RATE;
localparam BPS_COUNT_W  = $clog2(BPS_COUNT);

// input sync & deglitch
logic [2:0]             in_pipe;
logic                   in_state;
logic                   all_zero;
logic                   all_one;

assign                  all_zero    = ~|in_pipe[2:1];
assign                  all_one     = &in_pipe[2:1];

always_ff @(posedge clk) begin
    if (rst_i) begin
        // assume RX input idle at start
        in_pipe         <= '1;
        in_state        <= 1'b1;
    end else begin
        // shift in a bit
        in_pipe         <= { in_pipe[1:0], rx_serial_i };

        // update state
        if (in_state && all_zero) begin
            in_state        <= 1'b0;
        end else if(!in_state && all_one) begin
            in_state        <= 1'b1;
        end
    end
end

// receive machine
logic [8:0]             rx_sr;
logic [3:0]             rx_bcnt;
logic [BPS_COUNT_W-1:0] rx_rcnt;
logic                   rx_busy;
always_ff @(posedge clk) begin
    if (rst_i) begin
        rx_busy     <= 1'b0;
        rx_stb_o    <= 1'b0;
    end else begin
        // clear the strobe
        rx_stb_o    <= 1'b0;
        if (!rx_busy) begin
            if (!in_state) begin
                // found start bit - wait 1/2 bit to sample
                rx_bcnt     <= 4'h9;
                rx_rcnt     <= BPS_COUNT_W'(BPS_COUNT / 2);
                rx_busy     <= 1'b1;
            end
        end else begin
            if (~|rx_rcnt) begin
                // sample data and restart for next bit
                rx_sr       <= { in_state, rx_sr[8:1] };
                rx_rcnt     <= BPS_COUNT_W'(BPS_COUNT);
                rx_bcnt     <= rx_bcnt - 1'b1;

                if (~|rx_bcnt) begin
                    // final bit - check for err and finish
                    rx_dat_o    <= rx_sr[8:1];
                    rx_busy     <= 1'b0;
                    if (in_state && ~rx_sr[0]) begin
                        // framing OK
                        rx_stb_o    <= 1'b1;
                    end else begin
                        // ignore framing err
                    end
                end
            end else begin
                rx_rcnt     <= rx_rcnt - 1'b1;
            end
        end
    end
end

endmodule

`endif
`endif
`default_nettype wire               // restore default
