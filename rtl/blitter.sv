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
    output      logic           blit_busy_o,        // current status
    output      logic           blit_full_o,        // current status
    output      logic           blit_done_intr_o,   // interrupt signal when done
    // VRAM/XR bus signals
    output      logic           blit_vram_sel_o,    // vram select
    input  wire logic           blit_vram_ack_i,    // VRAM access ack (true when data read/written)

    output      logic           blit_wr_o,          // blit write
    output      logic  [3:0]    blit_wr_mask_o,     // blit VRAM nibble mask

    output      addr_t          blit_addr_o,        // address out (vram/XR)
    input  wire word_t          blit_data_i,        // data word data in
    output      word_t          blit_data_o,        // data word data out
    // standard signals
    input  wire logic           reset_i,            // system reset in
    input  wire logic           clk                 // clock (video pixel clock)
/* verilator lint_on UNUSED */
);

typedef enum logic [2:0] {
    BLIT_IDLE,
    BLIT_SETUP,
    BLIT_COPY,
    BLIT_COPY_READ,
    BLIT_COPY_WRITE,
    BLIT_FILL,
    BLIT_FILL_WRITE,

    BLIT_DONE
} blit_state_t;

// blitter registers
logic           xreg_rd_xr;
logic           xreg_wr_xr;
logic           xreg_fill;
logic  [1:0]    xreg_shift;
logic [15:0]    xreg_rd_mod;
logic [15:0]    xreg_wr_mod;
logic [11:0]    xreg_wr_mask;
logic [15:0]    xreg_width;
logic [15:0]    xreg_rd_addr;
logic [15:0]    xreg_wr_addr;
logic [15:0]    xreg_count;

logic           xreg_queued;

// blitter FSM
logic  [2:0]    blit_state;

logic           blit_rd_xr;
logic           blit_wr_xr;
/* verilator lint_off UNUSED */
logic  [1:0]    blit_shift;
logic  [3:0]    blit_wr_mask_left;
logic  [3:0]    blit_wr_mask_middle;
logic  [3:0]    blit_wr_mask_right;
/* verilator lint_on UNUSED */
logic [15:0]    blit_rd_addr;
logic [15:0]    blit_wr_addr;
logic [16:0]    blit_count;                         // extra bit for underflow done flag

/* verilator lint_off UNUSED */
logic           blit_left_edge;
logic [27:0]    shift_reg_next;                     // combinatorial 7 nibble shift buffer
logic [11:0]    shift_reg;                          // 3 nibble "carries" to rotate in next word
/* verilator lint_on UNUSED */

logic  blit_done;
assign blit_done    = blit_count[16];               // count underflow is done flag (on last word)

// assign outputs
assign blit_busy_o  = (blit_state != BLIT_IDLE);    // blit operation in progress
assign blit_full_o  = xreg_queued;                  // blit register queue full

// blit registers read/write
always_ff @(posedge clk) begin
    if (reset_i) begin
        xreg_rd_xr      <= 1'b0;
        xreg_wr_xr      <= 1'b0;
        xreg_fill       <= 1'b0;
        xreg_shift      <= '0;
        xreg_rd_mod     <= '0;
        xreg_wr_mod     <= '0;
        xreg_wr_mask    <= '0;
        xreg_width      <= '0;
        xreg_rd_addr    <= '0;
        xreg_wr_addr    <= '0;
        xreg_count      <= '0;
        xreg_queued     <= 1'b0;
    end else begin
        if (blit_state == BLIT_SETUP) begin
            xreg_queued     <= 1'b0;
        end

        // blit register write
        if (xreg_wr_en_i) begin
            case ({ 2'b10, xreg_num_i })
                xv::XR_BLIT_MODE: begin
                    xreg_rd_xr      <= xreg_data_i[15];
                    xreg_wr_xr      <= xreg_data_i[14];
                    xreg_fill       <= xreg_data_i[13];
                    xreg_shift      <= xreg_data_i[1:0];
                end
                xv::XR_BLIT_RD_MOD: begin
                    xreg_rd_mod     <= xreg_data_i;
                end
                xv::XR_BLIT_WR_MOD: begin
                    xreg_wr_mod     <= xreg_data_i;
                end
                xv::XR_BLIT_WR_MASK: begin
                    xreg_wr_mask    <= xreg_data_i[11:0];
                end
                xv::XR_BLIT_WIDTH: begin
                    xreg_width      <= xreg_data_i;
                end
                xv::XR_BLIT_RD_ADDR: begin
                    xreg_rd_addr    <= xreg_data_i;
                end
                xv::XR_BLIT_WR_ADDR: begin
                    xreg_wr_addr    <= xreg_data_i;
                end
                xv::XR_BLIT_COUNT: begin
                    xreg_count      <= xreg_data_i;
                    xreg_queued     <= 1'b1;
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
        xv::XR_BLIT_MODE:           xreg_data_o <= { xreg_rd_xr, xreg_wr_xr, 12'b0, xreg_shift };
        xv::XR_BLIT_RD_MOD:         xreg_data_o <= xreg_rd_mod;
        xv::XR_BLIT_WR_MOD:         xreg_data_o <= xreg_wr_mod;
        xv::XR_BLIT_WR_MASK:        xreg_data_o <= { 4'h0, xreg_wr_mask };
        xv::XR_BLIT_WIDTH:          xreg_data_o <= xreg_width;
        xv::XR_BLIT_RD_ADDR:        xreg_data_o <= xreg_rd_addr;
        xv::XR_BLIT_WR_ADDR:        xreg_data_o <= xreg_wr_addr;
        xv::XR_BLIT_COUNT:          xreg_data_o <= xreg_count[15:0];
        default:                    xreg_data_o <= 16'h0000;
    endcase
end

assign blit_wr_mask_o   = (blit_left_edge ? blit_wr_mask_left : blit_wr_mask_middle) & (blit_done ? blit_wr_mask_right : blit_wr_mask_middle);

// blit state machine
always_ff @(posedge clk) begin
    if (reset_i) begin
        blit_done_intr_o    <= 1'b0;
        blit_vram_sel_o     <= 1'b0;
        blit_wr_o           <= 1'b0;
        blit_addr_o         <= '0;
        blit_data_o         <= '0;

        blit_state          <= BLIT_IDLE;

        blit_rd_xr          <= '0;
        blit_wr_xr          <= '0;
        blit_shift          <= '0;
        blit_rd_addr        <= '0;
        blit_wr_addr        <= '0;
        blit_count          <= '0;

        blit_left_edge      <= 1'b0;

        blit_wr_mask_left   <= '0;
        blit_wr_mask_middle <= '0;
        blit_wr_mask_right  <= '0;


    end else begin
        blit_done_intr_o    <= 1'b0;

        case (blit_state)
            BLIT_IDLE: begin
                if (xreg_queued) begin
                    blit_state          <= BLIT_SETUP;
                end
            end
            BLIT_SETUP: begin
                blit_rd_xr          <= xreg_rd_xr;
                blit_wr_xr          <= xreg_wr_xr;
                blit_shift          <= xreg_shift;
                blit_rd_addr        <= xreg_rd_addr;
                blit_wr_addr        <= xreg_wr_addr;
                blit_wr_mask_left   <= xreg_wr_mask[11:8];
                blit_wr_mask_middle <= xreg_wr_mask[7:4];
                blit_wr_mask_right  <= xreg_wr_mask[3:0];
                blit_count          <= { 1'b0, xreg_count } - 1'b1;

                if (xreg_fill) begin
                    blit_state          <= BLIT_FILL;
                end else begin
                    blit_state          <= BLIT_COPY;
                end
            end
            BLIT_COPY: begin
                blit_vram_sel_o     <= ~blit_rd_xr;
                blit_wr_o           <= 1'b0;
                blit_addr_o         <= blit_rd_addr;
                blit_rd_addr        <= blit_rd_addr + 1'b1;

                blit_left_edge      <= 1'b1;

                blit_state          <= BLIT_COPY_READ;
            end
            BLIT_COPY_READ: begin
                if (blit_vram_ack_i) begin
                    blit_vram_sel_o     <= ~blit_wr_xr;
                    blit_wr_o           <= 1'b1;
                    blit_addr_o         <= blit_wr_addr;
                    blit_data_o         <= shift_reg_next[27:12];
                    shift_reg           <= shift_reg_next[11:0];

                    blit_wr_addr        <= blit_wr_addr + 1'b1;

                    blit_state          <= BLIT_COPY_WRITE;
                end
            end
            BLIT_COPY_WRITE: begin
                if (blit_vram_ack_i) begin
                    blit_left_edge      <= 1'b0;
                    blit_addr_o         <= blit_rd_addr;
                    blit_rd_addr        <= blit_rd_addr + 1'b1;
                    blit_count          <= blit_count - 1'b1;

                    if (blit_done) begin
                        blit_state          <= BLIT_DONE;
                    end else begin
                        blit_vram_sel_o     <= ~blit_rd_xr;
                        blit_wr_o           <= 1'b0;

                        blit_state          <= BLIT_COPY_READ;
                    end
                end
            end
            BLIT_FILL: begin
                blit_vram_sel_o     <= ~blit_wr_xr;
                blit_wr_o           <= 1'b1;
                blit_addr_o         <= blit_wr_addr;
                blit_data_o         <= blit_rd_addr;

                blit_wr_addr        <= blit_wr_addr + 1'b1;

                blit_left_edge      <= 1'b1;

                blit_state          <= BLIT_FILL_WRITE;
            end
            BLIT_FILL_WRITE: begin
                if (blit_vram_ack_i) begin
                    blit_left_edge      <= 1'b0;
                    blit_addr_o         <= blit_wr_addr;
                    blit_wr_addr        <= blit_wr_addr + 1'b1;
                    blit_count          <= blit_count - 1'b1;
                    if (blit_done) begin
                        blit_state          <= BLIT_DONE;
                    end else begin
                        blit_vram_sel_o     <= ~blit_wr_xr;
                        blit_wr_o           <= 1'b1;

                        blit_state          <= BLIT_FILL_WRITE;
                    end
                end
            end
            BLIT_DONE: begin
                blit_vram_sel_o     <= 1'b0;
                blit_wr_o           <= 1'b0;
                blit_done_intr_o    <= 1'b1;

                if (xreg_queued) begin
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

always_comb begin
    case (blit_shift)
        2'b00:   shift_reg_next = { blit_data_i[15:12], blit_data_i[11:8],  blit_data_i[7:4],   blit_data_i[3:0],   4'b0,              4'b0,             4'b0};
        2'b01:   shift_reg_next = { shift_reg[11:8],    blit_data_i[15:12], blit_data_i[11:8],  blit_data_i[7:4],   blit_data_i[3:0],  4'b0,             4'b0};
        2'b10:   shift_reg_next = { shift_reg[11:8],    shift_reg[7:4],     blit_data_i[15:12], blit_data_i[11:8],  blit_data_i[7:4],  blit_data_i[3:0], 4'b0};
        2'b11:   shift_reg_next = { shift_reg[11:8],    shift_reg[7:4],     shift_reg[3:0],     blit_data_i[15:12], blit_data_i[11:8], blit_data_i[7:4], blit_data_i[3:0]};
    endcase
end

endmodule
`default_nettype wire               // restore default
