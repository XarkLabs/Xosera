// reg_interface.sv
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

module reg_interface(
    input  wire logic            bus_cs_n_i,        // register select strobe
    input  wire logic            bus_rd_nwr_i,      // 0 = write, 1 = read
    input  wire logic  [3:0]     bus_reg_num_i,     // register number
    input  wire logic            bus_bytesel_i,     // 0=even byte, 1=odd byte
    input  wire logic  [7:0]     bus_data_i,        // 8-bit data bus input
    output      logic  [7:0]     bus_data_o,        // 8-bit data bus output
    input  wire logic            vgen_sel_i,        // true during video generation cycle
    output      logic            regs_vram_sel_o,   // VRAM select
    output      logic            regs_xr_sel_o,     // XR select
    output      logic            regs_wr_o,         // VRAM/XR read/write
    output      logic  [3:0]     regs_mask_o,       // VRAM nibble write masks
    output      logic [15:0]     regs_addr_o,       // VRAM/XR address
    input  wire logic [15:0]     regs_data_i,       // VRAM read data
    output      logic [15:0]     regs_data_o,       // VRAM/XR write data
    input  wire logic [15:0]     xr_data_i,         // XR read data
    output      logic            reconfig_o,        // reconfigure iCE40 from flash
    output      logic  [1:0]     boot_select_o,     // reconfigure congigureation number (0-3)
    output      logic  [3:0]     intr_mask_o,       // enabled interrupts
    output      logic  [3:0]     intr_clear_o,      // interrupt CPU acknowledge
    output      logic            bus_ack_o,         // TODO ACK strobe for debug
    input  wire logic            reset_i,
    input  wire logic            clk
    );

// read/write storage for first 4 reg_interface registers
logic [15:0]    reg_xr_addr;            // XR read/write address (XR_ADDR)
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

logic           bus_write_strobe;       // strobe when a word of data written
logic           bus_read_strobe;        // strobe when a word of data read
logic           bus_bytesel;            // msb/lsb on bus
logic  [7:0]    bus_data_byte;          // data byte from bus

logic [15:0]    ms_timer;               // 1/10 ms timer (visible 16 bits)
logic [11:0]    ms_timer_frac;          // internal clock counter for 1/10 ms

// output interrupt mask
assign intr_mask_o = intr_mask;

// debug "ack" bus strobe
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

        xv::XM_SYS_CTRL[3:0]:   reg_read = !b_sel ? { 4'b0, regs_mask_o }: { 4'b0, intr_mask };
        xv::XM_TIMER[3:0]:      reg_read = !b_sel ? ms_timer[15:8]      : ms_timer[7:0];

        xv::XM_UNUSED_A[3:0]:   reg_read = 8'b0;
        xv::XM_UNUSED_B[3:0]:   reg_read = 8'b0;

        xv::XM_RW_INCR[3:0]:    reg_read = !b_sel ? reg_rw_incr[15:8]   : reg_rw_incr[7:0];
        xv::XM_RW_ADDR[3:0]:    reg_read = !b_sel ? reg_rw_addr[15:8]   : reg_rw_addr[7:0];

        xv::XM_RW_DATA[3:0],
        xv::XM_RW_DATA_2[3:0]:  reg_read = !b_sel ? vram_rw_data[15:8]  : vram_rw_data[7:0];
    endcase
endfunction

// 1/10th ms timer counter
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

// even byte to write (since write happens on odd byte)
assign reg_even_byte = (reg_other_reg == bus_reg_num) ? reg_other_even : 8'h00;

always_ff @(posedge clk) begin
    if (reset_i) begin
        // control signals
        reconfig_o      <= 1'b0;
        boot_select_o   <= 2'b00;
        intr_clear_o    <= 4'b0;
        regs_mask_o     <= 4'b1111;
        intr_mask       <= 4'b1000;
        vram_rd         <= 1'b0;
        vram_rd_ack     <= 1'b0;
        xr_rd           <= 1'b0;
        xr_rd_ack       <= 1'b0;
        regs_vram_sel_o <= 1'b0;
        regs_xr_sel_o   <= 1'b0;
        regs_wr_o       <= 1'b0;
        // addr/data out
        regs_addr_o     <= 16'h0000;
        regs_data_o     <= 16'h0000;

        // xosera registers
        reg_xr_addr     <= 16'h0000;
        reg_rd_addr     <= 16'h0000;
        reg_rd_incr     <= 16'h0000;
        reg_wr_addr     <= 16'h0000;
        reg_wr_incr     <= 16'h0000;
        reg_rw_addr     <= 16'h0000;
        reg_rw_incr     <= 16'h0000;
        reg_data_even   <= 8'h00;
        reg_other_even  <= 8'h00;
        reg_other_reg   <= 4'h0;
    end
    else begin
        intr_clear_o    <= 4'b0;

        // if a rd read ack is pending, save value from vram
        if (vram_rd_ack) begin
            vram_rd_data    <= regs_data_i;
        end
        vram_rd_ack <= 1'b0;

        // if a rw read ack is pending, save value from vram
        if (vram_rw_ack) begin
            vram_rw_data     <= regs_data_i;
        end
        vram_rw_ack <= 1'b0;

        // if a xr read ack is pending, save value from xr data
        if (xr_rd_ack) begin
            xr_rd_data     <= xr_data_i;
        end
        xr_rd_ack <= 1'b0;

        if (!vgen_sel_i) begin
            vram_rd_ack <= vram_rd;     // ack is one cycle after read with reg_interface access
            vram_rw_ack <= vram_rw_rd;  // ack is one cycle after read with reg_interface access
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
            if (regs_vram_sel_o && regs_wr_o && !vram_rw_wr) begin
                reg_wr_addr  <= reg_wr_addr + reg_wr_incr;
            end

            // if we did a rw write, increment rw addr
            if (vram_rw_wr) begin
                reg_rw_addr  <= reg_rw_addr + reg_rw_incr;
            end
  
            // if xr write auto increment
            if (regs_xr_sel_o && regs_wr_o) begin
                reg_xr_addr  <= reg_xr_addr + 1'b1;
            end

            regs_addr_o     <= reg_wr_addr;     // assume VRAM write output address
            regs_vram_sel_o <= 1'b0;            // clear vram select
            regs_xr_sel_o   <= 1'b0;            // clear xr select
            regs_wr_o       <= 1'b0;            // clear write
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
                    xv::XM_RW_ADDR:
                        reg_rw_addr[15:8]   <= bus_data_byte;
                    xv::XM_XR_DATA:
                        reg_xr_data_even    <= bus_data_byte;   // data xr reg even byte storage
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
                        regs_addr_o         <= { reg_xr_addr[15:8], bus_data_byte };      // output read address
                        regs_xr_sel_o       <= 1'b1;            // select XR
                        xr_rd               <= 1'b1;            // remember pending aux read request
                    end
                    xv::XM_XR_DATA: begin
                        regs_addr_o         <= reg_xr_addr;
                        regs_data_o         <= { reg_xr_data_even, bus_data_byte };
                        regs_xr_sel_o       <= 1'b1;
                        regs_wr_o           <= 1'b1;
                    end
                    xv::XM_RD_INCR: begin
                        reg_rd_incr         <= { reg_even_byte, bus_data_byte };
                    end
                    xv::XM_RD_ADDR: begin
                        reg_rd_addr[7:0]    <= bus_data_byte;
                        regs_addr_o         <= { reg_rd_addr[15:8], bus_data_byte };      // output read address
                        regs_vram_sel_o     <= 1'b1;            // select VRAM
                        vram_rd             <= 1'b1;            // remember pending vramread request
                    end
                    xv::XM_WR_INCR: begin
                        reg_wr_incr         <= { reg_even_byte, bus_data_byte };
                    end
                    xv::XM_WR_ADDR: begin
                        reg_wr_addr[7:0]    <= bus_data_byte;
                    end
                    xv::XM_DATA,
                    xv::XM_DATA_2: begin
                        regs_addr_o         <= reg_wr_addr;    // output write address
                        regs_data_o         <= { reg_data_even, bus_data_byte };      // output write data
                        regs_vram_sel_o     <= 1'b1;            // select VRAM
                        regs_wr_o           <= 1'b1;            // write
                    end
                    xv::XM_SYS_CTRL: begin
                        reconfig_o          <= reg_even_byte[7];
                        boot_select_o       <= reg_even_byte[6:5];
                        regs_mask_o         <= reg_even_byte[3:0];
                        intr_mask           <= bus_data_byte[3:0];
                    end
                    xv::XM_TIMER: begin
                        intr_clear_o        <= bus_data_byte[3:0];
                    end
                    xv::XM_UNUSED_A: begin
                    end
                    xv::XM_UNUSED_B: begin
                    end
                    xv::XM_RW_INCR: begin
                        reg_rw_incr         <= { reg_even_byte, bus_data_byte };
                    end
                    xv::XM_RW_ADDR: begin
                        reg_rw_addr[7:0]    <= bus_data_byte;
                        regs_addr_o         <= { reg_rw_addr[15:8], bus_data_byte };      // output read address
                        regs_vram_sel_o     <= 1'b1;            // select VRAM
                        vram_rd             <= 1'b1;            // remember pending vramread request
                        vram_rw_rd          <= 1'b1;            // remember rw read
                    end
                    xv::XM_RW_DATA,
                    xv::XM_RW_DATA_2: begin
                        regs_addr_o         <= reg_rw_addr;    // output write address
                        regs_data_o         <= { reg_data_even, bus_data_byte };      // output write data
                        regs_vram_sel_o     <= 1'b1;            // select VRAM
                        regs_wr_o           <= 1'b1;            // write
                        vram_rw_wr          <= 1'b1;            // remember rw write
                    end
                endcase
            end // bus_bytesel
        end // bus_write_strobe

        if (bus_read_strobe & bus_bytesel) begin
            // if read from data then pre-read next vram rd address
            if (bus_reg_num == xv::XM_DATA || bus_reg_num == xv::XM_DATA_2) begin
                regs_addr_o         <= reg_rd_addr;      // output read address
                regs_vram_sel_o     <= 1'b1;            // select VRAM
                vram_rd             <= 1'b1;            // remember pending vram read request
            end
            // if read from rw_data then pre-read next vram rw address
            if (bus_reg_num == xv::XM_RW_DATA || bus_reg_num == xv::XM_RW_DATA_2) begin
                regs_addr_o         <= reg_rw_addr;      // output read address
                regs_vram_sel_o     <= 1'b1;            // select VRAM
                vram_rw_rd          <= 1'b1;            // remember pending vram read request
            end
        end
    end
end
endmodule

`default_nettype wire               // restore default
