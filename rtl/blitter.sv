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

module blitter(
    // video registers and control
    input  wire logic           xreg_wr_en_i,       // strobe to write internal config register number
    input  wire logic  [3:0]    xreg_num_i,         // internal config register number (for reads)
    input  wire word_t          xreg_data_i,        // data for internal config register
    output      word_t          xreg_data_o,        // register/status data reads
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
logic           xreg_ctrl_B_useA;
logic           xreg_ctrl_C_useB;
logic           xreg_ctrl_decrement;
logic           xreg_ctrl_transp_8b;                // 4-bit/8-bit transparency zero check

logic  [1:0]    xreg_shift_count;
logic  [3:0]    xreg_shift_l_mask;
logic  [3:0]    xreg_shift_r_mask;
word_t          xreg_mod_A;
word_t          xreg_src_A;
word_t          xreg_mod_B;
word_t          xreg_src_B;
word_t          xreg_mod_D;
word_t          xreg_dst_D;
word_t          xreg_val_T;
word_t          xreg_val_C;
logic [14:0]    xreg_lines;                         // "limitation" of 32768 lines
word_t          xreg_words;

logic           xreg_blit_queued;                   // blit operation is queued in xreg registers

// assign status outputs
assign blit_busy_o  = (blit_state != BLIT_IDLE);    // blit operation in progress
assign blit_full_o  = xreg_blit_queued;             // blit register queue full

// blit registers write
always_ff @(posedge clk) begin
    if (reset_i) begin
        xreg_ctrl_A_const   <= '0;
        xreg_ctrl_B_const   <= '0;
        xreg_ctrl_B_useA    <= '0;
        xreg_ctrl_C_useB    <= '0;
        xreg_ctrl_decrement <= '0;
        xreg_ctrl_transp_8b <= '0;
        xreg_shift_count    <= '0;
        xreg_shift_l_mask   <= '0;
        xreg_shift_r_mask   <= '0;
        xreg_val_T          <= '0;
        xreg_mod_A          <= '0;
        xreg_mod_B          <= '0;
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
        if (blit_state == BLIT_SETUP) begin
            xreg_blit_queued     <= 1'b0;
        end

        // blit register write
        if (xreg_wr_en_i) begin
            case ({ 2'b10, xreg_num_i })
                xv::XR_BLIT_CTRL: begin
                    xreg_ctrl_transp_8b <= xreg_data_i[5];
                    xreg_ctrl_decrement <= xreg_data_i[4];
                    xreg_ctrl_C_useB    <= xreg_data_i[3];
                    xreg_ctrl_B_useA    <= xreg_data_i[2];
                    xreg_ctrl_B_const   <= xreg_data_i[1];
                    xreg_ctrl_A_const   <= xreg_data_i[0];
                end
                xv::XR_BLIT_SHIFT: begin
                    xreg_shift_l_mask   <= xreg_data_i[15:12];
                    xreg_shift_r_mask   <= xreg_data_i[11:8];
                    xreg_shift_count    <= xreg_data_i[1:0];
                end
                xv::XR_BLIT_MOD_A: begin
                    xreg_mod_A          <= xreg_data_i;
                end
                xv::XR_BLIT_MOD_B: begin
                    xreg_mod_B          <= xreg_data_i;
                end
                xv::XR_BLIT_VAL_T: begin
                    xreg_val_T          <= xreg_data_i;
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

`ifdef ENABLE_BLIT_REG_READ
// blit registers read
always_ff @(posedge clk) begin
    case ({ 2'b10, xreg_num_i })
        xv::XR_BLIT_CTRL:
            xreg_data_o     <= { 10'b0, xreg_ctrl_transp_8b, xreg_ctrl_decrement, xreg_ctrl_C_useB, xreg_ctrl_B_useA, xreg_ctrl_B_const, xreg_ctrl_A_const};
        xv::XR_BLIT_SHIFT:
            xreg_data_o     <= { xreg_shift_l_mask, xreg_shift_r_mask, 6'b0, xreg_shift_count };
        xv::XR_BLIT_MOD_A:
            xreg_data_o     <= xreg_mod_A;
        xv::XR_BLIT_MOD_B:
            xreg_data_o     <= xreg_mod_B;
        xv::XR_BLIT_VAL_T:
            xreg_data_o     <= xreg_val_T;
        xv::XR_BLIT_MOD_D:
            xreg_data_o     <= xreg_mod_D;
        xv::XR_BLIT_SRC_A:
            xreg_data_o     <= xreg_src_A;
        xv::XR_BLIT_SRC_B:
            xreg_data_o     <= xreg_src_B;
        xv::XR_BLIT_VAL_C:
            xreg_data_o     <= xreg_val_C;
        xv::XR_BLIT_DST_D:
            xreg_data_o     <= xreg_dst_D;
        xv::XR_BLIT_LINES:
            xreg_data_o     <= { 1'b0, xreg_lines} ;
        xv::XR_BLIT_WORDS:
            xreg_data_o     <= xreg_words;
        default:
            xreg_data_o     <= '0;
    endcase
end
`else
assign xreg_data_o     = '0;
`endif

// blitter operational registers (for blit in progress)
logic           blit_ctrl_A_const;
logic           blit_ctrl_B_const;
logic           blit_ctrl_B_useA;
logic           blit_ctrl_C_useB;
logic           blit_ctrl_decrement;
logic           blit_ctrl_transp_8b;
logic  [1:0]    blit_shift_count;
logic  [3:0]    blit_shift_l_mask;
logic  [3:0]    blit_shift_r_mask;
word_t          blit_mod_A;
word_t          blit_mod_B;
word_t          blit_val_T;
word_t          blit_mod_D;
word_t          blit_src_A;
word_t          blit_src_B;
word_t          blit_val_C;
word_t          blit_dst_D;
word_t          blit_lines;             // bit 15 is underflow done flag
word_t          blit_words;

// blitter flags and word counter
logic [16:0]    blit_count;             // word counter (extra underflow bit used line done flag)
logic           blit_first_word;
logic           blit_last_word;
assign          blit_last_word  = blit_count[16];   // underflow flag for last word/last word of line
logic           blit_last_line;
assign          blit_last_line  = blit_lines[15];   // underflow flag for last line (for rectangular blit)

// nibble shifter // TODO: see if any savings by leaving old data vs setting to zero
function automatic [27:0] lsr_4(
        input  [1:0]    nibble_shift,   // 0 to 3 nibbles to shift right
        input [11:0]    shift_in,       // 3 nibbles shifted in (from previous word shift out)
        input word_t    data            // data word to shift
    );
    begin
        case (nibble_shift)
            2'h0:   lsr_4 = { data[15:12],    data[11:8],    data[7:4],     data[3:0],      // word result
                            4'b0,           4'b0,          4'b0};                           // nibbles shifted out
            2'h1:   lsr_4 = { shift_in[11:8], data[15:12],   data[11:8],    data[7:4],      // word result
                            data[3:0],      4'b0,          4'b0};                           // nibbles shifted out
            2'h2:   lsr_4 = { shift_in[11:8], shift_in[7:4], data[15:12],   data[11:8],     // word result
                            data[7:4],      data[3:0],     4'b0};                           // nibbles shifted out
            2'h3:   lsr_4 = { shift_in[11:8], shift_in[7:4], shift_in[3:0], data[15:12],    // word result
                            data[11:8],     data[7:4],     data[3:0] };                     // nibbles shifted out
        endcase
    end
endfunction

logic [27:0]    lsr_A;                  // 0 to 3 nibble right shifted A value
logic [11:0]    lsr_spill_A;            // up to 3 nibbles right shifted out of A, to shift in with next A word
always_comb     lsr_A           = lsr_4(blit_shift_count, lsr_spill_A, blit_data_i);    // shift value read from memory

logic [27:0]    lsr_B;                  // 0 to 3 nibble right shifted B value
logic [11:0]    lsr_spill_B;            // up to 3 nibbles right shifted out of B, to shift in with next B word
always_comb     lsr_B           = lsr_4(blit_shift_count, lsr_spill_B, blit_data_i);    // shift value read from memory

// logic op calculation
word_t          val_A;                  // value read from blit_src_A VRAM or const
word_t          val_B;                  // value read from blit_src_B VRAM or const
word_t          result_B;               // value read from blit_src_B VRAM or const
word_t          result_C;               // value read from blit_src_B VRAM or const
word_t          result_D;               // value to write to blit_dst_D

always_comb     result_B        = blit_ctrl_B_useA ? val_A : val_B;         // effective B term
always_comb     result_C        = blit_ctrl_C_useB ? blit_val_C : val_B;    // effective C term
always_comb     result_D        = val_A & result_B ^ result_C;              // calc logic op result

assign          blit_data_o = result_D; // result_D is output to VRAM

// transparency testing
word_t          val_T;                  // transparency test word (B ^ blit_val_T)
logic  [3:0]    result_T;               // transparency result (4 bit nibble mask)

always_comb begin
    if (blit_ctrl_transp_8b) begin
        result_T = { |val_T[15:8],  |val_T[15:8], |val_T[7:0], |val_T[7:0] };   // 8-bpp test
    end else begin
        result_T = { |val_T[15:12], |val_T[11:8], |val_T[7:4], |val_T[3:0] };   // 4-bpp test
    end
end

assign blit_wr_mask_o   = (blit_first_word ? blit_shift_l_mask : 4'b1111) &     // output VRAM write mask
                          (blit_last_word  ? blit_shift_r_mask : 4'b1111) &
                          result_T;

// blit state machine
typedef enum logic [2:0] {
    BLIT_IDLE,          // wait for blit operation (a write to xreg_blit_count)
    BLIT_SETUP,         // copy xreg registers to blit registers and setup for blit
    BLIT_LINE_START,    // copy update counters, initiate A/B read or D write
    BLIT_WAIT_RD_A,     // wait for A read result, initiate A read, else write result
    BLIT_WAIT_RD_B,     // wait for B read result, initiate A read, else write result
    BLIT_WAIT_WR_D,     // wait for D write, initiate A/B read or D write, loop if more words
    BLIT_LINE_FINISH,   // add modulo values, loop if more lines
    BLIT_DONE
} blit_state_t;

logic  [2:0]    blit_state;

always_ff @(posedge clk) begin
    if (reset_i) begin
        blit_done_intr_o    <= '0;
        blit_vram_sel_o     <= '0;
        blit_wr_o           <= '0;
        blit_addr_o         <= '0;

        blit_state          <= BLIT_IDLE;

        blit_ctrl_A_const   <= '0;
        blit_ctrl_B_const   <= '0;
        blit_ctrl_B_useA    <= '0;
        blit_ctrl_C_useB    <= '0;
        blit_ctrl_transp_8b <= '0;
        blit_ctrl_decrement <= '0;
        blit_shift_l_mask   <= '0;
        blit_shift_r_mask   <= '0;
        blit_shift_count    <= '0;
        blit_mod_A          <= '0;
        blit_mod_B          <= '0;
        blit_val_T          <= '0;
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

    end else begin
        blit_done_intr_o    <= 1'b0;

        blit_vram_sel_o     <= 1'b0;
        blit_wr_o           <= 1'b0;
        blit_addr_o         <= '0;      // TODO: check this cost

        case (blit_state)
            BLIT_IDLE: begin
                if (xreg_blit_queued) begin
                    blit_state          <= BLIT_SETUP;
                end
            end
            BLIT_SETUP: begin
                blit_ctrl_A_const   <= xreg_ctrl_A_const;
                blit_ctrl_B_const   <= xreg_ctrl_B_const;
                blit_ctrl_B_useA    <= xreg_ctrl_B_useA;
                blit_ctrl_C_useB    <= xreg_ctrl_C_useB;
                blit_ctrl_decrement <= xreg_ctrl_decrement;
                blit_ctrl_transp_8b <= xreg_ctrl_transp_8b;
                blit_shift_count    <= xreg_shift_count;
                blit_shift_l_mask   <= xreg_shift_l_mask;
                blit_shift_r_mask   <= xreg_shift_r_mask;
                blit_mod_A          <= xreg_mod_A;
                blit_mod_B          <= xreg_mod_B;
                blit_val_T          <= xreg_val_T;
                blit_mod_D          <= xreg_mod_D;
                blit_src_A          <= xreg_src_A;
                blit_src_B          <= xreg_src_B;
                blit_val_C          <= xreg_val_C;
                blit_dst_D          <= xreg_dst_D;
                blit_lines          <= { 1'b0, xreg_lines };
                blit_words          <= xreg_words;

                blit_first_word     <= 1'b1;
                val_A               <= xreg_src_A;                      // setup for possible use as const
                val_B               <= xreg_src_B;                      // setup for possible use as const
                val_T               <= xreg_src_B ^ xreg_val_T;         // calc const transparency test word

                blit_state          <= BLIT_LINE_START;
            end
            BLIT_LINE_START: begin
                blit_lines          <= blit_lines - 1'b1;               // pre-decrement, bit[15] underflow indicates last line (1-32768)
                blit_count          <= { 1'b0, blit_words }  - 1'b1;    // pre-decrement, bit[16] underflow indicates last word (1-65536)

                if (!blit_ctrl_A_const) begin
                    blit_vram_sel_o     <= 1'b1;                        // setup A addr for read
                    blit_wr_o           <= 1'b0;
                    blit_addr_o         <= blit_src_A;

                    blit_state          <= BLIT_WAIT_RD_A;
                end else if (!blit_ctrl_B_const) begin
                    blit_vram_sel_o     <= 1'b1;                        // setup B addr for read
                    blit_wr_o           <= 1'b0;
                    blit_addr_o         <= blit_src_B;

                    blit_state          <= BLIT_WAIT_RD_B;
                end else begin
                    blit_vram_sel_o     <= 1'b1;                        // setup D addr for write
                    blit_wr_o           <= 1'b1;
                    blit_addr_o         <= blit_dst_D;

                    blit_state          <= BLIT_WAIT_WR_D;
                end
            end
            BLIT_WAIT_RD_A: begin
                if (!blit_vram_ack_i) begin                             // read ack received?
                    blit_vram_sel_o     <= 1'b1;                        // keep reading A
                    blit_wr_o           <= 1'b0;
                    blit_addr_o         <= blit_src_A;
                end else begin
                    val_A               <= lsr_A[27:12];                // set A to shifted read result
                    lsr_spill_A         <= lsr_A[11:0];                 // save any nibbles shifted out
                    if (blit_ctrl_decrement) begin
                        blit_src_A          <= blit_src_A - 1'b1;       // update A addr
                    end else begin
                        blit_src_A          <= blit_src_A + 1'b1;       // update A addr
                    end

                    if (!blit_ctrl_B_const) begin
                        blit_vram_sel_o     <= 1'b1;                    // setup B addr for read
                        blit_wr_o           <= 1'b0;
                        blit_addr_o         <= blit_src_B;

                        blit_state          <= BLIT_WAIT_RD_B;
                    end else begin
                        blit_vram_sel_o     <= 1'b1;                    // setup D addr for write
                        blit_wr_o           <= 1'b1;
                        blit_addr_o         <= blit_dst_D;

                        blit_state          <= BLIT_WAIT_WR_D;
                    end
                end
            end
            BLIT_WAIT_RD_B: begin
                if (!blit_vram_ack_i) begin                             // read ack received?
                    blit_vram_sel_o     <= 1'b1;                        // keep reading B
                    blit_wr_o           <= 1'b0;
                    blit_addr_o         <= blit_src_B;
                end else begin
                    val_B               <= lsr_B[27:12];                // set B to shifted read result
                    val_T               <= lsr_B[27:12] ^ blit_val_T;   // calc transparency test word
                    lsr_spill_B         <= lsr_B[11:0];                 // save any nibbles shifted out
                    if (blit_ctrl_decrement) begin
                        blit_src_B          <= blit_src_B - 1'b1;       // update B addr
                    end else begin
                        blit_src_B          <= blit_src_B + 1'b1;       // update B addr
                    end

                    blit_vram_sel_o     <= 1'b1;                        // setup D addr for write
                    blit_wr_o           <= 1'b1;
                    blit_addr_o         <= blit_dst_D;

                    blit_state          <= BLIT_WAIT_WR_D;
                end
            end
            BLIT_WAIT_WR_D: begin
                if (!blit_vram_ack_i) begin                             // write ack received?
                    blit_vram_sel_o     <= 1'b1;                        // keep writing D
                    blit_wr_o           <= 1'b1;
                    blit_addr_o         <= blit_dst_D;
                end else begin
                    if (blit_ctrl_decrement) begin
                        blit_dst_D          <= blit_dst_D + 1'b1;       // update D addr
                        blit_addr_o         <= blit_dst_D + 1'b1;       // setup VRAM addr for constant write
                    end else begin
                        blit_dst_D          <= blit_dst_D + 1'b1;       // update D addr
                        blit_addr_o         <= blit_dst_D + 1'b1;       // setup VRAM addr for constant write
                    end
                    blit_count          <= blit_count - 1'b1;           // decrement word count

                    blit_first_word     <= 1'b0;                        // clear first word flag

                    if (blit_last_word) begin                           // was that the last word?
                        blit_state          <= BLIT_LINE_FINISH;        // we are finshed with this line
                    end else if (!blit_ctrl_A_const) begin
                        blit_vram_sel_o     <= 1'b1;                    // setup A addr for read
                        blit_wr_o           <= 1'b0;
                        blit_addr_o         <= blit_src_A;

                        blit_state          <= BLIT_WAIT_RD_A;
                    end else if (!blit_ctrl_B_const) begin
                        blit_vram_sel_o     <= 1'b1;                    // setup B addr for read
                        blit_wr_o           <= 1'b0;
                        blit_addr_o         <= blit_src_B;

                        blit_state          <= BLIT_WAIT_RD_B;
                    end else begin
                        blit_vram_sel_o     <= 1'b1;                    // setup D addr for write
                        blit_wr_o           <= 1'b1;

                        blit_state          <= BLIT_WAIT_WR_D;
                    end
                end
            end
            BLIT_LINE_FINISH: begin
                blit_first_word <= 1'b1;
                // update addresses with end of line modulo value
                blit_src_A      <= blit_src_A + blit_mod_A;
                blit_src_B      <= blit_src_B + blit_mod_B;
                blit_dst_D      <= blit_dst_D + blit_mod_D;
                // update constants with nibble addition for A and XOR for B
`ifdef BLIT_ENABLE_CONST_MOD
`ifdef BLIT_ENABLE_CONST_ADD_A
                val_A           <=  {  val_A[15:12] + blit_mod_A[15:12],
                                       val_A[11:8]  + blit_mod_A[11:8],
                                       val_A[7:4]   + blit_mod_A[7:4],
                                       val_A[3:0]   + blit_mod_A[3:0]  };
`else
                val_A           <= val_A ^ blit_mod_A;
`endif
                val_B           <= val_B ^ blit_mod_B;
`endif

                if (blit_last_line) begin
                    blit_done_intr_o    <= 1'b1;
                    if (xreg_blit_queued) begin
                        blit_state          <= BLIT_SETUP;
                    end else begin
                        blit_state          <= BLIT_IDLE;
                    end
                end else begin
                    blit_state          <= BLIT_LINE_START;
                end
            end
            default: begin
                blit_state          <= BLIT_IDLE;
            end
        endcase
    end
end

endmodule
`default_nettype wire               // restore default
