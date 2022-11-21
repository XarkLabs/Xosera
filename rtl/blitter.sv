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
`ifndef EN_SLIM_BLIT

module blitter(
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
`ifdef EN_BLIT_DECR
logic           xreg_ctrl_decrement;
`endif
logic           xreg_ctrl_transp_8b;                // 4-bit/8-bit transparency zero check
logic [7:0]     xreg_ctrl_transp_T;                 // 8-bit transparency value

logic  [1:0]    xreg_shift_amount;
logic  [3:0]    xreg_shift_f_mask;
logic  [3:0]    xreg_shift_l_mask;
word_t          xreg_mod_A;
word_t          xreg_src_A;
word_t          xreg_mod_B;
word_t          xreg_src_B;
`ifdef EN_BLIT_XOR_CONST_C
word_t          xreg_mod_C;
`endif
word_t          xreg_val_C;
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
        xreg_ctrl_A_const   <= '0;
        xreg_ctrl_B_const   <= '0;
        xreg_ctrl_B_not     <= '0;
        xreg_ctrl_C_use_B   <= '0;
`ifdef EN_BLIT_DECR
        xreg_ctrl_decrement <= '0;
`endif
        xreg_ctrl_transp_8b <= '0;
        xreg_ctrl_transp_T  <= '0;
        xreg_shift_amount    <= '0;
        xreg_shift_f_mask   <= '0;
        xreg_shift_l_mask   <= '0;
        xreg_mod_A          <= '0;
        xreg_mod_B          <= '0;
`ifdef EN_BLIT_XOR_CONST_C
        xreg_mod_C          <= '0;
`endif
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
        if (blit_setup) begin
            xreg_blit_queued     <= 1'b0;
        end

        // blit register write
        if (xreg_wr_en_i) begin
            case ({ xv::XR_BLIT_REGS[6:4], xreg_num_i })
                xv::XR_BLIT_CTRL: begin
                    xreg_ctrl_transp_T  <= xreg_data_i[15:8];
                    xreg_ctrl_transp_8b <= xreg_data_i[5];
`ifdef EN_BLIT_DECR
                    xreg_ctrl_decrement <= xreg_data_i[4];
`endif
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
`ifdef EN_BLIT_XOR_CONST_C
                    xreg_mod_C          <= xreg_data_i;
`endif
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
logic           blit_ctrl_A_const;
logic           blit_ctrl_B_const;
logic           blit_ctrl_B_not;
logic           blit_ctrl_C_use_B;
`ifdef EN_BLIT_DECR
logic           blit_ctrl_decrement;
`endif
logic           blit_ctrl_transp_8b;
logic  [7:0]    blit_ctrl_transp_T;
logic  [1:0]    blit_shift_amount;
logic  [3:0]    blit_shift_f_mask;
logic  [3:0]    blit_shift_l_mask;
word_t          blit_mod_A;
word_t          blit_mod_B;
`ifdef EN_BLIT_XOR_CONST_C
word_t          blit_mod_C;
`endif
word_t          blit_mod_D;
word_t          blit_src_A;
word_t          blit_src_B;
word_t          blit_val_C;
word_t          blit_dst_D;

word_t          blit_lines;             // bit 15 is underflow done flag
word_t          blit_words;
logic [16:0]    blit_count;             // word counter (extra underflow bit used line done flag)

word_t          val_A;                  // value read from blit_src_A VRAM or const
word_t          val_B;                  // value read from blit_src_B VRAM (or NOT of value if notB) or const
word_t          val_B_prime;            // value read from blit_src_B VRAM (or NOT of value if notB) or const
word_t          last_A;                 // last A word save
word_t          last_B;                 // last B word save
logic [ 3:0]    blit_f_mask;
logic           blit_done_intr;

logic           blit_vram_sel, blit_vram_sel_next;    // vram select
logic           blit_wr, blit_wr_next;          // blit write
addr_t          blit_addr, blit_addr_next;        // VRAM address out

// combinitorial FSM signals
word_t          blit_src_A_next;
word_t          blit_src_B_next;
word_t          blit_val_C_next;
word_t          blit_dst_D_next;
word_t          val_A_next;                  // value read from blit_src_A VRAM or const
word_t          val_B_next;                  // value read from blit_src_B VRAM (or NOT of value if notB) or const
word_t          last_A_next;                 // last A word save
word_t          last_B_next;                 // last B word save
word_t          blit_lines_next;             // bit 15 is underflow done flag
logic [16:0]    blit_count_next;             // word counter (extra underflow bit used line done flag)
logic [ 3:0]    blit_f_mask_next;
logic           blit_done_intr_next;

// blitter flags and word counter

logic           blit_last_word;
assign          blit_last_word  = blit_count[16];   // underflow flag for last word/last word of line
logic           blit_last_line;
assign          blit_last_line  = blit_lines[15];   // underflow flag for last line (for rectangular blit)

// nibble shifter
function automatic word_t shifter(
`ifdef EN_BLIT_DECR_LSHIFT
        input           decr,           // 1'b0 = increment, 1'b1 = decrement
`endif
        input  [1:0]    shift_amount,   // nibbles to shift (right for inc, left for dec)
        input [15:0]    data_word,      // incoming data word to shift
`ifdef EN_BLIT_DECR_LSHIFT
        input [15:0]    prev_word       // previous data word to shift in
`else
        input [11:0]    prev_word       // previous data word to shift in
`endif
    );
    begin
`ifdef EN_BLIT_DECR_LSHIFT
        if (decr) begin
            case (shift_amount)
                // left shift (decrement)
                2'b00:  shifter =   {   data_word[0*4+:4],  prev_word[3*4+:4],  prev_word[2*4+:4],  prev_word[1*4+:4]   };  // Dabc
                2'b01:  shifter =   {   data_word[1*4+:4],  data_word[0*4+:4],  prev_word[3*4+:4],  prev_word[2*4+:4]   };  // CDab
                2'b10:  shifter =   {   data_word[2*4+:4],  data_word[1*4+:4],  data_word[0*4+:4],  prev_word[3*4+:4]   };  // BCDa
                2'b11:  shifter =   {   data_word[3*4+:4],  data_word[2*4+:4],  data_word[1*4+:4],  data_word[0*4+:4]   };  // ABCD
            endcase
        end else
`endif
        begin
            case (shift_amount)
                // right shift (increment)
                2'b00:  shifter =   {   data_word[3*4+:4],  data_word[2*4+:4],  data_word[1*4+:4],  data_word[0*4+:4]   };  // ABCD
                2'b01:  shifter =   {   prev_word[0*4+:4],  data_word[3*4+:4],  data_word[2*4+:4],  data_word[1*4+:4]   };  // dABC
                2'b10:  shifter =   {   prev_word[1*4+:4],  prev_word[0*4+:4],  data_word[3*4+:4],  data_word[2*4+:4]   };  // cdAB
                2'b11:  shifter =   {   prev_word[2*4+:4],  prev_word[1*4+:4],  prev_word[0*4+:4],  data_word[3*4+:4]   };  // bcdA
            endcase
        end
    end
endfunction

// transparency testing
logic  [3:0]    result_T4;               // transparency result (4 bit nibble mask)
logic  [3:0]    result_T8;               // transparency result (4 bit nibble mask)

assign  result_T4       = { (val_B[12+:4] != blit_ctrl_transp_T[7:4]),
                            (val_B[ 8+:4] != blit_ctrl_transp_T[3:0]),
                            (val_B[ 4+:4] != blit_ctrl_transp_T[7:4]),
                            (val_B[ 0+:4] != blit_ctrl_transp_T[3:0])    };
assign  result_T8       = { (val_B[8+:8] != blit_ctrl_transp_T),
                            (val_B[8+:8] != blit_ctrl_transp_T),
                            (val_B[0+:8] != blit_ctrl_transp_T),
                            (val_B[0+:8] != blit_ctrl_transp_T)    };

assign blit_vram_sel_o  = blit_vram_sel;
assign blit_wr_o        = blit_wr;
assign blit_wr_mask_o   = blit_f_mask &     // output VRAM write mask
                          (blit_last_word  ? blit_shift_l_mask : 4'b1111) &
                          (blit_ctrl_transp_8b ? result_T8 : result_T4);
assign blit_addr_o      = blit_addr;

assign blit_done_intr_o = blit_done_intr;

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

assign  blit_data_o = val_A & val_B_prime ^ blit_val_C;

always_ff @(posedge clk) begin
    if (reset_i) begin
        blit_state          <= IDLE;

        blit_ctrl_A_const   <= '0;
        blit_ctrl_B_const   <= '0;
        blit_ctrl_B_not     <= '0;
        blit_ctrl_C_use_B   <= '0;
`ifdef EN_BLIT_DECR
        blit_ctrl_decrement <= '0;
`endif
        blit_ctrl_transp_8b <= '0;
        blit_ctrl_transp_T  <= '0;
        blit_shift_f_mask   <= '0;
        blit_shift_l_mask   <= '0;
        blit_shift_amount   <= '0;
        blit_mod_A          <= '0;
        blit_mod_B          <= '0;
`ifdef EN_BLIT_XOR_CONST_C
        blit_mod_C          <= '0;
`endif
        blit_mod_D          <= '0;
        blit_src_A          <= '0;
        blit_src_B          <= '0;
        blit_val_C          <= '0;
        blit_dst_D          <= '0;
        blit_lines          <= '0;
        blit_words          <= '0;
        blit_count          <= '0;

        blit_vram_sel       <= '0;
        blit_wr             <= '0;
        blit_addr           <= '0;
        blit_f_mask         <= '0;
        val_A               <= '0;
        val_B               <= '0;
        val_B_prime         <= '0;
        last_A              <= '0;
        last_B              <= '0;

    end else begin

        // only advance state if vram not selected, or ack'd
        if (!blit_vram_sel || blit_vram_ack_i) begin
            blit_state      <= blit_state_next;

            blit_vram_sel       <= blit_vram_sel_next;
            blit_wr             <= blit_wr_next;
            blit_addr           <= blit_addr_next;
            blit_src_A          <= blit_src_A_next;
            blit_src_B          <= blit_src_B_next;
            blit_dst_D          <= blit_dst_D_next;
            val_A               <= val_A_next;
            val_B               <= val_B_next;
            if (blit_ctrl_C_use_B) begin
                blit_val_C          <= val_B_next;
            end else begin
                blit_val_C          <= blit_val_C_next;
            end
            if (blit_ctrl_B_not) begin
                val_B_prime         <= ~val_B_next;
            end else begin
                val_B_prime         <= val_B_next;
            end
            last_A              <= last_A_next;
            last_B              <= last_B_next;
            blit_f_mask         <= blit_f_mask_next;
            blit_lines          <= blit_lines_next;
            blit_count          <= blit_count_next;
            blit_done_intr      <= blit_done_intr_next;
        end

        if (blit_setup) begin
            blit_ctrl_A_const   <= xreg_ctrl_A_const;
            blit_ctrl_B_const   <= xreg_ctrl_B_const;
            blit_ctrl_B_not     <= xreg_ctrl_B_not;
            blit_ctrl_C_use_B   <= xreg_ctrl_C_use_B;
`ifdef EN_BLIT_DECR
            blit_ctrl_decrement <= xreg_ctrl_decrement;
`endif
            blit_ctrl_transp_8b <= xreg_ctrl_transp_8b;
            blit_ctrl_transp_T  <= xreg_ctrl_transp_T;
            blit_shift_amount    <= xreg_shift_amount;
            blit_shift_f_mask   <= xreg_shift_f_mask;
            blit_shift_l_mask   <= xreg_shift_l_mask;
            blit_mod_A          <= xreg_mod_A;
            blit_mod_B          <= xreg_mod_B;
`ifdef EN_BLIT_XOR_CONST_C
            blit_mod_C          <= xreg_mod_C;
`endif
            blit_mod_D          <= xreg_mod_D;
            blit_src_A          <= xreg_src_A;
            blit_src_B          <= xreg_src_B;
            blit_val_C          <= xreg_val_C;
            blit_dst_D          <= xreg_dst_D;
            blit_lines          <= xreg_lines;
            blit_words          <= xreg_words;

            val_A               <= xreg_src_A;                      // setup for possible use as const
            val_B               <= xreg_src_B;                      // setup for possible use as const
        end
    end
end

// blit state machine
typedef enum logic [2:0] {
    IDLE,           // wait for blit operation (a write to xreg_blit_count)
    SETUP,          // copy xreg registers to blit registers and setup for blit
    LINE_BEG,       // copy update counters, initiate A/B read or D write
    RD_A,           // do A read
    RD_B,           // do A read
    WR_D,           // do D write
    LINE_END        // add modulo values, loop if more lines
} blit_state_t;

blit_state_t    blit_state, blit_state_next;

always_comb begin

    blit_vram_sel_next  = '0;               // vram select
    blit_wr_next        = '0;               // blit write
    blit_addr_next      = blit_addr;        // VRAM address out
    blit_f_mask_next    = blit_f_mask;
    blit_src_A_next     = blit_src_A;
    blit_src_B_next     = blit_src_B;
    blit_val_C_next     = blit_val_C;
    blit_dst_D_next     = blit_dst_D;
    val_A_next          = val_A;
    val_B_next          = val_B;
    last_A_next         = last_A;
    last_B_next         = last_B;
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

            blit_lines_next     = blit_lines - 1'b1;               // pre-decrement, bit[15] underflow indicates last line (1-32768)
            blit_count_next     = { 1'b0, blit_words }  - 1'b1;    // pre-decrement, bit[16] underflow indicates last word (1-65536)

            if (!blit_ctrl_A_const) begin
                blit_vram_sel_next  = 1'b1;                        // setup A addr for read
                blit_wr_next        = 1'b0;
                blit_addr_next      = blit_src_A;

                blit_state_next     = RD_A;
            end else if (!blit_ctrl_B_const) begin
                blit_vram_sel_next  = 1'b1;                    // setup B addr for read
                blit_wr_next        = 1'b0;
                blit_addr_next      = blit_src_B;

                blit_state_next     = RD_B;
            end else begin
                blit_vram_sel_next  = 1'b1;                    // setup D addr for write
                blit_wr_next        = 1'b1;
                blit_addr_next      = blit_dst_D;

                blit_state_next     = WR_D;
            end
        end
        RD_A: begin
`ifdef EN_BLIT_DECR_LSHIFT
            val_A_next          = shifter(blit_ctrl_decrement, blit_shift_amount, blit_data_i, last_A);
`else
            val_A_next          = shifter(blit_shift_amount, blit_data_i, 12'(last_A));
`endif
            last_A_next         = blit_data_i;

`ifdef EN_BLIT_DECR
            if (blit_ctrl_decrement) begin
                blit_src_A_next     = blit_addr - 1'b1;      // update A addr
            end else
`endif
            begin
                blit_src_A_next     = blit_addr + 1'b1;      // update A addr
            end

            if (!blit_ctrl_B_const) begin
                blit_vram_sel_next  = 1'b1;                    // setup B addr for read
                blit_wr_next        = 1'b0;
                blit_addr_next      = blit_src_B;

                blit_state_next     = RD_B;
            end else begin
                blit_vram_sel_next  = 1'b1;                    // setup D addr for write
                blit_wr_next        = 1'b1;
                blit_addr_next      = blit_dst_D;

                blit_state_next     = WR_D;
            end
        end
        RD_B: begin
`ifdef EN_BLIT_DECR_LSHIFT
            val_B_next          = shifter(blit_ctrl_decrement, blit_shift_amount, blit_data_i, last_B);
`else
            val_B_next          = shifter(blit_shift_amount, blit_data_i, 12'(last_B));
`endif
            last_B_next         = blit_data_i;

`ifdef EN_BLIT_DECR
            if (blit_ctrl_decrement) begin
                blit_src_B_next     = blit_addr - 1'b1;      // update A addr
            end else
`endif
            begin
                blit_src_B_next     = blit_addr + 1'b1;      // update A addr
            end

            blit_vram_sel_next  = 1'b1;                    // setup D addr for write
            blit_wr_next        = 1'b1;
            blit_addr_next      = blit_dst_D;

            blit_state_next     = WR_D;
        end
        WR_D: begin
`ifdef EN_BLIT_DECR
            if (blit_ctrl_decrement) begin
                blit_dst_D_next     = blit_addr - 1'b1;       // update D addr
                blit_addr_next      = blit_addr - 1'b1;       // setup VRAM addr for constant write
            end else
`endif
            begin
                blit_dst_D_next     = blit_addr + 1'b1;       // update D addr
                blit_addr_next      = blit_addr + 1'b1;       // setup VRAM addr for constant write
            end
            blit_count_next     = blit_count - 1'b1;           // decrement word count
            blit_f_mask_next    = '1;                          // clear first word mask

            if (blit_last_word) begin                           // was that the last word?
                blit_vram_sel_next  = 1'b0;                        // setup A addr for read
                blit_wr_next        = 1'b0;

                blit_state_next     = LINE_END;                // we are finshed with this line
            end else if (!blit_ctrl_A_const) begin
                blit_vram_sel_next  = 1'b1;                        // setup A addr for read
                blit_wr_next        = 1'b0;
                blit_addr_next      = blit_src_A;

                blit_state_next     = RD_A;
            end else if (!blit_ctrl_B_const) begin
                blit_vram_sel_next  = 1'b1;                    // setup B addr for read
                blit_wr_next        = 1'b0;
                blit_addr_next      = blit_src_B;

                blit_state_next     = RD_B;
            end else begin
                blit_vram_sel_next  = 1'b1;                    // setup D addr for write
                blit_wr_next        = 1'b1;

                blit_state_next     = WR_D;
            end
        end
        LINE_END: begin
            // update addresses with end of line modulo value
            blit_src_A_next = blit_src_A + blit_mod_A;
            blit_src_B_next = blit_src_B + blit_mod_B;
            blit_dst_D_next = blit_dst_D + blit_mod_D;

`ifdef EN_BLIT_XOR_CONST_AB
            // update constants using modulo value as XOR
            val_A_next      = val_A ^ blit_mod_A;
            val_B_next      = val_B ^ blit_mod_B;
`endif
`ifdef EN_BLIT_XOR_CONST_C
            blit_val_C_next = blit_val_C ^ blit_mod_C;
`endif

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
`endif
`default_nettype wire               // restore default
