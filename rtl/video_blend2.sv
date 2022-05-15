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
//rgb_t           blend_result;

logic [7:0] alphaA;
logic [7:0] inv_alphaA;

logic [15:0] rA;
logic [15:0] gA;
logic [15:0] bA;

logic [15:0] rB;
logic [15:0] gB;
logic [15:0] bB;

// void blend4(uint8_t * result, uint8_t fg, uint8_t fa, uint8_t bg)
// {
//     unsigned int alpha     = fa + 1;
//     unsigned int inv_alpha = 16 - fa;
//     *result                = (uint8_t)((alpha * fg + inv_alpha * bg) >> 4);
// }

always_comb alphaA      = 8'h00;    //8'(colorB_xrgb_i[15:12]) + 8'h01;
always_comb inv_alphaA  = 8'h10;    //8'h10 - 8'(colorB_xrgb_i[15:12]);

always_comb rA      = 8'(colorA_xrgb_i[11:8]) * alphaA;
always_comb gA      = 8'(colorA_xrgb_i[ 7:4]) * alphaA;
always_comb bA      = 8'(colorA_xrgb_i[ 3:0]) * alphaA;

always_comb rB      = 8'(colorB_xrgb_i[11:8]) * inv_alphaA;
always_comb gB      = 8'(colorB_xrgb_i[ 7:4]) * inv_alphaA;
always_comb bB      = 8'(colorB_xrgb_i[ 3:0]) * inv_alphaA;

logic unused_bits;
always_comb unused_bits = &{ 1'b0, colorA_xrgb_i, colorB_xrgb_i, rA, gA, bA, rB, gB, bB, EN_BLEND_ADDCLAMP || EN_BLEND ? 1'b0 : 1'b0 };

//generate
//    if (EN_BLEND) begin : opt_BLEND
        // always_comb begin
        //     // Conceptually, for alpha purposes A is the bottom "destination" surface,
        //     // and B is "source" playfield blended on top, over it.
        //     blend_result  = {   rA[3:0] + rB[3:0],
        //                         gA[3:0] + gB[3:0],
        //                         bA[3:0] + bB[3:0] };
        // end
    // end else begin
    //     assign blend_result = (~colorA_xrgb_i[15] & colorB_xrgb_i[15]) ? colorB_xrgb_i[11:0] : colorA_xrgb_i[11:0];
    // end
//endgenerate

// color RAM lookup (delays video 1 cycle for BRAM)
always_ff @(posedge clk) begin

    // delay signals for color lookup
    vsync_1     <= vsync_i;
    hsync_1     <= hsync_i;
    dv_de_1     <= dv_de_i;

    // output signals with blend_rgb_o
    dv_de_o     <= dv_de_1;
    vsync_o     <= vsync_1;
    hsync_o     <= hsync_1;

    // color lookup happened on dv_de cycle
    if (dv_de_1) begin
        blend_rgb_o <= {    rA[3:0] + rB[3:0],
                            gA[3:0] + gB[3:0],
                            bA[3:0] + bB[3:0] };
    end else begin
        blend_rgb_o <= '0;
    end

end

endmodule
`default_nettype wire               // restore default
