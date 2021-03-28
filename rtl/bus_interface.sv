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
           input  logic         bus_cs_n_i,            // register select strobe
           input  logic         bus_rd_nwr_i,           // 0 = write, 1 = read
           input  logic  [3:0]  bus_reg_num_i,          // register number
           input  logic         bus_bytesel_i,          // 0=even byte, 1=odd byte
           input  logic  [7:0]  bus_data_i,             // 8-bit data bus input
           output logic  [7:0]  bus_data_o,             // 8-bit data bus output
           output logic         write_strobe_o,         // strobe for vram access
           output logic  [3:0]  reg_num_o,              // register number read/written
           output logic [15:0]  reg_data_o,             // word written to register
           input  logic [15:0]  reg_data_i,             // word to read from register
           input  logic         reset_i                 // reset
       );

// input synchronizers (shifts right each cycle with high bit set from inputs and bit 0 is acted on)
logic [2:0] sel_r;               // has additional "previous bit" for detecting edges 0 -> 1
logic [1:0] read_r;
logic [1:0] bytesel_r;
logic [3:0] reg_num_r [1:0];
logic [7:0] data_r [1:0];

logic       sel         = sel_r[0];
logic       read        = read_r[0];
logic [3:0] reg_num     = reg_num_r[0];
logic       bytesel     = bytesel_r[0];
logic [7:0] data        = data_r[0];

logic       sel_rise;
assign      sel_rise    = (sel_r[1:0] == 2'b10);    // true on rising edge cycle of bus_cs_n_i

logic [3:0] even_byte_reg   = 4'h0;     // register flag for buffered even address write data
logic [7:0] even_byte_data  = 8'h00;    // buffer for even address write data (output on odd)

integer i;
initial begin
    sel_r           = 3'b0;               // has additional "previous bit" for detecting edges 0 -> 1
    read_r          = 2'b0;
    bytesel_r       = 2'b0;
    reg_num_r[0]    = 4'h0;
    reg_num_r[1]    = 4'h0;
    data_r[0]       = 8'h00;
    data_r[1]       = 8'h00;
end

always_ff @(posedge clk) begin
    if (reset_i) begin
        write_strobe_o  <= 1'b0;
        reg_num_o       <= 4'h0;
        reg_data_o      <= 16'h0000;
        bus_data_o      <= 8'h00;
        sel_r           <= 3'h0;
    end
    else begin
        // synchronize new input on leftmost bit, shifting remaining bits right
        sel_r           <= { ~bus_cs_n_i, sel_r[2: 1] };
        read_r          <= { bus_rd_nwr_i, read_r[1] };
        reg_num_r[0]    <= reg_num_r[1];
        reg_num_r[1]    <= bus_reg_num_i;
        bytesel_r       <= { bus_bytesel_i, bytesel_r[1] };
        data_r[0]       <= data_r[1];
        data_r[1]       <= bus_data_i;

        // set outputs
        write_strobe_o  <= 1'b0;                                        // assume no write
        reg_num_o       <= reg_num_r[0];                                // output register number
        if (reg_num_r[0] == even_byte_reg) begin
            reg_data_o      <= { even_byte_data, data_r[0] };           // output full word
        end
        else begin
            reg_data_o      <= { 8'h00, data_r[0] };                   //  output odd byte
        end

        if (sel_rise) begin                                             // if this is a rising edge of select
            if (bytesel_r[0]) begin                                     // if bytesel (2nd half of word)
                write_strobe_o  <= 1'b1;                                // strobe logic write
            end
            else begin
                even_byte_reg   <= reg_num_r[0];                        // save reg num
                even_byte_data  <= data_r[0][7:0];                      // save byte
            end
        end
    end
end
endmodule
