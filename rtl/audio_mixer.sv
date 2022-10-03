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
    input  wire logic                               audio_enable_i,
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
localparam  CHAN_WPO    = $clog2(AUDIO_NCHAN+1);
localparam  DAC_W       = 8;

typedef enum {
    AUD_FETCH_DMA,
    AUD_FETCH_READ
} audio_fetch_ph;

logic [CHAN_W-1:0]                  fetch_chan;
audio_fetch_ph                      fetch_phase;

logic [CHAN_WPO:0]                  mix_chan;

logic                               mix_clr;            // clear mix accumulator
sbyte_t                             mix_val_temp;
sbyte_t                             vol_l_temp;
sbyte_t                             vol_r_temp;
logic signed [9:0]                  mix_l_acc;          // extra bits for saturation testing
logic signed [9:0]                  mix_r_acc;          // extra bits for saturation testing

logic [DAC_W-1:0]                   output_l;           // mixed left channel to output to DAC (unsigned)
logic [DAC_W-1:0]                   output_r;           // mixed right channel to output to DAC (unsigned)

logic [AUDIO_NCHAN-1:0]             chan_output;        // channel sample output strobe
logic [AUDIO_NCHAN-1:0]             chan_2nd;           // 2nd sample from sample word
logic [AUDIO_NCHAN-1:0]             chan_buff_ok;       // DMA buffer has data
logic [AUDIO_NCHAN-1:0]             chan_tile;          // current sample memtile flag
logic [8*AUDIO_NCHAN-1:0]           chan_val;           // current channel value sent to DAC
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
logic                               chan_restart[AUDIO_NCHAN];
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

always_ff @(posedge clk) begin : chan_process
    if (reset_i) begin
        audio_req_o         <= '0;
        audio_tile_o        <= '0;
        audio_addr_o        <= '0;

        fetch_chan          <= '0;
        fetch_phase         <= AUD_FETCH_DMA;

        chan_val            <= '0;
        chan_addr           <= '0;
        chan_buff           <= '0;
        chan_period         <= '0;
        chan_length         <= '0;          // remaining length for sample data (bytes)
        chan_buff_ok        <= '0;
        chan_2nd            <= '0;
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
                chan_val[i*8+:8]        <= chan_2nd[i] ? chan_buff[16*i+:8] : chan_buff[16*i+8+:8];
                // if 2nd sample of sample word, prepare sample address
                if (chan_2nd[i]) begin
`ifndef SYNTHESIS
                    chan_buff[16*i+:16] <= chan_buff[16*i+:16] ^ 16'h8080;  // obvious "glitch" to verify not used again
`endif
                    chan_buff_ok[i]     <= 1'b0;
                    // if length already underflowed, or will next cycle
                    if (chan_length[16*i+15] || chan_length_n[i][15]) begin
                        // if restart, reload sample parameters from registers
                        chan_tile[i]                        <= audio_tile_nchan_i[i];
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

            if (!audio_enable_i) begin
                chan_length[16*i+15]    <= 1'b1;    // force sample addr, tile, len reload
                chan_period[16*i+15]    <= 1'b1;    // force sample period expire
                chan_2nd[i]             <= 1'b0;    // set 1nd sample for next sendout
            end
        end

        case (fetch_phase)
            // setup DMA fetch for channel
            AUD_FETCH_DMA: begin
                    audio_req_o         <= 1'b0;
                    if (audio_enable_i && !chan_buff_ok[fetch_chan]) begin
                        audio_req_o     <= 1'b1;
                    end
                    audio_tile_o    <= chan_tile[fetch_chan];
                    audio_addr_o    <= chan_addr[fetch_chan*xv::VRAM_W+:xv::VRAM_W];

                    fetch_phase     <= AUD_FETCH_READ;
            end
            // if req, wait for ack, latch new word if ack, get ready to mix channel
            AUD_FETCH_READ: begin
                if (audio_req_o) begin
                    if (audio_ack_i) begin
                        audio_req_o                     <= 1'b0;
                        chan_buff[16*fetch_chan+:16]    <= audio_word_i;
                        chan_buff_ok[fetch_chan]        <= 1'b1;
                        fetch_chan                      <= fetch_chan + 1'b1;

                        fetch_phase         <= AUD_FETCH_DMA;
                    end
                end else begin
                    fetch_chan          <= fetch_chan + 1'b1;

                    fetch_phase         <= AUD_FETCH_DMA;
                end
            end
        endcase
    end
end

always_ff @(posedge clk) begin : mix_fsm
    if (reset_i) begin
        mix_clr         <= '0;

        mix_chan        <= '0;

        mix_val_temp    <= '0;
        vol_l_temp      <= '0;
        vol_r_temp      <= '0;

`ifndef SYNTHESIS
        output_l        <= 8'hFF;      // HACK: to force full scale display for analog signal view in GTKWave
        output_r        <= 8'hFF;
`else
        output_l        <= '0;
        output_r        <= '0;
`endif
    end else begin
        if (mix_chan == AUDIO_NCHAN) begin
            mix_chan        <=  '0;
            mix_clr         <=  1'b1;

            // clamp and convert to unsigned result for DAC
            if (mix_l_acc < -128) begin
                output_l        <= 8'h00;
            end else if (mix_l_acc > 127) begin
                output_l        <= 8'hFF;
            end else begin
                output_l        <= 8'(mix_l_acc) ^ 8'h80;
            end
            if (mix_r_acc < -128) begin
                output_r        <= 8'h00;
            end else if (mix_r_acc > 127) begin
                output_r        <= 8'hFF;
            end else begin
                output_r        <= 8'(mix_r_acc) ^ 8'h80;
            end

        end else begin
            vol_l_temp      <= { 1'b0, audio_vol_l_nchan_i[7*mix_chan+:7] };
            vol_r_temp      <= { 1'b0, audio_vol_r_nchan_i[7*mix_chan+:7] };
            mix_val_temp    <= chan_val[mix_chan*8+:8];
            mix_chan        <= mix_chan + 1'b1;
            mix_clr         <= !audio_enable_i;
        end
    end
end

`ifdef ICE40UP5K
`define USE_FMAC        // use two SB_MAC16 units for multiply and accumulate
`endif

`ifndef USE_FMAC        // generic inferred multiply and fabric accumulate
sword_t                 acc_l;
sword_t                 acc_r;

assign mix_l_acc = acc_l[15:6];
assign mix_r_acc = acc_r[15:6];

always_ff @(posedge clk) begin
    if (mix_clr) begin
        acc_l       <= '0;
        acc_r       <= '0;
    end else begin
        acc_l       <= (mix_val_temp * vol_l_temp) + acc_l;
        acc_r       <= (mix_val_temp * vol_r_temp) + acc_r;
    end
end

`else    // iCE40UltraPlus5K specific

logic [21:0] unused_l;
logic [21:0] unused_r;

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
    .TOPOUTPUT_SELECT(2'b00),           // 00=add/sub, 01=add/sub registered, 10=8x8 mult, 11=16x16 mult
    .TOPADDSUB_LOWERINPUT(2'b10),       // 00=input A, 01=add/sub registered, 10=8x8 mult, 11=16x16 mult
    .TOPADDSUB_UPPERINPUT(1'b0),        // 0=add/sub accumulate, 1=input C
    .TOPADDSUB_CARRYSELECT(2'b00),      // 00=carry 0, 01=carry 1, 10=lower add/sub ACCUMOUT, 11=lower add/sub CO
    .BOTOUTPUT_SELECT(2'b00),           // 00=add/sub, 01=add/sub registered, 10=8x8 mult, 11=16x16 mult
    .BOTADDSUB_LOWERINPUT(2'b10),       // 00=input A, 01=add/sub registered, 10=8x8 mult, 11=16x16 mult
    .BOTADDSUB_UPPERINPUT(1'b0),        // 0=add/sub accumulate, 1=input D
    .BOTADDSUB_CARRYSELECT(2'b00),      // 00=carry 0, 01=carry 1, 10=lower DSP ACCUMOUT, 11=lower DSP CO
    .MODE_8x8(1'b0),                    // 0=16x16 mode, 1=8x8 mode (low power)
    .A_SIGNED(1'b1),                    // 0=unsigned/1=signed input A
    .B_SIGNED(1'b1)                     // 0=unsigned/1=signed input B
) SB_MAC16_l (
    .CLK(clk),                          // clock
    .CE(1'b1),                          // clock enable
    .A({mix_val_temp, 8'h00 }),         // 16-bit input A
    .B({vol_l_temp, 8'h00 }),           // 16-bit input B
    .C('0),                             // 16-bit input C
    .D('0),                             // 16-bit input D
    .AHOLD(1'b0),                       // 0=load, 1=hold input A
    .BHOLD(1'b0),                       // 0=load, 1=hold input B
    .CHOLD(1'b0),                       // 0=load, 1=hold input C
    .DHOLD(1'b0),                       // 0=load, 1=hold input D
    .IRSTTOP(1'b0),                     // 1=reset input A, C and 8x8 mult upper
    .IRSTBOT(1'b0),                     // 1=reset input A, C and 8x8 mult lower
    .ORSTTOP(1'b0),                     // 1=reset output accumulator upper
    .ORSTBOT(1'b1),                     // 1=reset output accumulator lower
    .OLOADTOP(mix_clr),                 // 0=no load/1=load top accumulator from input C
    .OLOADBOT(1'b0),                    // 0=no load/1=load bottom accumulator from input D
    .ADDSUBTOP(1'b0),                   // 0=add/1=sub for top accumulator
    .ADDSUBBOT(1'b0),                   // 0=add/1=sub for bottom accumulator
    .OHOLDTOP(1'b0),                    // 0=load/1=hold into top accumulator
    .OHOLDBOT(1'b0),                    // 0=load/1=hold into bottom accumulator
    .CI(1'b0),                          // cascaded add/sub carry in from previous DSP block
    .ACCUMCI(1'b0),                     // cascaded accumulator carry in from previous DSP block
    .SIGNEXTIN(1'b0),                   // cascaded sign extension in from previous DSP block
    .O({ mix_l_acc, unused_l }),        // 32-bit result output
    .CO(),                              // cascaded add/sub carry output to next DSP block
    .ACCUMCO(),                         // cascaded accumulator carry output to next DSP block
    .SIGNEXTOUT()                       // cascaded sign extension output to next DSP block
);
/* verilator lint_on PINCONNECTEMPTY */

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
    .TOPOUTPUT_SELECT(2'b00),           // 00=add/sub, 01=add/sub registered, 10=8x8 mult, 11=16x16 mult
    .TOPADDSUB_LOWERINPUT(2'b10),       // 00=input A, 01=add/sub registered, 10=8x8 mult, 11=16x16 mult
    .TOPADDSUB_UPPERINPUT(1'b0),        // 0=add/sub accumulate, 1=input C
    .TOPADDSUB_CARRYSELECT(2'b00),      // 00=carry 0, 01=carry 1, 10=lower add/sub ACCUMOUT, 11=lower add/sub CO
    .BOTOUTPUT_SELECT(2'b00),           // 00=add/sub, 01=add/sub registered, 10=8x8 mult, 11=16x16 mult
    .BOTADDSUB_LOWERINPUT(2'b10),       // 00=input A, 01=add/sub registered, 10=8x8 mult, 11=16x16 mult
    .BOTADDSUB_UPPERINPUT(1'b0),        // 0=add/sub accumulate, 1=input D
    .BOTADDSUB_CARRYSELECT(2'b00),      // 00=carry 0, 01=carry 1, 10=lower DSP ACCUMOUT, 11=lower DSP CO
    .MODE_8x8(1'b0),                    // 0=16x16 mode, 1=8x8 mode (low power)
    .A_SIGNED(1'b1),                    // 0=unsigned/1=signed input A
    .B_SIGNED(1'b1)                     // 0=unsigned/1=signed input B
) SB_MAC16_r (
    .CLK(clk),                          // clock
    .CE(1'b1),                          // clock enable
    .A({ mix_val_temp, 8'h00 }),        // 16-bit input A
    .B({ vol_r_temp, 8'h00 }),          // 16-bit input B
    .C('0),                             // 16-bit input C
    .D('0),                             // 16-bit input D
    .AHOLD(1'b0),                       // 0=load, 1=hold input A
    .BHOLD(1'b0),                       // 0=load, 1=hold input B
    .CHOLD(1'b0),                       // 0=load, 1=hold input C
    .DHOLD(1'b0),                       // 0=load, 1=hold input D
    .IRSTTOP(1'b0),                     // 1=reset input A, C and 8x8 mult upper
    .IRSTBOT(1'b0),                     // 1=reset input A, C and 8x8 mult lower
    .ORSTTOP(1'b0),                     // 1=reset output accumulator upper
    .ORSTBOT(1'b1),                     // 1=reset output accumulator lower
    .OLOADTOP(mix_clr),                 // 0=no load/1=load top accumulator from input C
    .OLOADBOT(1'b0),                    // 0=no load/1=load bottom accumulator from input D
    .ADDSUBTOP(1'b0),                   // 0=add/1=sub for top accumulator
    .ADDSUBBOT(1'b0),                   // 0=add/1=sub for bottom accumulator
    .OHOLDTOP(1'b0),                    // 0=load/1=hold into top accumulator
    .OHOLDBOT(1'b0),                    // 0=load/1=hold into bottom accumulator
    .CI(1'b0),                          // cascaded add/sub carry in from previous DSP block
    .ACCUMCI(1'b0),                     // cascaded accumulator carry in from previous DSP block
    .SIGNEXTIN(1'b0),                   // cascaded sign extension in from previous DSP block
    .O({ mix_r_acc, unused_r }),        // 32-bit result output
    .CO(),                              // cascaded add/sub carry output to next DSP block
    .ACCUMCO(),                         // cascaded accumulator carry output to next DSP block
    .SIGNEXTOUT()                       // cascaded sign extension output to next DSP block
);
/* verilator lint_on PINCONNECTEMPTY */

`endif

endmodule

`endif
`default_nettype wire               // restore default
