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
    input  wire logic            blit_busy_i,       // blit operation in progress
    input  wire logic            blit_full_i,       // blit register queue full
    // iCE40 reconfigure
    output      logic            reconfig_o,        // reconfigure iCE40 from flash
    output      logic  [1:0]     boot_select_o,     // reconfigure iCE40 flash config number
    // interrupt management
    output      logic  [3:0]     intr_mask_o,       // enabled interrupts
    output      logic  [3:0]     intr_clear_o,      // interrupt CPU acknowledge

`ifdef BUS_DEBUG_SIGNALS
    output      logic            bus_ack_o,         // ACK strobe for bus debug
`endif

    input  wire logic            reset_i,
    input  wire logic            clk
);

// read/write storage for main interface registers
addr_t          reg_xr_addr;            // XR read/write address (XR_ADDR)
word_t          reg_xr_data;            // word read from XR bus

word_t          reg_rd_incr;            // VRAM read increment
addr_t          reg_rd_addr;            // VRAM read address
word_t          reg_rd_data;            // word read from VRAM (for RD_ADDR)

word_t          reg_wr_incr;            // VRAM write increment
addr_t          reg_wr_addr;            // VRAM write address

word_t          reg_rw_incr;            // VRAM read/write increment
addr_t          reg_rw_addr;            // VRAM read/write address
word_t          reg_rw_data;            // word read from VRAM (for RW_ADDR)

logic           reg_rw_rd_inc;          // flag to enable RW_ADDR add of RW_INCR on read

// read flags
logic           xr_rd;                  // flag for XR_DATA read outstanding
logic           vram_rd;                // flag for DATA read outstanding
logic           vram_rw_rd;             // flag for RW_DATA read outstanding
logic           vram_rw_wr;             // flag for RW_DATA write outstanding


// internal storage
logic  [3:0]    intr_mask;              // interrupt mask
logic  [3:0]    bus_reg_num;            // bus register on bus

logic  [7:0]    reg_xr_data_even;       // byte written to even byte of XR_DATA
logic  [7:0]    reg_data_even;          // byte written to even byte of XM_DATA/XM_DATA_2

logic           bus_write_strobe;       // strobe when a word of data written
logic           bus_read_strobe;        // strobe when a word of data read
logic           bus_bytesel;            // msb/lsb on bus
logic  [7:0]    bus_data_byte;          // data byte from bus

word_t          reg_timer;              // 1/10 ms timer (visible 16 bits)
logic [11:0]    reg_timer_frac;         // internal clock counter for 1/10 ms

`ifdef ENABLE_TIMERLATCH
logic [7:0]     timer_latch_val;
`endif

`ifdef ENABLE_LFSR
parameter               LFSR_SIZE = 19; // NOTE: if changed, must change taps
logic [LFSR_SIZE-1:0]   LFSR;
word_t          reg_LFSR;
`endif

logic mem_wait;
assign mem_wait    = xr_rd | vram_rd | vram_rw_rd | regs_wr_o | vram_rw_wr;

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
always_comb begin
    case (bus_reg_num)
        xv::XM_XR_ADDR:
            bus_data_o  = !bus_bytesel ? reg_xr_addr[15:8]      : reg_xr_addr[7:0];
        xv::XM_XR_DATA:
            bus_data_o  = !bus_bytesel ? reg_xr_data[15:8]      : reg_xr_data[7:0];
        xv::XM_RD_INCR:
            bus_data_o  = !bus_bytesel ? reg_rd_incr[15:8]      : reg_rd_incr[7:0];
        xv::XM_RD_ADDR:
            bus_data_o  = !bus_bytesel ? reg_rd_addr[15:8]      : reg_rd_addr[7:0];
        xv::XM_WR_INCR:
            bus_data_o  = !bus_bytesel ? reg_wr_incr[15:8]      : reg_wr_incr[7:0];
        xv::XM_WR_ADDR:
            bus_data_o  = !bus_bytesel ? reg_wr_addr[15:8]      : reg_wr_addr[7:0];
        xv::XM_DATA,
        xv::XM_DATA_2:
            bus_data_o  = !bus_bytesel ? reg_rd_data[15:8]      : reg_rd_data[7:0];
        xv::XM_SYS_CTRL:
            bus_data_o  = !bus_bytesel ? { 4'b0, intr_mask }    : { mem_wait, blit_busy_i, blit_full_i, reg_rw_rd_inc, regs_wrmask_o };
        xv::XM_TIMER:
`ifdef ENABLE_TIMERLATCH
            bus_data_o  = !bus_bytesel ? reg_timer[15:8]        : timer_latch_val;
`else
            bus_data_o  = !bus_bytesel ? reg_timer[15:8]        : reg_timer[7:0];
`endif
`ifdef ENABLE_LFSR
        xv::XM_LFSR:
            bus_data_o  = !bus_bytesel ? reg_LFSR[15:8]         : reg_LFSR[7:0];
`else
        xv::XM_UNUSED_A:
            bus_data_o  = 8'b0;
`endif
        xv::XM_UNUSED_B:
            bus_data_o  = 8'b0;
        xv::XM_RW_INCR:
            bus_data_o  = !bus_bytesel ? reg_rw_incr[15:8]      : reg_rw_incr[7:0];
        xv::XM_RW_ADDR:
            bus_data_o  = !bus_bytesel ? reg_rw_addr[15:8]      : reg_rw_addr[7:0];
        xv::XM_RW_DATA,
        xv::XM_RW_DATA_2:
            bus_data_o  = !bus_bytesel ? reg_rw_data[15:8]     : reg_rw_data[7:0];
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

// LFSR for random numbers
`ifdef ENABLE_LFSR
always_ff @(posedge clk) begin
    if (reset_i) begin
        LFSR <= LFSR_SIZE'(1);
    end else begin
        LFSR <= {LFSR[LFSR_SIZE-2:0], LFSR[18] ^~ LFSR[5] ^~ LFSR[1] ^~ LFSR[0]};

        // latch a new LFSR into reg_LFSR on bus activity
        if (bus_read_strobe || bus_write_strobe) begin
            reg_LFSR    <= LFSR[16:1];
        end
    end
end
`endif

always_ff @(posedge clk) begin
    if (reset_i) begin
        // control signals
        reconfig_o      <= 1'b0;
        boot_select_o   <= 2'b00;
        intr_clear_o    <= 4'b0;
        intr_mask       <= 4'b0000;
        // register signals
        regs_vram_sel_o <= 1'b0;
        regs_xr_sel_o   <= 1'b0;
        regs_wr_o       <= 1'b0;
        regs_wrmask_o   <= 4'b1111;
        vram_rd         <= 1'b0;
        vram_rw_wr      <= 1'b0;
        xr_rd           <= 1'b0;
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
`ifdef ENABLE_TIMERLATCH
        timer_latch_val <= 8'h00;
`endif
        reg_rw_rd_inc <= '0;

    end else begin

        intr_clear_o    <= 4'b0;

        // VRAM access acknowledge
        if (vram_ack_i) begin
            // if rd read then save rd data, increment rd_addr
            if (vram_rd) begin
                reg_rd_data    <= regs_data_i;
                reg_rd_addr     <= reg_rd_addr + reg_rd_incr;
            end

            // if rw read then save rw data, increment rw_addr
            if (vram_rw_rd) begin
                reg_rw_data    <= regs_data_i;
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
                reg_xr_data      <= xr_data_i;
            end

            if (regs_wr_o) begin
                reg_xr_addr     <= reg_xr_addr + 1'b1;  // TODO: optional xr rd increment?
            end

            regs_xr_sel_o   <= 1'b0;            // clear xr select
            regs_wr_o       <= 1'b0;            // clear write
            xr_rd           <= 1'b0;            // clear pending xr read
        end

        // even register byte write
        if (bus_write_strobe && !bus_bytesel) begin
            case (bus_reg_num)
                xv::XM_XR_ADDR:
                    reg_xr_addr[15:8]   <= bus_data_byte;
                xv::XM_XR_DATA:
                    reg_xr_data_even    <= bus_data_byte;   // data xr reg even byte storage
                xv::XM_RD_INCR:
                    reg_rd_incr[15:8]   <= bus_data_byte;
                xv::XM_RD_ADDR:
                    reg_rd_addr[15:8]   <= bus_data_byte;
                xv::XM_WR_INCR:
                    reg_wr_incr[15:8]   <= bus_data_byte;
                xv::XM_WR_ADDR:
                    reg_wr_addr[15:8]   <= bus_data_byte;
                xv::XM_DATA,
                xv::XM_DATA_2:
                    reg_data_even       <= bus_data_byte;   // data reg even byte storage
                xv::XM_SYS_CTRL:
                    { reconfig_o, boot_select_o, intr_mask} <= { bus_data_byte[7], bus_data_byte[6:5], bus_data_byte[3:0] };
                xv::XM_TIMER:
                    ;
`ifdef ENABLE_LFSR
                xv::XM_LFSR:
                    ;
`else
                xv::XM_UNUSED_A:
                    ;
`endif
                xv::XM_UNUSED_B:
                    ;
                xv::XM_RW_INCR:
                    reg_rw_incr[15:8]   <= bus_data_byte;
                xv::XM_RW_ADDR:
                    reg_rw_addr[15:8]   <= bus_data_byte;
                xv::XM_RW_DATA,
                xv::XM_RW_DATA_2:
                    reg_data_even       <= bus_data_byte;   // data reg even byte storage
            endcase
        end

        // odd register byte write (actives action)
        if (bus_write_strobe && bus_bytesel) begin
            case (bus_reg_num)
                xv::XM_XR_ADDR: begin
                    reg_xr_addr[7:0]    <= bus_data_byte;
                    regs_addr_o         <= { reg_xr_addr[15:8], bus_data_byte };    // output read addr (pre-read)
                    regs_xr_sel_o       <= 1'b1;            // select XR
                    xr_rd               <= 1'b1;            // remember pending aux read request
                end
                xv::XM_XR_DATA: begin
                    regs_addr_o         <= reg_xr_addr;
                    regs_data_o         <= { reg_xr_data_even, bus_data_byte };     // output write addr
                    regs_xr_sel_o       <= 1'b1;            // select XR
                    regs_wr_o           <= 1'b1;
                end
                xv::XM_RD_INCR: begin
                    reg_rd_incr[7:0]    <= bus_data_byte;
                end
                xv::XM_RD_ADDR: begin
                    reg_rd_addr[7:0]    <= bus_data_byte;
                    regs_addr_o         <= { reg_rd_addr[15:8], bus_data_byte };      // output read address
                    regs_vram_sel_o     <= 1'b1;            // select VRAM
                    vram_rd             <= 1'b1;            // remember pending vramread request
                end
                xv::XM_WR_INCR: begin
                    reg_wr_incr[7:0]    <= bus_data_byte;
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
                    reg_rw_rd_inc       <= bus_data_byte[4];
                    regs_wrmask_o       <= bus_data_byte[3:0];
                end
                xv::XM_TIMER: begin
                    intr_clear_o        <= bus_data_byte[3:0];
                end
`ifdef ENABLE_LFSR
                xv::XM_LFSR: begin
                end
`else
                xv::XM_UNUSED_A: begin
                end
`endif
                xv::XM_UNUSED_B: begin
                    ;
                end
                xv::XM_RW_INCR: begin
                    reg_rw_incr[7:0]    <= bus_data_byte;
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
        end

        if (bus_read_strobe & bus_bytesel) begin
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
`ifdef ENABLE_TIMERLATCH
        if (bus_read_strobe && !bus_bytesel) begin
            timer_latch_val <= reg_timer[7:0];
        end
`endif
    end
end
endmodule

`default_nettype wire               // restore default
