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
/* verilator lint_off UNUSED */
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
/* verilator lint_on UNUSED */
);

typedef enum logic [1:0] {
    OP_A_XOR_C           = 2'b00,       // D = A ^ C        (B can still be transparency)
    OP_A_ADD_B_XOR_C     = 2'b01,       // D = (A + B) ^ C  (nibble addition, no carries)
    OP_A_AND_B_XOR_C     = 2'b10,       // D = A & B ^ C;
    OP_A_XOR_B_XOR_C     = 2'b11        // D = A ^ B ^ C;
} blit_op_t;

typedef enum logic [1:0] {
    TRANS_A_4B    = 2'b00,
    TRANS_B_4B    = 2'b01,
    TRANS_A_8B    = 2'b10,
    TRANS_B_8B    = 2'b11
} blit_trans_t;

// blitter xreg register data (holds "queued" blit)
logic           xreg_ctrl_A_const;
logic           xreg_ctrl_B_const;
logic  [1:0]    xreg_ctrl_transp;
logic  [2:0]    xreg_ctrl_op;
logic           xreg_ctrl_fast;

logic  [1:0]    xreg_shift;
word_t          xreg_mod_A;
word_t          xreg_mod_B;
word_t          xreg_mod_C;
word_t          xreg_mod_D;
word_t          xreg_src_A;
word_t          xreg_src_B;
word_t          xreg_val_C;
word_t          xreg_dst_D;
word_t          xreg_lines;
word_t          xreg_count;

logic           xreg_blit_queued;                   // blit operation is queued in xreg registers

// assign status outputs
assign blit_busy_o  = (blit_state != BLIT_IDLE);    // blit operation in progress
assign blit_full_o  = xreg_blit_queued;             // blit register queue full

// blit registers write
always_ff @(posedge clk) begin
    if (reset_i) begin
        xreg_blit_queued    <= '0;

        xreg_ctrl_A_const   <= '0;
        xreg_ctrl_B_const   <= '0;
        xreg_ctrl_transp    <= '0;
        xreg_ctrl_op        <= '0;
        xreg_ctrl_fast      <= '0;
        xreg_shift          <= '0;
        xreg_mod_A          <= '0;
        xreg_mod_B          <= '0;
        xreg_mod_C          <= '0;
        xreg_mod_D          <= '0;
        xreg_src_A          <= '0;
        xreg_src_B          <= '0;
        xreg_val_C          <= '0;
        xreg_dst_D          <= '0;
        xreg_lines          <= '0;
        xreg_count          <= '0;
    end else begin
        // clear queued blit when state machine copies xreg data
        if (blit_state == BLIT_SETUP) begin
            xreg_blit_queued     <= 1'b0;
        end

        // blit register write
        if (xreg_wr_en_i) begin
            case ({ 2'b10, xreg_num_i })
                xv::XR_BLIT_CTRL: begin
                    xreg_ctrl_fast      <= xreg_data_i[7];
                    xreg_ctrl_op        <= xreg_data_i[6:4];
                    xreg_ctrl_transp    <= xreg_data_i[3:2];
                    xreg_ctrl_B_const   <= xreg_data_i[1];
                    xreg_ctrl_A_const   <= xreg_data_i[0];
                end
                xv::XR_BLIT_SHIFT: begin
                    xreg_shift     <= xreg_data_i[1:0];
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
                    xreg_lines          <= xreg_data_i;
                end
                xv::XR_BLIT_COUNT: begin
                    xreg_count          <= xreg_data_i;
                    xreg_blit_queued    <= 1'b1;
                end
                default: begin
                end
            endcase
        end
    end
end

// blit registers read
always_ff @(posedge clk) begin
    case ({ 2'b10, xreg_num_i })
        xv::XR_BLIT_CTRL:
            xreg_data_o     <= { 8'b0, xreg_ctrl_fast, xreg_ctrl_op,
                                 xreg_ctrl_transp, xreg_ctrl_B_const, xreg_ctrl_A_const };
        xv::XR_BLIT_SHIFT:
            xreg_data_o     <= { 14'b0, xreg_shift };
        xv::XR_BLIT_MOD_A:
            xreg_data_o     <= xreg_mod_A;
        xv::XR_BLIT_MOD_B:
            xreg_data_o     <= xreg_mod_B;
        xv::XR_BLIT_MOD_C:
            xreg_data_o     <= xreg_mod_C;
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
            xreg_data_o     <= xreg_lines;
        xv::XR_BLIT_COUNT:
            xreg_data_o     <= xreg_count;
        default:
            xreg_data_o     <= 16'h0000;
    endcase
end

// blitter operational registers (for blit in progress)
logic  [3:0]    blit_state;

/* verilator lint_off UNUSED */
logic           blit_ctrl_A_const;
logic           blit_ctrl_B_const;
logic  [1:0]    blit_ctrl_transp;
logic  [2:0]    blit_ctrl_op;
logic           blit_ctrl_fast;
logic  [3:0]    blit_mask_left;
logic  [2:0]    blit_mask_right;
logic  [1:0]    blit_shift;
word_t          blit_mod_A;
word_t          blit_mod_B;
word_t          blit_mod_C;
word_t          blit_mod_D;
word_t          blit_src_A;
word_t          blit_src_B;
word_t          blit_val_C;
word_t          blit_dst_D;
word_t          blit_lines;                 // bit 15 is underflow done flag
word_t          blit_width;
logic [16:0]    blit_count;                 // extra bit for underflow done flag
/* verilator lint_on UNUSED */

word_t          val_A;
word_t          val_B;
word_t          val_D;

logic           blit_first_word;
logic           blit_last_word;
assign          blit_last_word  = blit_count[16];   // underflow flag for last word/last word of line
logic           blit_last_line;
assign          blit_last_line  = blit_lines[15];   // underflow flag for last line (for rectangular blit)

// blit shifter // TODO: make a function

function automatic [27:0] lsr(
        input  [1:0]    nibble_shift,
        input [11:0]    shift_in,     // nibbles shifted out of previous word
        input word_t    data        // data word to shift
    );
    begin
        case (nibble_shift)
            2'h0:   lsr = { data[15:12], data[11:8], data[7:4], data[3:0],
                            4'b0, 4'b0, 4'b0};
            2'h1:   lsr = { shift_in[11:8], data[15:12], data[11:8], data[7:4],
                            data[3:0], 4'b0, 4'b0};
            2'h2:   lsr = { shift_in[11:8], shift_in[7:4], data[15:12], data[11:8],
                            data[7:4], data[3:0], 4'b0};
            2'h3:   lsr = { shift_in[11:8], shift_in[7:4], shift_in[3:0], data[15:12],
                            data[11:8], data[7:4], data[3:0] };
        endcase
    end
endfunction


logic [27:0]    lsr_A;               // 0 to 3 nibble shifted A value
logic [27:0]    lsr_B;               // 0 to 3 nibble shifted A value
logic [11:0]    lsr_out_A;           // up to 3 nibbles shifted out of A, to shift into next A word
logic [11:0]    lsr_out_B;           // up to 3 nibbles shifted out of A, to shift into next A word

assign  lsr_A = lsr(blit_shift, lsr_out_A, blit_data_i);
assign  lsr_B = lsr(blit_shift, lsr_out_B, blit_data_i);

always_comb begin : logic_ops
    case (blit_ctrl_op[1:0])
        2'b00: val_D   = val_A ^ blit_val_C;            // B can be used for transparency
        2'b01: val_D   = val_A & val_B ^ blit_val_C;
        2'b10: val_D   = val_A ^ val_B ^ blit_val_C;
        2'b11: val_D   = {  val_A[15:12] + val_B[15:12],
                            val_A[11:8]  + val_B[11:8],
                            val_A[7:4]   + val_B[ 7:4],
                            val_A[3:0]   + val_B[ 3:0]
                         } ^ blit_val_C;
    endcase
end

// transparency
logic  [3:0]    blit_mask_trans;

always_comb begin
    case (blit_ctrl_transp)
        TRANS_A_4B: blit_mask_trans = {
                        |val_A[15:12],
                        |val_A[11:8],
                        |val_A[7:4],
                        |val_A[3:0]
                    };
        TRANS_A_8B: blit_mask_trans = {
                        |val_A[15:8],
                        |val_A[15:8],
                        |val_A[7:0],
                        |val_A[7:0]
                    };
        TRANS_B_4B: blit_mask_trans = {
                        |val_B[15:12],
                        |val_B[11:8],
                        |val_B[7:4],
                        |val_B[3:0]
                    };
        TRANS_B_8B: blit_mask_trans = {
                        |val_B[15:8],
                        |val_B[15:8],
                        |val_B[7:0],
                        |val_B[7:0]
                    };
    endcase
end

// output VRAM write mask (for left/right mask and transparency)
assign blit_wr_mask_o   = (blit_first_word ? blit_mask_left  : 4'b1111) &
                          (blit_last_word  ? {blit_mask_right, 1'b1 } : 4'b1111) &
                          blit_mask_trans;

typedef enum logic [3:0] {
    BLIT_IDLE,
    BLIT_SETUP,
    BLIT_RD,
    BLIT_WAIT_RD_B,
    BLIT_WAIT_RD_A,
    BLIT_EXEC_OP,
    BLIT_WAIT_WR_D,
    FILL_WR_D,
    FILL_WAIT_WR_D,
    COPY_RD_A,
    COPY_WAIT_RD_A,
    COPY_WAIT_WR_D,
    LINE_DONE,
    BLIT_DONE
} blit_state_t;

// blit state machine
always_ff @(posedge clk) begin
    if (reset_i) begin
        blit_done_intr_o    <= '0;
        blit_vram_sel_o     <= '0;
        blit_wr_o           <= '0;
        blit_addr_o         <= '0;
        blit_data_o         <= '0;

        blit_state          <= BLIT_IDLE;

        blit_ctrl_A_const   <= '0;
        blit_ctrl_B_const   <= '0;
        blit_ctrl_transp    <= '0;
        blit_ctrl_op        <= '0;
        blit_ctrl_fast      <= '0;
        blit_mask_left      <= '0;
        blit_mask_right     <= '0;
        blit_shift          <= '0;
        blit_mod_A          <= '0;
        blit_mod_B          <= '0;
        blit_mod_C          <= '0;
        blit_mod_D          <= '0;
        blit_src_A          <= '0;
        blit_src_B          <= '0;
        blit_val_C          <= '0;
        blit_dst_D          <= '0;
        blit_lines          <= '0;
        blit_width          <= '0;
        blit_count          <= '0;

        blit_first_word     <= '0;
        val_A               <= '0;
        val_B               <= '0;

    end else begin
        blit_done_intr_o    <= 1'b0;

        case (blit_state)
            BLIT_IDLE: begin
                if (xreg_blit_queued) begin
                    blit_state          <= BLIT_SETUP;
                end
            end
            BLIT_SETUP: begin
                blit_ctrl_A_const   <= xreg_ctrl_A_const;
                blit_ctrl_B_const   <= xreg_ctrl_B_const;
                blit_ctrl_transp    <= xreg_ctrl_transp;
                blit_ctrl_op        <= xreg_ctrl_op;
                blit_ctrl_fast      <= xreg_ctrl_fast;
                blit_shift          <= xreg_shift;
                blit_mod_A          <= xreg_mod_A;
                blit_mod_B          <= xreg_mod_B;
                blit_mod_C          <= xreg_mod_C;
                blit_mod_D          <= xreg_mod_D;
                blit_src_A          <= xreg_src_A;
                blit_src_B          <= xreg_src_B;
                blit_val_C          <= xreg_val_C;
                blit_dst_D          <= xreg_dst_D;
                blit_lines          <= xreg_lines;
                blit_width          <= xreg_count;

                blit_first_word     <= 1'b1;
                val_A               <= blit_src_A;
                val_B               <= blit_src_B;

                case (xreg_shift)
                    2'h0:   { blit_mask_left, blit_mask_right } <= 7'b1111000;
                    2'h1:   { blit_mask_left, blit_mask_right } <= 7'b0111100;
                    2'h2:   { blit_mask_left, blit_mask_right } <= 7'b0011110;
                    2'h3:   { blit_mask_left, blit_mask_right } <= 7'b0001111;
                endcase

                if (xreg_ctrl_fast & xreg_ctrl_A_const) begin
                    blit_state          <= FILL_WR_D;
                end else if (xreg_ctrl_fast) begin
                    blit_state          <= COPY_RD_A;
                end else begin
                    blit_state          <= BLIT_RD;
                end
            end
            BLIT_RD: begin
                blit_lines          <= blit_lines - 1'b1;
                blit_count          <= { 1'b0, blit_width } - 1'b1;

                if (!blit_ctrl_B_const) begin
                    blit_vram_sel_o     <= 1'b1;
                    blit_wr_o           <= 1'b0;
                    blit_addr_o         <= blit_src_B;
                    blit_src_B          <= blit_src_B + 1'b1;

                    blit_state          <= BLIT_WAIT_RD_B;
                end else begin
                    blit_vram_sel_o     <= 1'b1;
                    blit_wr_o           <= 1'b0;
                    blit_addr_o         <= blit_src_A;
                    blit_src_A          <= blit_src_A + 1'b1;

                    blit_state          <= BLIT_WAIT_RD_A;
                end
            end
            BLIT_WAIT_RD_B: begin
                if (blit_vram_ack_i) begin
                    val_B               <= lsr_B[27:12];
                    lsr_out_B           <= lsr_B[11:0];

                    blit_vram_sel_o     <= 1'b1;
                    blit_wr_o           <= 1'b0;
                    blit_addr_o         <= blit_src_A;
                    blit_src_A          <= blit_src_A + 1'b1;

                    blit_state          <= BLIT_WAIT_RD_A;
                end
            end
            BLIT_WAIT_RD_A: begin
                if (blit_vram_ack_i) begin
                    val_A               <= lsr_A[27:12];
                    lsr_out_A           <= lsr_A[11:0];

                    blit_vram_sel_o     <= 1'b0;
                    blit_wr_o           <= 1'b0;

                    blit_state          <= BLIT_EXEC_OP;
                end
            end
            BLIT_EXEC_OP: begin
                blit_vram_sel_o     <= 1'b1;
                blit_wr_o           <= 1'b1;
                blit_data_o         <= val_D;
                blit_addr_o         <= blit_dst_D;
                blit_dst_D          <= blit_dst_D + 1'b1;

                blit_state          <= BLIT_WAIT_WR_D;
            end
            BLIT_WAIT_WR_D: begin
                if (blit_vram_ack_i) begin
                    blit_first_word     <= 1'b0;
                    blit_count          <= blit_count - 1'b1;

                    if (blit_last_word) begin
                        blit_vram_sel_o     <= 1'b0;
                        blit_wr_o           <= 1'b0;
                        blit_state          <= LINE_DONE;
                    end else if (!blit_ctrl_B_const) begin
                        blit_vram_sel_o     <= 1'b1;
                        blit_wr_o           <= 1'b0;
                        blit_addr_o         <= blit_src_B;
                        blit_src_B          <= blit_src_B + 1'b1;

                        blit_state          <= BLIT_WAIT_RD_B;
                    end else begin
                        blit_vram_sel_o     <= 1'b1;
                        blit_wr_o           <= 1'b0;
                        blit_addr_o         <= blit_src_A;
                        blit_src_A          <= blit_src_A + 1'b1;

                        blit_state          <= BLIT_WAIT_RD_A;
                    end
                end
            end
            FILL_WR_D: begin
                blit_first_word     <= 1'b1;
                blit_lines          <= blit_lines - 1'b1;

                blit_vram_sel_o     <= 1'b1;
                blit_wr_o           <= 1'b1;
                blit_data_o         <= val_A;
                blit_addr_o         <= blit_dst_D;
                blit_dst_D          <= blit_dst_D + 1'b1;

                blit_state          <= FILL_WAIT_WR_D;
            end
            FILL_WAIT_WR_D: begin
                if (blit_vram_ack_i) begin
                    blit_first_word     <= 1'b0;
                    blit_count          <= blit_count - 1'b1;
                    blit_vram_sel_o     <= 1'b1;
                    blit_wr_o           <= 1'b1;
                    blit_data_o         <= val_A;
                    blit_addr_o         <= blit_dst_D;
                    blit_dst_D          <= blit_dst_D + 1'b1;
                    if (blit_last_word) begin
                        blit_vram_sel_o     <= 1'b0;
                        blit_wr_o           <= 1'b0;
                        blit_state          <= LINE_DONE;
                    end else begin
                        blit_state          <= FILL_WAIT_WR_D;
                    end
                end
            end
            COPY_RD_A: begin
                blit_vram_sel_o     <= 1'b1;
                blit_wr_o           <= 1'b0;
                blit_addr_o         <= blit_src_A;
                blit_src_A          <= blit_src_A + 1'b1;

                blit_first_word     <= 1'b1;
                blit_lines          <= blit_lines - 1'b1;

                blit_state          <= COPY_RD_A;
            end
            COPY_WAIT_RD_A: begin
                if (blit_vram_ack_i) begin
                    blit_vram_sel_o     <= 1'b1;
                    blit_wr_o           <= 1'b1;
                    blit_data_o         <= lsr_A[27:12] ^ blit_val_C;
                    lsr_out_A           <= lsr_A[11:0];
                    blit_addr_o         <= blit_dst_D;
                    blit_dst_D          <= blit_dst_D + 1'b1;
                    blit_count          <= blit_count - 1'b1;

                    blit_state          <= COPY_WAIT_WR_D;
                end
            end
            COPY_WAIT_WR_D: begin
                if (blit_vram_ack_i) begin
                    blit_first_word     <= 1'b0;
                    blit_vram_sel_o     <= 1'b1;
                    blit_wr_o           <= 1'b1;
                    blit_addr_o         <= blit_dst_D;
                    blit_dst_D          <= blit_dst_D + 1'b1;
                    blit_count          <= blit_count - 1'b1;
                    if (blit_last_word) begin
                        blit_vram_sel_o     <= 1'b0;
                        blit_wr_o           <= 1'b0;
                        blit_state          <= LINE_DONE;
                    end else begin
                        blit_state          <= COPY_WAIT_RD_A;
                    end
                end
            end
            LINE_DONE: begin
                blit_src_A  <= blit_src_A + blit_mod_A;
                blit_src_B  <= blit_src_B + blit_mod_B;
//                blit_val_C  <= blit_val_C + blit_mod_C;
                blit_dst_D  <= blit_dst_D + blit_mod_D;

                if (blit_last_line) begin
                    blit_state          <= BLIT_DONE;
                end else if (blit_ctrl_fast & blit_ctrl_A_const) begin
                    blit_state          <= FILL_WR_D;
                end else if (blit_ctrl_fast) begin
                    blit_state          <= COPY_RD_A;
                end else begin
                    blit_state          <= BLIT_RD;
                end
            end
            BLIT_DONE: begin
                blit_done_intr_o    <= 1'b1;

                if (xreg_blit_queued) begin
                    blit_state          <= BLIT_SETUP;
                end else begin
                    blit_state          <= BLIT_IDLE;
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
