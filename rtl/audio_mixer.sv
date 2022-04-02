// audio_mixer.sv
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

module audio_mixer(
    input       logic           audio_enable_i,
    input       logic           audio_dma_start_i,
    input       logic           audio_dma_end_i,
    input  wire addr_t          audio_0_vol_i,                      // audio chan 0 L+R volume/pan
    input  wire logic [14:0]    audio_0_period_i,                     // audio chan 0 playback rate
    input  wire addr_t          audio_0_start_i,                     // audio chan 0 sample start address (in VRAM or TILE)
    input  wire logic [14:0]    audio_0_len_i,                      // audio chan 0 sample length in words

    output      logic           audio_0_fetch_o,
    output      addr_t          audio_0_addr_o,
    input       word_t          audio_0_word_i,

    output      logic           pdm_l_o,
    output      logic           pdm_r_o,

    input wire  logic           reset_i,
    input wire  logic           clk
);

typedef enum logic [1:0] {
    AUD_MIX_0       = 2'b00,
    AUD_MIX_1       = 2'b01
} audio_mix_st;

byte_t              output_l;   // mixed left channel to output to DAC
byte_t              output_r;   // mixed right channel to output to DAC

/* verilator lint_off UNUSED */
logic [1:0]     mix_state;  // mixer state

logic signed [7:0]  mix_l_temp;
logic signed [7:0]  mix_r_temp;
logic signed [7:0]  vol_r_temp;
logic signed [7:0]  vol_l_temp;
logic signed [15:0] mix_l_result;
logic signed [15:0] mix_r_result;
logic signed [7:0]  result_l;   // mixed left channel to output to DAC
logic signed [7:0]  result_r;   // mixed right channel to output to DAC
logic signed [7:0]  chan0_vol_l;
logic signed [7:0]  chan0_vol_r;


logic               chan0_sendout;
logic               chan0_restart;
logic               chan0_2nd;
logic               chan0_fetch;
addr_t              chan0_addr;       // current sample address
word_t              chan0_length;     // audio sample byte length counter (15=underflow flag)
word_t              chan0_length_n;   // audio sample byte length counter (15=underflow flag)
word_t              chan0_period;     // audio frequency period counter (15=underflow flag)
word_t              chan0_period_n;   // audio frequency period counter (15=underflow flag)
logic signed [7:0]  chan0_val;        // current channel value sent to DAC
word_t              chan0_word0;       // current audio word being sent to DAC
word_t              chan0_word1;       // buffered audio word being fetched from memory
word_t              chan0_word2;       // buffered audio word being fetched from memory
logic               chan0_word0_ok;       // current audio word being sent to DAC
logic               chan0_word1_ok;       // buffered audio word being fetched from memory
logic               chan0_word2_ok;       // buffered audio word being fetched from memory
/* verilator lint_on UNUSED */

logic unused_bits;
assign unused_bits = &{ 1'b0, mix_l_result[15:14], mix_l_result[5:0], mix_r_result[15:14], mix_r_result[5:0] };

assign chan0_vol_l = audio_0_vol_i[15:8];
assign chan0_vol_r = audio_0_vol_i[7:0];

assign chan0_length_n = chan0_length - 1'b1;
assign chan0_restart  = chan0_length_n[15];
assign chan0_period_n = chan0_period - 1'b1;
assign chan0_sendout  = chan0_period[15];

assign audio_0_addr_o   = chan0_addr;
assign audio_0_fetch_o  = chan0_fetch;

always_ff @(posedge clk) begin
    if (reset_i) begin
        mix_state       <= AUD_MIX_0;
`ifndef SYNTHESIS
        output_l        <= '1;      // HACK: to force full scale display for analog signal view in GTKWave
        output_r        <= '1;
`else
        output_l        <= '0;
        output_r        <= '0;
`endif
        chan0_val       <= '0;
        chan0_word0     <= '0;
        chan0_word1     <= '0;
        chan0_word2     <= '0;
        chan0_word0_ok  <= '0;
        chan0_word1_ok  <= '0;
        chan0_word2_ok  <= '0;
        mix_l_temp      <= '0;
        mix_r_temp      <= '0;
        vol_l_temp      <= '0;
        vol_r_temp      <= '0;
        chan0_fetch     <= '0;
        chan0_addr      <= '0;      // current address for sample data
        chan0_length    <= '0;      // remaining length for sample data (bytes)
        chan0_period    <= '1;      // countdown for next sample load
        chan0_2nd       <= '0;
    end else begin

        if (!audio_enable_i) begin
            output_l    <= chan0_vol_l;
            output_r    <= chan0_vol_r;
         end else begin

            if (audio_dma_start_i) begin
                if (!chan0_word2_ok) begin
                    chan0_fetch <= 1'b1;
                end
            end

            if (audio_dma_end_i) begin
                if (chan0_fetch) begin
                    chan0_word2     <= chan0_word1;
                    chan0_word2_ok  <= chan0_word1_ok;
                    chan0_word1     <= chan0_word0;
                    chan0_word1_ok  <= chan0_word0_ok;
                    chan0_word0     <= audio_0_word_i;
                    chan0_word0_ok  <= 1'b1;
                    chan0_fetch     <= 1'b0;

                    if (chan0_restart) begin
                        chan0_addr    <= audio_0_start_i;
                        chan0_length  <= { 1'b0, audio_0_len_i };
                    end else begin
                        chan0_addr    <= chan0_addr + 1'b1;
                        chan0_length  <= chan0_length_n;
                    end
                end
            end

            if (chan0_sendout) begin                                 // if time to output a new sample
                chan0_val       <= chan0_word2[15:8];
                chan0_word2[15:8] <= chan0_word2[7:0];
`ifndef NO_GLITCH // TODO: remove "glitch enhancer"
                chan0_word2[7] <= ~chan0_word2[7];
`endif
                chan0_2nd       <= !chan0_2nd;
                chan0_word2_ok  <= chan0_word2_ok && !chan0_2nd;
                chan0_period    <= { 1'b0, audio_0_period_i[14:0] };  // reset period counter
            end else begin
                chan0_period  <= chan0_period_n;                 // decrement audio period counter
            end

            case (mix_state)
                AUD_MIX_0: begin

                    mix_l_temp      <= chan0_val;
                    vol_l_temp      <= chan0_vol_l;

                    mix_r_temp      <= chan0_val;
                    vol_r_temp      <= chan0_vol_r;

                    mix_state       <= AUD_MIX_1;
                end
                AUD_MIX_1: begin
                    // convert to unsigned for DAC
                    result_l        <= mix_l_result[13:6];              // signed result (for simulation)
                    result_r        <= mix_r_result[13:6];
                    output_l        <= mix_l_result[13:6] + 8'h80;      // unsigned result for DAC
                    output_r        <= mix_r_result[13:6] + 8'h80;

                    mix_state       <= AUD_MIX_0;
                end
                default:
                    mix_state       <= AUD_MIX_0;
             endcase
         end
    end
end

always_comb begin
    mix_l_result    = mix_l_temp * vol_l_temp;
    mix_r_result    = mix_r_temp * vol_r_temp;
end

// audio left DAC outout
audio_dac#(
    .WIDTH(8)
) audio_l_dac (
    .value_i(output_l),
    .pulse_o(pdm_l_o),
    .reset_i(reset_i),
    .clk(clk)
);
// audio right DAC outout
audio_dac#(
    .WIDTH(8)
) audio_r_dac (
    .value_i(output_r),
    .pulse_o(pdm_r_o),
    .reset_i(reset_i),
    .clk(clk)
);

endmodule
`default_nettype wire               // restore default
