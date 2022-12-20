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

`ifdef EN_PF_B

module video_blend2 (
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
    // add A and B alpha blend result (with bit for overflow)
    result_r    = 8'(outA_r[15:9]) + 8'(outB_r[15:9]);
    result_g    = 8'(outA_g[15:9]) + 8'(outB_g[15:9]);
    result_b    = 8'(outA_b[15:9]) + 8'(outB_b[15:9]);
end

// color RAM lookup (delays video 1 cycle for BRAM)
always_ff @(posedge clk) begin
    // setup pipeline for next pixel
    colorA_r    <= { colorA_xrgb_i[11:8], colorA_xrgb_i[11:8] };
    colorA_g    <= { colorA_xrgb_i[ 7:4], colorA_xrgb_i[ 7:4] };
    colorA_b    <= { colorA_xrgb_i[ 3:0], colorA_xrgb_i[ 3:0] };

    colorB_r    <= { colorB_xrgb_i[11:8], colorB_xrgb_i[11:8] };
    colorB_g    <= { colorB_xrgb_i[ 7:4], colorB_xrgb_i[ 7:4] };
    colorB_b    <= { colorB_xrgb_i[ 3:0], colorB_xrgb_i[ 3:0] };

    if (colorA_xrgb_i[15]) begin
        alphaA  <= 8'hFF;
    end else begin
        alphaA  <= { ~colorB_xrgb_i[15:12], ~colorB_xrgb_i[15:12] };
    end

    if (colorA_xrgb_i[14]) begin
        alphaB  <= 8'h00;
    end else begin
        alphaB  <= { colorB_xrgb_i[15:12], colorB_xrgb_i[15:12] };
    end

    //     case (colorA_xrgb_i[15:14])
    //     2'b00: begin
    //         alphaA  <= { ~colorB_xrgb_i[15:12], ~colorB_xrgb_i[15:12] };
    //         alphaB  <= { colorB_xrgb_i[15:12], colorB_xrgb_i[15:12] };
    //     end
    //     2'b01: begin
    //         alphaA  <= 8'hFF;
    //         alphaB  <= { colorB_xrgb_i[15:12], colorB_xrgb_i[15:12] };
    //     end
    //     2'b10: begin
    //         alphaA  <= 8'hFF;
    //         alphaB  <= { colorB_xrgb_i[15:12], colorB_xrgb_i[15:12] };
    //     end
    //     2'b11: begin
    //         alphaA  <= 8'hFF;;
    //         alphaB  <= 8'h00;
    //     end
    // endcase

    // // remember if clamping or not for next cycle
    // clamp       <= colorA_xrgb_i[15];


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

// NOTE: Using dual 8x8 MAC16 mode
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
    .TOPOUTPUT_SELECT(2'b10),           // 00=add/sub, 01=add/sub registered, 10=8x8 mult, 11=16x16 mult
    .TOPADDSUB_LOWERINPUT(2'b10),       // 00=input A, 01=add/sub registered, 10=8x8 mult, 11=16x16 mult
    .TOPADDSUB_UPPERINPUT(1'b0),        // 0=add/sub accumulate, 1=input C
    .TOPADDSUB_CARRYSELECT(2'b00),      // 00=carry 0, 01=carry 1, 10=lower add/sub ACCUMOUT, 11=lower add/sub CO
    .BOTOUTPUT_SELECT(2'b10),           // 00=add/sub, 01=add/sub registered, 10=8x8 mult, 11=16x16 mult
    .BOTADDSUB_LOWERINPUT(2'b10),       // 00=input A, 01=add/sub registered, 10=8x8 mult, 11=16x16 mult
    .BOTADDSUB_UPPERINPUT(1'b1),        // 0=add/sub accumulate, 1=input D
    .BOTADDSUB_CARRYSELECT(2'b00),      // 00=carry 0, 01=carry 1, 10=lower DSP ACCUMOUT, 11=lower DSP CO
    .MODE_8x8(1'b0),                    // 0=16x16 mode, 1=8x8 mode (low power)
    .A_SIGNED(1'b0),                    // 0=unsigned/1=signed input A
    .B_SIGNED(1'b0)                     // 0=unsigned/1=signed input B
) SB_MAC16_r (
    .CLK(clk),                          // clock
    .CE(1'b1),                          // clock enable
    .A({colorA_r, colorB_r }),                        // 16-bit input A (dual 8-bit mode)
    .B({alphaA, alphaB }),                        // 16-bit input B (dual 8-bit mode)
    .C('0),                             // 16-bit input C
    .D({outA_r[15:12], 12'b0 }),        // 16-bit input D
    .AHOLD(1'b0),                       // 0=load, 1=hold input A
    .BHOLD(1'b0),                       // 0=load, 1=hold input B
    .CHOLD(1'b0),                       // 0=load, 1=hold input C
    .DHOLD(1'b0),                       // 0=load, 1=hold input D
    .IRSTTOP(1'b0),                     // 1=reset input A, C and 8x8 mult upper
    .IRSTBOT(1'b0),                     // 1=reset input A, C and 8x8 mult lower
    .ORSTTOP(1'b0),                     // 1=reset output accumulator upper
    .ORSTBOT(1'b0),                     // 1=reset output accumulator lower
    .OLOADTOP(1'b0),                    // 0=no load/1=load top accumulator from input C
    .OLOADBOT(1'b1),                    // 0=no load/1=load bottom accumulator from input D
    .ADDSUBTOP(1'b0),                   // 0=add/1=sub for top accumulator
    .ADDSUBBOT(1'b0),                   // 0=add/1=sub for bottom accumulator
    .OHOLDTOP(1'b0),                    // 0=load/1=hold into top accumulator
    .OHOLDBOT(1'b0),                    // 0=load/1=hold into bottom accumulator
    .CI(1'b0),                          // cascaded add/sub carry in from previous DSP block
    .ACCUMCI(1'b0),                     // cascaded accumulator carry in from previous DSP block
    .SIGNEXTIN(1'b0),                   // cascaded sign extension in from previous DSP block
    .O({ outA_r, outB_r }),             // 32-bit result output (dual 8x8=16-bit mode with top used)
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
    .TOPOUTPUT_SELECT(2'b10),           // 00=add/sub, 01=add/sub registered, 10=8x8 mult, 11=16x16 mult
    .TOPADDSUB_LOWERINPUT(2'b10),       // 00=input A, 01=add/sub registered, 10=8x8 mult, 11=16x16 mult
    .TOPADDSUB_UPPERINPUT(1'b0),        // 0=add/sub accumulate, 1=input C
    .TOPADDSUB_CARRYSELECT(2'b00),      // 00=carry 0, 01=carry 1, 10=lower add/sub ACCUMOUT, 11=lower add/sub CO
    .BOTOUTPUT_SELECT(2'b10),           // 00=add/sub, 01=add/sub registered, 10=8x8 mult, 11=16x16 mult
    .BOTADDSUB_LOWERINPUT(2'b10),       // 00=input A, 01=add/sub registered, 10=8x8 mult, 11=16x16 mult
    .BOTADDSUB_UPPERINPUT(1'b0),        // 0=add/sub accumulate, 1=input D
    .BOTADDSUB_CARRYSELECT(2'b00),      // 00=carry 0, 01=carry 1, 10=lower DSP ACCUMOUT, 11=lower DSP CO
    .MODE_8x8(1'b0),                    // 0=16x16 mode, 1=8x8 mode (low power)
    .A_SIGNED(1'b0),                    // 0=unsigned/1=signed input A
    .B_SIGNED(1'b0)                     // 0=unsigned/1=signed input B
) SB_MAC16_g (
    .CLK(clk),                          // clock
    .CE(1'b1),                          // clock enable
    .A({colorA_g, colorB_g }),                        // 16-bit input A (dual 8-bit mode)
    .B({alphaA, alphaB }),                        // 16-bit input B (dual 8-bit mode)
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
    .O({ outA_g, outB_g }),             // 32-bit result output (dual 8x8=16-bit mode with top used)
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
    .TOPOUTPUT_SELECT(2'b10),           // 00=add/sub, 01=add/sub registered, 10=8x8 mult, 11=16x16 mult
    .TOPADDSUB_LOWERINPUT(2'b10),       // 00=input A, 01=add/sub registered, 10=8x8 mult, 11=16x16 mult
    .TOPADDSUB_UPPERINPUT(1'b0),        // 0=add/sub accumulate, 1=input C
    .TOPADDSUB_CARRYSELECT(2'b00),      // 00=carry 0, 01=carry 1, 10=lower add/sub ACCUMOUT, 11=lower add/sub CO
    .BOTOUTPUT_SELECT(2'b10),           // 00=add/sub, 01=add/sub registered, 10=8x8 mult, 11=16x16 mult
    .BOTADDSUB_LOWERINPUT(2'b10),       // 00=input A, 01=add/sub registered, 10=8x8 mult, 11=16x16 mult
    .BOTADDSUB_UPPERINPUT(1'b0),        // 0=add/sub accumulate, 1=input D
    .BOTADDSUB_CARRYSELECT(2'b00),      // 00=carry 0, 01=carry 1, 10=lower DSP ACCUMOUT, 11=lower DSP CO
    .MODE_8x8(1'b0),                    // 0=16x16 mode, 1=8x8 mode (low power)
    .A_SIGNED(1'b0),                    // 0=unsigned/1=signed input A
    .B_SIGNED(1'b0)                     // 0=unsigned/1=signed input B
) SB_MAC16_b (
    .CLK(clk),                          // clock
    .CE(1'b1),                          // clock enable
    .A({colorA_b, colorB_b }),                        // 16-bit input A (dual 8-bit mode)
    .B({alphaA, alphaB }),                        // 16-bit input B (dual 8-bit mode)
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
    .O({ outA_b, outB_b }),             // 32-bit result output (dual 8x8=16-bit mode with top used)
    .CO(),                              // cascaded add/sub carry output to next DSP block
    .ACCUMCO(),                         // cascaded accumulator carry output to next DSP block
    .SIGNEXTOUT()                       // cascaded sign extension output to next DSP block
);
/* verilator lint_on PINCONNECTEMPTY */

endmodule

`endif
`default_nettype wire               // restore default
