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

module video_blend #(
    parameter EN_BLEND          = 1, // only overlap, no blending
    parameter EN_BLEND_ADDCLAMP = 1 // additive blend with clamp
)(
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

logic [5:0]     r_alpha25;
logic [5:0]     g_alpha25;
logic [5:0]     b_alpha25;

logic [4:0]     r_alpha50;
logic [4:0]     g_alpha50;
logic [4:0]     b_alpha50;

logic unused;
assign unused   = &{    1'b0, colorA_xrgb_i[14:12], colorB_xrgb_i[14:12],
                        r_alpha25, g_alpha25, b_alpha25,
                        r_alpha50, g_alpha50, b_alpha50     };

if (EN_BLEND) begin : opt_ALPHA
    always_comb begin
        // (A/4)+(A/2)+(B/4) = 6/8 A + 2/8 B
        r_alpha25 = {2'b00, colorA_xrgb_i[11:8]} + {1'b0, colorA_xrgb_i[11:8], 1'b0} + {2'b00, colorB_xrgb_i[11:8] };
        g_alpha25 = {2'b00, colorA_xrgb_i[7:4]}  + {1'b0, colorA_xrgb_i[7:4],  1'b0} + {2'b00, colorB_xrgb_i[7:4]  };
        b_alpha25 = {2'b00, colorA_xrgb_i[3:0]}  + {1'b0, colorA_xrgb_i[3:0],  1'b0} + {2'b00, colorB_xrgb_i[3:0]  };

        r_alpha50 = colorA_xrgb_i[11:8] + colorB_xrgb_i[11:8];
        g_alpha50 = colorA_xrgb_i[7:4]  + colorB_xrgb_i[7:4];
        b_alpha50 = colorA_xrgb_i[3:0]  + colorB_xrgb_i[3:0];
    end
end else begin
    assign r_alpha25 = '0;
    assign g_alpha25 = '0;
    assign b_alpha25 = '0;

    assign r_alpha50 = '0;
    assign g_alpha50 = '0;
    assign b_alpha50 = '0;
end

if (EN_BLEND) begin : opt_BLEND
    always_comb begin
        case (colorA_xrgb_i[15:14])
            2'b00:          // A alpha 00xx = Use 2-bit alpha B blend value for alpha blend
                // Conceptually, for alpha purposes A is the bottom "destination" surface,
                // and B is "source" playfield blended on top, over it.
                case (colorB_xrgb_i[15:14])
                    // 8/8 A + 0/8 B    (A 100% + B 0%)
                    2'b00:    blend_result  = colorA_xrgb_i[11:0];
                    // 6/8 A + 2/8 B    (A 75% + B 25%)
                    2'b01:    blend_result  = { r_alpha25[5:2], g_alpha25[5:2], b_alpha25[5:2] };
                    // 4/8 A + 4/8 B    (A 50% + B 50%)
                    2'b10:    blend_result  = { r_alpha50[4:1], g_alpha50[4:1], b_alpha50[4:1] };
                    // 0/8 A + 8/8 B    (A 0% + B 100%)
                    2'b11:    blend_result  = colorB_xrgb_i[11:0];
                endcase

            2'b01:  // A alpha 10XX = signed blend (wrap allowed)
                blend_result    = { r_alpha50[3:0],
                                    g_alpha50[3:0],
                                    b_alpha50[3:0] };
            2'b10:  // A alpha 01XX = additive clamp (clamped at 0xf)
                blend_result    = EN_BLEND_ADDCLAMP ?
                                    {   (colorA_xrgb_i[11] & colorB_xrgb_i[11]) ? 4'hF : r_alpha50[3:0],
                                        (colorA_xrgb_i[7] & colorB_xrgb_i[7])   ? 4'hF : g_alpha50[3:0],
                                        (colorA_xrgb_i[3] & colorB_xrgb_i[3])   ? 4'hF : b_alpha50[3:0] } :
                                    {   r_alpha50[3:0],
                                        g_alpha50[3:0],
                                        b_alpha50[3:0] };
            2'b11:  // A alpha = 11XX = 100% A, ignore B (aka A on top)
                blend_result    = colorA_xrgb_i[11:0];
        endcase
    end
end else begin
    assign blend_result = (~colorA_xrgb_i[15] & colorB_xrgb_i[15]) ? colorB_xrgb_i[11:0] : colorA_xrgb_i[11:0];
end

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
