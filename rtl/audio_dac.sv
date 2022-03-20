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

module audio_dac #(
    parameter WIDTH = 8
)
(
    input wire  logic [WIDTH-1:0]   value_i,
    output      logic               pulse_o,
    input wire  logic               clk
);

logic [WIDTH:0] accumulator = { 1'b1, { WIDTH{1'b0} } };

// simple 1st order sigma-delta DAC
always_ff @(posedge clk) begin
    accumulator <= { 1'b0, accumulator[WIDTH-1:0] } + { 1'b0, value_i };
    pulse_o     <= accumulator[WIDTH];
end

endmodule
`default_nettype wire               // restore default
