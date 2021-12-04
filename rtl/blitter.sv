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
logic  [1:0]    xreg_ctrl_op;
logic           xreg_ctrl_A_const;
logic           xreg_ctrl_B_const;
logic           xreg_ctrl_B_XOR_A;
logic           xreg_ctrl_transp_8b;
logic           xreg_ctrl_transp_B;

logic  [1:0]    xreg_shift;
logic  [3:0]    xreg_shift_l_mask;
logic  [3:0]    xreg_shift_r_mask;
word_t          xreg_mod_A;
word_t          xreg_mod_B;
word_t          xreg_mod_C;
word_t          xreg_mod_D;
word_t          xreg_src_A;
word_t          xreg_src_B;
word_t          xreg_val_C;
word_t          xreg_dst_D;
logic [14:0]    xreg_lines;                         // "limitation" of 32768 lines
word_t          xreg_words;

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
        xreg_ctrl_B_XOR_A   <= '0;
        xreg_ctrl_transp_8b <= '0;
        xreg_ctrl_transp_B  <= '0;
        xreg_ctrl_op        <= '0;
        xreg_shift          <= '0;
        xreg_shift_l_mask   <= '0;
        xreg_shift_r_mask   <= '0;
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
    end else begin
        // clear queued blit when state machine copies xreg data
        if (blit_state == BLIT_SETUP) begin
            xreg_blit_queued     <= 1'b0;
        end

        // blit register write
        if (xreg_wr_en_i) begin
            case ({ 2'b10, xreg_num_i })
                xv::XR_BLIT_CTRL: begin
                    // [15:8]
                    xreg_ctrl_op        <= xreg_data_i[7:6];
                    xreg_ctrl_transp_B  <= xreg_data_i[5];
                    xreg_ctrl_transp_8b <= xreg_data_i[4];
                    // [3]
                    xreg_ctrl_B_XOR_A   <= xreg_data_i[2];
                    xreg_ctrl_B_const   <= xreg_data_i[1];
                    xreg_ctrl_A_const   <= xreg_data_i[0];
                end
                xv::XR_BLIT_SHIFT: begin
                    xreg_shift_l_mask   <= xreg_data_i[15:12];
                    xreg_shift_r_mask   <= xreg_data_i[11:8];
                    xreg_shift          <= xreg_data_i[1:0];
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

// blit registers read
always_ff @(posedge clk) begin
    case ({ 2'b10, xreg_num_i })
        xv::XR_BLIT_CTRL:
            xreg_data_o     <= { 8'b0, xreg_ctrl_op, xreg_ctrl_transp_B, xreg_ctrl_transp_8b, 1'b0, xreg_ctrl_B_XOR_A, xreg_ctrl_B_const, xreg_ctrl_A_const};
        xv::XR_BLIT_SHIFT:
            xreg_data_o     <= { xreg_shift_l_mask, xreg_shift_r_mask, 6'b0, xreg_shift };
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
            xreg_data_o     <= { 1'b0, xreg_lines} ;
        xv::XR_BLIT_WORDS:
            xreg_data_o     <= xreg_words;
        default:
            xreg_data_o     <= '0;
    endcase
end

// blitter operational registers (for blit in progress)
logic  [1:0]    blit_ctrl_op;
logic           blit_ctrl_A_const;
logic           blit_ctrl_B_const;
logic           blit_ctrl_B_XOR_A;
logic           blit_ctrl_transp_8b;
logic           blit_ctrl_transp_B;
logic  [3:0]    blit_shift_l_mask;
logic  [3:0]    blit_shift_r_mask;
logic  [1:0]    blit_shift;
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

logic [16:0]    blit_count;             // width counter (extra bit for underflow line done flag)
logic           blit_first_word;
logic           blit_last_word;
assign          blit_last_word  = blit_count[16];   // underflow flag for last word/last word of line
logic           blit_last_line;
assign          blit_last_line  = blit_lines[15];   // underflow flag for last line (for rectangular blit)

// nibble shifter // TODO: see if any savings leaving old data vs setting to zero
function automatic [27:0] lsr_4(
        input  [1:0]    nibble_shift,   // 0 to 3 nibbles to shift right
        input [11:0]    shift_in,       // 3 nibbles shifted in (from previous word shift out)
        input word_t    data            // data word to shift
    );
    begin
        case (nibble_shift)
            2'h0:   lsr_4 = { data[15:12],    data[11:8],    data[7:4],     data[3:0],        // word result
                            4'b0,           4'b0,          4'b0};                           // nibbles shifted out
            2'h1:   lsr_4 = { shift_in[11:8], data[15:12],   data[11:8],    data[7:4],        // word result
                            data[3:0],      4'b0,          4'b0};                           // nibbles shifted out
            2'h2:   lsr_4 = { shift_in[11:8], shift_in[7:4], data[15:12],   data[11:8],       // word result
                            data[7:4],      data[3:0],     4'b0};                           // nibbles shifted out
            2'h3:   lsr_4 = { shift_in[11:8], shift_in[7:4], shift_in[3:0], data[15:12],      // word result
                            data[11:8],     data[7:4],     data[3:0] };                     // nibbles shifted out
        endcase
    end
endfunction

logic [27:0]    lsr_A;                  // 0 to 3 nibble shifted A value
logic [27:0]    lsr_B;                  // 0 to 3 nibble shifted A value
logic [11:0]    lsr_out_A;              // up to 3 nibbles shifted out of A, to shift into next A word
logic [11:0]    lsr_out_B;              // up to 3 nibbles shifted out of A, to shift into next A word

assign  lsr_A = lsr_4(blit_shift, lsr_out_A, blit_data_i);    // shifted value read from blit_src_A
assign  lsr_B = lsr_4(blit_shift, lsr_out_B, blit_data_i);    // shifted value read from blit_src_B

// logic ops
typedef enum logic [1:0] {
    OP_A_AND_C          = 2'b00,        // D = A & C
    OP_A_ADD_B_AND_C    = 2'b01,        // D = A + B & C
    OP_A_XOR_B_AND_C    = 2'b10,        // D = A ^ B & C
    OP_A_AND_B_XOR_C    = 2'b11         // D = A & B ^ C
} blit_op_t;

word_t              val_A;              // const or value read from blit_src_A
word_t              val_B;              // const or value read from blit_src_B
word_t              val_C;              // const value blit_val_C
word_t              val_D;              // value to write to blit_dst_D

assign val_C        = blit_val_C;       // val_C is alias for blit_val_C register
assign blit_data_o  = val_D;            // val_D is output to VRAM
always_comb begin : logic_ops           // TODO: check cost of these
    case (blit_ctrl_op[1:0])
        OP_A_AND_C:         val_D   = val_A & val_C;                    // COPY: A AND with const C (A, B or B=A^B can be used for transparency)
        OP_A_ADD_B_AND_C:   val_D   = { val_A[15:12] + val_B[15:12],    // ADD: add A + B nibbles, AND with const C
                                        val_A[11:8]  + val_B[11:8],
                                        val_A[7:4]   + val_B[ 7:4],
                                        val_A[3:0]   + val_B[ 3:0]
                                      } & val_C;
        OP_A_XOR_B_AND_C:   val_D   = val_A ^ val_B & val_C;
        OP_A_AND_B_XOR_C:   val_D   = val_A & val_B ^ val_C;            // ALTER: A AND B XOR C (clear, set or toggle bits)
    endcase
end
// Seems too chonky...
        // OP_A_MASK_B_XOR_C:   val_D   = val_A & { {4{(|val_B[15:12])}},   // MASK: mask out A nibble when B nibble is zero, XOR with const C
        //                                         {4{(|val_B[11:8])}},
        //                                         {4{(|val_B[7:4])}},
        //                                         {4{(|val_B[3:0])}}
        //                                       }  ^ val_C;
// transparency
logic  [3:0]    blit_mask_trans;

assign blit_wr_mask_o = (blit_first_word ? blit_shift_l_mask  : 4'b1111) &
                      (blit_last_word  ? blit_shift_r_mask : 4'b1111) &
                      blit_mask_trans;
always_comb begin
    if (blit_ctrl_transp_8b) begin
        if (blit_ctrl_transp_B) begin
            blit_mask_trans = { |val_B[15:8], |val_B[15:8], |val_B[7:0], |val_B[7:0] };
        end else begin
            blit_mask_trans = { |val_A[15:8], |val_A[15:8], |val_A[7:0], |val_A[7:0] };
        end
    end else begin
        if (blit_ctrl_transp_B) begin
            blit_mask_trans = { |val_B[15:12], |val_B[11:8], |val_B[7:4], |val_B[3:0] };
        end else begin
            blit_mask_trans = { |val_A[15:12], |val_A[11:8], |val_A[7:4], |val_A[3:0] };
        end
    end
end

// blit state machine
typedef enum logic [3:0] {
    BLIT_IDLE,          // wait for blit operation (a write to xreg_blit_count)
    BLIT_SETUP,         // copy xreg registers to blit registers and setup for blit
    BLIT_LINE_START,    // copy count to width, decrement width, height counters, initiate A/B read as needed
    BLIT_WAIT_RD_B,     // wait for B read result, initiate A read if needed
    BLIT_WAIT_RD_A,     // wait for A read result, set logic op calculation input
    BLIT_EXEC_OP,       // write logic op result D (executed once when A and B are const)
    BLIT_WAIT_WR_D,     // wait for D write, initiate A/B read or D write as needed, loop if width >= 0
    BLIT_LINE_FINISH,   // add modulo values, loop if height < 0
    BLIT_DONE
} blit_state_t;

logic  [3:0]    blit_state;

always_ff @(posedge clk) begin
    if (reset_i) begin
        blit_done_intr_o    <= '0;
        blit_vram_sel_o     <= '0;
        blit_wr_o           <= '0;
        blit_addr_o         <= '0;

        blit_state          <= BLIT_IDLE;

        blit_ctrl_A_const   <= '0;
        blit_ctrl_B_const   <= '0;
        blit_ctrl_B_XOR_A   <= '0;
        blit_ctrl_transp_8b <= '0;
        blit_ctrl_transp_B  <= '0;
        blit_ctrl_op        <= '0;
        blit_shift_l_mask   <= '0;
        blit_shift_r_mask   <= '0;
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
                blit_ctrl_op        <= xreg_ctrl_op;
                blit_ctrl_A_const   <= xreg_ctrl_A_const;
                blit_ctrl_B_const   <= xreg_ctrl_B_const;
                blit_ctrl_B_XOR_A   <= xreg_ctrl_B_XOR_A;
                blit_ctrl_transp_8b <= xreg_ctrl_transp_8b;
                blit_ctrl_transp_B  <= xreg_ctrl_transp_B;
                blit_shift          <= xreg_shift;
                blit_shift_l_mask   <= xreg_shift_l_mask;
                blit_shift_r_mask   <= xreg_shift_r_mask;

                blit_mod_A          <= xreg_mod_A;
                blit_mod_B          <= xreg_mod_B;
                blit_mod_C          <= xreg_mod_C;
                blit_mod_D          <= xreg_mod_D;
                blit_src_A          <= xreg_src_A;
                blit_src_B          <= xreg_src_B;
                blit_val_C          <= xreg_val_C;
                blit_dst_D          <= xreg_dst_D;
                blit_lines          <= { 1'b0, xreg_lines };
                blit_words          <= xreg_words - 1'b1;

                blit_first_word     <= 1'b1;    // first word flag after first write
                val_A               <= xreg_src_A;
                val_B               <= xreg_src_B;

                blit_state          <= BLIT_LINE_START;
            end
            BLIT_LINE_START: begin
                blit_lines          <= blit_lines - 1'b1;       // pre-decrement, bit[15] underflow indicates last line (1-32768)
                blit_count          <= { 1'b0, blit_words };     // pre-decrement, bit[16] underflow indicates last word (1-65536)

                if (!blit_ctrl_B_const) begin
                    blit_vram_sel_o     <= 1'b1;
                    blit_wr_o           <= 1'b0;
                    blit_addr_o         <= blit_src_B;

                    blit_state          <= BLIT_WAIT_RD_B;
                end else if (!blit_ctrl_A_const) begin
                    blit_vram_sel_o     <= 1'b1;
                    blit_wr_o           <= 1'b0;
                    blit_addr_o         <= blit_src_A;

                    blit_state          <= BLIT_WAIT_RD_A;
                end else begin
                    blit_state          <= BLIT_EXEC_OP;
                end
            end
            BLIT_WAIT_RD_B: begin
                if (blit_vram_ack_i) begin
                    val_B               <= lsr_B[27:12];
                    lsr_out_B           <= lsr_B[11:0];
                    blit_src_B          <= blit_src_B + 1'b1;

                    blit_vram_sel_o     <= 1'b1;
                    blit_wr_o           <= 1'b0;
                    blit_addr_o         <= blit_src_A;

                    blit_state          <= BLIT_WAIT_RD_A;
                end else begin
                    blit_vram_sel_o     <= 1'b1;
                    blit_wr_o           <= 1'b0;
                    blit_addr_o         <= blit_src_B;
                end
            end
            BLIT_WAIT_RD_A: begin
                if (blit_vram_ack_i) begin
                    val_A               <= lsr_A[27:12];
                    lsr_out_A           <= lsr_A[11:0];
                    blit_src_A          <= blit_src_A + 1'b1;

                    if (blit_ctrl_B_XOR_A) begin    // TODO: check this ideas cost
                        if (blit_ctrl_B_const) begin
                            val_B               <= lsr_A[27:12] ^ blit_src_B;
                        end else begin
                            val_B               <= lsr_A[27:12] ^ val_B;
                        end
                    end

                    blit_state          <= BLIT_EXEC_OP;
                end else begin
                    blit_vram_sel_o     <= 1'b1;
                    blit_wr_o           <= 1'b0;
                    blit_addr_o         <= blit_src_A;
                end
            end
            BLIT_EXEC_OP: begin
                blit_vram_sel_o     <= 1'b1;
                blit_wr_o           <= 1'b1;
                blit_addr_o         <= blit_dst_D;

                blit_state          <= BLIT_WAIT_WR_D;
            end
            BLIT_WAIT_WR_D: begin
                if (blit_vram_ack_i) begin
                    blit_dst_D          <= blit_dst_D + 1'b1;

                    blit_first_word     <= 1'b0;
                    blit_count          <= blit_count - 1'b1;

                    if (blit_last_word) begin
                        blit_state          <= BLIT_LINE_FINISH;
                    end else if (!blit_ctrl_B_const) begin
                        blit_vram_sel_o     <= 1'b1;
                        blit_wr_o           <= 1'b0;
                        blit_addr_o         <= blit_src_B;

                        blit_state          <= BLIT_WAIT_RD_B;
                    end else if (!blit_ctrl_A_const) begin
                        blit_vram_sel_o     <= 1'b1;
                        blit_wr_o           <= 1'b0;
                        blit_addr_o         <= blit_src_A;

                        blit_state          <= BLIT_WAIT_RD_A;
                    end else begin
                        blit_vram_sel_o     <= 1'b1;
                        blit_wr_o           <= 1'b1;
                        blit_addr_o         <= blit_dst_D + 1'b1;       // NOTE: increment "again" (since blit_dst_D increment in same cycle)
                        blit_state          <= BLIT_WAIT_WR_D;
                    end
                end else begin
                    blit_vram_sel_o     <= 1'b1;
                    blit_wr_o           <= 1'b1;
                    blit_addr_o         <= blit_dst_D;
                end
            end
            BLIT_LINE_FINISH: begin
                blit_first_word     <= 1'b1;


                blit_src_A  <= blit_src_A + blit_mod_A;
                blit_src_B  <= blit_src_B + blit_mod_B;
                blit_dst_D  <= blit_dst_D + blit_mod_D;

                val_A       <= val_A ^ blit_mod_A;      // TODO: nibble addition expensive?
                val_B       <= val_B ^ blit_mod_B;
                blit_val_C  <= blit_val_C ^ blit_mod_C;

                if (blit_last_line) begin
                    blit_state          <= BLIT_DONE;
                end else begin
                    blit_state          <= BLIT_LINE_START;
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
