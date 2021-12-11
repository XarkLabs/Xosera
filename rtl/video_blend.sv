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

module video_blend#(
    parameter EN_VID_PF_B               = 1,        // 2nd overlay playfield
    parameter EN_VID_PF_B_NO_BLEND      = 0,        // only super-impose, no blending
    parameter EN_VID_PF_B_BLEND_A8      = 0,        // blend with 8 levels vs 4
    parameter EN_VID_PF_B_BLEND_EXTRA   = 0         // blend with fancy modes
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

logic       dv_de_1;            // display enable delayed
logic       hsync_1;            // hsync delayed
logic       vsync_1;            // vsync delayed

logic [6:0] r_alpha12_5;
logic [6:0] g_alpha12_5;
logic [6:0] b_alpha12_5;

logic [5:0] r_alpha25;
logic [5:0] g_alpha25;
logic [5:0] b_alpha25;

logic [4:0] r_alpha50;
logic [4:0] g_alpha50;
logic [4:0] b_alpha50;

logic [5:0] r_alpha75;
logic [5:0] g_alpha75;
logic [5:0] b_alpha75;

logic [6:0] r_alpha87_5;
logic [6:0] g_alpha87_5;
logic [6:0] b_alpha87_5;

logic [4:0] r_subAB;
logic [4:0] g_subAB;
logic [4:0] b_subAB;

logic [4:0] r_addABx2;
logic [4:0] g_addABx2;
logic [4:0] b_addABx2;

logic unused_bits;  // NOTE: Verilator recognizes "unused" prefix
assign unused_bits = &{ 1'b0, colorA_xrgb_i[13:12],
                        r_alpha50[0], g_alpha50[0], b_alpha50[0],
                        r_alpha25[1:0], g_alpha25[1:0], b_alpha25[1:0],
                        r_alpha12_5[2:0], g_alpha12_5[2:0], b_alpha12_5[2:0],
                        r_alpha75[1:0], g_alpha75[1:0], b_alpha75[1:0],
                        r_alpha87_5[2:0], g_alpha87_5[2:0], b_alpha87_5[2:0] } ;

logic unused_bits2;
assign unused_bits2 = &{ 1'b0, r_subAB, g_subAB, b_subAB, r_addABx2, g_addABx2, b_addABx2 } ;

always_comb begin
    r_alpha50 = colorA_xrgb_i[11:8] + colorB_xrgb_i[11:8];
    g_alpha50 = colorA_xrgb_i[7:4]  + colorB_xrgb_i[7:4];
    b_alpha50 = colorA_xrgb_i[3:0]  + colorB_xrgb_i[3:0];

    // (A/4)+(A/2)+(B/4) = 6/8 A + 2/8 B
    r_alpha25 = {2'b00, colorA_xrgb_i[11:8]} + {1'b0, colorA_xrgb_i[11:8], 1'b0} + {2'b00, colorB_xrgb_i[11:8] };
    g_alpha25 = {2'b00, colorA_xrgb_i[7:4]}  + {1'b0, colorA_xrgb_i[7:4],  1'b0} + {2'b00, colorB_xrgb_i[7:4]  };
    b_alpha25 = {2'b00, colorA_xrgb_i[3:0]}  + {1'b0, colorA_xrgb_i[3:0],  1'b0} + {2'b00, colorB_xrgb_i[3:0]  };
end

generate
    if (EN_VID_PF_B_BLEND_A8) begin
        always_comb begin
            // A-(A/8)+(B/8) = 7/8 A + 1/8 B
            r_alpha12_5 = {colorA_xrgb_i[11:8], 3'b000} - {3'b000, colorA_xrgb_i[11:8]} + {3'b000, colorB_xrgb_i[11:8] };
            g_alpha12_5 = {colorA_xrgb_i[7:4] , 3'b000} - {3'b000, colorA_xrgb_i[7:4] } + {3'b000, colorB_xrgb_i[7:4]  };
            b_alpha12_5 = {colorA_xrgb_i[3:0] , 3'b000} - {3'b000, colorA_xrgb_i[3:0] } + {3'b000, colorB_xrgb_i[3:0]  };

            // (A/4)+(B/2)+(B/4) = 2/8 A + 6/8 B
            r_alpha75 = {2'b00, colorA_xrgb_i[11:8]} + {1'b0, colorB_xrgb_i[11:8], 1'b0} + {2'b00, colorB_xrgb_i[11:8] };
            g_alpha75 = {2'b00, colorA_xrgb_i[7:4]}  + {1'b0, colorB_xrgb_i[7:4],  1'b0} + {2'b00, colorB_xrgb_i[7:4]  };
            b_alpha75 = {2'b00, colorA_xrgb_i[3:0]}  + {1'b0, colorB_xrgb_i[3:0],  1'b0} + {2'b00, colorB_xrgb_i[3:0]  };

            // B-(B/8)+(A/8) = 1/8 A + 7/8 B
            r_alpha87_5 = {colorB_xrgb_i[11:8], 3'b000} - {3'b000, colorB_xrgb_i[11:8]} + {3'b000, colorA_xrgb_i[11:8] };
            g_alpha87_5 = {colorB_xrgb_i[7:4] , 3'b000} - {3'b000, colorB_xrgb_i[7:4] } + {3'b000, colorA_xrgb_i[7:4]  };
            b_alpha87_5 = {colorB_xrgb_i[3:0] , 3'b000} - {3'b000, colorB_xrgb_i[3:0] } + {3'b000, colorA_xrgb_i[3:0]  };
        end
    end else begin
        assign r_alpha12_5  = '0;
        assign g_alpha12_5  = '0;
        assign b_alpha12_5  = '0;

        assign r_alpha75    = '0;
        assign g_alpha75    = '0;
        assign b_alpha75    = '0;

        assign r_alpha87_5  = '0;
        assign g_alpha87_5  = '0;
        assign b_alpha87_5  = '0;
    end
endgenerate

generate
    if (EN_VID_PF_B_BLEND_EXTRA) begin
        always_comb begin
            r_subAB = colorA_xrgb_i[11:8] - colorB_xrgb_i[11:8];
            g_subAB = colorA_xrgb_i[7:4]  - colorB_xrgb_i[7:4];
            b_subAB = colorA_xrgb_i[3:0]  - colorB_xrgb_i[3:0];

            r_addABx2 = {1'b0, colorA_xrgb_i[11:8]} + {colorB_xrgb_i[11:8], 1'b0};
            g_addABx2 = {1'b0, colorA_xrgb_i[7:4] } + {colorB_xrgb_i[7:4],  1'b0};
            b_addABx2 = {1'b0, colorA_xrgb_i[3:0] } + {colorB_xrgb_i[3:0],  1'b0};
        end
    end else begin
        assign r_subAB      = '0;
        assign g_subAB      = '0;
        assign b_subAB      = '0;

        assign r_addABx2    = '0;
        assign g_addABx2    = '0;
        assign b_addABx2    = '0;
    end
endgenerate


// color RAM lookup (delays video 1 cycle for BRAM)
always_ff @(posedge clk) begin

    // color lookup happened on dv_de cycle
    if (dv_de_1) begin
        // Conceptually, A is the bottom "destination" playfield, and B is "source" playfield
        // rendered on top of it.

        if (EN_VID_PF_B) begin
            if (EN_VID_PF_B_NO_BLEND) begin
                if (colorB_xrgb_i[15]) begin
                    blend_rgb_o     <= colorB_xrgb_i[11:0];
                end else begin
                    blend_rgb_o     <= colorA_xrgb_i[11:0];
                end
            end else if (EN_VID_PF_B_BLEND_EXTRA && (colorA_xrgb_i[15] | colorB_xrgb_i[12])) begin
                case (colorA_xrgb_i[15:14])
                    // // A + signed B
                    // 2'b10:    blend_rgb_o <= { r_alpha50[4] ? (colorB_xrgb_i[11] ? 4'h0 : 4'hF) : r_alpha50[3:0],
                    //                             g_alpha50[4] ? (colorB_xrgb_i[ 7] ? 4'h0 : 4'hF) : g_alpha50[3:0],
                    //                             b_alpha50[4] ? (colorB_xrgb_i[ 3] ? 4'h0 : 4'hF) : b_alpha50[3:0] };
                    // // A + signed B*2
                    // 2'b11:    blend_rgb_o <= { r_addABx2[4] ? (colorB_xrgb_i[11] ? 4'h0 : 4'hF) : r_addABx2[3:0],
                    //                             g_addABx2[4] ? (colorB_xrgb_i[ 7] ? 4'h0 : 4'hF) : g_addABx2[3:0],
                    //                             b_addABx2[4] ? (colorB_xrgb_i[ 3] ? 4'h0 : 4'hF) : b_addABx2[3:0] };
                    // A + B wrap
                    2'b00:    blend_rgb_o <= {  r_alpha50[3:0],
                                                g_alpha50[3:0],
                                                b_alpha50[3:0] };
                    // A - B wrap
                    2'b01:    blend_rgb_o <= {  r_subAB[3:0],
                                                g_subAB[3:0],
                                                b_subAB[3:0] };
                    // A + B clamp
                    2'b10:    blend_rgb_o <= {  r_alpha50[4] ? 4'hF : r_alpha50[3:0],
                                                g_alpha50[4] ? 4'hF : g_alpha50[3:0],
                                                b_alpha50[4] ? 4'hF : b_alpha50[3:0] };
                    // A - B clamp
                    2'b11:    blend_rgb_o <= {  r_subAB[4] ? 4'h0 : r_subAB[3:0],
                                                g_subAB[4] ? 4'h0 : g_subAB[3:0],
                                                b_subAB[4] ? 4'h0 : b_subAB[3:0] };
                endcase
            end else begin
                if (EN_VID_PF_B_BLEND_A8) begin
                    case (colorB_xrgb_i[15:13])
                    // 8/8 A + 0/8 B    (A 100% + B 0%)
                    3'h0:    blend_rgb_o <= colorA_xrgb_i[11:0];
                    // 7/8 A + 1/8 B    (A 87.5% + B 12.5%)
                    3'h1:    blend_rgb_o <= {r_alpha12_5[6:3], g_alpha12_5[6:3], b_alpha12_5[6:3] };
                    // 6/8 A + 2/8 B    (A 75% + B 25%)
                    3'h2:    blend_rgb_o <= { r_alpha25[5:2], g_alpha25[5:2], b_alpha25[5:2] };
                    // 5/8 A + 3/8 B    (A 62.5% + B 37.5%)
                    3'h3:    blend_rgb_o <= { r_alpha50[4:1], g_alpha50[4:1], b_alpha50[4:1] };    // TODO: temp, not right
                    // 4/8 A + 4/8 B    (A 50% + B 50%)
                    3'h4:    blend_rgb_o <= { r_alpha50[4:1], g_alpha50[4:1], b_alpha50[4:1] };
                    // 2/8 A + 6/8 B    (A 50% + B 50%)
                    3'h5:    blend_rgb_o <= { r_alpha75[5:2], g_alpha75[5:2], b_alpha75[5:2] };
                    // 1/8 A + 7/8 B    (A 12.5% + B 87.5%)
                    3'h6:    blend_rgb_o <=  {r_alpha87_5[6:3], g_alpha87_5[6:3], b_alpha87_5[6:3] };
                    // 0/8 A + 8/8 B    (A 0% + B 100%)
                    3'h7:    blend_rgb_o <= colorB_xrgb_i[11:0];
                    endcase
                end else begin
                    case (colorB_xrgb_i[15:14])
                    // 8/8 A + 0/8 B    (A 100% + B 0%)
                    2'h0:    blend_rgb_o <= colorA_xrgb_i[11:0];
                    // 6/8 A + 2/8 B    (A 75% + B 25%)
                    2'h1:    blend_rgb_o <= { r_alpha25[5:2], g_alpha25[5:2], b_alpha25[5:2] };
                    // 4/8 A + 4/8 B    (A 50% + B 50%)
                    2'h2:    blend_rgb_o <= { r_alpha50[4:1], g_alpha50[4:1], b_alpha50[4:1] };
                    // 0/8 A + 8/8 B    (A 0% + B 100%)
                    2'h3:    blend_rgb_o <= colorB_xrgb_i[11:0];
                    endcase
                end
            end
        end else begin
            blend_rgb_o     <= colorA_xrgb_i[11:0];
        end
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
