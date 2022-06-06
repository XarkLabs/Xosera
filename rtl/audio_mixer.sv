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
`ifndef NO_MODULE_PORT_ARRAYS   // Yosys doesn't allow arrays in module ports
    input  wire logic           audio_enable_i[AUDIO_NCHAN],    // channel enabled
    input  wire logic [15:0]    audio_vol_i[AUDIO_NCHAN],       // channel L+R volume/pan
    input  wire logic [14:0]    audio_period_i[AUDIO_NCHAN],    // channel playback rate
    input  wire logic           audio_tile_i[AUDIO_NCHAN],      // channel sample memory (0=VRAM, 1=TILE)
    input  wire addr_t          audio_start_i[AUDIO_NCHAN],     // channel sample start address (in VRAM or TILE)
    input  wire logic [14:0]    audio_len_i[AUDIO_NCHAN],       // channel sample length in words
    input  wire logic           audio_restart_i[AUDIO_NCHAN],   // channel force sample restart
    output      logic           audio_reload_o[AUDIO_NCHAN],    // channel sample reloaded start/addr
`else   // must flatten and pass as vectors
    input  wire logic [AUDIO_NCHAN-1:0]             audio_enable_nchan_i,
    input  wire logic [16*AUDIO_NCHAN-1:0]          audio_vol_nchan_i,
    input  wire logic [15*AUDIO_NCHAN-1:0]          audio_period_nchan_i,
    input  wire logic [AUDIO_NCHAN-1:0]             audio_tile_nchan_i,
    input  wire logic [xv::VRAM_W*AUDIO_NCHAN-1:0]  audio_start_nchan_i,
    input  wire logic [15*AUDIO_NCHAN-1:0]          audio_len_nchan_i,
    input  wire logic [AUDIO_NCHAN-1:0]             audio_restart_nchan_i,
    output      logic [AUDIO_NCHAN-1:0]             audio_reload_nchan_o,
`endif

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

logic               chan_sendout[AUDIO_NCHAN];      // flag when period underflows (next sample output)
logic               chan_restart[AUDIO_NCHAN];      // flag when length underflows (reload sample addr and length)
logic               chan_2nd[AUDIO_NCHAN];
logic               chan_fetch[AUDIO_NCHAN];
logic               chan_tile[AUDIO_NCHAN];         // current sample mem type
addr_t              chan_addr[AUDIO_NCHAN];         // current sample address
word_t              chan_length[AUDIO_NCHAN];       // audio sample byte length counter (15=underflow flag)
word_t              chan_length_n[AUDIO_NCHAN];     // audio sample byte length counter (15=underflow flag)
word_t              chan_period[AUDIO_NCHAN];       // audio frequency period counter (15=underflow flag)
logic signed [7:0]  chan_val[AUDIO_NCHAN];          // current channel value sent to DAC
word_t              chan_buff[AUDIO_NCHAN];         // DMA word buffer
logic               chan_buff_ok[AUDIO_NCHAN];      // DMA buffer has data
logic signed [7:0]  chan_vol_l[AUDIO_NCHAN];
logic signed [7:0]  chan_vol_r[AUDIO_NCHAN];
/* verilator lint_on UNUSED */

logic unused_bits;
assign unused_bits = &{ 1'b0, mix_l_result[15:14], mix_l_result[5:0], mix_r_result[15:14], mix_r_result[5:0] };

`ifdef NO_MODULE_PORT_ARRAYS    // Yosys doesn't allow arrays in module ports
logic           audio_enable_i[AUDIO_NCHAN];
logic [15:0]    audio_vol_i[AUDIO_NCHAN];
logic [14:0]    audio_period_i[AUDIO_NCHAN];
logic           audio_tile_i[AUDIO_NCHAN];
addr_t          audio_start_i[AUDIO_NCHAN];
logic [14:0]    audio_len_i[AUDIO_NCHAN];
logic           audio_restart_i[AUDIO_NCHAN];
logic           audio_reload_o[AUDIO_NCHAN];

// convert flat port vectors into arrays
for (genvar i = 0; i < AUDIO_NCHAN; i = i + 1) begin
    // un-flatten port parameters
    assign audio_enable_i[i]    = audio_enable_nchan_i[i];
    assign audio_vol_i[i]       = audio_vol_nchan_i[i*16+:16];
    assign audio_period_i[i]    = audio_period_nchan_i[i*15+:15];
    assign audio_tile_i[i]      = audio_tile_nchan_i[i];
    assign audio_start_i[i]     = audio_start_nchan_i[i*xv::VRAM_W+:16];
    assign audio_len_i[i]       = audio_len_nchan_i[i*15+:15];
    assign audio_restart_i[i]   = audio_restart_nchan_i[i];
    assign audio_reload_nchan_o[i] = audio_reload_o[i];
end
`endif

// setup alias signals
for (genvar i = 0; i < AUDIO_NCHAN; i = i + 1) begin
    assign chan_vol_l[i]    = { 1'b0, audio_vol_i[i][15:9] };
    assign chan_vol_r[i]    = { 1'b0, audio_vol_i[i][7:1] };
    assign chan_length_n[i] = chan_length[i] - 1'b1;
    assign chan_restart[i]  = chan_length_n[i][15] || audio_restart_i[i];
    assign chan_sendout[i]  = chan_period[i][15] || audio_restart_i[i];
end

always_ff @(posedge clk) begin
    if (reset_i) begin
        fetch_state     <= AUD_DMA_0;

        audio_fetch_o   <= '0;
        audio_tile_o    <= '0;
        audio_addr_o    <= '0;

        for (integer i = 0; i < AUDIO_NCHAN; i = i + 1) begin
            chan_tile[i]    <= '0;          // current mem type
            chan_addr[i]    <= '0;          // current address for sample data
            chan_length[i]  <= '0;          // remaining length for sample data (bytes)
            chan_period[i]  <= '0;          // countdown for next sample load

            chan_fetch[i]   <= '0;
            chan_buff_ok[i] <= '0;
            chan_2nd[i]     <= '0;

            chan_val[i]     <= '0;
            chan_buff[i]    <= '0;
        end

    end else begin
        // loop over all audio channels
        for (integer i = 0; i < AUDIO_NCHAN; i = i + 1) begin
            audio_reload_o[i]   <= 1'b0;        // clear reload strobe
            // decrement period
            if (audio_enable_i[i]) begin
                chan_period[i]      <= chan_period[i] - 1'b1;
            end
            // if period underflowed, output next sample
            if (chan_sendout[i]) begin
                chan_2nd[i]         <= !chan_2nd[i];
                chan_period[i]      <= { !audio_enable_i[i], audio_period_i[i] };
                chan_val[i]         <= chan_buff[i][15:8];
                chan_buff[i][15:8]  <= chan_buff[i][7:0];
`ifndef SYNTHESIS
                chan_buff[i][7]     <= ~chan_buff[i][7];
`endif
                chan_buff_ok[i]     <= !chan_2nd[i];

                // if 2nd sample of sample word, prepare sample address
                if (!chan_2nd[i]) begin
                    chan_fetch[i]           <= audio_enable_i[i];
                    if (chan_restart[i]) begin
                        // if restart, reload sample parameters from registers
                        chan_tile[i]        <= audio_tile_i[i];
                        chan_addr[i]        <= audio_start_i[i];
                        chan_length[i]      <= { 1'b0, audio_len_i[i] };
                        audio_reload_o[i]   <= 1'b1;                        // set reload strobe
                    end else begin
                        // increment sample address, decrement remaining length
                        chan_addr[i]        <= chan_addr[i] + 1'b1;
                        chan_length[i]      <= chan_length_n[i];
                    end
                end
            end

            // silent if disabled
            if (!audio_enable_i[i]) begin
                chan_val[i]     <= '0;
            end
        end

        case (fetch_state)
            // setup DMA fetch for channel (no effect unless chan_fetch set)
            AUD_DMA_0: begin
                    if (chan_fetch[0] && !chan_buff_ok[0]) begin
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
                        chan_buff_ok[0]         <= 1'b1;
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

always_comb begin
    mix_l_result    = mix_l_temp * vol_l_temp;
    mix_r_result    = mix_r_temp * vol_r_temp;
end

always_ff @(posedge clk) begin
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
                output_l        <= { ~mix_l_result[13], mix_l_result[12:6] };      // unsigned result for DAC
                output_r        <= { ~mix_r_result[13], mix_r_result[12:6] };

                mix_state       <= AUD_MULT_0;
            end
            default: begin
                mix_state       <= AUD_MULT_0;
            end
        endcase
    end
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
