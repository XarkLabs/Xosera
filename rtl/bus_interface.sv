// bus_interface.sv
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

module bus_interface(
            // bus interface signals
           input  wire logic         bus_cs_n_i,             // register select strobe
           input  wire logic         bus_rd_nwr_i,           // 0 = write, 1 = read
           input  wire logic  [3:0]  bus_reg_num_i,          // register number
           input  wire logic         bus_bytesel_i,          // 0=even byte, 1=odd byte
           input  wire logic  [7:0]  bus_data_i,             // 8-bit data bus input (broken out from bi-dir data bus)
           // blitter interface signals
           output      logic         write_strobe_o,         // strobe for register write
           output      logic         read_strobe_o,          // strobe for register read
           output      logic  [3:0]  reg_num_o,              // register number read/written
           output      logic         bytesel_o,              // byte selected of register read/written
           output      logic  [7:0]  bytedata_o,             // byte written to register
           // standard signals
           input  wire logic         clk,                    // input clk (should be > 2x faster than bus signals)
           input  wire logic         reset_i                 // reset
       );

// input synchronizers (shifts right each cycle with high bit set from inputs
// and bit 0 is acted on)
logic [4:0] cs_r;               // also "history bits" for detecting rising edge two cycles delayed (to allow bus to settle)
logic [1:0] read_r;
logic [1:0] bytesel_r;
logic [3:0] reg_num_r [1:0];
logic [7:0] data_r [2:0];

// aliases for synchronized inputs (low bit of synchronizers)
logic       cs_edge;
assign      cs_edge    = (cs_r[3:0] == 4'b1110);   // true on rising edge select with cycle delay (and ignore spurious edge)
logic       write;
assign      write       = ~read_r[0];
logic [3:0] reg_num;
assign      reg_num     = reg_num_r[0];
logic       bytesel;
assign      bytesel     = bytesel_r[0];
logic [7:0] data;
assign      data        = data_r[0];

initial begin
    cs_r            = 5'b0;               // CS has additional "history bits" to detect edge
    read_r          = 2'b0;
    bytesel_r       = 2'b0;
    reg_num_r[0]    = 4'h0;
    reg_num_r[1]    = 4'h0;
    data_r[0]       = 8'h00;
    data_r[1]       = 8'h00;
    data_r[2]       = 8'h00;
end

always_ff @(posedge clk) begin
    if (reset_i) begin
        cs_r            <= 5'h0;
        write_strobe_o  <= 1'b0;
        read_strobe_o   <= 1'b0;
        reg_num_o       <= 4'h0;
        bytesel_o       <= 1'b0;
        bytedata_o      <= 8'h00;
    end
    else begin
        // synchronize new input on leftmost bit, shifting bits right
        // NOTE: inverting bus_cs_n_i here to make it active high
        cs_r            <= { (bus_cs_n_i == xv::cs_ENABLED) ? 1'b1 : 1'b0, cs_r[4: 1] };
        read_r          <= { (bus_rd_nwr_i == xv::RnW_READ), read_r[1] };
        reg_num_r[0]    <= reg_num_r[1];
        reg_num_r[1]    <= bus_reg_num_i;
        bytesel_r       <= { bus_bytesel_i, bytesel_r[1] };
        data_r[0]       <= data_r[1];
        data_r[1]       <= data_r[2];
        data_r[2]       <= bus_data_i;

        // set outputs
        write_strobe_o  <= 1'b0;                // assume no write
        read_strobe_o   <= 1'b0;                // assume no read
        reg_num_o       <= reg_num;             // output selected register number
        bytesel_o       <= bytesel;             // output selected byte of register
        bytedata_o      <= data;                // output current data byte on bus

        if (cs_edge) begin                      // if CS edge
            if (write) begin
                write_strobe_o  <= 1'b1;        // output write strobe
            end
            else begin
                read_strobe_o  <= 1'b1;         // output read strobe
            end
        end
    end
end

endmodule
`default_nettype wire               // restore default
