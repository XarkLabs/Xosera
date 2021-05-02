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
    input  logic            bus_cs_n_i,         // register select strobe
    input  logic            bus_rd_nwr_i,       // 0 = write, 1 = read
    input  logic  [3:0]     bus_reg_num_i,      // register number
    input  logic            bus_bytesel_i,      // 0=even byte, 1=odd byte
    input  logic  [7:0]     bus_data_i,         // 8-bit data bus input
    output logic  [7:0]     bus_data_o,         // 8-bit data bus output
    input  logic            vgen_sel_i,         // 0 = blitter, 1=video generation
    output logic            vgen_ena_o,         // 0 = video blank, 1 = video on
    output logic            blit_vram_sel_o,    // VRAM select
    output logic            blit_aux_sel_o,     // AUX select
    output logic            blit_wr_o,          // VRAM/AUX read/write
    output logic    [15:0]  blit_addr_o,        // VRAM/AUX address
    input  logic    [15:0]  blit_data_i,        // VRAM read data
    output logic    [15:0]  blit_data_o,        // VRAM/AUX write data
    input  logic    [15:0]  aux_data_i,         // AUX read data
    output logic            bus_ack_o,          // TODO ACK strobe for debug
    input  logic            reset_i,
    input  logic            clk
    );

localparam [31:0] githash = 32'H`GITHASH;
logic [14*8:1]  logostring = "Xosera v0.12 #";    // boot msg

localparam CLEARDATA = 16'h0220;    // value VRAM cleared to on init (blue+white space) TODO: zero for final?

typedef enum logic [4:0] {
    INIT, CLEAR, LOGO_1, LOGO_2, LOGO_3, LOGO_4, LOGO_5, LOGO_6, LOGO_7, LOGO_8, LOGO_9, LOGO_10, LOGO_11, LOGO_12, LOGO_13, LOGO_14,
    LOGO_H0, LOGO_H1, LOGO_H2, LOGO_H3, LOGO_H4, LOGO_H5, LOGO_H6, LOGO_H7, LOGO_END,
    IDLE
} blit_state_t;

assign bus_ack_o = (bus_write_strobe | bus_read_strobe);    // TODO: debug

blit_state_t    blit_state;

// read/write storage for first 4 blitter registers
logic [15:0]    reg_aux_addr;           // ctrl reg/font/palette address
logic [15:0]    reg_const;              // blitter constant data
logic [15:0]    reg_rd_addr;            // VRAM read address
logic [15:0]    reg_wr_addr;            // VRAM write address

// write only storage for the rest
logic [16:0]    reg_count;             // (extra bit for underflow/flag)
logic [15:0]    reg_rd_inc;
logic [15:0]    reg_wr_inc;
logic [15:0]    reg_rd_mod;
logic [15:0]    reg_wr_mod;
logic [15:0]    reg_width;
logic           blit_2d;
logic           blit_const;

logic           blit_busy;
assign          blit_busy = ~reg_count[16]; // when reg_count underflows, high bit will be set
logic           blit_line_end;
assign          blit_line_end = width_counter[16];  // when width_counter underflows, high bit will be set
logic [16:0]    width_counter;          // blit count (extra bit for underflow/done)
logic [15:0]    blit_rd_addr;           // TODO VRAM read address
logic [15:0]    blit_wr_addr;           // TODO VRAM write address

logic [15:0]    vram_rd_data;           // word read from VRAM (for RD_ADDR)
logic           blit_vram_rd;
logic           blit_vram_rd_ack;

logic [15:0]    aux_rd_data;
logic           blit_aux_rd;
logic           blit_aux_rd_ack;

logic  [7:0]    reg_data_even;           // word written to even byte of XVID_DATA/XVID_DATA_2
logic  [7:0]    reg_other_even;          // other even byte (zeroed each write)
logic  [3:0]    reg_other_reg;           // register associated with reg_other_even
logic  [7:0]    reg_even_byte;
assign          reg_even_byte = (reg_other_reg == bus_reg_num) ? reg_other_even : 8'h00;

logic           bus_write_strobe;       // strobe when a word of data written
logic           bus_read_strobe;        // strobe when a word of data read
logic  [3:0]    bus_reg_num;            // bus register on bus
logic           bus_bytesel;            // msb/lsb on bus
logic  [7:0]    bus_data_byte;          // data byte from bus

// bus_interface handles signal synchronization, CS and register writes to Xosera
bus_interface bus(
                  .bus_cs_n_i(bus_cs_n_i),              // register select strobe
                  .bus_rd_nwr_i(bus_rd_nwr_i),          // 0=write, 1=read
                  .bus_reg_num_i(bus_reg_num_i),        // register number
                  .bus_bytesel_i(bus_bytesel_i),        // 0=even byte, 1=odd byte
                  .bus_data_i(bus_data_i),              // 8-bit data bus input
                  .write_strobe_o(bus_write_strobe),    // strobe for bus byte write
                  .read_strobe_o(bus_read_strobe),      // strobe for bus byte read
                  .reg_num_o(bus_reg_num),              // register number from bus
                  .bytesel_o(bus_bytesel),              // register number from bus
                  .bytedata_o(bus_data_byte),           // byte data from bus
                  .clk(clk),                            // input clk (should be > 2x faster than bus signals)
                  .reset_i(reset_i)                     // reset
              );

logic           reconfig;                   // set to 1 to force reconfigure of FPGA
logic   [1:0]   boot_select;                // two bit number for flash configuration to load on reconfigure

`ifdef SYNTHESIS
SB_WARMBOOT boot(
                .BOOT(reconfig),
                .S0(boot_select[0]),
                .S1(boot_select[1])
            );
`else
always @* begin
    if (reconfig) begin
        $display("XOSERA REBOOT: To flash config #0x%x", boot_select);
        $finish;
    end
end
`endif

// continuously output byte selected for read from Xosera (to be put on bus when selected for read)
assign bus_data_o = reg_read(bus_bytesel, bus_reg_num);

// function to continuously select read value to put on bus
function [7:0] reg_read(
    input logic         b_sel,
    input logic [3:0]   r_sel
    );
    // NOTE: ignores register bit 3 on reads
    if (r_sel[2] == 1'b0) begin
        // first 4 registers directly readable
        case (r_sel[1:0])
            xv::XVID_AUX_ADDR[1:0]: reg_read = (~b_sel) ? reg_aux_addr[15:8] : reg_aux_addr[7:0];
            xv::XVID_CONST[1:0]:    reg_read = (~b_sel) ? reg_const[15:8]    : reg_const[7:0];
            xv::XVID_RD_ADDR[1:0]:  reg_read = (~b_sel) ? reg_rd_addr[15:8]  : reg_rd_addr[7:0];
            xv::XVID_WR_ADDR[1:0]:  reg_read = (~b_sel) ? reg_wr_addr[15:8]  : reg_wr_addr[7:0];
        endcase
    end
    else begin
        // the other 4 read registers are "synthetic"
        case (r_sel[1:0])
            xv::XVID_DATA[1:0],
            xv::XVID_DATA_2[1:0]:   reg_read = (~b_sel) ? vram_rd_data[15:8] : vram_rd_data[7:0];
            xv::XVID_AUX_DATA[1:0]: reg_read = (~b_sel) ? aux_rd_data[15:8]  : aux_rd_data[7:0];
            xv::XVID_COUNT[1:0]:    reg_read = { blit_busy, 7'b0 };
        endcase
    end
endfunction

function [7:0] hex_digit(
    input logic[3:0]    n
    );
    if (n > 9) begin
        hex_digit = 8'h57 + { 4'h0, n };
    end
    else begin
        hex_digit = 8'h30 + { 4'h0, n };
    end
endfunction

always_ff @(posedge clk) begin
    if (reset_i) begin
        // control signals
        reconfig            <= 1'b0;
        boot_select         <= 2'b00;
        vgen_ena_o          <= 1'b0;
        blit_vram_rd        <= 1'b0;
        blit_vram_rd_ack    <= 1'b0;
        blit_aux_rd         <= 1'b0;
        blit_aux_rd_ack     <= 1'b0;
        blit_vram_sel_o     <= 1'b0;
        blit_aux_sel_o      <= 1'b0;
        blit_wr_o           <= 1'b0;
        // addr/data out
        blit_addr_o         <= 16'h0000;
        blit_data_o         <= 16'h0000;

        // internal blitter state
        blit_state          <= INIT;
        blit_2d             <= 1'b0;
        blit_const          <= 1'b0;
        width_counter       <= 17'h00000;   // 16th bit is set on underflow
        blit_rd_addr        <= 16'h0000;    // TODO
        blit_wr_addr        <= 16'h0000;

        // xosera registers
        reg_aux_addr        <= 16'h0000;
        reg_const           <= 16'h0000;
        reg_rd_addr         <= 16'h0000;
        reg_wr_addr         <= 16'h0000;
        reg_rd_inc          <= 16'h0000;
        reg_wr_inc          <= 16'h0000;
        reg_rd_mod          <= 16'h0000;
        reg_wr_mod          <= 16'h0000;
        reg_width           <= 16'h0000;
        reg_count           <= 17'h10000;   // 16th bit is set on underfow
        reg_data_even       <= 8'h00;
        reg_other_even      <= 8'h00;
        reg_other_reg       <= 4'h0;

    end
    else begin
        // if a read ack is pending, save value from vram/aux
        if (blit_vram_rd_ack) begin
            vram_rd_data    <= blit_data_i;
        end
        blit_vram_rd_ack <= 1'b0;

        if (blit_aux_rd_ack) begin
            aux_rd_data     <= aux_data_i;
        end
        blit_aux_rd_ack <= 1'b0;

        if (!vgen_sel_i) begin
            blit_vram_rd_ack    <= blit_vram_rd;    // ack is one cycle after read with blitter access
            blit_aux_rd_ack     <= blit_aux_rd;     // ack is one cycle after read with aux access

            // if we did a read, increment read addr
            if (blit_vram_rd) begin
                reg_rd_addr  <= reg_rd_addr + reg_rd_inc;   //(blit_line_end ? reg_rd_mod : reg_rd_inc);
            end

            // if we did a write, increment write addr
            if (blit_vram_sel_o && blit_wr_o) begin
                reg_wr_addr  <= reg_wr_addr + reg_wr_inc;   // (blit_line_end ? reg_wr_mod : reg_wr_inc);

                // if width counter exhausted, reload width else decrement
                if (blit_line_end) begin
                    width_counter <= { 1'b0, reg_width };
                end
                else begin
                    width_counter <= width_counter - 1;
                    // if not 2-D blit, prevent blit_end_line (bit 16 of width_counter)
                    if (!blit_2d) begin
                        width_counter[16] <= 1'b0;
                    end
                end

                // decrement count register if blitter busy
                if (blit_busy) begin
                    reg_count    <= reg_count - 1;
                end
            end
            blit_vram_rd    <= 1'b0;
            blit_aux_rd     <= 1'b0;
            blit_vram_sel_o <= 1'b0;            // clear vram select
            blit_aux_sel_o  <= 1'b0;            // clear aux select
            blit_wr_o       <= 1'b0;            // clear write
            blit_addr_o     <= reg_wr_addr;    // assume VRAM write output address // TODO is this a good idea?
        end

        if (bus_write_strobe) begin
            if (!bus_bytesel) begin
                // special storage for certain registers
                case (bus_reg_num)
                    xv::XVID_AUX_ADDR: begin
                        reg_aux_addr[15:8]  <= bus_data_byte;
                    end
                    xv::XVID_CONST: begin
                        reg_const[15:8]     <= bus_data_byte;
                    end
                    xv::XVID_RD_ADDR: begin
                        reg_rd_addr[15:8]   <= bus_data_byte;
                    end
                    xv::XVID_WR_ADDR: begin
                        reg_wr_addr[15:8]   <= bus_data_byte;
                    end
                    xv::XVID_DATA,
                    xv::XVID_DATA_2: begin
                        reg_data_even       <= bus_data_byte;   // data reg even byte storage
                    end
                    default: begin
                        reg_other_even      <= bus_data_byte;   // generic even byte storage
                        reg_other_reg       <= bus_reg_num;
                    end
                endcase
            end
            else begin
                case (bus_reg_num)
                    xv::XVID_AUX_ADDR: begin
                        reg_aux_addr[7:0]   <= bus_data_byte;
                        blit_addr_o         <= { reg_aux_addr[15:8], bus_data_byte };      // output read address
                        blit_aux_sel_o     <= 1'b1;            // select AUX
                        blit_aux_rd        <= 1'b1;            // remember pending aux read request
                    end
                    xv::XVID_CONST: begin
                        reg_const[7:0]      <= bus_data_byte;
                    end
                    xv::XVID_RD_ADDR: begin
                        reg_rd_addr[7:0]    <= bus_data_byte;
                        blit_addr_o         <= { reg_rd_addr[15:8], bus_data_byte };      // output read address
                        blit_vram_sel_o     <= 1'b1;            // select VRAM
                        blit_vram_rd        <= 1'b1;            // remember pending vramread request
                    end
                    xv::XVID_WR_ADDR: begin
                        reg_wr_addr[7:0]    <= bus_data_byte;
                    end
                    xv::XVID_DATA,
                    xv::XVID_DATA_2: begin
                        blit_addr_o         <= reg_wr_addr;    // output write address
                        blit_data_o         <= { reg_data_even, bus_data_byte };      // output write data
                        blit_vram_sel_o     <= 1'b1;            // select VRAM
                        blit_wr_o           <= 1'b1;            // write
                    end
                    xv::XVID_AUX_DATA: begin
                        blit_addr_o         <= reg_aux_addr;
                        blit_data_o         <= { reg_even_byte, bus_data_byte };
                        blit_aux_sel_o      <= 1'b1;
                        blit_wr_o           <= 1'b1;
                    end
                    xv::XVID_COUNT: begin
                        reg_count           <= { 1'b0, reg_even_byte, bus_data_byte };    // TODO async operations
                        width_counter       <=  { 1'b0, reg_width };
                    end
                    xv::XVID_RD_INC: begin
                        reg_rd_inc          <= { reg_even_byte, bus_data_byte };
                    end
                    xv::XVID_WR_INC: begin
                        reg_wr_inc          <= { reg_even_byte, bus_data_byte };
                    end
                    xv::XVID_RD_MOD: begin
                        reg_rd_mod          <= { reg_even_byte, bus_data_byte };
                    end
                    xv::XVID_WR_MOD: begin
                        reg_wr_mod          <= { reg_even_byte, bus_data_byte };
                    end
                    xv::XVID_WIDTH: begin
                        reg_width           <= { reg_even_byte, bus_data_byte };
                    end
                    xv::XVID_BLIT_CTRL: begin
                        blit_2d             <= bus_data_byte[0];
                        blit_const          <= bus_data_byte[1];
                        reconfig            <= reg_even_byte[7:6] == 2'b10 && bus_data_byte[7:6] == 2'b10;
                        boot_select         <= reg_even_byte[1:0];
                    end
                    xv::XVID_UNUSED_E: begin
                    end
                    xv::XVID_UNUSED_F: begin
                    end
                endcase
            end // bus_bytesel
        end // bus_write_strobe

        if (bus_read_strobe & bus_bytesel) begin
            if (bus_reg_num == xv::XVID_DATA || bus_reg_num == xv::XVID_DATA_2) begin
                // if we did a read, increment read addr
                blit_addr_o         <= reg_rd_addr;      // output read address
                blit_vram_sel_o     <= 1'b1;            // select VRAM
                blit_vram_rd        <= 1'b1;            // remember pending vramread request
            end
        end

        case (vgen_sel_i ? IDLE : blit_state)
            IDLE: begin
            end
            INIT: begin
                // NOTE: relies on initial state set by reset
                blit_vram_sel_o <= 1'b1;
                blit_wr_o       <= 1'b1;
`ifdef TESTPATTERN
                blit_data_o     <= 16'h0000;
`else
                blit_data_o     <= CLEARDATA;
`endif
                blit_addr_o     <= 16'h0000;
`ifdef SYNTHESIS
                reg_count       <= 17'h0FFFF;
`else
                reg_count       <= 17'h00FFF;        // smaller clear for simulation
`endif
                reg_wr_inc      <= 16'h0001;
                blit_state      <= CLEAR;
            end
            CLEAR: begin
                if (blit_busy) begin
                    blit_vram_sel_o <= 1'b1;
                    blit_wr_o       <= 1'b1;
`ifdef TESTPATTERN
                    blit_data_o     <= {(reg_wr_addr[7:4] ^ 4'b1111), reg_wr_addr[7:4], reg_wr_addr[7:0] };
//                        blit_data_o     <= ~reg_wr_addr;
`endif
                    blit_state      <= CLEAR;
                end
                else begin
`ifdef TESTPATTERN
                    blit_state      <= LOGO_END;
`else
                    blit_state      <= LOGO_1;
`endif
                end
            end
            LOGO_1: begin
                blit_vram_sel_o     <= 1'b1;
                blit_wr_o           <= 1'b1;
                blit_addr_o         <= (1 * xv::TILES_WIDE + 1);
                reg_wr_addr         <= (1 * xv::TILES_WIDE + 2);
                blit_data_o         <= { 8'h0F, logostring[14*8-:8] };
                blit_state          <= LOGO_2;
            end
            LOGO_2: begin
                blit_vram_sel_o     <= 1'b1;
                blit_wr_o           <= 1'b1;
                blit_data_o         <= { 8'h0e, logostring[13*8-:8] };
                blit_state          <= LOGO_3;
            end
            LOGO_3: begin
                blit_vram_sel_o     <= 1'b1;
                blit_wr_o           <= 1'b1;
                blit_data_o         <= { 8'h0c, logostring[12*8-:8] };
                blit_state          <= LOGO_4;
            end
            LOGO_4: begin
                blit_vram_sel_o     <= 1'b1;
                blit_wr_o           <= 1'b1;
                blit_data_o         <= { 8'h0b, logostring[11*8-:8] };
                blit_state          <= LOGO_5;
            end
            LOGO_5: begin
                blit_vram_sel_o     <= 1'b1;
                blit_wr_o           <= 1'b1;
                blit_data_o         <= { 8'h02, logostring[10*8-:8] };
                blit_state          <= LOGO_6;
            end
            LOGO_6: begin
                blit_vram_sel_o     <= 1'b1;
                blit_wr_o           <= 1'b1;
                blit_data_o         <= { 8'h05, logostring[9*8-:8] };
                blit_state          <= LOGO_7;
            end
            LOGO_7: begin
                blit_vram_sel_o     <= 1'b1;
                blit_wr_o           <= 1'b1;
                blit_data_o         <= { 8'h07, logostring[8*8-:8] };
                blit_state          <= LOGO_8;
            end
            LOGO_8: begin
                blit_vram_sel_o     <= 1'b1;
                blit_wr_o           <= 1'b1;
                blit_data_o         <= { 8'h02, logostring[7*8-:8] };
                blit_state          <= LOGO_9;
            end
            LOGO_9: begin
                blit_vram_sel_o     <= 1'b1;
                blit_wr_o           <= 1'b1;
                blit_data_o         <= { 8'h02, logostring[6*8-:8] };
                blit_state          <= LOGO_10;
            end
            LOGO_10: begin
                blit_vram_sel_o     <= 1'b1;
                blit_wr_o           <= 1'b1;
                blit_data_o         <= { 8'h02, logostring[5*8-:8] };
                blit_state          <= LOGO_11;
            end
            LOGO_11: begin
                blit_vram_sel_o     <= 1'b1;
                blit_wr_o           <= 1'b1;
                blit_data_o         <= { 8'h02, logostring[4*8-:8] };
                blit_state          <= LOGO_12;
            end
            LOGO_12: begin
                blit_vram_sel_o     <= 1'b1;
                blit_wr_o           <= 1'b1;
                blit_data_o         <= { 8'h02, logostring[3*8-:8] };
                blit_state          <= LOGO_13;
            end
            LOGO_13: begin
                blit_vram_sel_o     <= 1'b1;
                blit_wr_o           <= 1'b1;
                blit_data_o         <= { 8'h02, logostring[2*8-:8] };
                blit_state          <= LOGO_14;
            end
            LOGO_14: begin
                blit_vram_sel_o     <= 1'b1;
                blit_wr_o           <= 1'b1;
                blit_data_o         <= { 8'h02, logostring[1*8-:8] };
                blit_state          <= LOGO_H0;
            end
            LOGO_H0: begin
                blit_vram_sel_o     <= 1'b1;
                blit_wr_o           <= 1'b1;
                blit_data_o         <= { 8'h02, hex_digit(githash[8*4-1-:4]) };
                blit_state          <= LOGO_H1;
            end
            LOGO_H1: begin
                blit_vram_sel_o     <= 1'b1;
                blit_wr_o           <= 1'b1;
                blit_data_o         <= { 8'h02, hex_digit(githash[7*4-1-:4]) };
                blit_state          <= LOGO_H2;
            end
            LOGO_H2: begin
                blit_vram_sel_o     <= 1'b1;
                blit_wr_o           <= 1'b1;
                blit_data_o         <= { 8'h02, hex_digit(githash[6*4-1-:4]) };
                blit_state          <= LOGO_H3;
            end
            LOGO_H3: begin
                blit_vram_sel_o     <= 1'b1;
                blit_wr_o           <= 1'b1;
                blit_data_o         <= { 8'h02, hex_digit(githash[5*4-1-:4]) };
                blit_state          <= LOGO_H4;
            end
            LOGO_H4: begin
                blit_vram_sel_o     <= 1'b1;
                blit_wr_o           <= 1'b1;
                blit_data_o         <= { 8'h02, hex_digit(githash[4*4-1-:4]) };
                blit_state          <= LOGO_H5;
            end
            LOGO_H5: begin
                blit_vram_sel_o     <= 1'b1;
                blit_wr_o           <= 1'b1;
                blit_data_o         <= { 8'h02, hex_digit(githash[3*4-1-:4]) };
                blit_state          <= LOGO_H6;
            end
            LOGO_H6: begin
                blit_vram_sel_o     <= 1'b1;
                blit_wr_o           <= 1'b1;
                blit_data_o         <= { 8'h02, hex_digit(githash[2*4-1-:4]) };
                blit_state          <= LOGO_H7;
            end
            LOGO_H7: begin
                blit_vram_sel_o     <= 1'b1;
                blit_wr_o           <= 1'b1;
                blit_data_o         <= { 8'h02, hex_digit(githash[1*4-1-:4]) };
                blit_state          <= LOGO_END;
            end
            LOGO_END: begin
                vgen_ena_o         <= 1'b1;            // enable video after clear
                reg_wr_addr        <= 16'h0000;
                blit_state          <= IDLE;
            end
            default: begin
                blit_state <= IDLE;
            end
        endcase
    end
end
endmodule
