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

module blitter #(
    parameter   EN_BLIT_DECR_MODE       = 1,        // enable blit pointer decrementing
    parameter   EN_BLIT_DECR_LSHIFT     = 1         // enable blit left shift when decrementing
)(
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
logic           xreg_ctrl_A_const;
logic           xreg_ctrl_B_const;
logic           xreg_ctrl_B_not;
logic           xreg_ctrl_C_use_B;
logic           xreg_ctrl_decrement;
logic           xreg_ctrl_transp_8b;                // 4-bit/8-bit transparency zero check
logic [7:0]     xreg_ctrl_transp_T;                 // 8-bit transparency value

logic  [1:0]    xreg_shift_amount;
logic  [3:0]    xreg_shift_f_mask;
logic  [3:0]    xreg_shift_l_mask;
word_t          xreg_mod_A;
word_t          xreg_src_A;
word_t          xreg_mod_B;
word_t          xreg_src_B;
word_t          xreg_mod_C;
word_t          xreg_val_C;
word_t          xreg_mod_D;
word_t          xreg_dst_D;
logic [14:0]    xreg_lines;                         // "limitation" of 32768 lines
word_t          xreg_words;

logic           xreg_blit_queued;                   // blit operation is queued in xreg registers

// assign status outputs
assign blit_busy_o  = (blit_state != IDLE);    // blit operation in progress
assign blit_full_o  = xreg_blit_queued;             // blit register queue full

// blit registers write
always_ff @(posedge clk) begin
    if (reset_i) begin
        xreg_ctrl_A_const   <= '0;
        xreg_ctrl_B_const   <= '0;
        xreg_ctrl_B_not     <= '0;
        xreg_ctrl_C_use_B   <= '0;
        xreg_ctrl_decrement <= '0;
        xreg_ctrl_transp_8b <= '0;
        xreg_ctrl_transp_T  <= '0;
        xreg_shift_amount    <= '0;
        xreg_shift_f_mask   <= '0;
        xreg_shift_l_mask   <= '0;
        xreg_mod_A          <= '0;
        xreg_mod_B          <= '0;
        xreg_mod_C          <= '0;
        xreg_mod_D          <= '0;
        xreg_src_A          <= '0;
        xreg_src_B          <= '0;
        xreg_val_C          <= '0;
        xreg_dst_D          <= '0;
        xreg_lines          <= '0;
        xreg_words          <= '0;
        xreg_blit_queued    <= '0;
    end else begin
        // clear queued blit when state machine copies xreg data
        if (blit_state == SETUP) begin
            xreg_blit_queued     <= 1'b0;
        end

        // blit register write
        if (xreg_wr_en_i) begin
            case ({ xv::XR_BLIT_CTRL[6:4], xreg_num_i })
                xv::XR_BLIT_CTRL: begin
                    xreg_ctrl_transp_T  <= xreg_data_i[15:8];
                    xreg_ctrl_transp_8b <= xreg_data_i[5];
                    xreg_ctrl_decrement <= EN_BLIT_DECR_MODE ? xreg_data_i[4] : '0;
                    xreg_ctrl_C_use_B   <= xreg_data_i[3];
                    xreg_ctrl_B_not     <= xreg_data_i[2];
                    xreg_ctrl_B_const   <= xreg_data_i[1];
                    xreg_ctrl_A_const   <= xreg_data_i[0];
                end
                xv::XR_BLIT_SHIFT: begin
                    xreg_shift_f_mask   <= xreg_data_i[15:12];
                    xreg_shift_l_mask   <= xreg_data_i[11:8];
                    xreg_shift_amount    <= xreg_data_i[1:0];
                end
                xv::XR_BLIT_MOD_A: begin
                    xreg_mod_A          <= xreg_data_i;
                end
                xv::XR_BLIT_MOD_B: begin
                    xreg_mod_B          <= xreg_data_i;
                end
                xv::XR_BLIT_MOD_C: begin
                    xreg_mod_C          <= xreg_data_i;
                end
                xv::XR_BLIT_MOD_D: begin
                    xreg_mod_D          <= xreg_data_i;
                end
                xv::XR_BLIT_SRC_A: begin
                    xreg_src_A          <= xreg_data_i;
                end
                xv::XR_BLIT_SRC_B: begin
                    xreg_src_B          <= xreg_data_i;
                end
                xv::XR_BLIT_VAL_C: begin
                    xreg_val_C          <= xreg_data_i;
                end
                xv::XR_BLIT_DST_D: begin
                    xreg_dst_D          <= xreg_data_i;
                end
                xv::XR_BLIT_LINES: begin
                    xreg_lines          <= xreg_data_i[14:0];
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
logic           blit_ctrl_A_const;
logic           blit_ctrl_B_const;
logic           blit_ctrl_B_not;
logic           blit_ctrl_C_use_B;
logic           blit_ctrl_decrement;
logic           blit_ctrl_transp_8b;
logic  [7:0]    blit_ctrl_transp_T;
logic  [1:0]    blit_shift_amount;
logic  [3:0]    blit_shift_f_mask;
logic  [3:0]    blit_shift_l_mask;
word_t          blit_mod_A;
word_t          blit_mod_B;
word_t          blit_mod_C;
word_t          blit_mod_D;
word_t          blit_src_A;
word_t          blit_src_B;
word_t          blit_val_C;
word_t          blit_dst_D;
word_t          blit_lines;             // bit 15 is underflow done flag
word_t          blit_words;

// blitter flags and word counter
logic [16:0]    blit_count;             // word counter (extra underflow bit used line done flag)
logic [ 3:0]    blit_first_word;
logic           blit_last_word;
assign          blit_last_word  = blit_count[16];   // underflow flag for last word/last word of line
logic           blit_last_line;
assign          blit_last_line  = blit_lines[15];   // underflow flag for last line (for rectangular blit)

// nibble shifter
word_t      last_A;         // last A word save
word_t      last_B;         // last B word save
word_t      last_word;      // last word to shift in
word_t      shift_out;      // word 0 to 3 nibble rotated ()

if (EN_BLIT_DECR_LSHIFT) begin : opt_LSHIFT
    always_comb begin
        case ({ blit_ctrl_decrement, blit_shift_amount })
            // right shift
            3'b000:   shift_out = { blit_data_i[12+:4], blit_data_i[ 8+:4], blit_data_i[ 4+:4], blit_data_i[ 0+:4]  };
            3'b001:   shift_out = {   last_word[ 0+:4], blit_data_i[12+:4], blit_data_i[ 8+:4], blit_data_i[ 4+:4]  };
            3'b010:   shift_out = {   last_word[ 4+:4],   last_word[ 0+:4], blit_data_i[12+:4], blit_data_i[ 8+:4]  };
            3'b011:   shift_out = {   last_word[ 8+:4],   last_word[ 4+:4],   last_word[ 0+:4], blit_data_i[12+:4]  };
            // left shift (decrement)
            3'b100:   shift_out = { blit_data_i[ 0+:4],   last_word[12+:4],   last_word[ 8+:4],   last_word[ 4+:4]  };
            3'b101:   shift_out = { blit_data_i[ 4+:4], blit_data_i[ 0+:4],   last_word[12+:4],   last_word[ 8+:4]  };
            3'b110:   shift_out = { blit_data_i[ 8+:4], blit_data_i[ 4+:4], blit_data_i[ 0+:4],   last_word[12+:4]  };
            3'b111:   shift_out = { blit_data_i[12+:4], blit_data_i[ 8+:4], blit_data_i[ 4+:4], blit_data_i[ 0+:4]  };
        endcase
    end
end

if (!EN_BLIT_DECR_LSHIFT) begin : no_LSHIFT
    logic unused_bits;
    assign unused_bits = &{1'b0, last_word};
    always_comb begin
        case (blit_shift_amount)
            // right shift
            2'b00:   shift_out = { blit_data_i[12+:4], blit_data_i[ 8+:4], blit_data_i[ 4+:4], blit_data_i[ 0+:4]  };
            2'b01:   shift_out = {   last_word[ 0+:4], blit_data_i[12+:4], blit_data_i[ 8+:4], blit_data_i[ 4+:4]  };
            2'b10:   shift_out = {   last_word[ 4+:4],   last_word[ 0+:4], blit_data_i[12+:4], blit_data_i[ 8+:4]  };
            2'b11:   shift_out = {   last_word[ 8+:4],   last_word[ 4+:4],   last_word[ 0+:4], blit_data_i[12+:4]  };
        endcase
    end
end

// logic op calculation

// No flags:
//   D = A AND B XOR C
//
// notB flag (substitute NOT B for B)
//   D = A AND NOT B XOR C
//
// CuseB flag (substitute B for C)
//   D = NOT A AND B    (same as A AND B XOR B)
//
// notB & CuseB flags (both of above)
//   D = A OR B         (same as A AND NOT B XOR B)

word_t          val_A;                  // value read from blit_src_A VRAM or const
word_t          val_B;                  // value read from blit_src_B VRAM (or NOT of value if notB) or const

assign          blit_data_o = blit_ctrl_B_not ? (val_A & ~val_B ^ blit_val_C) :
                                                (val_A & val_B ^ blit_val_C);  // calc logic op result as data out

// transparency testing
logic  [3:0]    result_T4;               // transparency result (4 bit nibble mask)
logic  [3:0]    result_T8;               // transparency result (4 bit nibble mask)

assign blit_wr_mask_o   = blit_first_word &     // output VRAM write mask
                          (blit_last_word  ? blit_shift_l_mask : 4'b1111) &
                          (blit_ctrl_transp_8b ? result_T8 : result_T4);

// blit state machine
typedef enum logic [2:0] {
    IDLE,           // wait for blit operation (a write to xreg_blit_count)
    SETUP,          // copy xreg registers to blit registers and setup for blit
    LINE_BEG,       // copy update counters, initiate A/B read or D write
    WAIT_RD_A,      // wait for A read result, initiate A read, else write result
    WAIT_RD_B,      // wait for B read result, initiate A read, else write result
    WAIT_WR_D,      // wait for D write, initiate A/B read or D write, loop if more words
    LINE_END        // add modulo values, loop if more lines
} blit_state_t;

blit_state_t    blit_state;

always_ff @(posedge clk) begin
    if (reset_i) begin
        blit_done_intr_o    <= '0;
        blit_vram_sel_o     <= '0;
        blit_wr_o           <= '0;
        blit_addr_o         <= '0;

        blit_state          <= IDLE;

        blit_ctrl_A_const   <= '0;
        blit_ctrl_B_const   <= '0;
        blit_ctrl_B_not     <= '0;
        blit_ctrl_C_use_B   <= '0;
        blit_ctrl_decrement <= '0;
        blit_ctrl_transp_8b <= '0;
        blit_ctrl_transp_T  <= '0;
        blit_shift_f_mask   <= '0;
        blit_shift_l_mask   <= '0;
        blit_shift_amount    <= '0;
        blit_mod_A          <= '0;
        blit_mod_B          <= '0;
        blit_mod_C          <= '0;
        blit_mod_D          <= '0;
        blit_src_A          <= '0;
        blit_src_B          <= '0;
        blit_val_C          <= '0;
        blit_dst_D          <= '0;
        blit_lines          <= '0;
        blit_words          <= '0;
        blit_count          <= '0;

        blit_first_word     <= '0;
        val_A               <= '0;
        val_B               <= '0;
        result_T4           <= '0;
        result_T8           <= '0;

        last_word           <= '0;
        last_A              <= '0;
        last_B              <= '0;

    end else begin
        blit_done_intr_o    <= 1'b0;

        blit_vram_sel_o     <= 1'b0;
        blit_wr_o           <= 1'b0;
        blit_addr_o         <= '0;
        last_word           <= '0;

        case (blit_state)
            IDLE: begin
                if (xreg_blit_queued) begin
                    blit_state          <= SETUP;
                end else begin
                    blit_state          <= IDLE;
                end
            end
            SETUP: begin
                blit_ctrl_A_const   <= xreg_ctrl_A_const;
                blit_ctrl_B_const   <= xreg_ctrl_B_const;
                blit_ctrl_B_not     <= xreg_ctrl_B_not;
                blit_ctrl_C_use_B   <= xreg_ctrl_C_use_B;
                blit_ctrl_decrement <= EN_BLIT_DECR_MODE ? xreg_ctrl_decrement : '0;
                blit_ctrl_transp_8b <= xreg_ctrl_transp_8b;
                blit_ctrl_transp_T  <= xreg_ctrl_transp_T;
                blit_shift_amount    <= xreg_shift_amount;
                blit_shift_f_mask   <= xreg_shift_f_mask;
                blit_shift_l_mask   <= xreg_shift_l_mask;
                blit_mod_A          <= xreg_mod_A;
                blit_mod_B          <= xreg_mod_B;
                blit_mod_C          <= xreg_mod_C;
                blit_mod_D          <= xreg_mod_D;
                blit_src_A          <= xreg_src_A;
                blit_src_B          <= xreg_src_B;
                blit_val_C          <= xreg_val_C;
                blit_dst_D          <= xreg_dst_D;
                blit_lines          <= { 1'b0, xreg_lines };
                blit_words          <= xreg_words;

                val_A               <= xreg_src_A;                      // setup for possible use as const
                val_B               <= xreg_src_B;                      // setup for possible use as const

                blit_state          <= LINE_BEG;
            end
            LINE_BEG: begin
                blit_first_word     <= blit_shift_f_mask;

                blit_lines          <= blit_lines - 1'b1;               // pre-decrement, bit[15] underflow indicates last line (1-32768)
                blit_count          <= { 1'b0, blit_words }  - 1'b1;    // pre-decrement, bit[16] underflow indicates last word (1-65536)

                result_T8   <= { (val_B[8+:8] != blit_ctrl_transp_T),
                                 (val_B[8+:8] != blit_ctrl_transp_T),
                                 (val_B[0+:8] != blit_ctrl_transp_T),
                                 (val_B[0+:8] != blit_ctrl_transp_T)    };
                result_T4   <= { (val_B[12+:4] != blit_ctrl_transp_T[7:4]),
                                 (val_B[ 8+:4] != blit_ctrl_transp_T[3:0]),
                                 (val_B[ 4+:4] != blit_ctrl_transp_T[7:4]),
                                 (val_B[ 0+:4] != blit_ctrl_transp_T[3:0])    };

                if (!blit_ctrl_A_const) begin
                    blit_vram_sel_o     <= 1'b1;                        // setup A addr for read
                    blit_wr_o           <= 1'b0;
                    blit_addr_o         <= blit_src_A;

                    last_word           <= last_A;

                    blit_state          <= WAIT_RD_A;
                end else if (!blit_ctrl_B_const) begin
                    blit_vram_sel_o     <= 1'b1;                        // setup B addr for read
                    blit_wr_o           <= 1'b0;
                    blit_addr_o         <= blit_src_B;

                    last_word           <= last_B;

                    blit_state          <= WAIT_RD_B;
                end else begin
                    blit_vram_sel_o     <= 1'b1;                        // setup D addr for write
                    blit_wr_o           <= 1'b1;
                    blit_addr_o         <= blit_dst_D;

                    blit_state          <= WAIT_WR_D;
                end
            end
            WAIT_RD_A: begin
                if (!blit_vram_ack_i) begin                             // read ack received?
                    blit_vram_sel_o     <= 1'b1;                        // keep reading A
                    blit_wr_o           <= 1'b0;
                    blit_addr_o         <= blit_src_A;

                    last_word           <= last_A;

                    blit_state          <= WAIT_RD_A;
                end else begin
                    val_A               <= shift_out;               // set A to shifted read result
                    last_A              <= blit_data_i;             // save any nibbles shifted out
                    if (EN_BLIT_DECR_MODE && blit_ctrl_decrement) begin
                        blit_src_A          <= blit_addr_o - 1'b1;      // update A addr
                    end else begin
                        blit_src_A          <= blit_addr_o + 1'b1;      // update A addr
                    end

                    if (!blit_ctrl_B_const) begin
                        blit_vram_sel_o     <= 1'b1;                    // setup B addr for read
                        blit_wr_o           <= 1'b0;
                        blit_addr_o         <= blit_src_B;

                        last_word           <= last_B;

                        blit_state          <= WAIT_RD_B;
                    end else begin
                        blit_vram_sel_o     <= 1'b1;                    // setup D addr for write
                        blit_wr_o           <= 1'b1;
                        blit_addr_o         <= blit_dst_D;

                        blit_state          <= WAIT_WR_D;
                    end
                end
            end
            WAIT_RD_B: begin
                if (!blit_vram_ack_i) begin                             // read ack received?
                    blit_vram_sel_o     <= 1'b1;                        // keep reading B
                    blit_wr_o           <= 1'b0;
                    blit_addr_o         <= blit_src_B;

                    last_word           <= last_B;

                    blit_state          <= WAIT_RD_B;
                end else begin
                    val_B               <= shift_out;
                    last_B              <= blit_data_i;
                    if (blit_ctrl_C_use_B) begin
                        blit_val_C          <= shift_out;
                    end

                    result_T8    <= { (shift_out[8+:8] != blit_ctrl_transp_T),
                                      (shift_out[8+:8] != blit_ctrl_transp_T),
                                      (shift_out[0+:8] != blit_ctrl_transp_T),
                                      (shift_out[0+:8] != blit_ctrl_transp_T)    };
                    result_T4    <= { (shift_out[12+:4] != blit_ctrl_transp_T[7:4]),
                                      (shift_out[ 8+:4] != blit_ctrl_transp_T[3:0]),
                                      (shift_out[ 4+:4] != blit_ctrl_transp_T[7:4]),
                                      (shift_out[ 0+:4] != blit_ctrl_transp_T[3:0])    };

                    if (EN_BLIT_DECR_MODE && blit_ctrl_decrement) begin
                        blit_src_B          <= blit_addr_o - 1'b1;       // update B addr
                    end else begin
                        blit_src_B          <= blit_addr_o + 1'b1;       // update B addr
                    end

                    blit_vram_sel_o     <= 1'b1;                        // setup D addr for write
                    blit_wr_o           <= 1'b1;
                    blit_addr_o         <= blit_dst_D;

                    blit_state          <= WAIT_WR_D;
                end
            end
            WAIT_WR_D: begin
                if (!blit_vram_ack_i) begin                             // write ack received?
                    blit_vram_sel_o     <= 1'b1;                        // keep writing D
                    blit_wr_o           <= 1'b1;
                    blit_addr_o         <= blit_dst_D;

                    blit_state          <= WAIT_WR_D;
                end else begin
                    if (EN_BLIT_DECR_MODE && blit_ctrl_decrement) begin
                        blit_dst_D          <= blit_addr_o - 1'b1;       // update D addr
                        blit_addr_o         <= blit_addr_o - 1'b1;       // setup VRAM addr for constant write
                    end else begin
                        blit_dst_D          <= blit_addr_o + 1'b1;       // update D addr
                        blit_addr_o         <= blit_addr_o + 1'b1;       // setup VRAM addr for constant write
                    end
                    blit_count          <= blit_count - 1'b1;           // decrement word count

                    blit_first_word     <= '1;                          // clear first word mask

                    if (blit_last_word) begin                           // was that the last word?
                        blit_state          <= LINE_END;                // we are finshed with this line
                    end else if (!blit_ctrl_A_const) begin
                        blit_vram_sel_o     <= 1'b1;                    // setup A addr for read
                        blit_wr_o           <= 1'b0;
                        blit_addr_o         <= blit_src_A;

                        last_word           <= last_A;

                        blit_state          <= WAIT_RD_A;
                    end else if (!blit_ctrl_B_const) begin
                        blit_vram_sel_o     <= 1'b1;                    // setup B addr for read
                        blit_wr_o           <= 1'b0;
                        blit_addr_o         <= blit_src_B;

                        last_word           <= last_B;

                        blit_state          <= WAIT_RD_B;
                    end else begin
                        blit_vram_sel_o     <= 1'b1;                    // setup D addr for write
                        blit_wr_o           <= 1'b1;

                        blit_state          <= WAIT_WR_D;
                    end
                end
            end
            LINE_END: begin
                // update addresses with end of line modulo value
                blit_src_A      <= blit_src_A + blit_mod_A;
                blit_src_B      <= blit_src_B + blit_mod_B;
                blit_dst_D      <= blit_dst_D + blit_mod_D;
                // update constants using modulo value as XOR
                val_A           <= val_A ^ blit_mod_A;
                val_B           <= val_B ^ blit_mod_B;
                blit_val_C      <= blit_val_C ^ blit_mod_C;

                if (blit_last_line) begin
                    blit_done_intr_o    <= 1'b1;
                    if (xreg_blit_queued) begin
                        blit_state          <= SETUP;
                    end else begin
                        blit_state          <= IDLE;
                    end
                end else begin
                    blit_state          <= LINE_BEG;
                end
            end
            default: begin
                blit_state          <= IDLE;
            end
        endcase
    end
end

endmodule
`default_nettype wire               // restore default
