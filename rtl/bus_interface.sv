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
    // register interface signals
    output      logic         write_strobe_o,         // strobe for register write
    output      logic         read_strobe_o,          // strobe for register read
    output      logic  [3:0]  reg_num_o,              // register number read/written
    output      logic         bytesel_o,              // byte selected of register read/written
    output      logic  [7:0]  bytedata_o,             // byte written to register
    // standard signals
    input  wire logic         clk,                    // input clk (should be > 2x faster than bus signals)
    input  wire logic         reset_i                 // reset
);

// input synchronizers
logic       cs_n_ff0;
logic       cs_n_ff1;       // NOTE: needs extra FF, or read is too early
logic       cs_n;
logic       cs_n_last;      // previous state to determine edge
logic       rd_nwr_ff0;
logic       rd_nwr;
logic       bytesel_ff0;
logic       bytesel;
logic [3:0] reg_num_ff0;
logic [3:0] reg_num;
byte_t      data_ff0;
byte_t      data;

// async signal synchronizers
always_ff @(posedge clk) begin
    if (reset_i) begin
        cs_n_ff0    <= 1'b0;
        cs_n_ff1    <= 1'b0;
        cs_n        <= 1'b0;
        cs_n_last   <= 1'b0;
        rd_nwr_ff0  <= 1'b0;
        rd_nwr      <= 1'b0;
        bytesel_ff0 <= 1'b0;
        bytesel     <= 1'b0;
        reg_num_ff0 <= 4'b0;
        reg_num     <= 4'b0;
        data_ff0    <= 8'b0;
        data        <= 8'b0;
    end else begin
        cs_n_ff0    <= bus_cs_n_i;
        cs_n_ff1    <= cs_n_ff0;
        cs_n        <= cs_n_ff1;
        cs_n_last   <= cs_n;

        rd_nwr_ff0  <= bus_rd_nwr_i;
        rd_nwr      <= rd_nwr_ff0;

        bytesel_ff0 <= bus_bytesel_i;
        bytesel     <= bytesel_ff0;

        reg_num_ff0 <= bus_reg_num_i;
        reg_num     <= reg_num_ff0;

        data_ff0    <= bus_data_i;
        data        <= data_ff0;
    end
end

always_ff @(posedge clk) begin
    if (reset_i) begin
        write_strobe_o  <= 1'b0;
        read_strobe_o   <= 1'b0;
        reg_num_o       <= 4'h0;
        bytesel_o       <= 1'b0;
        bytedata_o      <= 8'h00;
    end else begin
        // set outputs
        reg_num_o       <= reg_num;             // output selected register number
        bytesel_o       <= bytesel;             // output selected byte of register
        bytedata_o      <= data;                // output current data byte on bus

        write_strobe_o  <= 1'b0;                // clear write strobe
        read_strobe_o   <= 1'b0;                // clear read strobe

        // if CS edge
        if (cs_n_last == xv::CS_DISABLED && cs_n == xv::CS_ENABLED /*  && cs_n_ff1 == xv::CS_ENABLED */) begin
            if (rd_nwr == xv::RnW_WRITE) begin
                write_strobe_o  <= 1'b1;        // output write strobe
            end else begin
                read_strobe_o   <= 1'b1;        // output read strobe
            end
        end
    end
end

endmodule
`default_nettype wire               // restore default
