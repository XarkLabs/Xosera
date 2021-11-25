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

module video_blend(
    // video RGB inputs
    input wire  logic           vsync_i,
    input wire  logic           hsync_i,
    input wire  logic           dv_de_i,
    input wire  logic  [15:0]   colorA_xrgb_i,
`ifdef ENABLE_PB
    input wire  logic  [15:0]   colorB_xrgb_i,
`endif
    output      logic  [11:0]   blend_rgb_o,
    output      logic           hsync_o,
    output      logic           vsync_o,
    output      logic           dv_de_o,
    input wire  logic           clk
);

logic       dv_de_1;            // display enable delayed
logic       hsync_1;            // hsync delayed
logic       vsync_1;            // vsync delayed

`ifdef ENABLE_PB
logic [4:0] r_addAB;
logic [4:0] g_addAB;
logic [4:0] b_addAB;

logic [4:0] r_subAB;
logic [4:0] g_subAB;
logic [4:0] b_subAB;

logic [5:0] r_alpha25;
logic [5:0] g_alpha25;
logic [5:0] b_alpha25;

logic [4:0] r_addABx2;
logic [4:0] g_addABx2;
logic [4:0] b_addABx2;

logic unused_bits;  // NOTE: Verilator recognizes "unused" prefix
assign unused_bits = &{ 1'b0, colorA_xrgb_i[14:12], colorB_xrgb_i[13:12], r_alpha25[1:0], g_alpha25[1:0], b_alpha25[1:0] } ;

always_comb begin
    r_addAB = colorA_xrgb_i[11:8] + colorB_xrgb_i[11:8];
    g_addAB = colorA_xrgb_i[7:4]  + colorB_xrgb_i[7:4];
    b_addAB = colorA_xrgb_i[3:0]  + colorB_xrgb_i[3:0];

    r_subAB = colorA_xrgb_i[11:8] - colorB_xrgb_i[11:8];
    g_subAB = colorA_xrgb_i[7:4]  - colorB_xrgb_i[7:4];
    b_subAB = colorA_xrgb_i[3:0]  - colorB_xrgb_i[3:0];

    // (A/4)+(A/2)+(B/4) = 75% A + 25% B
    r_alpha25 = {2'b00, colorA_xrgb_i[11:8]} + {1'b0, colorA_xrgb_i[11:8], 1'b0} + {2'b00, colorB_xrgb_i[11:8] };
    g_alpha25 = {2'b00, colorA_xrgb_i[7:4]}  + {1'b0, colorA_xrgb_i[7:4],  1'b0} + {2'b00, colorB_xrgb_i[7:4]  };
    b_alpha25 = {2'b00, colorA_xrgb_i[3:0]}  + {1'b0, colorA_xrgb_i[3:0],  1'b0} + {2'b00, colorB_xrgb_i[3:0]  };

    r_addABx2 = {1'b0, colorA_xrgb_i[11:8]} + {colorB_xrgb_i[11:8], 1'b0};
    g_addABx2 = {1'b0, colorA_xrgb_i[7:4] } + {colorB_xrgb_i[7:4],  1'b0};
    b_addABx2 = {1'b0, colorA_xrgb_i[3:0] } + {colorB_xrgb_i[3:0],  1'b0};
end
`else
logic unused_bits;  // NOTE: Verilator recognizes "unused" prefix
assign unused_bits = &{ 1'b0, colorA_xrgb_i[15:12] } ;
`endif

// color RAM lookup (delays video 1 cycle for BRAM)
always_ff @(posedge clk) begin

    // color lookup happened on dv_de cycle
    if (dv_de_1) begin
`ifdef ENABLE_PB
        // Conceptually, A is the bottom "destination" playfield, and B is "source" playfield
        // rendered on top of it.

        // A     B    (alpha color values)
        // 0xxx  00xx  = A                      [A alpha 100% + B alpha   0%]
        // 0xxx  01xx  = (A/4 + A/2 + B/4)      [A alpha  75% + B alpha  25%]
        // 0xxx  10xx  = (A/2 + B/2)            [A alpha  50% + B alpha  50%]
        // 0xxx  11xx  = B                      [A alpha   0% + B alpha 100%]
        // (blend operations below are clamped at 0 and F)
        // 1xxx  01xx  = A + B                  [additive blend]
        // 1xxx  01xx  = A - B                  [subtractive blend]
        // 1xxx  10xx  = A + signed B           [delta blend (-8/+7)]
        // 1xxx  11xx  = A + signed B*2         [delta * 2 blend (even -16/+14)]

        case ({colorA_xrgb_i[15], colorB_xrgb_i[15:14]})
        // 100% A
        3'b0_00:    blend_rgb_o  <= colorA_xrgb_i[11:0];
        // 75% A + 25% B
        3'b0_01:    blend_rgb_o  <= { r_alpha25[5:2],
                                      g_alpha25[5:2],
                                      b_alpha25[5:2] };
        // 50% A + 50% B
        3'b0_10:    blend_rgb_o  <= { r_addAB[4:1],
                                      g_addAB[4:1],
                                      b_addAB[4:1] };
        // 100% B
        3'b0_11:    blend_rgb_o  <= colorB_xrgb_i[11:0];
        // A + B
        3'b1_00:    blend_rgb_o  <= { r_addAB[4] ? 4'hF : r_addAB[3:0],
                                      g_addAB[4] ? 4'hF : g_addAB[3:0],
                                      b_addAB[4] ? 4'hF : b_addAB[3:0] };
        // A - B
        3'b1_01:    blend_rgb_o  <= { r_subAB[4] ? 4'h0 : r_subAB[3:0],
                                      g_subAB[4] ? 4'h0 : g_subAB[3:0],
                                      b_subAB[4] ? 4'h0 : b_subAB[3:0] };
        // A + signed B
        3'b1_10:    blend_rgb_o  <= { r_addAB[4] ? (colorB_xrgb_i[11] ? 4'h0 : 4'hF) : r_addAB[3:0],
                                      g_addAB[4] ? (colorB_xrgb_i[ 7] ? 4'h0 : 4'hF) : g_addAB[3:0],
                                      b_addAB[4] ? (colorB_xrgb_i[ 3] ? 4'h0 : 4'hF) : b_addAB[3:0] };
        // A + signed B*2
        3'b1_11:    blend_rgb_o  <= { r_addABx2[4] ? (colorB_xrgb_i[11] ? 4'h0 : 4'hF) : r_addABx2[3:0],
                                      g_addABx2[4] ? (colorB_xrgb_i[ 7] ? 4'h0 : 4'hF) : g_addABx2[3:0],
                                      b_addABx2[4] ? (colorB_xrgb_i[ 3] ? 4'h0 : 4'hF) : b_addABx2[3:0] };
        endcase
`else
        red_o       <= colorA_xrgb_i[11:8];
        green_o     <= colorA_xrgb_i[7:4];
        blue_o      <= colorA_xrgb_i[3:0];
`endif

    end else begin
        blend_rgb_o <= 12'h0;
    end

    // delay signals for color lookup
    vsync_1     <= vsync_i;
    hsync_1     <= hsync_i;
    dv_de_1     <= dv_de_i;
    // output signals
    dv_de_o     <= dv_de_1;
    vsync_o     <= vsync_1;
    hsync_o     <= hsync_1;
end

endmodule
`default_nettype wire               // restore default
