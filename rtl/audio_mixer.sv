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

module audio_mixer #(
    parameter AUDIO_NCHAN = 1
)(
    input  wire logic [AUDIO_NCHAN-1:0]             audio_enable_nchan_i,
    input  wire logic [7*AUDIO_NCHAN-1:0]           audio_vol_l_nchan_i,
    input  wire logic [7*AUDIO_NCHAN-1:0]           audio_vol_r_nchan_i,
    input  wire logic [15*AUDIO_NCHAN-1:0]          audio_period_nchan_i,
    input  wire logic [AUDIO_NCHAN-1:0]             audio_tile_nchan_i,
    input  wire logic [xv::VRAM_W*AUDIO_NCHAN-1:0]  audio_start_nchan_i,
    input  wire logic [15*AUDIO_NCHAN-1:0]          audio_len_nchan_i,
    input  wire logic [AUDIO_NCHAN-1:0]             audio_restart_nchan_i,
    output      logic [AUDIO_NCHAN-1:0]             audio_reload_nchan_o,

    output      logic           audio_fetch_o,
    input wire  logic           audio_ack_i,
    output      logic           audio_tile_o,
    output      addr_t          audio_addr_o,
    input       word_t          audio_word_i,

    output      logic           pdm_l_o,
    output      logic           pdm_r_o,

    input wire  logic           reset_i,
    input wire  logic           clk
);

typedef enum logic [1:0] {
    AUD_DMA_0       = 2'h0,
    AUD_READ_0      = 2'h1
} audio_fetch_st;

typedef enum logic [1:0] {
    AUD_MULT_0      = 2'h0,
    AUD_MIX_0       = 2'h1
} audio_mix_st;

byte_t              output_l;   // mixed left channel to output to DAC
byte_t              output_r;   // mixed right channel to output to DAC

/* verilator lint_off UNUSED */
logic [1:0] fetch_state;  // fetch state
logic [1:0] mix_state;  // mixer state

logic signed [7:0]  mix_l_temp;
logic signed [7:0]  mix_r_temp;
logic signed [7:0]  vol_r_temp;
logic signed [7:0]  vol_l_temp;
logic signed [15:0] mix_l_result;
logic signed [15:0] mix_r_result;

logic               chan_restart[AUDIO_NCHAN];      // flag when length underflows (reload sample addr and length)
logic               chan_2nd[AUDIO_NCHAN];
logic               chan_fetch[AUDIO_NCHAN];
logic               chan_tile[AUDIO_NCHAN];         // current sample mem type
addr_t              chan_addr[AUDIO_NCHAN];         // current sample address
word_t              chan_length[AUDIO_NCHAN];       // audio sample byte length counter (15=underflow flag)
word_t              chan_length_n[AUDIO_NCHAN];     // audio sample byte length counter next (15=underflow flag)
word_t              chan_period[AUDIO_NCHAN];       // audio frequency period counter (15=underflow flag)
logic signed [7:0]  chan_val[AUDIO_NCHAN];          // current channel value sent to DAC
word_t              chan_buff[AUDIO_NCHAN];         // DMA word buffer
logic [1:0]         chan_buff_ok[AUDIO_NCHAN];      // DMA buffer has data
logic signed [7:0]  chan_vol_l[AUDIO_NCHAN];
logic signed [7:0]  chan_vol_r[AUDIO_NCHAN];
/* verilator lint_on UNUSED */

logic unused_bits;
assign unused_bits = &{ 1'b0, mix_l_result[15:14], mix_l_result[5:0], mix_r_result[15:14], mix_r_result[5:0] };

// setup alias signals
always_comb begin : alias_block
    for (integer i = 0; i < AUDIO_NCHAN; i = i + 1) begin
        chan_vol_l[i]    = { 1'b0, audio_vol_l_nchan_i[i*7+:7] };    // positive 7 bit signed L volume
        chan_vol_r[i]    = { 1'b0, audio_vol_r_nchan_i[i*7+:7] };    // positive 7 bit signed R volume
        chan_length_n[i] = chan_length[i] - 1'b1;                    // length next cycle
        chan_restart[i]  = chan_length_n[i][15];                     // restart channel if length next cycle will underflow
    end
end

always_ff @(posedge clk) begin : chan_process
    if (reset_i) begin
        fetch_state         <= AUD_DMA_0;

        audio_fetch_o       <= '0;
        audio_tile_o        <= '0;
        audio_addr_o        <= '0;

        for (integer i = 0; i < AUDIO_NCHAN; i = i + 1) begin
            chan_tile[i]        <= '0;          // current mem type
            chan_addr[i]        <= '0;          // current address for sample data
            chan_length[i]      <= '0;          // remaining length for sample data (bytes)
            chan_period[i]      <= '0;          // countdown for next sample load

            chan_fetch[i]       <= '0;
            chan_buff_ok[i]     <= '0;
            chan_2nd[i]         <= '0;

            chan_val[i]         <= '0;
            chan_buff[i]        <= '0;
        end

    end else begin
        // loop over all audio channels
        for (integer i = 0; i < AUDIO_NCHAN; i = i + 1) begin
            audio_reload_nchan_o[i]   <= 1'b0;        // clear reload strobe

            // decrement period
            chan_period[i]      <= chan_period[i] - 1'b1;

            // if period underflowed, output next sample
            if (chan_period[i][15]) begin
                chan_2nd[i]         <= !chan_2nd[i];
                chan_period[i]      <= { 1'b0, audio_period_nchan_i[i*15+:15] };
                chan_val[i]         <= chan_buff[i][15:8];
                chan_buff[i][15:8]  <= chan_buff[i][7:0];
`ifndef SYNTHESIS
                chan_buff[i][7]     <= ~chan_buff[i][7];    // obvious "glitch" to verify not used again
`endif
                chan_buff_ok[i]     <= { chan_buff_ok[i][0], 1'b0 };

                // if 2nd sample of sample word, prepare sample address
                if (chan_2nd[i]) begin
                    chan_fetch[i]           <= 1'b1;
                    if (chan_restart[i]) begin
                        // if restart, reload sample parameters from registers
                        chan_tile[i]        <= audio_tile_nchan_i[i];
                        chan_addr[i]        <= audio_start_nchan_i[i*xv::VRAM_W+:16];
                        chan_length[i]      <= { 1'b0, audio_len_nchan_i[i*15+:15] };
                        audio_reload_nchan_o[i] <= 1'b1;            // set reload strobe
                    end else begin
                        // increment sample address, decrement remaining length
                        chan_addr[i]        <= chan_addr[i] + 1'b1;
                        chan_length[i]      <= chan_length_n[i];
                    end
                end
            end

            if (audio_restart_nchan_i[i] || !audio_enable_nchan_i[i]) begin
                chan_length[i][15]      <= 1'b1;    // force sample addr, tile, len reload
                chan_period[i][15]      <= 1'b1;
                chan_buff_ok[i]         <= 2'b00;   // clear sample buffer status
                chan_2nd[i]             <= 1'b1;    // set 2nd sample to switch next sendout
            end

            if (!audio_enable_nchan_i[i]) begin
                chan_val[i]         <= '0;            // silent if disabled
            end
        end

        case (fetch_state)
            // setup DMA fetch for channel (no effect unless chan_fetch set)
            AUD_DMA_0: begin
                    // HACK: || audio_restart_nchan_i[i];
                    // if (chan_fetch[0] && chan_buff_ok[0] == 2'b00) begin
                    if (chan_fetch[0] && (audio_restart_nchan_i[0] || chan_buff_ok[0] == 2'b00)) begin
                        audio_fetch_o   <= 1'b1;
                    end
                    audio_tile_o    <= chan_tile[0];
                    audio_addr_o    <= chan_addr[0];
                    fetch_state     <= AUD_READ_0;
            end
            // if chan_fetch, wait for ack, latch new word if ack, get ready to mix channel
            AUD_READ_0: begin
                if (audio_fetch_o) begin
                    if (audio_ack_i) begin
                        audio_fetch_o           <= 1'b0;
                        chan_fetch[0]           <= 1'b0;
                        chan_buff[0][15:0]      <= audio_word_i;
                        chan_buff_ok[0]         <= 2'b11;
                        fetch_state             <= AUD_DMA_0;
                    end
                end else begin
                    fetch_state           <= AUD_DMA_0;
                end
            end
            default: begin
                fetch_state       <= AUD_DMA_0;
            end
        endcase
    end
end

always_comb begin : mix_block
    mix_l_result    = mix_l_temp * vol_l_temp;
    mix_r_result    = mix_r_temp * vol_r_temp;
end

always_ff @(posedge clk) begin : mix_fsm
    if (reset_i) begin
        mix_state       <= AUD_MIX_0;

        mix_l_temp      <= '0;
        mix_r_temp      <= '0;
        vol_l_temp      <= '0;
        vol_r_temp      <= '0;
`ifndef SYNTHESIS
        output_l        <= '1;      // HACK: to force full scale display for analog signal view in GTKWave
        output_r        <= '1;
`else
        output_l        <= '0;
        output_r        <= '0;
`endif
    end else begin
        case (mix_state)
            AUD_MULT_0: begin
                mix_l_temp      <= chan_val[0];
                mix_r_temp      <= chan_val[0];
                vol_l_temp      <= chan_vol_l[0];
                vol_r_temp      <= chan_vol_r[0];
                mix_state       <= AUD_MIX_0;
            end
            AUD_MIX_0: begin
                // convert to unsigned for DAC
                if (mix_l_result[14] != mix_l_result[15]) begin
                    output_l        <= mix_l_result[15] ? 8'h00 : 8'hFF;
                end else begin
                    output_l        <= { ~mix_l_result[13], mix_l_result[12:6] };      // unsigned result for DAC
                end
                if (mix_r_result[14] != mix_r_result[15]) begin
                    output_r        <= mix_r_result[15] ? 8'h00 : 8'hFF;
                end else begin
                    output_r        <= { ~mix_r_result[13], mix_r_result[12:6] };
                end
                mix_state       <= AUD_MULT_0;
            end
            default: begin
                mix_state       <= AUD_MULT_0;
            end
        endcase
    end
end

// audio left DAC outout
audio_dac #(
    .WIDTH(8)
) audio_l_dac (
    .value_i(output_l),
    .pulse_o(pdm_l_o),
    .reset_i(reset_i),
    .clk(clk)
);
// audio right DAC outout
audio_dac #(
    .WIDTH(8)
) audio_r_dac (
    .value_i(output_r),
    .pulse_o(pdm_r_o),
    .reset_i(reset_i),
    .clk(clk)
);

endmodule
`default_nettype wire               // restore default
