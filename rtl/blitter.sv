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
    input  wire logic            bus_cs_n_i,        // register select strobe
    input  wire logic            bus_rd_nwr_i,      // 0 = write, 1 = read
    input  wire logic  [3:0]     bus_reg_num_i,     // register number
    input  wire logic            bus_bytesel_i,     // 0=even byte, 1=odd byte
    input  wire logic  [7:0]     bus_data_i,        // 8-bit data bus input
    output      logic  [7:0]     bus_data_o,        // 8-bit data bus output
    input  wire logic            vgen_sel_i,        // 0 = blitter, 1=video generation
    output      logic            blit_vram_sel_o,   // VRAM select
    output      logic            blit_xr_sel_o,     // XR select
    output      logic            blit_wr_o,         // VRAM/XR read/write
    output      logic  [3:0]     blit_mask_o,       // VRAM nibble write masks
    output      logic [15:0]     blit_addr_o,       // VRAM/XR address
    input  wire logic [15:0]     blit_data_i,       // VRAM read data
    output      logic [15:0]     blit_data_o,       // VRAM/XR write data
    input  wire logic [15:0]     xr_data_i,         // XR read data
    output      logic            reconfig_o,        // reconfigure iCE40 from flash
    output      logic  [1:0]     boot_select_o,     // reconfigure congigureation number (0-3)
    output      logic  [3:0]     intr_mask_o,       // enabled interrupts
    output      logic  [3:0]     intr_clear_o,      // interrupt CPU acknowledge
    output      logic            bus_ack_o,         // TODO ACK strobe for debug
    input  wire logic            reset_i,
    input  wire logic            clk
    );

localparam [31:0] githash = 32'H`GITHASH;
localparam [11:0] version = 12'H`VERSION;

logic [8*8:1]  logostring = "Xosera v";    // boot msg

localparam CLEARDATA = 16'h0220;    // value VRAM cleared to on init (blue+white space) TODO: zero for final?

typedef enum logic [4:0] {
    INIT, CLEAR, LOGO_1, LOGO_2, LOGO_3, LOGO_4, LOGO_5, LOGO_6, LOGO_7, LOGO_8,
    LOGO_V0, LOGO_V1, LOGO_V2, LOGO_V3, LOGO_V4, LOGO_V5,
    LOGO_H0, LOGO_H1, LOGO_H2, LOGO_H3, LOGO_H4, LOGO_H5, LOGO_H6, LOGO_H7, LOGO_END,
    IDLE
} blit_state_t;

blit_state_t    blit_state;

// read/write storage for first 4 blitter registers
logic [15:0]    reg_xr_addr;           // XR read/write address (XR_ADDR)
logic [15:0]    xr_rd_data;             // word read from XR
logic           xr_rd;                  // flag for xr read outstanding
logic           xr_rd_ack;              // flag for xr read acknowledged 
logic [15:0]    reg_rd_incr;            // VRAM read increment
logic [15:0]    reg_rd_addr;            // VRAM read address
logic [15:0]    vram_rd_data;           // word read from VRAM (for RD_ADDR)
logic           vram_rd;                // flag for VRAM read outstanding
logic           vram_rd_ack;            // flag for VRAM read acknowledged 
logic [15:0]    reg_wr_incr;            // VRAM write increment
logic [15:0]    reg_wr_addr;            // VRAM write address
logic [15:0]    reg_rw_incr;            // VRAM read/write increment
logic [15:0]    reg_rw_addr;            // VRAM read/write address
logic [15:0]    vram_rw_data;           // word read from VRAM (for RW_ADDR)
logic           vram_rw_rd;             // flag for VRAM RW read outstanding
logic           vram_rw_wr;             // flag for VRAM RW write outstanding
logic           vram_rw_ack;            // flag for VRAM RW read acknowledged 

// internal storage
logic  [3:0]    intr_mask;              // interrupt mask
logic  [3:0]    bus_reg_num;            // bus register on bus

logic  [7:0]    reg_xr_data_even;       // byte written to even byte of XR_DATA
logic  [7:0]    reg_data_even;          // byte written to even byte of XM_DATA/XM_DATA_2

logic  [3:0]    reg_other_reg;          // register associated with reg_other_even
logic  [7:0]    reg_other_even;         // even byte storage (until odd byte)
logic  [7:0]    reg_even_byte;          // either reg_other_even or zero if different register
assign          reg_even_byte = (reg_other_reg == bus_reg_num) ? reg_other_even : 8'h00;

logic           bus_write_strobe;       // strobe when a word of data written
logic           bus_read_strobe;        // strobe when a word of data read
logic           bus_bytesel;            // msb/lsb on bus
logic  [7:0]    bus_data_byte;          // data byte from bus

// write only storage for the rest
logic [16:0]    blit_count;             // (extra bit for underflow/flag)
// TODO logic [15:0]    reg_rd_mod;
// TODO logic [15:0]    reg_wr_mod;
// TODO logic [15:0]    reg_width;
// TODO logic           blit_2d;
// TODO logic           blit_const;

// TODO logic [16:0]    width_counter;          // blit count (extra bit for underflow/done)
// TODO logic [15:0]    blit_rd_addr;           // TODO VRAM read address
// TODO logic [15:0]    blit_wr_addr;           // TODO VRAM write address
logic           blit_busy;
assign          blit_busy = ~blit_count[16]; // when blit_count underflows, high bit will be set
// TODO logic           blit_line_end;
// TODO assign          blit_line_end = width_counter[16];  // when width_counter underflows, high bit will be set

logic [15:0] ms_timer;      // TODO
logic [11:0] ms_timer_frac;  // TODO
logic [3:0] wr_nibmask;      // TODO

assign intr_mask_o = intr_mask;

assign bus_ack_o = (bus_write_strobe | bus_read_strobe);    // TODO: debug

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

// continuously output byte selected for read from Xosera (to be put on bus when selected for read)
assign bus_data_o = reg_read(bus_bytesel, bus_reg_num);

// function to continuously select read value to put on bus
function [7:0] reg_read(
    input logic         b_sel,
    input logic [3:0]   r_sel
    );
    case (r_sel)
        xv::XM_XR_ADDR[3:0]:    reg_read = !b_sel ? reg_xr_addr[15:8]   : reg_xr_addr[7:0];
        xv::XM_XR_DATA[3:0]:    reg_read = !b_sel ? xr_rd_data[15:8]    : xr_rd_data[7:0];

        xv::XM_RD_INCR[3:0]:    reg_read = !b_sel ? reg_rd_incr[15:8]   : reg_rd_incr[7:0];
        xv::XM_RD_ADDR[3:0]:    reg_read = !b_sel ? reg_rd_addr[15:8]   : reg_rd_addr[7:0];

        xv::XM_WR_INCR[3:0]:    reg_read = !b_sel ? reg_wr_incr[15:8]   : reg_wr_incr[7:0];
        xv::XM_WR_ADDR[3:0]:    reg_read = !b_sel ? reg_wr_addr[15:8]   : reg_wr_addr[7:0];

        xv::XM_DATA[3:0],
        xv::XM_DATA_2[3:0]:     reg_read = !b_sel ? vram_rd_data[15:8]  : vram_rd_data[7:0];

        xv::XM_SYS_CTRL[3:0]:   reg_read = !b_sel ? { 4'bx, wr_nibmask }: { blit_busy, 3'bx, intr_mask };
        xv::XM_TIMER[3:0]:      reg_read = !b_sel ? ms_timer[15:8]      : ms_timer[7:0];

        xv::XM_UNUSED_A[3:0]:   reg_read = 8'bx;
        xv::XM_UNUSED_B[3:0]:   reg_read = 8'bx;

        xv::XM_RW_INCR[3:0]:    reg_read = !b_sel ? reg_rw_incr[15:8]   : reg_rw_incr[7:0];
        xv::XM_RW_ADDR[3:0]:    reg_read = !b_sel ? reg_rw_addr[15:8]   : reg_rw_addr[7:0];

        xv::XM_RW_DATA[3:0],
        xv::XM_RW_DATA_2[3:0]:  reg_read = !b_sel ? vram_rw_data[15:8]  : vram_rw_data[7:0];
    endcase
endfunction

function [7:0] hex_digit(
    input logic[3:0]    nib
    );
    if (nib > 9) begin
        hex_digit = 8'h57 + { 4'h0, nib };
    end
    else begin
        hex_digit = 8'h30 + { 4'h0, nib };
    end
endfunction

always_ff @(posedge clk) begin
    if (reset_i) begin
        ms_timer <= 16'h0000;
        ms_timer_frac <= 12'h000;
    end else begin
        ms_timer_frac <= ms_timer_frac + 1'b1;
        if (ms_timer_frac == 12'(xv::PCLK_HZ / 10000)) begin
            ms_timer_frac   <= 12'h000;
            ms_timer        <= ms_timer + 1;
        end
    end
end

assign blit_mask_o = { blit_wr_o, blit_wr_o,  blit_wr_o, blit_wr_o };   // TODO replace blit_wr with just mask (non-zero for write)

always_ff @(posedge clk) begin
    if (reset_i) begin
        // control signals
        reconfig_o      <= 1'b0;
        boot_select_o   <= 2'b00;
        intr_clear_o    <= 4'b0;
        intr_mask       <= 4'b1000;
        vram_rd         <= 1'b0;
        vram_rd_ack     <= 1'b0;
        xr_rd           <= 1'b0;
        xr_rd_ack       <= 1'b0;
        blit_vram_sel_o <= 1'b0;
        blit_xr_sel_o   <= 1'b0;
        blit_wr_o       <= 1'b0;
        // addr/data out
        blit_addr_o     <= 16'h0000;
        blit_data_o     <= 16'h0000;

        // internal blitter state
        blit_state      <= INIT;
// TODO         blit_2d             <= 1'b0;
// TODO         blit_const          <= 1'b0;
// TODO         width_counter       <= 17'h00000;   // 16th bit is set on underflow
// TODO         blit_rd_addr        <= 16'h0000;
// TODO         blit_wr_addr        <= 16'h0000;
// TODO         reg_rd_mod          <= 16'h0000;
// TODO         reg_wr_mod          <= 16'h0000;
// TODO         reg_width           <= 16'h0000;

        // xosera registers
        reg_xr_addr     <= 16'h0000;
        reg_rd_addr     <= 16'h0000;
        reg_rd_incr     <= 16'h0000;
        reg_wr_addr     <= 16'h0000;
        reg_wr_incr     <= 16'h0000;
        reg_rw_addr     <= 16'h0000;
        reg_rw_incr     <= 16'h0000;
        blit_count      <= 17'h10000;   // 16th bit is set on underfow
        reg_data_even   <= 8'h00;
        reg_other_even  <= 8'h00;
        reg_other_reg   <= 4'h0;
    end
    else begin
        intr_clear_o    <= 4'b0;

        // if a rd read ack is pending, save value from vram
        if (vram_rd_ack) begin
            vram_rd_data    <= blit_data_i;
        end
        vram_rd_ack <= 1'b0;

        // if a rw read ack is pending, save value from vram
        if (vram_rw_ack) begin
            vram_rw_data     <= blit_data_i;
        end
        vram_rw_ack <= 1'b0;

        // if a xr read ack is pending, save value from xr data
        if (xr_rd_ack) begin
            xr_rd_data     <= xr_data_i;
        end
        xr_rd_ack <= 1'b0;

        if (!vgen_sel_i) begin
            vram_rd_ack <= vram_rd;     // ack is one cycle after read with blitter access
            vram_rw_ack <= vram_rw_rd;  // ack is one cycle after read with blitter access
            xr_rd_ack   <= xr_rd;       // ack is one cycle after read with aux access

            // if we did a rd read, increment read addr
            if (vram_rd) begin
                reg_rd_addr  <= reg_rd_addr + reg_rd_incr;
            end

            // if we did a rw read, increment read addr
            if (vram_rw_rd) begin
                reg_rw_addr  <= reg_rw_addr + reg_rw_incr;
            end

            // if we did a wr write, increment wr addr
            if (blit_vram_sel_o && blit_wr_o && !vram_rw_wr) begin
                reg_wr_addr  <= reg_wr_addr + reg_wr_incr;

                // decrement count register if blitter busy
                if (blit_busy) begin
                    blit_count    <= blit_count - 1'b1;
                end
            end

            // if we did a rw write, increment rw addr
            if (vram_rw_wr) begin
                reg_wr_addr  <= reg_wr_addr + reg_wr_incr;
            end
  
            // if xr write auto increment
            if (blit_xr_sel_o && blit_wr_o) begin
                reg_xr_addr  <= reg_xr_addr + 1'b1;
            end

            blit_addr_o     <= reg_wr_addr;     // assume VRAM write output address
            blit_vram_sel_o <= 1'b0;            // clear vram select
            blit_xr_sel_o   <= 1'b0;            // clear xr select
            blit_wr_o       <= 1'b0;            // clear write
            xr_rd           <= 1'b0;            // clear pending xr read
            vram_rd         <= 1'b0;            // clear pending rd read
            vram_rw_rd      <= 1'b0;            // clear pending rw read
            vram_rw_wr      <= 1'b0;            // clear rw write
        end

        if (bus_write_strobe) begin
            if (!bus_bytesel) begin // even byte write (saved specially for certain registers)
                case (bus_reg_num)
                    xv::XM_XR_ADDR:
                        reg_xr_addr[15:8]   <= bus_data_byte;
                    xv::XM_RD_ADDR:
                        reg_rd_addr[15:8]   <= bus_data_byte;
                    xv::XM_WR_ADDR:
                        reg_wr_addr[15:8]   <= bus_data_byte;
                    xv::XM_XR_DATA:
                        reg_xr_data_even    <= bus_data_byte;   // data reg even byte storage
                    xv::XM_DATA,
                    xv::XM_DATA_2,
                    xv::XM_RW_DATA,
                    xv::XM_RW_DATA_2:
                        reg_data_even       <= bus_data_byte;   // data reg even byte storage
                    default: begin
                        reg_other_even      <= bus_data_byte;   // generic even byte storage
                        reg_other_reg       <= bus_reg_num;
                    end
                endcase
            end
            else begin              // odd byte write (actives action)
                case (bus_reg_num)
                    xv::XM_XR_ADDR: begin
                        reg_xr_addr[7:0]    <= bus_data_byte;
                        blit_addr_o         <= { reg_xr_addr[15:8], bus_data_byte };      // output read address
                        blit_xr_sel_o       <= 1'b1;            // select XR
                        xr_rd               <= 1'b1;            // remember pending aux read request
                    end
                    xv::XM_XR_DATA: begin
                        blit_addr_o         <= reg_xr_addr;
                        blit_data_o         <= { reg_xr_data_even, bus_data_byte };
                        blit_xr_sel_o       <= 1'b1;
                        blit_wr_o           <= 1'b1;
                    end
                    xv::XM_RD_INCR: begin
                        reg_rd_incr         <= { reg_even_byte, bus_data_byte };
                    end
                    xv::XM_RD_ADDR: begin
                        reg_rd_addr[7:0]    <= bus_data_byte;
                        blit_addr_o         <= { reg_rd_addr[15:8], bus_data_byte };      // output read address
                        blit_vram_sel_o     <= 1'b1;            // select VRAM
                        vram_rd             <= 1'b1;            // remember pending vramread request
                    end
                    xv::XM_WR_INCR: begin
                        reg_rw_incr         <= { reg_even_byte, bus_data_byte };
                    end
                    xv::XM_WR_ADDR: begin
                        reg_wr_addr[7:0]    <= bus_data_byte;
                    end
                    xv::XM_DATA,
                    xv::XM_DATA_2: begin
                        blit_addr_o         <= reg_wr_addr;    // output write address
                        blit_data_o         <= { reg_data_even, bus_data_byte };      // output write data
                        blit_vram_sel_o     <= 1'b1;            // select VRAM
                        blit_wr_o           <= 1'b1;            // write
                    end
                    xv::XM_SYS_CTRL: begin
                        reconfig_o          <= reg_even_byte[7];
                        boot_select_o       <= reg_even_byte[6:5];
                        wr_nibmask          <= reg_even_byte[3:0];
                        intr_mask           <= bus_data_byte[3:0];
                    end
                    xv::XM_TIMER: begin
                        intr_clear_o        <= bus_data_byte[3:0];
                    end
                    xv::XM_UNUSED_A: begin
                    end
                    xv::XM_UNUSED_B: begin
                    end
                    xv::XM_RW_ADDR: begin
                        reg_rw_addr[7:0]    <= bus_data_byte;
                        blit_addr_o         <= { reg_rw_addr[15:8], bus_data_byte };      // output read address
                        blit_vram_sel_o     <= 1'b1;            // select VRAM
                        vram_rd             <= 1'b1;            // remember pending vramread request
                        vram_rw_rd          <= 1'b1;            // remember rw read
                    end
                    xv::XM_RW_DATA: begin
                        blit_addr_o         <= reg_rw_addr;    // output write address
                        blit_data_o         <= { reg_data_even, bus_data_byte };      // output write data
                        blit_vram_sel_o     <= 1'b1;            // select VRAM
                        blit_wr_o           <= 1'b1;            // write
                        vram_rw_wr          <= 1'b1;            // remember rw write
                    end
                    default: begin
                    end
                endcase
            end // bus_bytesel
        end // bus_write_strobe

        if (bus_read_strobe & bus_bytesel) begin
            // if read from data then pre-read next vram rd address
            if (bus_reg_num == xv::XM_DATA || bus_reg_num == xv::XM_DATA_2) begin
                blit_addr_o         <= reg_rd_addr;      // output read address
                blit_vram_sel_o     <= 1'b1;            // select VRAM
                vram_rd             <= 1'b1;            // remember pending vramread request
            end
            // if read from rw_data then pre-read next vram rw address
            if (bus_reg_num == xv::XM_RW_DATA || bus_reg_num == xv::XM_RW_DATA_2) begin
                blit_addr_o         <= reg_rw_addr;      // output read address
                blit_vram_sel_o     <= 1'b1;            // select VRAM
                vram_rw_rd          <= 1'b1;            // remember pending vramread request
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
                blit_count       <= 17'h0FFFF;
`else
                blit_count       <= 17'h00FFF;        // smaller clear for simulation
`endif
                reg_wr_incr      <= 16'h0001;
                blit_state      <= CLEAR;
            end
            CLEAR: begin
                if (blit_busy) begin
                    blit_vram_sel_o <= 1'b1;
                    blit_wr_o       <= 1'b1;
`ifdef TESTPATTERN
                    if (reg_wr_addr[3:0] == 4'h1) begin
                        blit_data_o     <= { 8'h02, reg_wr_addr[15:8] };
                    end
                    else if (reg_wr_addr[3:0] == 4'h2) begin
                        blit_data_o     <= { 8'h02, reg_wr_addr[7:4], 4'h0 };
                    end
                    else begin
                        blit_data_o     <= {(reg_wr_addr[7:4] ^ 4'b1111), reg_wr_addr[7:4], reg_wr_addr[7:0] };
                    end

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
                blit_data_o         <= { 8'h0F, logostring[8*8-:8] };
                blit_state          <= LOGO_2;
            end
            LOGO_2: begin
                blit_vram_sel_o     <= 1'b1;
                blit_wr_o           <= 1'b1;
                blit_data_o         <= { 8'h0e, logostring[7*8-:8] };
                blit_state          <= LOGO_3;
            end
            LOGO_3: begin
                blit_vram_sel_o     <= 1'b1;
                blit_wr_o           <= 1'b1;
                blit_data_o         <= { 8'h0c, logostring[6*8-:8] };
                blit_state          <= LOGO_4;
            end
            LOGO_4: begin
                blit_vram_sel_o     <= 1'b1;
                blit_wr_o           <= 1'b1;
                blit_data_o         <= { 8'h0b, logostring[5*8-:8] };
                blit_state          <= LOGO_5;
            end
            LOGO_5: begin
                blit_vram_sel_o     <= 1'b1;
                blit_wr_o           <= 1'b1;
                blit_data_o         <= { 8'h02, logostring[4*8-:8] };
                blit_state          <= LOGO_6;
            end
            LOGO_6: begin
                blit_vram_sel_o     <= 1'b1;
                blit_wr_o           <= 1'b1;
                blit_data_o         <= { 8'h05, logostring[3*8-:8] };
                blit_state          <= LOGO_7;
            end
            LOGO_7: begin
                blit_vram_sel_o     <= 1'b1;
                blit_wr_o           <= 1'b1;
                blit_data_o         <= { 8'h07, logostring[2*8-:8] };
                blit_state          <= LOGO_8;
            end
            LOGO_8: begin
                blit_vram_sel_o     <= 1'b1;
                blit_wr_o           <= 1'b1;
                blit_data_o         <= { 8'h02, logostring[1*8-:8] };
                blit_state          <= LOGO_V0;
            end
            LOGO_V0: begin
                blit_vram_sel_o     <= 1'b1;
                blit_wr_o           <= 1'b1;
                blit_data_o         <= { 8'h02, hex_digit(version[3*4-1-:4]) };
                blit_state          <= LOGO_V1;
            end
            LOGO_V1: begin
                blit_vram_sel_o     <= 1'b1;
                blit_wr_o           <= 1'b1;
                blit_data_o         <= { 8'h02, 8'h2e };    // '.'
                blit_state          <= LOGO_V2;
            end
            LOGO_V2: begin
                blit_vram_sel_o     <= 1'b1;
                blit_wr_o           <= 1'b1;
                blit_data_o         <= { 8'h02, hex_digit(version[2*4-1-:4]) };
                blit_state          <= LOGO_V3;
            end
            LOGO_V3: begin
                blit_vram_sel_o     <= 1'b1;
                blit_wr_o           <= 1'b1;
                blit_data_o         <= { 8'h02, hex_digit(version[1*4-1-:4]) };
                blit_state          <= LOGO_V4;
            end
            LOGO_V4: begin
                blit_vram_sel_o     <= 1'b1;
                blit_wr_o           <= 1'b1;
                blit_data_o         <= { 8'h02, 8'h20 };    // ' '
                blit_state          <= LOGO_V5;
            end
            LOGO_V5: begin
                blit_vram_sel_o     <= 1'b1;
                blit_wr_o           <= 1'b1;
                blit_data_o         <= { 8'h02, 8'h23 };    // '#'
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

`default_nettype wire               // restore default
