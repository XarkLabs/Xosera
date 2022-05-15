// video_blend.sv
//
// vim: set et ts=4 sw=4
//
// Copyright (c) 2021 Xark - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)
//

`default_nettype none               // mandatory for Verilog sanity
`timescale 1ns/1ps                  // mandatory to shut up Icarus Verilog

`include "xosera_pkg.sv"

module video_blend2#(
    parameter EN_BLEND          = 1,// only overlap, no blending
    parameter EN_BLEND_ADDCLAMP = 1 // additive blend with clamp
)
(
    // video RGB inputs
    input wire  logic           vsync_i,
    input wire  logic           hsync_i,
    input wire  logic           dv_de_i,
    input wire  argb_t          colorA_xrgb_i,
    input wire  argb_t          colorB_xrgb_i,
    output      rgb_t           blend_rgb_o,
    output      logic           hsync_o,
    output      logic           vsync_o,
    output      logic           dv_de_o,
    input wire  logic           clk
);

logic           dv_de_1;            // display enable delayed
logic           hsync_1;            // hsync delayed
logic           vsync_1;            // vsync delayed
rgb_t           blend_result;

logic [7:0] rA;
logic [7:0] gA;
logic [7:0] bA;

logic [7:0] rB;
logic [7:0] gB;
logic [7:0] bB;

logic [3:0] r_mix;
logic [3:0] g_mix;
logic [3:0] b_mix;

always_comb rA      = { colorA_xrgb_i[11:8], colorA_xrgb_i[11:8] } * { 4'b0, ~colorB_xrgb_i[15:12] };
always_comb gA      = { colorA_xrgb_i[ 7:4], colorA_xrgb_i[ 7:4] } * { 4'b0, ~colorB_xrgb_i[15:12] };
always_comb bA      = { colorA_xrgb_i[ 3:0], colorA_xrgb_i[ 3:0] } * { 4'b0, ~colorB_xrgb_i[15:12] };

always_comb rB      = { colorB_xrgb_i[11:8], colorB_xrgb_i[11:8] } * { 4'b0, colorB_xrgb_i[15:12] };
always_comb gB      = { colorB_xrgb_i[ 7:4], colorB_xrgb_i[ 7:4] } * { 4'b0, colorB_xrgb_i[15:12] };
always_comb bB      = { colorB_xrgb_i[ 3:0], colorB_xrgb_i[ 3:0] } * { 4'b0, colorB_xrgb_i[15:12] };

always_comb r_mix   = rA[7:4] + rB[7:4];
always_comb g_mix   = gA[7:4] + gB[7:4];
always_comb b_mix   = bA[7:4] + bB[7:4];

logic unused_bits;
always_comb unused_bits = &{ 1'b0, colorA_xrgb_i[14:12], colorB_xrgb_i[13:12], rA[3:0], gA[3:0], bA[3:0], rB[3:0], gB[3:0], bB[3:0], EN_BLEND_ADDCLAMP ? 1'b0 : 1'b0 };

generate
    if (EN_BLEND) begin : opt_BLEND
        always_comb begin
            case (colorA_xrgb_i[15])
                1'b0:          // A alpha 00xx = Use 2-bit alpha B blend value for alpha blend
                    // Conceptually, for alpha purposes A is the bottom "destination" surface,
                    // and B is "source" playfield blended on top, over it.
                    blend_result  = {   r_mix[3:0],
                                        g_mix[3:0],
                                        b_mix[3:0] };

                1'b1:  // A alpha = 11XX = 100% A, ignore B (aka A on top)
                    blend_result    = colorA_xrgb_i[11:0];
            endcase
        end
    end else begin
        assign blend_result = (~colorA_xrgb_i[15] & colorB_xrgb_i[15]) ? colorB_xrgb_i[11:0] : colorA_xrgb_i[11:0];
    end
endgenerate

// color RAM lookup (delays video 1 cycle for BRAM)
always_ff @(posedge clk) begin

    // color lookup happened on dv_de cycle
    if (dv_de_1) begin
        blend_rgb_o <= blend_result;
    end else begin
        blend_rgb_o <= '0;
    end

    // delay signals for color lookup
    vsync_1     <= vsync_i;
    hsync_1     <= hsync_i;
    dv_de_1     <= dv_de_i;

    // output signals with blend_rgb_o
    dv_de_o     <= dv_de_1;
    vsync_o     <= vsync_1;
    hsync_o     <= hsync_1;
end

endmodule
`default_nettype wire               // restore default
