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
    // bus interface signals
    input  wire logic            bus_cs_n_i,        // register select strobe
    input  wire logic            bus_rd_nwr_i,      // 0 = write, 1 = read
    input  wire logic  [3:0]     bus_reg_num_i,     // register number
    input  wire logic            bus_bytesel_i,     // 0=even byte, 1=odd byte
    input  wire logic  [7:0]     bus_data_i,        // 8-bit data bus input
    output      logic  [7:0]     bus_data_o,        // 8-bit data bus output
    // VRAM/XR access signals
    input  wire logic            vram_ack_i,        // VRAM access ack (true when data read/written)
    input  wire logic            xr_ack_i,          // XR bus access ack (true when data read/written)
    output      logic            regs_vram_sel_o,   // VRAM select
    output      logic            regs_xr_sel_o,     // XR select
    output      logic            regs_wr_o,         // VRAM/XR read/write
    output      logic  [3:0]     regs_wrmask_o,     // VRAM nibble write masks
    output      addr_t           regs_addr_o,       // VRAM/XR address
    output      word_t           regs_data_o,       // VRAM/XR write data out
    input  wire word_t           regs_data_i,       // VRAM read data in
    input  wire word_t           xr_data_i,         // XR read data in
    // status signals
    input  wire logic            blit_full_i,       // blit register queue full
    input  wire logic            blit_busy_i,       // blit operation in progress
    input  wire logic            h_blank_i,         // pixel outside of visible range (before left edge)
    input  wire logic            v_blank_i,         // line outside of visible range (after bottom line)
    // iCE40 reconfigure
    output      logic            reconfig_o,        // reconfigure iCE40 from flash
    // interrupt management
    output      logic  [3:0]     intr_mask_o,       // enabled interrupts
    output      logic  [3:0]     intr_clear_o,      // interrupt CPU acknowledge

`ifdef BUS_DEBUG_SIGNALS
    output      logic            bus_ack_o,         // ACK strobe for bus debug
`endif

    input  wire logic            reset_i,           // reset signal
    input  wire logic            clk                // pixel clock
);

// read/write storage for main interface registers
addr_t          reg_rd_xaddr;           // XR read address (RD_XADDR)
addr_t          reg_wr_xaddr;           // XR write address (WR_XADDR)
word_t          reg_xdata;              // word read from XR bus (for RD_XDATA)

word_t          reg_rd_incr;            // VRAM read increment
addr_t          reg_rd_addr;            // VRAM read address
word_t          reg_data;               // word read from VRAM (for RD_ADDR)

word_t          reg_wr_incr;            // VRAM write increment
addr_t          reg_wr_addr;            // VRAM write address

word_t          reg_rw_incr;            // VRAM read/write increment
addr_t          reg_rw_addr;            // VRAM read/write address
word_t          reg_rw_data;            // word read from VRAM (for RW_ADDR)

word_t          reg_timer;              // 1/10 ms timer (visible 16 bits)
logic [11:0]    reg_timer_frac;         // internal clock counter for 1/10 ms

logic  [3:0]    intr_mask;              // interrupt mask
logic           reg_rw_rd_inc;          // flag to enable RW_ADDR add of RW_INCR on read

// read flags
logic           xr_rd;                  // flag for XR_DATA read outstanding
logic           vram_rd;                // flag for DATA read outstanding
logic           vram_rw_rd;             // flag for RW_DATA read outstanding
logic           vram_rw_wr;             // flag for RW_DATA write outstanding

logic  [3:0]    bus_reg_num;            // bus register on bus
logic           bus_write_strobe;       // strobe when a word of data written
logic           bus_read_strobe;        // strobe when a word of data read
logic           bus_bytesel;            // msb/lsb on bus
byte_t          bus_data_byte;          // data byte from bus

byte_t          timer_latch_val;        // low byte of timer (latched on high byte read)
byte_t          reg_xdata_even;         // byte written to even byte of XR_XDATA
byte_t          reg_data_even;          // byte written to even byte of XM_DATA/XM_DATA_2
byte_t          reg_rw_data_even;       // byte written to even byte of XM_RW_DATA/XM_RW_DATA_2


logic mem_wait;
assign mem_wait    = xr_rd | vram_rd | vram_rw_rd | regs_wr_o;  // believed unneeded: | vram_rw_wr

// output interrupt mask
assign intr_mask_o = intr_mask;

`ifdef BUS_DEBUG_SIGNALS    // debug "ack" bus strobe
assign bus_ack_o = (bus_write_strobe | bus_read_strobe);
`endif

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

word_t      rd_temp_word;
always_comb bus_data_o = !bus_bytesel ? rd_temp_word[15:8] : rd_temp_word[7:0];

always_comb begin
    case (bus_reg_num)
        xv::XM_SYS_CTRL:
            rd_temp_word  = { mem_wait, blit_full_i, blit_busy_i, 1'b0, h_blank_i, v_blank_i, 1'b0, reg_rw_rd_inc, 4'b0, regs_wrmask_o };
        xv::XM_INT_CTRL:
            rd_temp_word  = { 4'b0, intr_mask, 8'b0 };
        xv::XM_TIMER:
            rd_temp_word  = { reg_timer[15:8], timer_latch_val };
        xv::XM_RD_XADDR:
            rd_temp_word  = reg_rd_xaddr;
        xv::XM_WR_XADDR:
            rd_temp_word  = reg_wr_xaddr;
        xv::XM_XDATA:
            rd_temp_word  = reg_xdata;
        xv::XM_RD_INCR:
            rd_temp_word  = reg_rd_incr;
        xv::XM_RD_ADDR:
            rd_temp_word  = reg_rd_addr;
        xv::XM_WR_INCR:
            rd_temp_word  = reg_wr_incr;
        xv::XM_WR_ADDR:
            rd_temp_word  = reg_wr_addr;
        xv::XM_DATA,
        xv::XM_DATA_2:
            rd_temp_word  = reg_data;
        xv::XM_RW_INCR:
            rd_temp_word  = reg_rw_incr;
        xv::XM_RW_ADDR:
            rd_temp_word  = reg_rw_addr;
        xv::XM_RW_DATA,
        xv::XM_RW_DATA_2:
            rd_temp_word  = reg_rw_data;
    endcase
end

// 1/10th ms timer counter
always_ff @(posedge clk) begin
    if (reset_i) begin
        reg_timer <= 16'h0000;
        reg_timer_frac <= 12'h000;
    end else begin
        reg_timer_frac <= reg_timer_frac + 1'b1;
        if (reg_timer_frac == 12'(xv::PCLK_HZ / 10000)) begin
            reg_timer_frac   <= 12'h000;
            reg_timer        <= reg_timer + 1;
        end
    end
end

always_ff @(posedge clk) begin
    if (reset_i) begin
        // control signal strobes
        reconfig_o      <= 1'b0;
        intr_clear_o    <= 4'b0000;

        // control signals
        regs_vram_sel_o <= 1'b0;
        regs_xr_sel_o   <= 1'b0;
        regs_wr_o       <= 1'b0;
        vram_rd         <= 1'b0;
        xr_rd           <= 1'b0;

        // addr/data out
        regs_addr_o     <= 16'h0000;
        regs_data_o     <= 16'h0000;

        // xosera registers
        reg_rd_xaddr    <= 16'h0000;
        reg_wr_xaddr    <= 16'h0000;
        reg_rd_addr     <= 16'h0000;
        reg_rd_incr     <= 16'h0000;
        reg_wr_addr     <= 16'h0000;
        reg_wr_incr     <= 16'h0000;
        reg_rw_addr     <= 16'h0000;
        reg_rw_incr     <= 16'h0000;
        regs_wrmask_o   <= 4'b1111;
        intr_mask       <= 4'b0000;
        reg_rw_rd_inc   <= 1'b0;

        // temp registers
        timer_latch_val <= 8'h00;
        reg_data_even   <= 8'h00;
        reg_xdata_even  <= 8'h00;
        reg_rw_data_even<= 8'h00;

        reg_data        <= '0;
        reg_xdata       <= '0;
        reg_rw_data     <= '0;

    end else begin
        // clear strobe signals
        intr_clear_o    <= 4'b0000;

        // VRAM access acknowledge
        if (vram_ack_i) begin
            // if rd read then save rd data, increment rd_addr
            if (vram_rd) begin
                reg_data        <= regs_data_i;
                reg_rd_addr     <= reg_rd_addr + reg_rd_incr;
            end

            // if rw read then save rw data, increment rw_addr
            if (vram_rw_rd) begin
                reg_rw_data     <= regs_data_i;
                if (reg_rw_rd_inc) begin
                    reg_rw_addr     <= reg_rw_addr + reg_rw_incr;
                end
            end

            // if we did a wr write, increment wr addr
            if (regs_wr_o && !vram_rw_wr) begin
                reg_wr_addr     <= reg_wr_addr + reg_wr_incr;
            end

            // if we did a rw write, increment rw addr
            if (vram_rw_wr) begin
                reg_rw_addr     <= reg_rw_addr + reg_rw_incr;
            end

            regs_vram_sel_o <= 1'b0;
            regs_wr_o       <= 1'b0;
            vram_rd         <= 1'b0;
            vram_rw_wr      <= 1'b0;
            vram_rw_rd      <= 1'b0;
        end

        // XR access acknowledge
        if (xr_ack_i) begin
            if (xr_rd) begin
                reg_xdata       <= xr_data_i;
                reg_rd_xaddr    <= reg_rd_xaddr + 1'b1;
            end

            if (regs_wr_o) begin
                reg_wr_xaddr    <= reg_wr_xaddr + 1'b1;
            end

            regs_xr_sel_o   <= 1'b0;            // clear xr select
            regs_wr_o       <= 1'b0;            // clear write
            xr_rd           <= 1'b0;            // clear pending xr read
        end

        // register write
        if (bus_write_strobe) begin
            case (bus_reg_num)
                xv::XM_SYS_CTRL: begin
                    if (!bus_bytesel) begin
                        reg_rw_rd_inc       <= bus_data_byte[0];
                    end else begin
                        regs_wrmask_o       <= bus_data_byte[3:0];
                    end
                end
                xv::XM_INT_CTRL: begin
                    if (!bus_bytesel) begin
                        intr_mask           <= bus_data_byte[3:0];
                    end else begin
                        intr_clear_o        <= bus_data_byte[3:0];
                    end
                end
                xv::XM_TIMER: begin
                    if (!bus_bytesel) begin
                        reconfig_o          <= (bus_data_byte[7:4] == 4'hB) ? 1'b1 : 1'b0;
                    end
                end
                xv::XM_RD_XADDR: begin
                    if (!bus_bytesel) begin
                        reg_rd_xaddr[15:8]  <= bus_data_byte;
                    end else begin
                        reg_rd_xaddr[7:0]   <= bus_data_byte;
                        regs_xr_sel_o       <= 1'b1;            // select XR
                        xr_rd               <= 1'b1;            // remember pending XR read request
                    end
                    regs_addr_o         <= { reg_rd_xaddr[15:8], bus_data_byte };    // output read addr (pre-read)
                end
                xv::XM_WR_XADDR: begin
                    if (!bus_bytesel) begin
                        reg_wr_xaddr[15:8]  <= bus_data_byte;
                    end else begin
                        reg_wr_xaddr[7:0]   <= bus_data_byte;
                    end
                end
                xv::XM_XDATA: begin
                    if (!bus_bytesel) begin
                        reg_xdata_even      <= bus_data_byte;   // data xr reg even byte storage
                    end else begin
                        regs_xr_sel_o       <= 1'b1;            // select XR
                        regs_wr_o           <= 1'b1;
                    end
                    regs_addr_o         <= reg_wr_xaddr;
                    regs_data_o         <= { reg_xdata_even, bus_data_byte };     // output write addr
                end
                xv::XM_RD_INCR: begin
                    if (!bus_bytesel) begin
                        reg_rd_incr[15:8]   <= bus_data_byte;
                    end else begin
                        reg_rd_incr[7:0]    <= bus_data_byte;
                    end
                end
                xv::XM_RD_ADDR: begin
                    if (!bus_bytesel) begin
                        reg_rd_addr[15:8]   <= bus_data_byte;
                    end else begin
                        reg_rd_addr[7:0]    <= bus_data_byte;
                        regs_vram_sel_o     <= 1'b1;            // select VRAM
                        vram_rd             <= 1'b1;            // remember pending VRAM read request
                    end
                    regs_addr_o         <= { reg_rd_addr[15:8], bus_data_byte };      // output read address
                end
                xv::XM_WR_INCR: begin
                    if (!bus_bytesel) begin
                        reg_wr_incr[15:8]   <= bus_data_byte;
                    end else begin
                        reg_wr_incr[7:0]    <= bus_data_byte;
                    end
                end
                xv::XM_WR_ADDR: begin
                    if (!bus_bytesel) begin
                        reg_wr_addr[15:8]   <= bus_data_byte;
                    end else begin
                        reg_wr_addr[7:0]    <= bus_data_byte;
                    end
                end
                xv::XM_DATA,
                xv::XM_DATA_2: begin
                    if (!bus_bytesel) begin
                        reg_data_even       <= bus_data_byte;   // data reg even byte storage
                    end else begin
                        regs_vram_sel_o     <= 1'b1;            // select VRAM
                        regs_wr_o           <= 1'b1;            // write
                    end
                    regs_addr_o         <= reg_wr_addr;    // output write address
                    regs_data_o         <= { reg_data_even, bus_data_byte };      // output write data
                end
                xv::XM_RW_INCR: begin
                    if (!bus_bytesel) begin
                        reg_rw_incr[15:8]   <= bus_data_byte;
                    end else begin
                        reg_rw_incr[7:0]    <= bus_data_byte;
                    end
                end
                xv::XM_RW_ADDR: begin
                    if (!bus_bytesel) begin
                        reg_rw_addr[15:8]   <= bus_data_byte;
                    end else begin
                        reg_rw_addr[7:0]    <= bus_data_byte;
                        regs_vram_sel_o     <= 1'b1;            // select VRAM
                        vram_rd             <= 1'b1;            // remember pending vramread request
                        vram_rw_rd          <= 1'b1;            // remember rw read
                    end
                    regs_addr_o         <= { reg_rw_addr[15:8], bus_data_byte };      // output read address
                end
                xv::XM_RW_DATA,
                xv::XM_RW_DATA_2: begin
                    if (!bus_bytesel) begin
                        reg_rw_data_even    <= bus_data_byte;   // data reg even byte storage
                    end else begin
                        regs_vram_sel_o     <= 1'b1;            // select VRAM
                        regs_wr_o           <= 1'b1;            // write
                        vram_rw_wr          <= 1'b1;            // remember rw write
                    end
                    regs_addr_o         <= reg_rw_addr;    // output write address
                    regs_data_o         <= { reg_rw_data_even, bus_data_byte };      // output write data
                end
            endcase
        end

        // if data read, start next pre-read
        if (bus_read_strobe && bus_bytesel) begin
            // if read from xdata then pre-read next xr rd address
            if (bus_reg_num == xv::XM_XDATA) begin
                regs_addr_o         <= reg_rd_xaddr;    // output read address
                regs_xr_sel_o       <= 1'b1;            // select XR
                xr_rd               <= 1'b1;            // remember pending vram read request
            end
            // if read from data then pre-read next vram rd address
            if (bus_reg_num == xv::XM_DATA || bus_reg_num == xv::XM_DATA_2) begin
                regs_addr_o         <= reg_rd_addr;     // output read address
                regs_vram_sel_o     <= 1'b1;            // select VRAM
                vram_rd             <= 1'b1;            // remember pending vram read request
            end
            // if read from rw_data then pre-read next vram rw address
            if (bus_reg_num == xv::XM_RW_DATA || bus_reg_num == xv::XM_RW_DATA_2) begin
                regs_addr_o         <= reg_rw_addr;     // output read address
                regs_vram_sel_o     <= 1'b1;            // select VRAM
                vram_rw_rd          <= 1'b1;            // remember pending vram read request
            end
        end

        // latch low byte of timer when upper byte read
        if (bus_read_strobe && !bus_bytesel) begin
            timer_latch_val <= reg_timer[7:0];
        end
    end
end
endmodule

`default_nettype wire               // restore default
