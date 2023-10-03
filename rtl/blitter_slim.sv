// blitter.sv
//
// vim: set et ts=4 sw=4
//
// Copyright (c) 2020 Xark - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)
//

`default_nettype none               // mandatory for Verilog sanity
`timescale 1ns/1ps                  // mandatory to shut up Icarus Verilog

`include "xosera_pkg.sv"

`ifdef EN_BLIT

module blitter_slim(
    // video registers and control
    input  wire logic           xreg_wr_en_i,       // strobe to write internal config register number
    input  wire logic  [3:0]    xreg_num_i,         // internal config register number (for reads)
    input  wire word_t          xreg_data_i,        // data for internal config register
    // blitter signals
    output      logic           blit_busy_o,        // blitter idle or busy status
    output      logic           blit_full_o,        // blitter ready or queue full status
    output      logic           blit_done_intr_o,   // interrupt signal when done
    // VRAM/XR bus signals
    output      logic           blit_vram_sel_o,    // vram select
    input  wire logic           blit_vram_ack_i,    // VRAM access ack (true when data read/written)
    output      logic           blit_wr_o,          // blit write
    output      logic  [3:0]    blit_wr_mask_o,     // blit VRAM nibble write mask
    output      addr_t          blit_addr_o,        // VRAM address out
    input  wire word_t          blit_data_i,        // data word data in
    output      word_t          blit_data_o,        // data word data out
    // standard signals
    input  wire logic           reset_i,            // system reset in
    input  wire logic           clk                 // clock
);

// blitter xreg register data (holds "queued" blit)
logic           xreg_ctrl_S_const;
logic           xreg_ctrl_transp;                   // transparency enable
logic           xreg_ctrl_transp_8b;                // 4-bit/8-bit transparency zero check
logic [7:0]     xreg_ctrl_transp_T;                 // 8-bit transparency value

logic  [1:0]    xreg_shift_amount;
logic  [3:0]    xreg_shift_f_mask;
logic  [3:0]    xreg_shift_l_mask;
word_t          xreg_mod_S;
word_t          xreg_src_S;
word_t          xreg_val_CA;
word_t          xreg_val_CX;
word_t          xreg_mod_D;
word_t          xreg_dst_D;
word_t          xreg_lines;                         // "limitation" of 32768 lines
word_t          xreg_words;

logic           xreg_blit_queued;                   // blit operation is queued in xreg registers
logic           blit_setup;

// assign status outputs
assign blit_busy_o  = (blit_state != IDLE);    // blit operation in progress
assign blit_full_o  = xreg_blit_queued;             // blit register queue full

// blit registers write
always_ff @(posedge clk) begin
    if (reset_i) begin
        xreg_ctrl_S_const   <= '0;
        xreg_ctrl_transp    <= '0;
        xreg_ctrl_transp_8b <= '0;
        xreg_ctrl_transp_T  <= '0;
        xreg_shift_amount   <= '0;
        xreg_shift_f_mask   <= '0;
        xreg_shift_l_mask   <= '0;
        xreg_mod_S          <= '0;
        xreg_mod_D          <= '0;
        xreg_src_S          <= '0;
        xreg_val_CA         <= '0;
        xreg_val_CX         <= '0;
        xreg_dst_D          <= '0;
        xreg_lines          <= '0;
        xreg_words          <= '0;
        xreg_blit_queued    <= '0;
    end else begin
        // clear queued blit when state machine copies xreg data
        if (blit_setup) begin
            xreg_blit_queued     <= 1'b0;
        end

        // blit register write
        if (xreg_wr_en_i) begin
            case ({ xv::XR_BLIT_REGS[6:4], xreg_num_i })
                xv::XR_BLIT_CTRL: begin
                    xreg_ctrl_transp_T  <= xreg_data_i[15:8];
                    xreg_ctrl_transp_8b <= xreg_data_i[5];
                    xreg_ctrl_transp    <= xreg_data_i[4];
                    xreg_ctrl_S_const   <= xreg_data_i[0];
                end
                xv::XR_BLIT_ANDC: begin
                    xreg_val_CA         <= xreg_data_i;
                end
                xv::XR_BLIT_XOR: begin
                    xreg_val_CX         <= xreg_data_i;
                end
                xv::XR_BLIT_MOD_S: begin
                    xreg_mod_S          <= xreg_data_i;
                end
                xv::XR_BLIT_SRC_S: begin
                    xreg_src_S          <= xreg_data_i;
                end
                xv::XR_BLIT_MOD_D: begin
                    xreg_mod_D          <= xreg_data_i;
                end
                xv::XR_BLIT_DST_D: begin
                    xreg_dst_D          <= xreg_data_i;
                end
                xv::XR_BLIT_SHIFT: begin
                    xreg_shift_f_mask   <= xreg_data_i[15:12];
                    xreg_shift_l_mask   <= xreg_data_i[11:8];
                    xreg_shift_amount   <= xreg_data_i[1:0];
                end
                xv::XR_BLIT_LINES: begin
                    xreg_lines          <= xreg_data_i;
                end
                xv::XR_BLIT_WORDS: begin
                    xreg_words          <= xreg_data_i;
                    xreg_blit_queued    <= 1'b1;
                end
                default: begin
                end
            endcase
        end
    end
end

// blitter operational registers (for blit in progress)
logic           blit_ctrl_S_const;
logic           blit_ctrl_transp;
logic           blit_ctrl_transp_8b;
logic  [7:0]    blit_ctrl_transp_T;
logic  [1:0]    blit_shift_amount;
logic  [3:0]    blit_shift_f_mask;
logic  [3:0]    blit_shift_l_mask;
word_t          blit_mod_S;
word_t          blit_mod_D;
word_t          blit_val_CA;
word_t          blit_val_CX;
word_t          blit_src_S;
word_t          blit_dst_D;

word_t          blit_lines;             // bit 15 is underflow done flag
word_t          blit_words;
logic [16:0]    blit_count;             // word counter (extra underflow bit used line done flag)

word_t          val_S;                  // value read from blit_src_S VRAM or const
word_t          val_D_CA;
logic [11:0]    last_S;                 // last S word save (3 nibbles)
logic [ 3:0]    blit_f_mask;
logic           blit_done_intr;

logic           blit_vram_sel, blit_vram_sel_next;  // vram select
logic           blit_wr, blit_wr_next;              // blit write
addr_t          blit_addr, blit_addr_next;          // VRAM address out

// combinitorial FSM signals
word_t          blit_src_S_next;
word_t          blit_dst_D_next;
word_t          val_S_next;                         // value read from blit_src_S VRAM or const
logic [11:0]    last_S_next;                        // last S word save
word_t          blit_lines_next;                    // bit 15 is underflow done flag
logic [16:0]    blit_count_next;                    // word counter (extra underflow bit used line done flag)
logic [ 3:0]    blit_f_mask_next;
logic           blit_done_intr_next;

// blitter flags and word counter

logic           blit_last_word;
assign          blit_last_word  = blit_count[16];   // underflow flag for last word/last word of line
logic           blit_last_line;
assign          blit_last_line  = blit_lines[15];   // underflow flag for last line (for rectangular blit)

// nibble shifter
function automatic word_t shifter(
        input  [1:0]    shift_amount,   // nibbles to shift right
        input [15:0]    data_word,      // incoming data word to shift
        input [11:0]    prev_word       // previous data word to shift in
    );
    begin
        case (shift_amount)
            // right shift (increment)
            2'b00:  shifter =   data_word;                                  // ABCD
            2'b01:  shifter =   (16'(prev_word) << 12) | (data_word >>  4); // dABC
            2'b10:  shifter =   (16'(prev_word) <<  8) | (data_word >>  8); // cdAB
            2'b11:  shifter =   (16'(prev_word) <<  4) | (data_word >> 12); // bcdA
        endcase
    end
endfunction

// transparency testing
logic  [3:0]    result_T4;               // transparency result (4 bit nibble mask)
logic  [3:0]    result_T8;               // transparency result (4 bit nibble mask)

// logic op calculation

assign  val_D_CA        = val_S & (~blit_val_CA); // D = S AND (NOT CA) for transparency

// compute transparency
assign  result_T4       = { (val_D_CA[12+:4] != blit_ctrl_transp_T[7:4] || !blit_ctrl_transp),
                            (val_D_CA[ 8+:4] != blit_ctrl_transp_T[3:0] || !blit_ctrl_transp),
                            (val_D_CA[ 4+:4] != blit_ctrl_transp_T[7:4] || !blit_ctrl_transp),
                            (val_D_CA[ 0+:4] != blit_ctrl_transp_T[3:0] || !blit_ctrl_transp) };
assign  result_T8       = { (val_D_CA[ 8+:8] != blit_ctrl_transp_T      || !blit_ctrl_transp),
                            (val_D_CA[ 8+:8] != blit_ctrl_transp_T      || !blit_ctrl_transp),
                            (val_D_CA[ 0+:8] != blit_ctrl_transp_T      || !blit_ctrl_transp),
                            (val_D_CA[ 0+:8] != blit_ctrl_transp_T      || !blit_ctrl_transp) };

assign  blit_data_o     = val_D_CA ^ blit_val_CX; // output result XOR CX
assign  blit_vram_sel_o = blit_vram_sel;
assign  blit_wr_o       = blit_wr;
assign  blit_wr_mask_o  = blit_f_mask &         // output VRAM write mask
                          (blit_last_word  ? blit_shift_l_mask : 4'b1111) &
                          (blit_ctrl_transp_8b ? result_T8 : result_T4);
assign blit_addr_o      = blit_addr;

assign blit_done_intr_o = blit_done_intr;

always_ff @(posedge clk) begin
    if (reset_i) begin
        blit_state          <= IDLE;

        blit_ctrl_S_const   <= '0;
        blit_ctrl_transp    <= '0;
        blit_ctrl_transp_8b <= '0;
        blit_ctrl_transp_T  <= '0;
        blit_shift_f_mask   <= '0;
        blit_shift_l_mask   <= '0;
        blit_shift_amount   <= '0;
        blit_mod_S          <= '0;
        blit_mod_D          <= '0;
        blit_src_S          <= '0;
        blit_val_CA         <= '0;
        blit_val_CX         <= '0;
        blit_dst_D          <= '0;
        blit_lines          <= '0;
        blit_words          <= '0;
        blit_count          <= '0;

        blit_vram_sel       <= '0;
        blit_wr             <= '0;
        blit_addr           <= '0;
        blit_f_mask         <= '0;
        val_S               <= '0;
        last_S              <= '0;

    end else begin

        // only advance state if vram not selected, or ack'd
        if (!blit_vram_sel || blit_vram_ack_i) begin
            blit_state          <= blit_state_next;
            blit_vram_sel       <= blit_vram_sel_next;
            blit_wr             <= blit_wr_next;
            blit_addr           <= blit_addr_next;
            val_S               <= val_S_next;
            blit_src_S          <= blit_src_S_next;
            blit_dst_D          <= blit_dst_D_next;
            last_S              <= last_S_next;
            blit_f_mask         <= blit_f_mask_next;
            blit_lines          <= blit_lines_next;
            blit_count          <= blit_count_next;
            blit_done_intr      <= blit_done_intr_next;
        end

        if (blit_setup) begin
            blit_ctrl_S_const   <= xreg_ctrl_S_const;
            blit_ctrl_transp    <= xreg_ctrl_transp;
            blit_ctrl_transp_8b <= xreg_ctrl_transp_8b;
            blit_ctrl_transp_T  <= xreg_ctrl_transp_T;
            blit_shift_amount   <= xreg_shift_amount;
            blit_shift_f_mask   <= xreg_shift_f_mask;
            blit_shift_l_mask   <= xreg_shift_l_mask;
            blit_mod_S          <= xreg_mod_S;
            blit_mod_D          <= xreg_mod_D;
            blit_src_S          <= xreg_src_S;
            blit_val_CA         <= xreg_val_CA;
            blit_val_CX         <= xreg_val_CX;
            blit_dst_D          <= xreg_dst_D;
            blit_lines          <= xreg_lines;
            blit_words          <= xreg_words;
            val_S               <= xreg_src_S;                      // setup for possible use as const
        end
    end
end

// blit state machine
`ifdef SYNTHESIS
typedef enum {                  // smaller area
`else
typedef enum logic [2:0] {      // more friendly in GTKWave
`endif
    IDLE,           // wait for blit operation (a write to xreg_blit_count)
    SETUP,          // copy xreg registers to blit registers and setup for blit
    LINE_BEG,       // copy update counters, initiate S read or D write
    RD_S,           // do S read
    WR_D,           // do D write
    LINE_END        // add modulo values, loop if more lines
} blit_state_t;

blit_state_t    blit_state, blit_state_next;

always_comb begin

    blit_vram_sel_next  = '0;               // vram select
    blit_wr_next        = '0;               // blit write
    blit_addr_next      = blit_addr;        // VRAM address out
    blit_f_mask_next    = blit_f_mask;
    blit_src_S_next     = blit_src_S;
    blit_dst_D_next     = blit_dst_D;
    val_S_next          = val_S;
    last_S_next         = last_S;
    blit_lines_next     = blit_lines;
    blit_count_next     = blit_count;
    blit_done_intr_next = '0;
    blit_setup          = '0;

    case (blit_state)
        IDLE: begin
            if (xreg_blit_queued) begin
                blit_state_next     = SETUP;
            end else begin
                blit_state_next     = IDLE;
            end
        end
        SETUP: begin
            blit_setup          = '1;
            blit_state_next     = LINE_BEG;
        end
        LINE_BEG: begin
            blit_f_mask_next    = blit_shift_f_mask;

            blit_lines_next     = blit_lines - 1'b1;                // pre-decrement, bit[15] underflow indicates last line (1-32768)
            blit_count_next     = { 1'b0, blit_words }  - 1'b1;     // pre-decrement, bit[16] underflow indicates last word (1-65536)

            if (!blit_ctrl_S_const) begin
                blit_vram_sel_next  = 1'b1;                         // setup S addr for read
                blit_wr_next        = 1'b0;
                blit_addr_next      = blit_src_S;

                blit_state_next     = RD_S;
            end else begin
                blit_vram_sel_next  = 1'b1;                         // setup D addr for write
                blit_wr_next        = 1'b1;
                blit_addr_next      = blit_dst_D;

                blit_state_next     = WR_D;
            end
        end
        RD_S: begin
            val_S_next          = shifter(blit_shift_amount, blit_data_i, last_S);
            last_S_next         = 12'(blit_data_i);

            blit_src_S_next     = blit_addr + 1'b1;             // update A addr

            blit_vram_sel_next  = 1'b1;                         // setup D addr for write
            blit_wr_next        = 1'b1;
            blit_addr_next      = blit_dst_D;

            blit_state_next     = WR_D;
        end
        WR_D: begin
            blit_dst_D_next     = blit_addr + 1'b1;       // update D addr
            blit_addr_next      = blit_addr + 1'b1;       // setup VRAM addr for constant write
            blit_count_next     = blit_count - 1'b1;           // decrement word count
            blit_f_mask_next    = '1;                          // clear first word mask

            if (blit_last_word) begin                           // was that the last word?
                blit_vram_sel_next  = 1'b0;                        // setup A addr for read
                blit_wr_next        = 1'b0;

                blit_state_next     = LINE_END;                // we are finshed with this line
            end else if (!blit_ctrl_S_const) begin
                blit_vram_sel_next  = 1'b1;                        // setup A addr for read
                blit_wr_next        = 1'b0;
                blit_addr_next      = blit_src_S;

                blit_state_next     = RD_S;
            end else begin
                blit_vram_sel_next  = 1'b1;                    // setup D addr for write
                blit_wr_next        = 1'b1;

                blit_state_next     = WR_D;
            end
        end
        LINE_END: begin
            // update addresses with end of line modulo value
            blit_src_S_next = blit_src_S + blit_mod_S;
            blit_dst_D_next = blit_dst_D + blit_mod_D;

            if (blit_last_line) begin
                blit_done_intr_next = 1'b1;

                if (xreg_blit_queued) begin
                    blit_state_next     = SETUP;
                end else begin
                    blit_state_next     = IDLE;
                end
            end else begin
                blit_state_next     = LINE_BEG;
            end
        end
        default: begin
            blit_state_next = IDLE;
        end
    endcase
end

endmodule

`endif
`default_nettype wire               // restore default
