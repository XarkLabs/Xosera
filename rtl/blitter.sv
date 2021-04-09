// blitter.sv
//
// vim: set et ts=4 sw=4
//
// Copyright (c) 2020 Xark - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)
//

`default_nettype none             // mandatory for Verilog sanity
`timescale 1ns/1ps

module blitter(
    input  logic            clk,
    input  logic         bus_cs_n_i,             // register select strobe
    input  logic         bus_rd_nwr_i,           // 0 = write, 1 = read
    input  logic  [3:0]  bus_reg_num_i,          // register number
    input  logic         bus_bytesel_i,          // 0=even byte, 1=odd byte
    input  logic  [7:0]  bus_data_i,             // 8-bit data bus input
    output logic  [7:0]  bus_data_o,             // 8-bit data bus output
    input  logic            blit_cycle_i,           // 0 = video, 1 = blitter
    output logic            video_ena_o,            // 0 = video blank, 1 = video on
    output logic            blit_vram_sel_o,        // VRAM select
    output logic            blit_vram_wr_o,         // VRAM read/write
    output logic    [15:0]  blit_vram_addr_o,       // VRAM address
    input  logic    [15:0]  blit_vram_data_i,       // VRAM read data
    output logic    [15:0]  blit_vram_data_o,       // VRAM write data
    output logic            bus_ack_o,              // ACK strobe for debug
//    input  logic            reg_write_strobe_i,     // strobe for register write
//    input  logic     [3:0]  reg_num_i,              // register number read/written
//    input  logic    [15:0]  reg_data_i,             // word to read into register
//    output logic    [15:0]  reg_data_o,             // word to write from register
    input  logic            reset_i
    );

`include "xosera_defs.svh"           // Xosera global Verilog definitions

localparam CLEARDATA = 16'h1F20;    // value VRAM cleared to on init (blue+white space) TODO: zero for final?

// video reg_controller registers (16x16-bit words)
typedef enum logic [3:0] {
		XVID_RD_INC,            // reg 0: read addr increment value
		XVID_WR_INC,            // reg 1: write addr increment value
		XVID_RD_MOD,            // reg 2: TODO read modulo width
		XVID_WR_MOD,            // reg 3: TODO write modulo width
		XVID_WIDTH,             // reg 4: TODO width for 2D blit
		XVID_RD_ADDR,           // reg 5: address to read from VRAM
		XVID_WR_ADDR,           // reg 6: address to write from VRAM
		XVID_DATA,              // reg 7: read/write word from/to VRAM RD/WR
		XVID_DATA_2,            // reg 8: read/write word from/to VRAM RD/WR (for 32-bit)
		XVID_COUNT,             // reg 9: TODO blitter "repeat" count
		XVID_VID_CTRL,          // reg A: TODO read status/write control settings
		XVID_VID_DATA,          // reg B: TODO video data (as set by VID_CTRL)
		XVID_AUX_RD_ADDR,       // reg C: TODO aux read address (font audio etc.?)
		XVID_AUX_WR_ADDR,       // reg D: TODO aux write address (font audio etc.?)
		XVID_AUX_CTRL,          // reg E: TODO audio and other control?
		XVID_AUX_DATA           // reg F: TODO aux memory/register data read/write value
} register_t;

typedef enum logic [3:0] {
    INIT, CLEAR, LOGO_X, LOGO_o, LOGO_s, LOGO_e, LOGO_r, LOGO_a, LOGO__, LOGO_v, LOGO_1, LOGO_2, LOGO_3, LOGO_4, LOGO_END,
    READY
} blit_state_t;

logic [12*8:1]  logostring = "Xosera v0.11";
logic           blit_read;
logic           blit_read_ack;
blit_state_t    blit_state;

logic [15:0]    blit_reg[0:7];          // read/write storage for first 8 blitter registers
logic  [1:0]    vid_ctrl_reg_sel;
logic [15:0]    vid_ctrl_reg_data;

logic [15:0]    vram_rd_data;           // word read from VRAM (for RD_ADDR)

logic [16:0]    blit_count;           // blit count (extra bit for underflow/done)
logic           blit_busy;
assign blit_busy = (blit_count[16] == 1'b0);   // when blit_count underflows, high bit will be set


logic           bus_write_strobe;      // strobe when a word of data written
logic           bus_read_strobe;       // strobe when a word of data written
logic  [3:0]    bus_reg_num;           // bus register on bus
logic           bus_bytesel;           // msb/lsb on bus
logic  [7:0]    bus_bytedata;          // data byte from bus

assign bus_ack_o = (bus_write_strobe | bus_read_strobe);

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
                  .bytedata_o(bus_bytedata),            // byte data from bus
                  .clk(clk),                            // input clk (should be > 2x faster than bus signals)
                  .reset_i(reset_i)                     // reset
              );

// continuously output byte selected for read from Xosera (to be put on bus when selected for read)
assign bus_data_o = reg_read(bus_bytesel, bus_reg_num);

// function to continuously select read value to put on bus
function [7:0] reg_read(
    input logic         b_sel,
    input logic [3:0]   r_sel
    );
    // first 8 registers are in a directly readable array
    if (!r_sel[3]) begin
        reg_read = (~b_sel) ? blit_reg[r_sel[2:0]][15:8] : blit_reg[r_sel[2:0]][7:0];
    end
    else begin
        // the next 8 registers are "synthetic"
        case (r_sel[2:0])
            XVID_DATA_2[2:0]:       reg_read = (~b_sel) ? vram_rd_data[15:8] : vram_rd_data[7:0];
            XVID_COUNT[2:0]:        reg_read = (~b_sel) ? 8'h81 : 8'h01;
            XVID_VID_CTRL[2:0]:     reg_read = (~b_sel) ? 8'h82 : 8'h02;
            XVID_VID_DATA[2:0]:     reg_read = (~b_sel) ? vid_ctrl_reg_data[15:8] : vid_ctrl_reg_data[7:0];
            XVID_AUX_RD_ADDR[2:0]:  reg_read = (~b_sel) ? 8'h84 : 8'h04;
            XVID_AUX_WR_ADDR[2:0]:  reg_read = (~b_sel) ? 8'h85 : 8'h05;
            XVID_AUX_CTRL[2:0]:     reg_read = (~b_sel) ? 8'h86 : 8'h06;
            XVID_AUX_DATA[2:0]:     reg_read = (~b_sel) ? 8'h87 : 8'h07;
            default: ;
        endcase
    end
endfunction

assign vid_ctrl_reg_data = vid_ctrl_read(vid_ctrl_reg_sel);

// function to continuously select read value to put on bus
function [15:0] vid_ctrl_read(
    input logic [1:0]   v_sel
    );
    case (v_sel)
        2'h0:  vid_ctrl_read = VISIBLE_WIDTH[15:0];
        2'h1:  vid_ctrl_read = VISIBLE_HEIGHT[15:0];
        2'h2:  vid_ctrl_read = 16'hDEAD;
        2'h3:  vid_ctrl_read = 16'hBEEF;
        default: ;
    endcase
endfunction

always_ff @(posedge clk) begin
    if (reset_i) begin
        video_ena_o         <= 1'b0;
        blit_read           <= 1'b0;
        blit_read_ack       <= 1'b0;
        blit_state          <= INIT;
        blit_vram_sel_o     <= 1'b0;
        blit_vram_wr_o      <= 1'b0;
        blit_vram_addr_o    <= 16'h0000;
        blit_vram_data_o    <= CLEARDATA;
        blit_reg[0]         <= 16'h0000;
        blit_reg[1]         <= 16'h0000;
        blit_reg[2]         <= 16'h0000;
        blit_reg[3]         <= 16'h0000;
        blit_reg[4]         <= 16'h0000;
        blit_reg[5]         <= 16'h0000;
        blit_reg[6]         <= 16'h0000;
        blit_reg[7]         <= 16'h0000;
        blit_count          <= 17'h10000;
        vid_ctrl_reg_sel    <= 2'b00;
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
                blit_reg[XVID_RD_ADDR[2:0]]  <= blit_reg[XVID_RD_ADDR[2:0]] + blit_reg[XVID_RD_INC[2:0]];
            end

            // if we did a write, increment write addr
            if (blit_vram_wr_o) begin
                blit_reg[XVID_WR_ADDR[2:0]]  <= blit_reg[XVID_WR_ADDR[2:0]] + blit_reg[XVID_WR_INC[2:0]];

                // decrement process count register if blitter busy
                if (blit_busy) begin
                    blit_count    <= blit_count - 1;
                end
            end

            blit_vram_sel_o     <= 1'b0;            // clear vram select
            blit_vram_wr_o      <= 1'b0;            // clear vram write
            blit_vram_addr_o    <= blit_reg[XVID_WR_ADDR[2:0]];    // assume write, output address

            if (bus_write_strobe) begin
                if (!bus_reg_num[3]) begin  // reg 0-7 array backed
                    if (bus_bytesel) begin
                        blit_reg[bus_reg_num[2:0]][7:0] <= bus_bytedata;
                    end
                    else begin
                        blit_reg[bus_reg_num[2:0]][15:8] <= bus_bytedata;
                    end
                end
                if (bus_bytesel) begin
                    case (bus_reg_num)
                        XVID_RD_ADDR: begin
                            blit_vram_addr_o    <= { blit_reg[XVID_RD_ADDR[2:0]][15:8], bus_bytedata };      // output read address
                            blit_read           <= 1'b1;            // remember pending read
                            blit_vram_sel_o     <= 1'b1;            // select VRAM
                        end
                        XVID_DATA: begin
                            blit_vram_data_o    <= { blit_reg[XVID_DATA[2:0]][15:8], bus_bytedata };      // output write data
                            blit_vram_addr_o    <= blit_reg[XVID_WR_ADDR[2:0]];    // output write address
                            blit_vram_wr_o      <= 1'b1;            // VRAM write
                            blit_vram_sel_o     <= 1'b1;            // select VRAM
                        end
                        XVID_DATA_2: begin
                            blit_vram_data_o    <= { blit_reg[XVID_DATA[2:0]][15:8], bus_bytedata };      // output write data
                            blit_vram_addr_o    <= blit_reg[XVID_WR_ADDR[2:0]];    // output write address
                            blit_vram_wr_o      <= 1'b1;            // VRAM write
                            blit_vram_sel_o     <= 1'b1;            // select VRAM
                        end
                        XVID_VID_CTRL: begin
                            vid_ctrl_reg_sel    <=  bus_bytedata[1:0];
                        end
                        default: begin
                        end
                    endcase
                end
            end

            blit_state <= READY;                     // default next state
            case (blit_state)
                READY: begin
                end
                INIT: begin
                    // NOTE: relies on initial state set by reset
                    blit_vram_sel_o     <= 1'b1;
                    blit_vram_wr_o      <= 1'b1;
                    blit_vram_data_o    <= CLEARDATA;
                    blit_vram_addr_o    <= 16'h0000;
`ifdef ZZZSYNTHESIS
                    blit_count    <= 17'h07FF;        // small clear for simulation
`else
                    blit_count    <= 17'hFFFF;
`endif
                    blit_reg[XVID_WR_INC[2:0]] <= 16'h0001;
                    blit_state          <= CLEAR;
                end
                CLEAR: begin
                    if (blit_busy) begin
                        blit_vram_sel_o <= 1'b1;
                        blit_vram_wr_o  <= 1'b1;
                        blit_state      <= CLEAR;
                    end
                    else begin
                        video_ena_o     <= 1'b1;            // enable video after clear
                        blit_state      <= LOGO_X;
                    end
                end
                LOGO_X: begin
                    blit_vram_sel_o     <= 1'b1;
                    blit_vram_wr_o      <= 1'b1;
                    blit_vram_addr_o    <= (1 * CHARS_WIDE + 2 + 2);
                    blit_reg[XVID_WR_ADDR[2:0]]  <= (1 * CHARS_WIDE + 2 + 3);
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
                    blit_state          <= LOGO_1;
                end
                LOGO_1: begin
                    blit_vram_sel_o     <= 1'b1;
                    blit_vram_wr_o      <= 1'b1;
                    blit_vram_data_o    <= { 8'h17, logostring[4*8-:8] };
                    blit_state          <= LOGO_2;
                end
                LOGO_2: begin
                    blit_vram_sel_o     <= 1'b1;
                    blit_vram_wr_o      <= 1'b1;
                    blit_vram_data_o    <= { 8'h17, logostring[3*8-:8] };
                    blit_state          <= LOGO_3;
                end
                LOGO_3: begin
                    blit_vram_sel_o     <= 1'b1;
                    blit_vram_wr_o      <= 1'b1;
                    blit_vram_data_o    <= { 8'h17, logostring[2*8-:8] };
                    blit_state          <= LOGO_4;
                end
                LOGO_4: begin
                    blit_vram_sel_o     <= 1'b1;
                    blit_vram_wr_o      <= 1'b1;
                    blit_vram_data_o    <= { 8'h17, logostring[1*8-:8] };
                    blit_state          <= LOGO_END;
                end
                LOGO_END: begin
                    blit_reg[XVID_WR_ADDR[2:0]]  <= 16'h0000;
                    blit_state          <= READY;
                end
                default: begin
                    blit_state <= READY;
                end
            endcase
        end
    end
end

endmodule
