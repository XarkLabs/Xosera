// video_blend.sv - "lite" 2-bit logic based blend
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

`ifdef EN_PF_B

module video_blend_2bit (
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

logic           dv_de_1;            // display enable delay 1
logic           hsync_1;            // hsync delay 1
logic           vsync_1;            // vsync delay 1
logic           dv_de_2;            // display enable delay 2
logic           hsync_2;            // hsync delay 2
logic           vsync_2;            // vsync delay 2

logic [3:0]     colorA_r;           // color A red
logic [3:0]     colorA_g;           // color A green
logic [3:0]     colorA_b;           // color A blue

logic [3:0]     colorB_r;           // color B red
logic [3:0]     colorB_g;           // color B green
logic [3:0]     colorB_b;           // color B blue

logic [1:0]     alphaA;             // alpha for A blend
logic [1:0]     alphaB;             // alpha for B blend

logic [5:0]     outA_r;             // colorA_r * alphaA result
logic [5:0]     outA_g;             // colorA_g * alphaA result
logic [5:0]     outA_b;             // colorA_g * alphaA result

logic [5:0]     outB_r;             // colorB_r * alphaB result
logic [5:0]     outB_g;             // colorB_g * alphaB result
logic [5:0]     outB_b;             // colorB_g * alphaB result

logic [4:0]     result_r;           // result of A red + B red (with overlow bit)
logic [4:0]     result_g;           // result of A green + B green (with overlow bit)
logic [4:0]     result_b;           // result of A blue + B blue (with overlow bit)

logic unused_signals    = &{ 1'b0, colorA_xrgb_i[13:12], colorB_xrgb_i[13:12] };

always_comb begin
    // add A and B 5-bit blend result (result high bit used to detect overflow)
    result_r    = 5'((7'(outA_r) + 7'(outB_r)) >> 2);
    result_g    = 5'((7'(outA_g) + 7'(outB_g)) >> 2);
    result_b    = 5'((7'(outA_b) + 7'(outB_b)) >> 2);
end

// color blending (delays video 1 cycle for MAC16)
always_ff @(posedge clk) begin
    // setup DSP input for next pixel
    colorA_r    <= colorA_xrgb_i[11:8];
    colorA_g    <= colorA_xrgb_i[ 7:4];
    colorA_b    <= colorA_xrgb_i[ 3:0];

    colorB_r    <= colorB_xrgb_i[11:8];
    colorB_g    <= colorB_xrgb_i[ 7:4];
    colorB_b    <= colorB_xrgb_i[ 3:0];

    // use color A  [15] to select alphaA to be either 1-alphaB or 1
    if (colorA_xrgb_i[15]) begin
        alphaA  <= 2'b11;
    end else begin
        alphaA  <= ~colorB_xrgb_i[15:14];
    end

    // use color A  [14] to select alphaB to be either alphaB or 0
    if (colorA_xrgb_i[14]) begin
        alphaB  <= 2'b0;
    end else begin
        alphaB  <= colorB_xrgb_i[15:14];
    end

    // delay signals for color lookup
    vsync_1     <= vsync_i;
    hsync_1     <= hsync_i;
    dv_de_1     <= dv_de_i;

    // delay signals for multiply
    dv_de_2     <= dv_de_1;
    vsync_2     <= vsync_1;
    hsync_2     <= hsync_1;

    // output signals with blend_rgb_o
    dv_de_o     <= dv_de_2;
    vsync_o     <= vsync_2;
    hsync_o     <= hsync_2;

    // output clamped result, or black if display enable off
    if (dv_de_2) begin
        blend_rgb_o <=  {   result_r[4] ? 4'hF : result_r[3:0],
                            result_g[4] ? 4'hF : result_g[3:0],
                            result_b[4] ? 4'hF : result_b[3:0] };
    end else begin
        blend_rgb_o <= '0;
    end
end

always_comb begin
    case (alphaA)
        // A 0%
        2'b00: begin
            outA_r  = '0;
            outA_g  = '0;
            outA_b  = '0;
        end
        // A 25%
        2'b01: begin
            outA_r  = { 2'b00, colorA_r };
            outA_g  = { 2'b00, colorA_g };
            outA_b  = { 2'b00, colorA_b };
        end
        // A 50%
        2'b10: begin
            outA_r  = {1'b0, colorA_r, 1'b0 };
            outA_g  = {1'b0, colorA_g, 1'b0 };
            outA_b  = {1'b0, colorA_b, 1'b0 };
        end
        // A 100%
        2'b11: begin
            outA_r  = { colorA_r, 2'b00 };
            outA_g  = { colorA_g, 2'b00 };
            outA_b  = { colorA_b, 2'b00 };
        end
    endcase

    case (alphaB)
        // B 0%
        2'b00: begin
            outB_r  = '0;
            outB_g  = '0;
            outB_b  = '0;
        end
        // B 25%
        2'b01: begin
            outB_r  = { 2'b00, colorB_r };
            outB_g  = { 2'b00, colorB_g };
            outB_b  = { 2'b00, colorB_b };
        end
        // B 50%
        2'b10: begin
            outB_r  = {1'b0, colorB_r, 1'b0 };
            outB_g  = {1'b0, colorB_g, 1'b0 };
            outB_b  = {1'b0, colorB_b, 1'b0 };
        end
        // B 100%
        2'b11: begin
            outB_r  = { colorB_r, 2'b00 };
            outB_g  = { colorB_g, 2'b00 };
            outB_b  = { colorB_b, 2'b00 };
        end
    endcase
end

endmodule

`endif
`default_nettype wire               // restore default
