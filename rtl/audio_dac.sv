// audio_dac.sv
//
// vim: set et ts=4 sw=4
//
// Copyright (c) 2022 Xark - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)
//

`default_nettype none               // mandatory for Verilog sanity
`timescale 1ns/1ps                  // mandatory to shut up Icarus Verilog

`include "xosera_pkg.sv"

`ifdef EN_AUDIO

module audio_dac #(
    parameter WIDTH = 8
)(
    input wire  logic [WIDTH-1:0]   value_i,
    output      logic               pulse_o,
    input wire  logic               reset_i,
    input wire  logic               clk
);

logic [WIDTH:0] accumulator;

// simple 1st order sigma-delta DAC
// See https://www.fpga4fun.com/PWM_DAC_2.html
// and http://retroramblings.net/?p=1686
always_ff @(posedge clk) begin
    if (reset_i) begin
        accumulator <= '0;
        pulse_o     <= '0;
    end else begin
        accumulator <= accumulator[WIDTH-1:0] + value_i;
        pulse_o     <= accumulator[WIDTH];
    end
end

endmodule

`endif
`default_nettype wire               // restore default
