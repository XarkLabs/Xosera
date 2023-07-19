// video_blend2.sv - full 4-bit blending (low/high precision)
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

module video_blend_4bit (
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

byte_t          colorA_r;           // color A red
byte_t          colorA_g;           // color A green
byte_t          colorA_b;           // color A blue

byte_t          colorB_r;           // color B red
byte_t          colorB_g;           // color B green
byte_t          colorB_b;           // color B blue

byte_t          alphaA;             // alpha for A blend
byte_t          alphaB;             // alpha for B blend

word_t          outA_r;             // colorA_r * alphaA result
word_t          outA_g;             // colorA_g * alphaA result
word_t          outA_b;             // colorA_g * alphaA result

word_t          outB_r;             // colorB_r * alphaB result
word_t          outB_g;             // colorB_g * alphaB result
word_t          outB_b;             // colorB_g * alphaB result

byte_t          result_r;           // result of A red + B red (with overlow bit)
byte_t          result_g;           // result of A green + B green (with overlow bit)
byte_t          result_b;           // result of A blue + B blue (with overlow bit)

logic unused_signals    = &{    1'b0, colorA_xrgb_i[13:12],
                                outA_r[8:0], outA_g[8:0], outA_b[8:0],
                                outB_r[8:0], outB_g[8:0], outB_b[8:0],
                                result_r[2:0], result_g[2:0], result_b[2:0] };

always_comb begin
    // add A and B 7-bit blend result (result high bit used to detect overflow)
    result_r    = 8'(outA_r[15:9]) + 8'(outB_r[15:9]);
    result_g    = 8'(outA_g[15:9]) + 8'(outB_g[15:9]);
    result_b    = 8'(outA_b[15:9]) + 8'(outB_b[15:9]);
end

// color blending (delays video 1 additional cycle for MAC16)
always_ff @(posedge clk) begin
    // setup DSP input for next pixel
    colorA_r    <= { colorA_xrgb_i[11:8], colorA_xrgb_i[11:8] };
    colorA_g    <= { colorA_xrgb_i[ 7:4], colorA_xrgb_i[ 7:4] };
    colorA_b    <= { colorA_xrgb_i[ 3:0], colorA_xrgb_i[ 3:0] };

    colorB_r    <= { colorB_xrgb_i[11:8], colorB_xrgb_i[11:8] };
    colorB_g    <= { colorB_xrgb_i[ 7:4], colorB_xrgb_i[ 7:4] };
    colorB_b    <= { colorB_xrgb_i[ 3:0], colorB_xrgb_i[ 3:0] };

    // use color A  [15] to select alphaA to be either 1-alphaB or 1
    if (colorA_xrgb_i[15]) begin
        alphaA  <= 8'hFF;
    end else begin
        alphaA  <= { ~colorB_xrgb_i[15:12], ~colorB_xrgb_i[15:12] };
    end

    // use color A  [14] to select alphaB to be either alphaB or 0
    if (colorA_xrgb_i[14]) begin
        alphaB  <= 8'h00;
    end else begin
        alphaB  <= { colorB_xrgb_i[15:12], colorB_xrgb_i[15:12] };
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

    // force black if display enable was off
    if (dv_de_2) begin
        blend_rgb_o <=  {   result_r[7] ? 4'hF : result_r[6:3],
                            result_g[7] ? 4'hF : result_g[6:3],
                            result_b[7] ? 4'hF : result_b[6:3] };
    end else begin
        blend_rgb_o <= '0;
    end
end

`ifndef ICE40UP5K   // if no MAC16 primitives
`ifndef EN_BLEND_FULL
// use 4-bit precision blending (with inferred multiply) to save logic
logic unused_low_bits = &{1'b0, colorA_r[3:0], colorA_g[3:0], colorA_b[3:0], colorB_r[3:0], colorB_g[3:0], colorB_b[3:0], alphaA[3:0], alphaB[3:0]};
always_comb begin
    if (alphaA[7:4] == 4'hF) begin  // special case 100% alpha
        outA_r  =  colorA_r[7:4] << 12;
        outA_g  =  colorA_g[7:4] << 12;
        outA_b  =  colorA_b[7:4] << 12;
    end else begin
        outA_r  =  (colorA_r[7:4] * alphaA[7:4]) << 8;
        outA_g  =  (colorA_g[7:4] * alphaA[7:4]) << 8;
        outA_b  =  (colorA_b[7:4] * alphaA[7:4]) << 8;
    end
    if (alphaB[7:4] == 4'hF) begin  // special case 100% alpha
        outB_r  =  colorB_r[7:4] << 12;
        outB_g  =  colorB_g[7:4] << 12;
        outB_b  =  colorB_b[7:4] << 12;
    end else begin
        outB_r  =  (colorB_r[7:4] * alphaB[7:4]) << 8;
        outB_g  =  (colorB_g[7:4] * alphaB[7:4]) << 8;
        outB_b  =  (colorB_b[7:4] * alphaB[7:4]) << 8;
    end
end
`else// full 8-bit precision blending (with inferred multiply)
always_comb begin
    outA_r  =  colorA_r * alphaA;
    outA_g  =  colorA_g * alphaA;
    outA_b  =  colorA_b * alphaA;

    outB_r  =  colorB_r * alphaB;
    outB_g  =  colorB_g * alphaB;
    outB_b  =  colorB_b * alphaB;
end
`endif
`else
// 8-bit precision blending using dual 8x8 MAC16 on iCE40UP5K
/* verilator lint_off PINCONNECTEMPTY */
SB_MAC16 #(
    .NEG_TRIGGER(1'b0),                 // 0=rising/1=falling clk edge
    .C_REG(1'b0),                       // 1=register input C
    .A_REG(1'b0),                       // 1=register input A
    .B_REG(1'b0),                       // 1=register input B
    .D_REG(1'b0),                       // 1=register input D
    .TOP_8x8_MULT_REG(1'b0),            // 1=register top 8x8 output
    .BOT_8x8_MULT_REG(1'b0),            // 1=register bot 8x8 output
    .PIPELINE_16x16_MULT_REG1(1'b0),    // 1=register reg1 16x16 output
    .PIPELINE_16x16_MULT_REG2(1'b0),    // 1=register reg2 16x16 output
    .TOPOUTPUT_SELECT(2'b10),           // 00=add/sub, 01=add/sub registered, 10=8x8 mult (used), 11=16x16 mult
    .TOPADDSUB_LOWERINPUT(2'b10),       // 00=input A, 01=add/sub registered, 10=8x8 mult (used), 11=16x16 mult
    .TOPADDSUB_UPPERINPUT(1'b0),        // 0=add/sub accumulate, 1=input C
    .TOPADDSUB_CARRYSELECT(2'b00),      // 00=carry 0, 01=carry 1, 10=lower add/sub ACCUMOUT, 11=lower add/sub CO
    .BOTOUTPUT_SELECT(2'b10),           // 00=add/sub, 01=add/sub registered, 10=8x8 mult (used), 11=16x16 mult
    .BOTADDSUB_LOWERINPUT(2'b10),       // 00=input A, 01=add/sub registered, 10=8x8 mult (used), 11=16x16 mult
    .BOTADDSUB_UPPERINPUT(1'b0),        // 0=add/sub accumulate, 1=input D
    .BOTADDSUB_CARRYSELECT(2'b00),      // 00=carry 0, 01=carry 1, 10=lower DSP ACCUMOUT, 11=lower DSP CO
    .MODE_8x8(1'b0),                    // 0=16x16 mode, 1=8x8 mode (low power)
    .A_SIGNED(1'b0),                    // 0=unsigned/1=signed input A
    .B_SIGNED(1'b0)                     // 0=unsigned/1=signed input B
) SB_MAC16_r (
    .CLK(clk),                          // clock
    .CE(1'b1),                          // clock enable
    .A({colorA_r, colorB_r }),          // 16-bit input A (using dual 8-bit mode)
    .B({alphaA, alphaB }),              // 16-bit input B (using dual 8-bit mode)
    .C('0),                             // 16-bit input C
    .D('0),                             // 16-bit input D
    .AHOLD(1'b0),                       // 0=load, 1=hold input A
    .BHOLD(1'b0),                       // 0=load, 1=hold input B
    .CHOLD(1'b0),                       // 0=load, 1=hold input C
    .DHOLD(1'b0),                       // 0=load, 1=hold input D
    .IRSTTOP(1'b0),                     // 1=reset input A, C and 8x8 mult upper
    .IRSTBOT(1'b0),                     // 1=reset input A, C and 8x8 mult lower
    .ORSTTOP(1'b0),                     // 1=reset output accumulator upper
    .ORSTBOT(1'b0),                     // 1=reset output accumulator lower
    .OLOADTOP(1'b0),                    // 0=no load/1=load top accumulator from input C
    .OLOADBOT(1'b0),                    // 0=no load/1=load bottom accumulator from input D
    .ADDSUBTOP(1'b0),                   // 0=add/1=sub for top accumulator
    .ADDSUBBOT(1'b0),                   // 0=add/1=sub for bottom accumulator
    .OHOLDTOP(1'b0),                    // 0=load/1=hold into top accumulator
    .OHOLDBOT(1'b0),                    // 0=load/1=hold into bottom accumulator
    .CI(1'b0),                          // cascaded add/sub carry in from previous DSP block
    .ACCUMCI(1'b0),                     // cascaded accumulator carry in from previous DSP block
    .SIGNEXTIN(1'b0),                   // cascaded sign extension in from previous DSP block
    .O({ outA_r, outB_r }),             // 32-bit result output (using both 8x8=16-bit results)
    .CO(),                              // cascaded add/sub carry output to next DSP block
    .ACCUMCO(),                         // cascaded accumulator carry output to next DSP block
    .SIGNEXTOUT()                       // cascaded sign extension output to next DSP block
);

SB_MAC16 #(
    .NEG_TRIGGER(1'b0),                 // 0=rising/1=falling clk edge
    .C_REG(1'b0),                       // 1=register input C
    .A_REG(1'b0),                       // 1=register input A
    .B_REG(1'b0),                       // 1=register input B
    .D_REG(1'b0),                       // 1=register input D
    .TOP_8x8_MULT_REG(1'b0),            // 1=register top 8x8 output
    .BOT_8x8_MULT_REG(1'b0),            // 1=register bot 8x8 output
    .PIPELINE_16x16_MULT_REG1(1'b0),    // 1=register reg1 16x16 output
    .PIPELINE_16x16_MULT_REG2(1'b0),    // 1=register reg2 16x16 output
    .TOPOUTPUT_SELECT(2'b10),           // 00=add/sub, 01=add/sub registered, 10=8x8 mult (used), 11=16x16 mult
    .TOPADDSUB_LOWERINPUT(2'b10),       // 00=input A, 01=add/sub registered, 10=8x8 mult (used), 11=16x16 mult
    .TOPADDSUB_UPPERINPUT(1'b0),        // 0=add/sub accumulate, 1=input C
    .TOPADDSUB_CARRYSELECT(2'b00),      // 00=carry 0, 01=carry 1, 10=lower add/sub ACCUMOUT, 11=lower add/sub CO
    .BOTOUTPUT_SELECT(2'b10),           // 00=add/sub, 01=add/sub registered, 10=8x8 mult (used), 11=16x16 mult
    .BOTADDSUB_LOWERINPUT(2'b10),       // 00=input A, 01=add/sub registered, 10=8x8 mult (used), 11=16x16 mult
    .BOTADDSUB_UPPERINPUT(1'b0),        // 0=add/sub accumulate, 1=input D
    .BOTADDSUB_CARRYSELECT(2'b00),      // 00=carry 0, 01=carry 1, 10=lower DSP ACCUMOUT, 11=lower DSP CO
    .MODE_8x8(1'b0),                    // 0=16x16 mode, 1=8x8 mode (low power)
    .A_SIGNED(1'b0),                    // 0=unsigned/1=signed input A
    .B_SIGNED(1'b0)                     // 0=unsigned/1=signed input B
) SB_MAC16_g (
    .CLK(clk),                          // clock
    .CE(1'b1),                          // clock enable
    .A({colorA_g, colorB_g }),          // 16-bit input A (using dual 8-bit mode)
    .B({alphaA, alphaB }),              // 16-bit input B (using dual 8-bit mode)
    .C('0),                             // 16-bit input C
    .D('0),                             // 16-bit input D
    .AHOLD(1'b0),                       // 0=load, 1=hold input A
    .BHOLD(1'b0),                       // 0=load, 1=hold input B
    .CHOLD(1'b0),                       // 0=load, 1=hold input C
    .DHOLD(1'b0),                       // 0=load, 1=hold input D
    .IRSTTOP(1'b0),                     // 1=reset input A, C and 8x8 mult upper
    .IRSTBOT(1'b0),                     // 1=reset input A, C and 8x8 mult lower
    .ORSTTOP(1'b0),                     // 1=reset output accumulator upper
    .ORSTBOT(1'b0),                     // 1=reset output accumulator lower
    .OLOADTOP(1'b0),                    // 0=no load/1=load top accumulator from input C
    .OLOADBOT(1'b0),                    // 0=no load/1=load bottom accumulator from input D
    .ADDSUBTOP(1'b0),                   // 0=add/1=sub for top accumulator
    .ADDSUBBOT(1'b0),                   // 0=add/1=sub for bottom accumulator
    .OHOLDTOP(1'b0),                    // 0=load/1=hold into top accumulator
    .OHOLDBOT(1'b0),                    // 0=load/1=hold into bottom accumulator
    .CI(1'b0),                          // cascaded add/sub carry in from previous DSP block
    .ACCUMCI(1'b0),                     // cascaded accumulator carry in from previous DSP block
    .SIGNEXTIN(1'b0),                   // cascaded sign extension in from previous DSP block
    .O({ outA_g, outB_g }),             // 32-bit result output (using both 8x8=16-bit results)
    .CO(),                              // cascaded add/sub carry output to next DSP block
    .ACCUMCO(),                         // cascaded accumulator carry output to next DSP block
    .SIGNEXTOUT()                       // cascaded sign extension output to next DSP block
);

SB_MAC16 #(
    .NEG_TRIGGER(1'b0),                 // 0=rising/1=falling clk edge
    .C_REG(1'b0),                       // 1=register input C
    .A_REG(1'b0),                       // 1=register input A
    .B_REG(1'b0),                       // 1=register input B
    .D_REG(1'b0),                       // 1=register input D
    .TOP_8x8_MULT_REG(1'b0),            // 1=register top 8x8 output
    .BOT_8x8_MULT_REG(1'b0),            // 1=register bot 8x8 output
    .PIPELINE_16x16_MULT_REG1(1'b0),    // 1=register reg1 16x16 output
    .PIPELINE_16x16_MULT_REG2(1'b0),    // 1=register reg2 16x16 output
    .TOPOUTPUT_SELECT(2'b10),           // 00=add/sub, 01=add/sub registered, 10=8x8 mult (used), 11=16x16 mult
    .TOPADDSUB_LOWERINPUT(2'b10),       // 00=input A, 01=add/sub registered, 10=8x8 mult (used), 11=16x16 mult
    .TOPADDSUB_UPPERINPUT(1'b0),        // 0=add/sub accumulate, 1=input C
    .TOPADDSUB_CARRYSELECT(2'b00),      // 00=carry 0, 01=carry 1, 10=lower add/sub ACCUMOUT, 11=lower add/sub CO
    .BOTOUTPUT_SELECT(2'b10),           // 00=add/sub, 01=add/sub registered, 10=8x8 mult (used), 11=16x16 mult
    .BOTADDSUB_LOWERINPUT(2'b10),       // 00=input A, 01=add/sub registered, 10=8x8 mult (used), 11=16x16 mult
    .BOTADDSUB_UPPERINPUT(1'b0),        // 0=add/sub accumulate, 1=input D
    .BOTADDSUB_CARRYSELECT(2'b00),      // 00=carry 0, 01=carry 1, 10=lower DSP ACCUMOUT, 11=lower DSP CO
    .MODE_8x8(1'b0),                    // 0=16x16 mode, 1=8x8 mode (low power)
    .A_SIGNED(1'b0),                    // 0=unsigned/1=signed input A
    .B_SIGNED(1'b0)                     // 0=unsigned/1=signed input B
) SB_MAC16_b (
    .CLK(clk),                          // clock
    .CE(1'b1),                          // clock enable
    .A({colorA_b, colorB_b }),          // 16-bit input A (using dual 8-bit mode)
    .B({alphaA, alphaB }),              // 16-bit input B (using dual 8-bit mode)
    .C('0),                             // 16-bit input C
    .D('0),                             // 16-bit input D
    .AHOLD(1'b0),                       // 0=load, 1=hold input A
    .BHOLD(1'b0),                       // 0=load, 1=hold input B
    .CHOLD(1'b0),                       // 0=load, 1=hold input C
    .DHOLD(1'b0),                       // 0=load, 1=hold input D
    .IRSTTOP(1'b0),                     // 1=reset input A, C and 8x8 mult upper
    .IRSTBOT(1'b0),                     // 1=reset input A, C and 8x8 mult lower
    .ORSTTOP(1'b0),                     // 1=reset output accumulator upper
    .ORSTBOT(1'b0),                     // 1=reset output accumulator lower
    .OLOADTOP(1'b0),                    // 0=no load/1=load top accumulator from input C
    .OLOADBOT(1'b0),                    // 0=no load/1=load bottom accumulator from input D
    .ADDSUBTOP(1'b0),                   // 0=add/1=sub for top accumulator
    .ADDSUBBOT(1'b0),                   // 0=add/1=sub for bottom accumulator
    .OHOLDTOP(1'b0),                    // 0=load/1=hold into top accumulator
    .OHOLDBOT(1'b0),                    // 0=load/1=hold into bottom accumulator
    .CI(1'b0),                          // cascaded add/sub carry in from previous DSP block
    .ACCUMCI(1'b0),                     // cascaded accumulator carry in from previous DSP block
    .SIGNEXTIN(1'b0),                   // cascaded sign extension in from previous DSP block
    .O({ outA_b, outB_b }),             // 32-bit result output (using both 8x8=16-bit results)
    .CO(),                              // cascaded add/sub carry output to next DSP block
    .ACCUMCO(),                         // cascaded accumulator carry output to next DSP block
    .SIGNEXTOUT()                       // cascaded sign extension output to next DSP block
);
/* verilator lint_on PINCONNECTEMPTY */
`endif

endmodule

`endif
`default_nettype wire               // restore default
