// bus_interface.sv
//
// vim: set et ts=4 sw=4
//
// Copyright (c) 2020 Xark - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)
//

`default_nettype none             // mandatory for Verilog sanity
`timescale 1ns/1ns

module bus_interface(
           input  logic         clk,                    // input clk (should be > 2x faster than bus signals)
           input  logic         bus_cs_n_i,             // register select strobe
           input  logic         bus_rd_nwr_i,           // 0 = write, 1 = read
           input  logic  [3:0]  bus_reg_num_i,          // register number
           input  logic         bus_bytesel_i,          // 0=even byte, 1=odd byte
           input  logic  [7:0]  bus_data_i,             // 8-bit data bus input
           output logic  [7:0]  bus_data_o,             // 8-bit data bus output
           output logic         write_strobe_o,         // strobe for register write
           output logic         read_strobe_o,          // strobe for register read
           output logic  [3:0]  reg_num_o,              // register number read/written
           output logic [15:0]  reg_data_o,             // word written to register
           input  logic [15:0]  reg_data_i,             // word to read from register
           input  logic         reset_i                 // reset
       );

`include "xosera_defs.svh"        // Xosera global Verilog definitions

// input synchronizers (shifts right each cycle with high bit set from inputs and bit 0 is acted on)
logic [2:0] sel_r;               // has additional "previous bit" for detecting edges 0 -> 1
logic [1:0] read_r;
logic [1:0] bytesel_r;
logic [3:0] reg_num_r [1:0];
logic [7:0] data_r [1:0];

// aliases for synchronized inputs (low bit of synchronizers)
logic       sel_rise;
assign      sel_rise    = (sel_r[1:0] == 2'b10);    // true on rising edge cycle of select
logic       write;
assign      write       = ~read_r[0];
logic [3:0] reg_num;
assign      reg_num     = reg_num_r[0];
logic       bytesel;
assign      bytesel     = bytesel_r[0];
logic [7:0] data;
assign      data        = data_r[0];

logic [3:0] even_byte_reg   = 4'h0;     // register flag for buffered even address write data
logic [7:0] even_byte_data  = 8'h00;    // buffer for even address write data (output on odd)

initial begin
    sel_r           = 3'b0;               // has additional "previous bit" for detecting edges 0 -> 1
    read_r          = 2'b0;
    bytesel_r       = 2'b0;
    reg_num_r[0]    = 4'h0;
    reg_num_r[1]    = 4'h0;
    data_r[0]       = 8'h00;
    data_r[1]       = 8'h00;
end


// select read data based on upper bits of reg select and byte select
assign bus_data_o = reg_read(bytesel, reg_num[3:2]);

// function to continuously select read value to put on bus
function [7:0] reg_read(
    input logic         b_sel,
    input logic [1:0]   r_sel
    );
case (r_sel)
    2'b00:  reg_read = (~b_sel) ? reg_data_i[15:8] : reg_data_i[7:0];   // VRAM read data reg 0-3
    2'b01:  reg_read = (~b_sel) ? 8'hA1 : 8'hB1;                        // test read data reg 4-7
    2'b10:  reg_read = (~b_sel) ? 8'hA2 : 8'hB2;                        // test read data reg 8-B
    2'b11:  reg_read = (~b_sel) ? 8'hA3 : 8'hB3;                        // test read data reg C-F
    default: ;
endcase
endfunction

always_ff @(posedge clk) begin
    if (reset_i) begin
        sel_r           <= 3'h0;
        write_strobe_o  <= 1'b0;
        read_strobe_o   <= 1'b0;
        reg_num_o       <= 4'h0;
    end
    else begin
        // synchronize new input on leftmost bit, shifting bits right
        // NOTE: inverting bus_cs_n_i here to make it active high
        sel_r           <= { (bus_cs_n_i == cs_ENABLED) ? 1'b1 : 1'b0, sel_r[2: 1] };
        read_r          <= { (bus_rd_nwr_i == RnW_READ), read_r[1] };
        reg_num_r[0]    <= reg_num_r[1];
        reg_num_r[1]    <= bus_reg_num_i;
        bytesel_r       <= { bus_bytesel_i, bytesel_r[1] };
        data_r[0]       <= data_r[1];
        data_r[1]       <= bus_data_i;

        // set outputs
        write_strobe_o  <= 1'b0;                                // assume no write
        read_strobe_o   <= 1'b0;                                // broadcast register read stobe

        reg_num_o       <= reg_num;                             // broadcast register number

        if (sel_rise) begin                                     // if select rising edge
            if (write) begin
                if (bytesel) begin                                  // if bytesel (2nd half of word)
                    write_strobe_o  <= 1'b1;                        // broadcast register write strobe
                end
                else begin
                    even_byte_reg   <= reg_num;                     // save reg num for even byte
                    even_byte_data  <= data;                        // save even byte
                end
            end
            else begin
                read_strobe_o  <= 1'b1;                        // broadcast register read stobe
            end
        end
    end
end

always_comb begin
    if (reg_num == even_byte_reg) begin                     // if same register as even byte
        reg_data_o      = { even_byte_data, data };         // broadcast full word
    end
    else begin
        reg_data_o      = { 8'h00, data };                  // broadcast odd byte with zero upper
    end
end
endmodule
