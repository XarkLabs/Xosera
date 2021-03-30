// blitter.sv
//
// vim: set et ts=4 sw=4
//
// Copyright (c) 2020 Xark - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)
//

`default_nettype none             // mandatory for Verilog sanity
`timescale 1ns/1ns

module blitter(
    input  logic            clk,
    input  logic            blit_cycle_i,           // 0 = video, 1 = blitter
    output logic            video_ena_o,            // 0 = video blank, 1 = video on
    output logic            blit_vram_sel_o,        // VRAM select
    output logic            blit_vram_wr_o,         // VRAM read/write
    output logic    [15:0]  blit_vram_addr_o,       // VRAM address
    input  logic    [15:0]  blit_vram_data_i,       // VRAM read data
    output logic    [15:0]  blit_vram_data_o,       // VRAM write data
    input  logic            reg_write_strobe_i,     // strobe for register write
    input  logic     [3:0]  reg_num_i,              // register number read/written
    input  logic    [15:0]  reg_data_i,             // word to read into register
    output logic    [15:0]  reg_data_o,             // word to write from register
    input  logic            reset_i
    );

`include "xosera_defs.svh"           // Xosera global Verilog definitions

localparam CLEARDATA = 16'h1F20;    // value VRAM cleared to on init (blue+white space) TODO: zero for final?

// video reg_controller registers (16x16-bit words)
typedef enum logic [3:0] {
		R_XVID_RD_ADDR,        // reg 0 0000: address to read from VRAM (write-only)
		R_XVID_WR_ADDR,        // reg 1 0001: address to write from VRAM (write-only)
		R_XVID_DATA,           // reg 2 0010: read/write word from/to VRAM RD/WR
		R_XVID_DATA_2,         // reg 3 0011: read/write word from/to VRAM RD/WR (for 32-bit)
		R_XVID_VID_MODE,       // reg 4 0100: TODO video display mode (write-only)
		R_XVID_BLIT_CTRL,      // reg 5 0101: TODO blitter mode/control/status (read/write)
		R_XVID_RD_INC,         // reg 6 0110: TODO read addr increment value (write-only)
		R_XVID_WR_INC,         // reg 7 0111: TODO write addr increment value (write-only)
		R_XVID_RD_MOD,         // reg 8 1000: TODO read modulo width (write-only)
		R_XVID_WR_MOD,         // reg A 1001: TODO write modulo width (write-only)
		R_XVID_WIDTH,          // reg 9 1010: TODO width for 2D blit (write-only)
		R_XVID_COUNT,          // reg B 1011: TODO blitter "repeat" count (write-only)
		R_XVID_AUX_RD_ADDR,    // reg C 1100: TODO aux read address (font audio etc.?) (write-only)
		R_XVID_AUX_WR_ADDR,    // reg D 1101: TODO aux write address (font audio etc.?) (write-only)
		R_XVID_AUX_DATA,       // reg E 1110: TODO aux memory/register data read/write value
		R_XVID_AUX_CTRL        // reg F 1111: TODO audio and other control? (read/write)
} register_t;

typedef enum logic [3:0] {
    INIT, CLEAR, LOGO_X, LOGO_o, LOGO_s, LOGO_e, LOGO_r, LOGO_a, LOGO__, LOGO_v, LOGO_1, LOGO_2, LOGO_3, LOGO_4, IDLE
} blit_state_t;

blit_state_t blit_state;
logic [12*8:1]  logostring = "Xosera v0.01";
logic           blit_busy;
logic           blit_read;
logic           blit_read_ack;
logic [15:0]    vram_rd_data;
logic [15:0]    blit_rd_addr;
logic [15:0]    blit_wr_addr;
logic [15:0]    blit_rd_incr;
logic [15:0]    blit_wr_incr;
logic [15:0]    blit_count;

assign reg_data_o = vram_rd_data;

always_ff @(posedge clk) begin
    if (reset_i) begin
        video_ena_o         <= 1'b0;
        blit_vram_sel_o     <= 1'b0;
        blit_vram_wr_o      <= 1'b0;
        blit_vram_addr_o    <= 16'h0000;
        blit_vram_data_o    <= 16'h0000;
        blit_busy           <= 1'b0;
        blit_read           <= 1'b0;
        blit_read_ack       <= 1'b0;
        blit_state          <= INIT;
        vram_rd_data        <= 16'he3e3;
        blit_rd_addr        <= 16'h0000;
        blit_wr_addr        <= 16'h0000;
        blit_rd_incr        <= 16'h0001;
        blit_wr_incr        <= 16'h0001;
        blit_count          <= 16'h0000;
    end
    else begin
        // if a read was pending, save value from vram
        if (blit_read_ack) begin
            vram_rd_data     <= blit_vram_data_i;
        end
        // if this is a blit cycle (vs video gen), or there is no pending blit vram access
        if (blit_cycle_i || !blit_vram_sel_o) begin

            blit_read_ack   <= blit_read;   // ack is one cycle after read with blitter access
            blit_read       <= 1'b0;

            // if we did a read, increment write addr
            if (blit_read) begin
                blit_rd_addr     <= blit_rd_addr + blit_rd_incr;
            end

            // if we did a write, increment write addr
            if (blit_vram_wr_o) begin
                blit_wr_addr    <= blit_wr_addr + blit_wr_incr;

                // decrement process count register if blitter busy
                if (blit_busy) begin
                    blit_count  <= blit_count - 1;
                end
            end

            if (blit_count == 16'h0000) begin
                blit_busy   <= 1'b0;
            end

            blit_vram_sel_o     <= 1'b0;            // clear vram select
            blit_vram_wr_o      <= 1'b0;            // clear vram write
            blit_vram_addr_o    <= blit_wr_addr;    // assume write, output address
            blit_state <= IDLE;                     // default next state

            case (blit_state)
                IDLE: begin
                    if (reg_write_strobe_i) begin
                        case (reg_num_i)
                            R_XVID_RD_ADDR: begin
                                blit_rd_addr        <= reg_data_i;      // save read address
                                blit_vram_addr_o    <= reg_data_i;      // output read address
                                blit_read           <= 1'b1;            // remember pending read
                                blit_vram_sel_o     <= 1'b1;            // select VRAM
                            end
                            R_XVID_WR_ADDR: begin
                                blit_wr_addr        <= reg_data_i;      // save write address
                            end
                            R_XVID_DATA,
                            R_XVID_DATA_2: begin
                                blit_vram_data_o    <= reg_data_i;      // output write data
                                blit_vram_addr_o    <= blit_wr_addr;    // output write address
                                blit_vram_wr_o      <= 1'b1;            // VRAM write
                                blit_vram_sel_o     <= 1'b1;            // select VRAM
                                blit_wr_addr        <= blit_wr_addr + blit_wr_incr;     // increment write address
                            end
                            R_XVID_VID_MODE: begin
                                video_ena_o         <= video_ena_o ^ reg_data_i[0];     // TODO toggle video enable bit 0
                            end
                            default:
                            ;
                        endcase
                    end
                end
                INIT: begin
                    video_ena_o         <= 1'b0;
                    blit_vram_sel_o     <= 1'b1;
                    blit_vram_wr_o      <= 1'b1;
                    blit_vram_addr_o    <= 16'h0000;
//                    blit_vram_data_o    <= CLEARDATA;
                    blit_vram_data_o    <= 16'h0000;    // TODO HACK
                    blit_count          <= 16'hFFFF;
                    blit_wr_incr        <= 16'h0001;
                    blit_busy           <= 1'b1;
                    blit_state          <= CLEAR;
                end
                CLEAR: begin
                    if (blit_busy) begin
                        blit_vram_sel_o <= 1'b1;
                        blit_vram_wr_o  <= 1'b1;
                        blit_vram_data_o    <= blit_vram_addr_o+1;    // TODO HACK
                        blit_state      <= CLEAR;
                    end
                    else begin
                        blit_state      <= LOGO_X;
                    end
                end
                LOGO_X: begin
                    blit_vram_sel_o     <= 1'b1;
                    blit_vram_wr_o      <= 1'b1;
                    blit_vram_addr_o    <= (1 * CHARS_WIDE + 2 + 2);
                    blit_wr_addr        <= (1 * CHARS_WIDE + 2 + 3);
                    blit_vram_data_o    <= { 8'h1F, logostring[12*8-:8] };
                    blit_state          <= LOGO_o;
                end
                LOGO_o: begin
                    blit_vram_sel_o     <= 1'b1;
                    blit_vram_wr_o      <= 1'b1;
                    blit_vram_data_o    <= { 8'h1e, logostring[11*8-:8] };
                    blit_state          <= LOGO_s;
                end
                LOGO_s: begin
                    blit_vram_sel_o     <= 1'b1;
                    blit_vram_wr_o      <= 1'b1;
                    blit_vram_data_o    <= { 8'h1c, logostring[10*8-:8] };
                    blit_state          <= LOGO_e;
                end
                LOGO_e: begin
                    blit_vram_sel_o     <= 1'b1;
                    blit_vram_wr_o      <= 1'b1;
                    blit_vram_data_o    <= { 8'h1b, logostring[9*8-:8] };
                    blit_state          <= LOGO_r;
                end
                LOGO_r: begin
                    blit_vram_sel_o     <= 1'b1;
                    blit_vram_wr_o      <= 1'b1;
                    blit_vram_data_o    <= { 8'h12, logostring[8*8-:8] };
                    blit_state          <= LOGO_a;
                end
                LOGO_a: begin
                    blit_vram_sel_o     <= 1'b1;
                    blit_vram_wr_o      <= 1'b1;
                    blit_vram_data_o    <= { 8'h15, logostring[7*8-:8] };
                    video_ena_o         <= 1'b1;
                    blit_state          <= LOGO__;
                end
                LOGO__: begin
                    blit_vram_sel_o     <= 1'b1;
                    blit_vram_wr_o      <= 1'b1;
                    blit_vram_data_o    <= { 8'h17, logostring[6*8-:8] };
                    video_ena_o         <= 1'b1;
                    blit_state          <= LOGO_v;
                end
                LOGO_v: begin
                    blit_vram_sel_o     <= 1'b1;
                    blit_vram_wr_o      <= 1'b1;
                    blit_vram_data_o    <= { 8'h17, logostring[5*8-:8] };
                    video_ena_o         <= 1'b1;
                    blit_state          <= LOGO_1;
                end
                LOGO_1: begin
                    blit_vram_sel_o     <= 1'b1;
                    blit_vram_wr_o      <= 1'b1;
                    blit_vram_data_o    <= { 8'h17, logostring[4*8-:8] };
                    video_ena_o         <= 1'b1;
                    blit_state          <= LOGO_2;
                end
                LOGO_2: begin
                    blit_vram_sel_o     <= 1'b1;
                    blit_vram_wr_o      <= 1'b1;
                    blit_vram_data_o    <= { 8'h17, logostring[3*8-:8] };
                    video_ena_o         <= 1'b1;
                    blit_state          <= LOGO_3;
                end
                LOGO_3: begin
                    blit_vram_sel_o     <= 1'b1;
                    blit_vram_wr_o      <= 1'b1;
                    blit_vram_data_o    <= { 8'h17, logostring[2*8-:8] };
                    video_ena_o         <= 1'b1;
                    blit_state          <= LOGO_4;
                end
                LOGO_4: begin
                    blit_vram_sel_o     <= 1'b1;
                    blit_vram_wr_o      <= 1'b1;
                    blit_vram_data_o    <= { 8'h17, logostring[1*8-:8] };
                    video_ena_o         <= 1'b1;
                end
                default: ;
            endcase
        end
    end
end

endmodule
