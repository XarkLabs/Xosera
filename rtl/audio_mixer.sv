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

`ifdef EN_AUDIO

// TODO: try packed struct

module audio_mixer (
    input  wire logic [AUDIO_NCHAN-1:0]             audio_enable_nchan_i,
    input  wire logic [7*AUDIO_NCHAN-1:0]           audio_vol_l_nchan_i,    // TODO: VOL_W
    input  wire logic [7*AUDIO_NCHAN-1:0]           audio_vol_r_nchan_i,
    input  wire logic [15*AUDIO_NCHAN-1:0]          audio_period_nchan_i,   // TODO: PERIOD_W
    input  wire logic [AUDIO_NCHAN-1:0]             audio_tile_nchan_i,
    input  wire logic [xv::VRAM_W*AUDIO_NCHAN-1:0]  audio_start_nchan_i,
    input  wire logic [15*AUDIO_NCHAN-1:0]          audio_len_nchan_i,      // TODO: LENGTH_W
    input  wire logic [AUDIO_NCHAN-1:0]             audio_restart_nchan_i,
    output      logic [AUDIO_NCHAN-1:0]             audio_reload_nchan_o,

    output      logic           audio_req_o,
    input wire  logic           audio_ack_i,
    output      logic           audio_tile_o,
    output      addr_t          audio_addr_o,
    input       word_t          audio_word_i,

    output      logic           pdm_l_o,
    output      logic           pdm_r_o,

    input wire  logic           reset_i,
    input wire  logic           clk
);

localparam  CHAN_W      = $clog2(AUDIO_NCHAN);
localparam  DAC_W       = 8;
localparam  ACC_W       = 18;
localparam  VOL_SHIFT   = 6;

typedef enum {
    AUD_FETCH_DMA,
    AUD_FETCH_READ
} audio_fetch_ph;

typedef enum {
    AUD_MIX_MULT,
    AUD_MIX_ACCUM
} audio_mix_ph;

logic [CHAN_W-1:0]                  fetch_chan;
logic [CHAN_W-1:0]                  mix_chan;
//logic                               fetch_chan;
//logic                               mix_chan;
audio_fetch_ph                      fetch_phase;
audio_mix_ph                        mix_phase;

sbyte_t                             mix_val_temp;
sbyte_t                             vol_l_temp;
sbyte_t                             vol_r_temp;
sword_t                             mult_l_result;
sword_t                             mult_r_result;
logic signed [ACC_W-1:0]            mix_l_acc;
logic signed [ACC_W-1:0]            mix_r_acc;

logic [DAC_W-1:0]                   output_l;           // mixed left channel to output to DAC (unsigned)
logic [DAC_W-1:0]                   output_r;           // mixed right channel to output to DAC (unsigned)

logic [AUDIO_NCHAN-1:0]             chan_output;        // channel sample output strobe
logic [AUDIO_NCHAN-1:0]             chan_2nd;           // 2nd sample from sample word
logic [AUDIO_NCHAN-1:0]             chan_buff_ok;       // DMA buffer has data
logic [AUDIO_NCHAN-1:0]             chan_fetch;         // channel DMA fetch flag
logic [AUDIO_NCHAN-1:0]             chan_tile;          // current sample memtile flag
logic [8*AUDIO_NCHAN-1:0]           chan_val;           // current channel value sent to DAC
logic [8*AUDIO_NCHAN-1:0]           chan_val2;          // current channel value sent to DAC
logic [xv::VRAM_W*AUDIO_NCHAN-1:0]  chan_addr;          // current sample address
logic [16*AUDIO_NCHAN-1:0]          chan_buff;          // channel DMA word buffer
logic [16*AUDIO_NCHAN-1:0]          chan_length;        // audio sample byte length counter (15=underflow flag)
logic [16*AUDIO_NCHAN-1:0]          chan_period;        // audio frequency period counter (15=underflow flag)

word_t                              chan_length_n[AUDIO_NCHAN];     // audio sample byte length -1 for next cycle (15=underflow flag)

// debug aid signals
`ifndef SYNTHESIS
/* verilator lint_off UNUSED */
byte_t                              chan_raw[AUDIO_NCHAN];          // channel value sent to DAC
byte_t                              chan_raw_u[AUDIO_NCHAN];          // channel value sent to DAC
word_t                              chan_word[AUDIO_NCHAN];         // channel DMA word buffer
addr_t                              chan_ptr[AUDIO_NCHAN];          // channel DMA address
logic [7:0]                         chan_vol_l[AUDIO_NCHAN];
logic [7:0]                         chan_vol_r[AUDIO_NCHAN];
sword_t                             chan_res_l[AUDIO_NCHAN];          // current channel value sent to DAC
sword_t                             chan_res_r[AUDIO_NCHAN];          // current channel value sent to DAC
logic                               chan_restart[AUDIO_NCHAN];
logic signed [ACC_W-1:0]            mix_res_l;
logic signed [ACC_W-1:0]            mix_res_r;
logic [ACC_W-1:0]                   mix_res_l_u;
logic [ACC_W-1:0]                   mix_res_r_u;
/* verilator lint_on UNUSED */
`endif


// setup alias signals
always_comb begin : alias_block
    for (integer i = 0; i < AUDIO_NCHAN; i = i + 1) begin
        chan_length_n[i]    = chan_length[16*i+:16] - 1'b1;         // length next cycle
        chan_output[i]      = chan_period[16*i+15];

        // debug aliases for easy viewing
`ifndef SYNTHESIS
        chan_vol_l[i]       = { 1'b0, audio_vol_l_nchan_i[7*i+:7]};
        chan_vol_r[i]       = { 1'b0, audio_vol_r_nchan_i[7*i+:7]};
        chan_raw[i]         = chan_val[i*8+:8];
        chan_raw_u[i]       = chan_val[i*8+:8] ^ 8'h80;
        chan_ptr[i]         = chan_addr[xv::VRAM_W*i+:xv::VRAM_W] - 1'b1;
        chan_word[i]        = chan_buff[16*i+:16] - 1'b1;
        chan_restart[i]     = audio_reload_nchan_o[i];
`endif
    end
end

always_ff @(posedge clk) begin : chan_process
    if (reset_i) begin
        audio_req_o         <= '0;
        audio_tile_o        <= '0;
        audio_addr_o        <= '0;

        fetch_chan          <= '0;
        fetch_phase         <= AUD_FETCH_DMA;

        chan_val            <= '0;
        chan_val2           <= '0;
        chan_addr           <= '0;
        chan_buff           <= '0;
        chan_period         <= '0;
        chan_length         <= '0;          // remaining length for sample data (bytes)
        chan_buff_ok        <= '0;
        chan_2nd            <= '0;
        chan_fetch          <= '0;
        chan_tile           <= '0;          // current mem type

    end else begin

        // loop over all audio channels
        for (integer i = 0; i < AUDIO_NCHAN; i = i + 1) begin
            audio_reload_nchan_o[i]   <= 1'b0;        // clear reload strobe

            // decrement period
            chan_period[16*i+:16]<= chan_period[16*i+:16] - 1'b1;

            // if period underflowed, output next sample
            if (chan_output[i]) begin
                chan_2nd[i]             <= !chan_2nd[i];
                chan_period[16*i+:16]   <= { 1'b0, audio_period_nchan_i[i*15+:15] };
                chan_val[i*8+:8]        <= chan_2nd[i] ? chan_val2[8*i+:8] : chan_buff[16*i+8+:8];
                chan_val2[i*8+:8]       <= chan_buff[16*i+:8];
                chan_buff_ok[i]         <= 1'b0;
                // if 2nd sample of sample word, prepare sample address
                if (chan_2nd[i]) begin
`ifndef SYNTHESIS
                    chan_val2[8*i+7]    <= ~chan_val2[8*i+7];  // obvious "glitch" to verify not used again
`endif
                    chan_fetch[i]       <= 1'b1;
                    // if length already underflowed, or will next cycle
                    if (chan_length[16*i+15] || chan_length_n[i][15]) begin
                        // if restart, reload sample parameters from registers
                        chan_tile[i]        <= audio_tile_nchan_i[i];
                        chan_addr[i*xv::VRAM_W+:xv::VRAM_W] <= audio_start_nchan_i[i*xv::VRAM_W+:xv::VRAM_W];
                        chan_length[16*i+:16]               <= { 1'b0, audio_len_nchan_i[i*15+:15] };
                        audio_reload_nchan_o[i]             <= 1'b1;            // set reload/ready strobe
                    end else begin
                        // increment sample address, decrement remaining length
                        chan_addr[i*xv::VRAM_W+:xv::VRAM_W] <= chan_addr[i*xv::VRAM_W+:xv::VRAM_W] + 1'b1;
                        chan_length[16*i+:16]               <= chan_length_n[i];
                    end
                end
            end

            if (audio_restart_nchan_i[i]) begin
                chan_length[16*i+15]    <= 1'b1;    // force sample addr, tile, len reload
                chan_period[16*i+15]    <= 1'b1;    // force sample period expire
                chan_buff_ok[i]         <= 1'b0;    // clear sample buffer status
                chan_2nd[i]             <= 1'b1;    // set 2nd sample to switch next sendout
            end

            if (!audio_enable_nchan_i[i]) begin
                chan_length[16*i+15]    <= 1'b1;    // force sample addr, tile, len reload
                chan_period[16*i+15]    <= 1'b1;    // force sample period expire
                chan_2nd[i]             <= 1'b0;    // set 2nd sample to switch next sendout
//                chan_val[i]             <= '0;      // silent if disabled   // TODO: is this desirable?
            end
        end

        case (fetch_phase)
            // setup DMA fetch for channel (no effect unless chan_fetch set)
            AUD_FETCH_DMA: begin
                    audio_req_o         <= 1'b0;
                    if (chan_fetch[fetch_chan] && !chan_buff_ok[fetch_chan]) begin
                        audio_req_o     <= 1'b1;
                    end
                    audio_tile_o    <= chan_tile[fetch_chan];
                    audio_addr_o    <= chan_addr[fetch_chan*xv::VRAM_W+:xv::VRAM_W];
                    fetch_phase     <= AUD_FETCH_READ;
            end
            // if chan_fetch, wait for ack, latch new word if ack, get ready to mix channel
            AUD_FETCH_READ: begin
                if (audio_req_o) begin
                    if (audio_ack_i) begin
                        audio_req_o                     <= 1'b0;
                        chan_fetch[fetch_chan]          <= 1'b0;
                        chan_buff[16*fetch_chan+:16]    <= audio_word_i;
                        chan_buff_ok[fetch_chan]        <= 1'b1;

                        fetch_chan          <= fetch_chan + 1'b1;
                        fetch_phase         <= AUD_FETCH_DMA;
                    end
                end else begin
                    if (AUDIO_NCHAN > 1) begin
                        fetch_chan          <= fetch_chan + 1'b1;
                    end

                    fetch_phase           <= AUD_FETCH_DMA;
                end
            end
        endcase
    end
end

always_comb begin : mix_block
    mult_l_result    = mix_val_temp * vol_l_temp;
    mult_r_result    = mix_val_temp * vol_r_temp;
end

always_ff @(posedge clk) begin : mix_fsm
    if (reset_i) begin
        mix_chan        <= '0;
        mix_phase       <= AUD_MIX_MULT;

        mix_val_temp    <= '0;
        vol_l_temp      <= '0;
        vol_r_temp      <= '0;

        mix_l_acc       <= '0;
        mix_r_acc       <= '0;

`ifndef SYNTHESIS
        output_l        <= '1;      // HACK: to force full scale display for analog signal view in GTKWave
        output_r        <= '1;
`else
        output_l        <= '0;
        output_r        <= '0;
`endif

`ifndef SYNTHESIS
        // reset debug signals
        for (integer i = 0; i < AUDIO_NCHAN; i = i + 1) begin
            chan_res_l[i]   <= '0;
            chan_res_r[i]   <= '0;
            mix_res_l       <= '0;
            mix_res_r       <= '0;
            mix_res_l_u     <= '0;
            mix_res_r_u     <= '0;
        end
`endif

    end else begin
        case (mix_phase)
            AUD_MIX_MULT: begin
                if (mix_chan == 0) begin
`ifndef SYNTHESIS
                    // debug mix result signals
                    mix_res_l   <= mix_l_acc;
                    mix_res_r   <= mix_r_acc;
                    mix_res_l_u <= mix_l_acc ^ (ACC_W'(1'b1) << ACC_W-1);
                    mix_res_r_u <= mix_r_acc ^ (ACC_W'(1'b1) << ACC_W-1);
`endif
                     // clamp and convert to unsigned result for DAC
                    if (mix_l_acc < (-128 <<< VOL_SHIFT)) begin
                        output_l        <= 8'h00;
                    end else if (mix_l_acc > (127 <<< VOL_SHIFT)) begin
                        output_l        <= 8'hFF;
                    end else begin
                        output_l        <= 8'(mix_l_acc >> VOL_SHIFT) ^ 8'h80;
                    end
                    if (mix_r_acc < (-128 <<< VOL_SHIFT)) begin
                        output_r        <= 8'h00;
                    end else if (mix_r_acc > (127 <<< VOL_SHIFT)) begin
                        output_r        <= 8'hFF;
                    end else begin
                        output_r        <= 8'(mix_r_acc >> VOL_SHIFT) ^ 8'h80;
                    end
                    mix_l_acc       <= '0;
                    mix_r_acc       <= '0;
                end
                mix_val_temp    <= chan_val[mix_chan*8+:8];
                vol_l_temp      <= { 1'b0, audio_vol_l_nchan_i[7*mix_chan+:7] };
                vol_r_temp      <= { 1'b0, audio_vol_r_nchan_i[7*mix_chan+:7] };

                mix_phase       <= AUD_MIX_ACCUM;
            end
            AUD_MIX_ACCUM: begin
`ifndef SYNTHESIS
                chan_res_l[mix_chan]  <= mult_l_result;
                chan_res_r[mix_chan]  <= mult_r_result;
`endif
                mix_l_acc       <= mix_l_acc + ACC_W'(mult_l_result);
                mix_r_acc       <= mix_r_acc + ACC_W'(mult_r_result);

                if (AUDIO_NCHAN > 1) begin
                    mix_chan        <= mix_chan + 1'b1;
                end

                mix_phase       <= AUD_MIX_MULT;
            end
        endcase
    end
end

// audio left DAC outout
audio_dac #(
    .WIDTH(DAC_W)
) audio_l_dac (
    .value_i(output_l),
    .pulse_o(pdm_l_o),
    .reset_i(reset_i),
    .clk(clk)
);
// audio right DAC outout
audio_dac #(
    .WIDTH(DAC_W)
) audio_r_dac (
    .value_i(output_r),
    .pulse_o(pdm_r_o),
    .reset_i(reset_i),
    .clk(clk)
);

endmodule

`endif
`default_nettype wire               // restore default
