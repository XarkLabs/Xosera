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

`ifdef ICE40UP5K
`define USE_FMAC        // use two SB_MAC16 units for multiply and accumulate
`endif

module audio_mixer_slim (
    input wire  logic                           audio_enable_i,

    input wire  logic                           audio_reg_wr_i,
    input wire  logic [xv::CHAN_W+2-1:0]        audio_reg_addr_i,   // CHAN_W + 2 bits reg per channe
    input       word_t                          audio_reg_data_i,

    input  wire logic [7*xv::AUDIO_NCHAN-1:0]   audio_vol_l_nchan_i,
    input  wire logic [7*xv::AUDIO_NCHAN-1:0]   audio_vol_r_nchan_i,
    input  wire logic [15*xv::AUDIO_NCHAN-1:0]  audio_period_nchan_i,
    input  wire logic [xv::AUDIO_NCHAN-1:0]     audio_restart_nchan_i,
    output      logic [xv::AUDIO_NCHAN-1:0]     audio_reload_nchan_o,

    output      logic                           audio_dma_vram_req_o,
    output      logic                           audio_dma_tile_req_o,
    output      addr_t                          audio_dma_addr_o,
    input wire  logic                           audio_dma_ack_i,
    input       word_t                          audio_dma_word_i,

    output      logic                           pdm_l_o,
    output      logic                           pdm_r_o,

    input wire  logic                           reset_i,
    input wire  logic                           clk
);

localparam  DAC_W       = 8;                            // DAC output bits

// output DAC signals
logic [DAC_W-1:0]                   output_l;           // mixed left channel to output to DAC (unsigned)
logic [DAC_W-1:0]                   output_r;           // mixed right channel to output to DAC (unsigned)

// channel mixing signals
logic [xv::CHAN_W:0]                mix_chan;           // NOTE: extra "channel" used to clamp, output and clear
logic                               mix_clr;            // clear L & R mix accumulator strobe
sbyte_t                             value_temp;         // sample value for FMAC attenuation
sbyte_t                             vol_l_temp;         // left volume for FMAC attenuation
sbyte_t                             vol_r_temp;         // right volume for FMAC attenuation
sword_t                             acc_l;              // left FMAC accumulator output
sword_t                             acc_r;              // right FMAC accumulator output
logic signed [9:0]                  mix_l_acc;          // left FMAC unclamped result (upper bits of acc_l)
logic signed [9:0]                  mix_r_acc;          // right FMAC unclamped result (upper bits of acc_r)

// sample fetch signals
typedef enum {
    AUD_CHECK,
    AUD_RD_PTRCNT,
    AUD_DEC_LENCNT,
    AUD_RELOAD,
    AUD_SET_LENCNT,
    AUD_RQ_SAMP,
    AUD_WAIT_ACK
} audio_fetch_st;                                       // sample fetch states

audio_fetch_st                      fetch_st;           // state of fetch FSM
logic [xv::CHAN_W-1:0]              fetch_chan;         // fetch channel being processed
logic [xv::AUDIO_NCHAN-1:0]         fetch_restart;      // channel restart flags
logic                               fetch_tile;         // next dma fetch from tilemem

// audio param BRAM constants (OR'd with channel << 2)
localparam  AUDn_PARAM_LENCNT   = (8'h80 | 8'(xv::XR_AUD0_LENGTH[1:0]));  // temp length storage (decremented)
localparam  AUDn_PARAM_START    = (8'h00 | 8'(xv::XR_AUD0_START[1:0]));   // start address register
localparam  AUDn_PARAM_LENGTH   = (8'h00 | 8'(xv::XR_AUD0_LENGTH[1:0]));  // length-1 register
localparam  AUDn_PARAM_PTRCNT   = (8'h80 | 8'(xv::XR_AUD0_START[1:0]));   // temp address storage (incremented)

logic                               audio_wr_en;        // fetch FSM write strobe
byte_t                              audio_wr_addr;      // fetch FSM write address (audio param BRAM)
word_t                              audio_wr_data;      // fetch FSM write data (audio param BRAM)
logic                               audio_reg_wr;       // audio register write (delayed)
logic [3:0]                         audio_reg_addr;     // audio register write address (delayed)
word_t                              audio_reg_data;     // audio register write data (delayed)
logic                               audio_reg_wr_next;  // audio register write (next cycle, when no FSM write)
logic                               audio_mem_wr_en;    // audio param BRAM write enable
byte_t                              audio_mem_wr_addr;  // audio param BRAM write address
word_t                              audio_mem_wr_data;  // audio param BRAM write data in
byte_t                              audio_mem_rd_addr;  // audio param BRAM read address
word_t                              audio_mem_rd_data;  // audio param BRAM read data out

logic [16*xv::AUDIO_NCHAN-1:0]      chan_buff;          // channel sample word buffer
logic [xv::AUDIO_NCHAN-1:0]         chan_buff_odd;      // channel odd (or low/2nd byte) output flag
logic [xv::AUDIO_NCHAN-1:0]         chan_buff_ok;       // chan_buff word valid (else new sample will be fetched)
logic [8*xv::AUDIO_NCHAN-1:0]       chan_val;           // channel signed sample value
logic [16*xv::AUDIO_NCHAN-1:0]      chan_period;        // channel period count down (bit 15=underflow flag)

// debug aid signals
`ifndef SYNTHESIS
/* verilator lint_off UNUSED */
logic [xv::AUDIO_NCHAN-1:0]         chan_underflow;                     // true if sample output underflow
logic [7:0]                         chan_vol_l[xv::AUDIO_NCHAN];        // channel left volume
logic [7:0]                         chan_vol_r[xv::AUDIO_NCHAN];        // channel right volume
byte_t                              chan_raw[xv::AUDIO_NCHAN];          // channel value sent to DAC
byte_t                              chan_raw_u[xv::AUDIO_NCHAN];        // channel value sent to DAC unsigned
word_t                              chan_word[xv::AUDIO_NCHAN];         // channel DMA word buffer
logic [xv::AUDIO_NCHAN-1:0]         chan_tile;                          // true if sample in TILEMEM
addr_t                              chan_ptrcnt[xv::AUDIO_NCHAN];       // channel DMA address (debug)
word_t                              chan_lencnt[xv::AUDIO_NCHAN];       // channel remaining length (debug)
word_t                              chan_pericnt[xv::AUDIO_NCHAN];      // channel period count
/* verilator lint_on UNUSED */

// setup debug signal aliases (Verilator, invisible to IVerilog)
always_comb begin : alias_block
    for (integer i = 0; i < xv::AUDIO_NCHAN; i = i + 1) begin
        // debug aliases for easy viewing
        chan_vol_l[i]       = { audio_vol_l_nchan_i[7*i+:7], 1'b0 };
        chan_vol_r[i]       = { audio_vol_r_nchan_i[7*i+:7], 1'b0 };
        chan_raw[i]         = chan_val[i*8+:8];
        chan_raw_u[i]       = chan_val[i*8+:8] ^ 8'h80;
        chan_word[i]        = chan_buff[16*i+:16];
        chan_pericnt[i]     = chan_period[16*i+:16];
    end
end
`endif

word_t audio_rd_data_minus1;        // used to decrement LEN (and underflow check)
assign audio_rd_data_minus1 = { 1'b0, audio_mem_rd_data[14:0] } - 1'b1;

// audio output and fetch
always_ff @(posedge clk) begin : chan_process
    if (reset_i) begin
        audio_dma_vram_req_o <= '0;
        audio_dma_tile_req_o <= '0;
        audio_dma_addr_o     <= '0;
        audio_reload_nchan_o <= '0;  // reset all channels reload

        audio_wr_en         <= '0;
        audio_wr_addr       <= '0;
        audio_wr_data       <= '0;

        audio_mem_rd_addr   <= '0;

        fetch_st            <= AUD_CHECK;
        fetch_chan          <= '0;
        fetch_restart       <= '0;
        fetch_tile          <= '0;

        chan_val            <= '0;
        chan_buff           <= '0;
        chan_period         <= '0;
        chan_buff_ok        <= '0;
        chan_buff_odd       <= '0;

`ifndef SYNTHESIS
        chan_underflow      <= '0;
        chan_tile           <= '0;
        for (integer i = 0; i < xv::AUDIO_NCHAN; i = i + 1) begin
            chan_ptrcnt[i] <= 16'hE3E3;
            chan_lencnt[i] <= 16'hE3E3;
        end
`endif

    end else begin
        // process all audio channel periods
        for (integer i = 0; i < xv::AUDIO_NCHAN; i = i + 1) begin
            // decrement period
            chan_period[16*i+:16]<= chan_period[16*i+:16] - 1'b1;

            // if period underflowed, output next sample
            if (fetch_restart[i] || (chan_period[16*i+xv::AUD_PER_RESTART_B] && chan_buff_ok[i])) begin
                chan_buff_odd[i]         <= !chan_buff_odd[i];
                chan_period[16*i+:16]   <= { 1'b0, audio_period_nchan_i[i*15+:15] };
                chan_val[i*8+:8]        <= chan_buff_odd[i] ? chan_buff[16*i+:8] : chan_buff[16*i+8+:8];
`ifndef SYNTHESIS
                chan_underflow[i]       <= 1'b0;
`endif
                // if 2nd sample of sample word, prepare sample address
                if (chan_buff_odd[i]) begin
                    chan_buff_ok[i]     <= 1'b0;                            // indicate sample needs loading
                end
            end
`ifndef SYNTHESIS
            else begin
                if (audio_enable_i && chan_period[16*i+15]) begin
                    chan_underflow[i]       <= 1'b1;
                end
            end
`endif

            if (!audio_enable_i || audio_restart_nchan_i[i]) begin
                fetch_restart[i]        <= 1'b1;    // force sample addr, tile, len reload
                chan_buff_ok[i]         <= 1'b0;    // clear sample buffer status
                chan_buff_odd[i]        <= 1'b0;    // output 1st sample from word
            end
        end

        // process audio sample FSM
        audio_reload_nchan_o    <= '0;  // reset all channels reload
        audio_wr_en             <= '0;  // reset param mem write strobe
        case (fetch_st)
            AUD_CHECK: begin    // queue LENCNT read, if any buffer empty then set fetch_chan and state
                audio_dma_vram_req_o    <= 1'b0;                            // clear audio sample request
                audio_dma_tile_req_o    <= 1'b0;                            // clear audio sample request
                if (!chan_buff_ok[0]) begin
                    fetch_chan              <= xv::CHAN_W'(0);
                    audio_mem_rd_addr       <= AUDn_PARAM_LENCNT | (8'(0) << 2);
                    fetch_st                <= AUD_RD_PTRCNT;
                end else if (!chan_buff_ok[1]) begin
                    fetch_chan              <= xv::CHAN_W'(1);
                    audio_mem_rd_addr       <= AUDn_PARAM_LENCNT | (8'(1) << 2);
                    fetch_st                <= AUD_RD_PTRCNT;
                end else if (!chan_buff_ok[2]) begin
                    fetch_chan              <= xv::CHAN_W'(2);
                    audio_mem_rd_addr       <= AUDn_PARAM_LENCNT | (8'(2) << 2);
                    fetch_st                <= AUD_RD_PTRCNT;
                end else if (!chan_buff_ok[3]) begin
                    fetch_chan              <= xv::CHAN_W'(3);
                    audio_mem_rd_addr       <= AUDn_PARAM_LENCNT | (8'(3) << 2);
                    fetch_st                <= AUD_RD_PTRCNT;
                end
            end
            AUD_RD_PTRCNT: begin    // queue PTRCNT read
                audio_mem_rd_addr       <= AUDn_PARAM_PTRCNT | (8'(fetch_chan) << 2);
                // waiting for LENCNT
                fetch_st                <= AUD_DEC_LENCNT;
            end
            AUD_DEC_LENCNT: begin    // read LENCNT data decremented
                // LENCNT data ready, waiting for PTRCNT
                // write back to LENCNT decremented (but preserve TILEMEM bit)
                audio_wr_en             <= 1'b1;
                audio_wr_addr           <= AUDn_PARAM_LENCNT | (8'(fetch_chan) << 2);
                audio_wr_data           <= { audio_mem_rd_data[xv::AUD_LEN_TILEMEM_B], audio_rd_data_minus1[14:0] };
`ifndef SYNTHESIS
                chan_lencnt[fetch_chan] <= { 1'b0, audio_rd_data_minus1[14:0] };
`endif
                // if underflow or restart then queue LENGTH read and reload else request sample
                if (audio_rd_data_minus1[15] || fetch_restart[fetch_chan]) begin
                    // queue LENGTH read
                    audio_mem_rd_addr       <= AUDn_PARAM_LENGTH | (8'(fetch_chan) << 2);
                    fetch_st                <= AUD_RELOAD;
                end else begin
                    fetch_tile              <= audio_mem_rd_data[xv::AUD_LEN_TILEMEM_B];
`ifndef SYNTHESIS
                    chan_tile[fetch_chan]   <= audio_mem_rd_data[xv::AUD_LEN_TILEMEM_B];
                    chan_lencnt[fetch_chan] <= { 1'b0, audio_rd_data_minus1[14:0] };
`endif
                    fetch_st                <= AUD_RQ_SAMP;
                end
                fetch_restart[fetch_chan] <= 1'b0;                              // clear restart flag
            end
            AUD_RELOAD: begin       // queue START read
                // ignore PTRCNT data, queue START read
                audio_mem_rd_addr       <= AUDn_PARAM_START | (8'(fetch_chan) << 2);
                fetch_st                <= AUD_SET_LENCNT;
            end
            AUD_SET_LENCNT: begin    // set LENCNT from LENGTH
                // LENGTH data ready, waiting for START
                // write to LENCNT (with TILEMEM flag)
                audio_wr_en             <= 1'b1;
                audio_wr_addr           <= AUDn_PARAM_LENCNT | (8'(fetch_chan) << 2);
                audio_wr_data           <= audio_mem_rd_data;
                fetch_tile              <= audio_mem_rd_data[xv::AUD_LEN_TILEMEM_B];
`ifndef SYNTHESIS
                chan_lencnt[fetch_chan] <= audio_mem_rd_data;
`endif
                audio_reload_nchan_o[fetch_chan] <= 1'b1;               // indicate channel reloaded
                fetch_st                <= AUD_RQ_SAMP;
            end
            AUD_RQ_SAMP: begin      // request audio DMA data
                // PTRCNT or START data ready
                // write PTRCNT+1 or START+1 to PTRCNT
                audio_wr_en             <= 1'b1;
                audio_wr_addr           <= AUDn_PARAM_PTRCNT | (8'(fetch_chan) << 2);
                audio_wr_data           <= audio_mem_rd_data + 1'b1;        // update PTRCNT
                // make dma request
                audio_dma_vram_req_o    <= ~fetch_tile;                    // request audio vram
                audio_dma_tile_req_o    <= fetch_tile;                     // or request audio tile
                audio_dma_addr_o        <= audio_mem_rd_data;               // audio sample address
`ifndef SYNTHESIS
                chan_ptrcnt[fetch_chan] <= audio_mem_rd_data;
`endif
                fetch_st                <= AUD_WAIT_ACK;
            end
            AUD_WAIT_ACK: begin      // request audio DMA
                if (audio_dma_ack_i) begin
                    audio_dma_vram_req_o    <= 1'b0;
                    audio_dma_tile_req_o    <= 1'b0;
                    chan_buff[16*fetch_chan+:16] <= audio_dma_word_i;
                    chan_buff_ok[fetch_chan] <= 1'b1;
                    fetch_st                <= AUD_CHECK;
                end
            end
        endcase

        // if audio disabled, reset fetch FSM
        if (!audio_enable_i) begin
            fetch_st        <= AUD_CHECK;
        end
    end
end

// audio memory interface write select (audio processing / regs)
always_ff @(posedge clk) begin
    if (reset_i) begin
        audio_reg_wr    <= 1'b0;
        audio_reg_addr  <= '0;
        audio_reg_data  <= '0;
    end else begin
        audio_reg_wr    <= audio_reg_wr_next;

        if (audio_reg_wr_i) begin
            audio_reg_wr    <= 1'b1;
            audio_reg_addr  <= audio_reg_addr_i;
            audio_reg_data  <= audio_reg_data_i;
        end
    end
end

always_comb begin
    audio_mem_wr_en     = 1'b0;             // default no write
    // default for audio processsing
    audio_reg_wr_next   = audio_reg_wr;     // preserve audio_reg_wr
    audio_mem_wr_addr   = audio_wr_addr;
    audio_mem_wr_data   = audio_wr_data;

    if (audio_wr_en) begin                      // audio processsing write has priority
        audio_mem_wr_en     = 1'b1;
    end else if (audio_reg_wr) begin            // else, write register data
        audio_reg_wr_next   = 1'b0;             // clear audio_reg_wr
        audio_mem_wr_en     = 1'b1;
        audio_mem_wr_addr   = 8'(audio_reg_addr);
        audio_mem_wr_data   = audio_reg_data;
    end
end

// audio parameter memory
audio_mem #(
    .AWIDTH(xv::AUDIO_W)
) audio_mem(
    .clk(clk),
    .rd_address_i(audio_mem_rd_addr),
    .rd_data_o(audio_mem_rd_data),
    .wr_clk(clk),
    .wr_en_i(audio_mem_wr_en),
    .wr_address_i(audio_mem_wr_addr),
    .wr_data_i(audio_mem_wr_data)
);

// channel mixing
always_ff @(posedge clk) begin : mix_fsm
    if (reset_i) begin
        mix_chan        <= '0;
        mix_clr         <= '0;

        value_temp      <= '0;
        vol_l_temp      <= '0;
        vol_r_temp      <= '0;

`ifndef SYNTHESIS
        output_l        <= 8'hFF;      // NOTE: to force full scale display for analog signal view in GTKWave
        output_r        <= 8'hFF;
`else
        output_l        <= '0;
        output_r        <= '0;
`endif
    end else begin
        if (mix_chan == xv::AUDIO_NCHAN) begin
            mix_chan        <= '0;      // reset to channel 0
            mix_clr         <= 1'b1;    // clear FMAC acc

            // clamp mixed output and convert to unsigned result for DAC
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
            mix_chan        <= mix_chan + 1'b1; // next channel
            mix_clr         <= 1'b0;            // don't clear FMAC acc

            // set input signals for FMAC blocks
            vol_l_temp      <= { 1'b0, audio_vol_l_nchan_i[7*mix_chan+:7] };
            vol_r_temp      <= { 1'b0, audio_vol_r_nchan_i[7*mix_chan+:7] };
            value_temp      <= chan_val[mix_chan*8+:8];
        end
    end
end

assign mix_l_acc        = acc_l[15:6];
assign mix_r_acc        = acc_r[15:6];

// low bits of accumulator results are not used
logic                   unused_bits = &{ 1'b0, acc_l[5:0], acc_r[5:0] };

// audio left DAC output
audio_dac #(
    .WIDTH(DAC_W)
) audio_l_dac (
    .value_i(output_l),
    .pulse_o(pdm_l_o),
    .reset_i(reset_i),
    .clk(clk)
);
// audio right DAC output
audio_dac #(
    .WIDTH(DAC_W)
) audio_r_dac (
    .value_i(output_r),
    .pulse_o(pdm_r_o),
    .reset_i(reset_i),
    .clk(clk)
);

`ifndef USE_FMAC        // generic inferred multiply and fabric accumulate
sword_t             res_l;
sword_t             res_r;

assign res_l        = value_temp * vol_l_temp;
assign res_r        = value_temp * vol_r_temp;

always_ff @(posedge clk) begin
    if (reset_i) begin
        acc_l   <= '0;
        acc_r   <= '0;
    end else begin
        if (mix_clr) begin
            acc_l   <= '0;
            acc_r   <= '0;
        end else begin
            acc_l   <= acc_l + res_l;
            acc_r   <= acc_r + res_r;
        end
    end
end

`else    // iCE40UltraPlus5K specific

logic [15:0]            unused_acc_l;
logic [15:0]            unused_acc_r;

// NOTE: Using dual 8x8 MAC16 mode, but ignoring lower unit since doesn't support signed multiply (AFAICT)
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
    .A({ value_temp, 8'h00 }),          // 16-bit input A
    .B({ vol_l_temp, 8'h00 }),          // 16-bit input B
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
    .O({ acc_l, unused_acc_l }),        // 32-bit result output (dual 8x8=16-bit mode with top used)
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
    .A({ value_temp, 8'h00 }),          // 16-bit input A
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
    .O({ acc_r, unused_acc_r }),        // 32-bit result output (dual 8x8=16-bit mode with top used)
    .CO(),                              // cascaded add/sub carry output to next DSP block
    .ACCUMCO(),                         // cascaded accumulator carry output to next DSP block
    .SIGNEXTOUT()                       // cascaded sign extension output to next DSP block
);
/* verilator lint_on PINCONNECTEMPTY */

`endif

endmodule

`endif
`default_nettype wire               // restore default
