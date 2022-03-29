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
    input  wire addr_t          audio_0_vol_i,                      // audio chan 0 L+R volume/pan
    input  wire word_t          audio_0_period_i,                     // audio chan 0 playback rate
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

typedef enum logic [2:0] {
    AUD_RAMP        = 3'b000,
    AUD_IDLE        = 3'b100,
    AUD_MIX_0       = 3'b101,
    AUD_RATE_0      = 3'b110,
    AUD_INCR_0      = 3'b111
} audio_mix_st;

byte_t              output_l;   // mixed left channel to output to DAC
byte_t              output_r;   // mixed right channel to output to DAC


logic [2:0]     mix_state;  // mixer state

logic signed [7:0]  mix_l_temp;
logic signed [7:0]  mix_r_temp;
logic signed [7:0]  vol_r_temp;
logic signed [7:0]  vol_l_temp;
logic signed [15:0] mix_l_result;
logic signed [15:0] mix_r_result;
/* verilator lint_off UNUSED */
logic signed [7:0]  result_l;   // mixed left channel to output to DAC
logic signed [7:0]  result_r;   // mixed right channel to output to DAC
/* verilator lint_on UNUSED */

logic               audio_0_odd;
logic               audio_0_nextsamp;
logic               audio_0_restart;
logic               audio_0_fetch;      // flag to do audio DMA next scanline
addr_t              audio_0_addr;       // current sample address
logic [16:0]        audio_0_length;     // audio sample byte length counter (plus underflow flag)
word_t              audio_0_period;     // audio frequency period counter

logic signed [7:0]  audio_0_vol_l;
logic signed [7:0]  audio_0_vol_r;

logic unused_bits;
assign unused_bits = &{ 1'b0, audio_0_period_i[15], mix_l_result[15:14], mix_l_result[5:0], mix_r_result[15:14], mix_r_result[5:0] };

assign audio_0_vol_l = audio_0_vol_i[15:8];
assign audio_0_vol_r = audio_0_vol_i[7:0];

assign audio_0_odd      = audio_0_length[0];
assign audio_0_restart  = audio_0_length[16];
assign audio_0_nextsamp = audio_0_period[15];

assign audio_0_fetch_o  = audio_0_fetch;
assign audio_0_addr_o   = audio_0_addr;

// Thanks to https://www.excamera.com/sphinx/vhdl-clock.html for this timing generation method
localparam                  AUDTIMER_WIDTH  = $clog2((xv::PCLK_HZ/1000)) + 1;   // NOTE: assumes frequencies are multiples of 1000 Hz
logic [AUDTIMER_WIDTH-1:0]  audio_timer;
logic                       audio_strobe;
assign                      audio_strobe    = ~audio_timer[AUDTIMER_WIDTH-1];

always_ff @(posedge clk) begin
    if (reset_i) begin
        mix_state       <= AUD_RAMP;
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
        audio_0_addr    <= '0;      // current address for sample data
`ifndef SYNTHESIS
        audio_0_length  <= 17'h00ff;      // remaining length for sample data (bytes)
`else
        audio_0_length  <= 17'hffff;      // remaining length for sample data (bytes)
`endif
        audio_0_period  <= '0;      // countdown for next sample load
    end else begin

        audio_timer <= audio_timer + (audio_timer[AUDTIMER_WIDTH-1] ? (AUDTIMER_WIDTH)'(xv::AUDIO_MAX_HZ/1000) : (AUDTIMER_WIDTH)'((xv::AUDIO_MAX_HZ/1000) - (xv::PCLK_HZ/1000)));
        audio_0_period   <= audio_0_period - 1'b1;            // decrement audio period counter

        if (1'b0 && !audio_enable) begin
            output_l    <= '0;
            output_r    <= '0;
         end else begin
             case (mix_state)
                AUD_RAMP: begin
                    if (!audio_0_restart) begin
`ifndef SYNTHESIS
                        output_l        <= { 1'b0, ~audio_0_length[7:1] };
                        output_r        <= { 1'b0, ~audio_0_length[7:1] };
`else
                        output_l        <= { 1'b0, ~audio_0_length[15:9] };
                        output_r        <= { 1'b0, ~audio_0_length[15:9] };
`endif
                        if (audio_strobe) begin
                            audio_0_length  <= audio_0_length - 1'b1;
                        end
                    end else begin
                        mix_state   <= AUD_IDLE;
                    end
                end
                AUD_IDLE: begin
                    if (audio_strobe) begin
                        audio_0_fetch   <= 1'b0;                        // clear audio fetch flag

                        mix_l_temp  <= audio_0_odd ? audio_0_word_i[15:8] : audio_0_word_i[7:0];
                        vol_l_temp  <= audio_0_vol_l;

                        mix_r_temp  <= audio_0_odd ? audio_0_word_i[15:8] : audio_0_word_i[7:0];;
                        vol_r_temp  <= audio_0_vol_r;

                        mix_state   <= AUD_MIX_0;
                    end
                end
                AUD_MIX_0: begin
                    // convert to unsigned for DAC
                    result_l        <= mix_l_result[13:6];              // signed result (for simulation)
                    result_r        <= mix_r_result[13:6];
                    output_l        <= mix_l_result[13:6] + 8'h80;      // unsigned result for DAC
                    output_r        <= mix_r_result[13:6] + 8'h80;

                    mix_state       <= AUD_RATE_0;
                end
                AUD_RATE_0: begin
                    if (audio_0_nextsamp) begin                           // if time for a new sample
                        audio_0_period  <= { 1'b0, audio_0_period_i[14:0] };  // reset period counter
                        audio_0_length  <= audio_0_length - 1'b1;

                        mix_state       <= AUD_INCR_0;
                    end else begin
                        mix_state       <= AUD_IDLE;
                    end
                end
                AUD_INCR_0: begin
                    if (audio_0_odd) begin                                  // if odd sample in word, increment address
                        audio_0_addr    <= audio_0_addr + 1'b1;
                        audio_0_fetch   <= 1'b1;                                // fetch new audio data word
                    end
                    if (audio_0_restart) begin                           // if length underflow, reset address and length
                        audio_0_length  <= { 1'b0, audio_0_len_i, 1'b1 };
                        audio_0_addr    <= audio_0_start_i;
                    end
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
