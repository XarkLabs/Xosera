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
    input       logic           audio_enable,
    input       logic           audio_mix_strobe,

    input  wire addr_t          audio_0_vol_i,                      // audio chan 0 L+R volume/pan
    input  wire word_t          audio_0_rate_i,                     // audio chan 0 playback rate
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
    AUD_IDLE        = 2'b00,
    AUD_MIX_0       = 2'b01,
    AUD_RATE_0      = 2'b10,
    AUD_INCR_0      = 2'b11
} audio_mix_st;

byte_t              output_l;   // mixed left channel to output to DAC
byte_t              output_r;   // mixed right channel to output to DAC

/* verilator lint_off UNUSED */
logic signed [7:0]  result_l;   // mixed left channel to output to DAC
logic signed [7:0]  result_r;   // mixed right channel to output to DAC
/* verilator lint_on UNUSED */

logic [1:0]     mix_state;  // mixer state

logic signed [7:0]  mix_l_temp;
logic signed [7:0]  vol_l_temp;
logic signed [15:0] mix_l_result;
logic signed [7:0]  mix_r_temp;
logic signed [7:0]  vol_r_temp;
logic signed [15:0] mix_r_result;

logic               audio_0_fetch;      // flag to do audio DMA next scanline
addr_t              audio_0_addr;       // current sample address
logic [16:0]        audio_0_length;     // 32768 bytes, plus underflow flag
word_t              audio_0_count;      // audio frequency rate counter

logic signed [7:0]  audio_0_vol_l;
logic signed [7:0]  audio_0_vol_r;

logic unused_bits;
assign unused_bits = &{ 1'b0, mix_l_result[15:14], mix_l_result[5:0], mix_r_result[15:14], mix_r_result[5:0] };

assign audio_0_vol_l = audio_0_vol_i[15:8];
assign audio_0_vol_r = audio_0_vol_i[7:0];

assign audio_0_fetch_o  = audio_0_fetch;
assign audio_0_addr_o   = audio_0_addr;

always_ff @(posedge clk) begin
    if (reset_i) begin
        mix_state       <= AUD_IDLE;
`ifndef SYNTHESIS
        output_l        <= '1;      // hack to force full scale display for analog signal view in GTKWave
        output_r        <= '1;
`else
        output_l        <= '0;
        output_r        <= '0;
`endif
        mix_l_temp      <= '0;
        mix_r_temp      <= '0;
        vol_l_temp      <= '0;
        vol_r_temp      <= '0;
        audio_0_addr    <= '0;
        audio_0_length  <= '0;
        audio_0_count   <= '0;
    end else begin
        if (!audio_enable) begin
            output_l    <= '0;
            output_r    <= '0;
         end else begin
             case (mix_state)
                AUD_IDLE: begin
                    if (audio_mix_strobe) begin
                        mix_l_temp  <= audio_0_length[0] ? audio_0_word_i[15:8] : audio_0_word_i[7:0];
                        vol_l_temp  <= audio_0_vol_l;

                        mix_r_temp  <= audio_0_length[0] ? audio_0_word_i[15:8] : audio_0_word_i[7:0];;
                        vol_r_temp  <= audio_0_vol_r;

                        mix_state   <= AUD_MIX_0;
                    end
                end
                AUD_MIX_0: begin
                    // convert to unsigned for DAC
                    result_l        <= mix_l_result[13:6];              // signed result
                    output_l        <= mix_l_result[13:6] + 8'h80;      // unsigned result for DAC
                    result_r        <= mix_r_result[13:6];
                    output_r        <= mix_r_result[13:6] + 8'h80;

                    audio_0_count   <= audio_0_count - 1'b1;            // decrement rate counter

                    mix_state       <= AUD_RATE_0;
                end
                AUD_RATE_0: begin
                    audio_0_fetch   <= 1'b0;                            // clear audio fetch
                    if (audio_0_count[15]) begin                        // if time for a new sample
                        audio_0_addr    <= audio_0_addr + 1'b1;         // update address
                        audio_0_length  <= audio_0_length - 1'b1;

                        audio_0_count   <= audio_0_rate_i;              // reset rate counter
                        mix_state       <= AUD_INCR_0;
                    end else begin
                        mix_state       <= AUD_IDLE;
                    end
                end
                AUD_INCR_0: begin
                    if (audio_0_length[15]) begin
                        audio_0_length  <= { 1'b0, audio_0_len_i, 1'b0 };
                        audio_0_addr    <= audio_0_start_i;
                    end
                    audio_0_fetch   <= 1'b1;                            // fetch new audio data
                    mix_state       <= AUD_IDLE;
                end
                default:
                    mix_state   <= AUD_IDLE;
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
