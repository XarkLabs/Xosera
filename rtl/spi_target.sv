// spi_target.sv
//
// vim: set et ts=4 sw=4
//
// Copyright (c) 2020 Xark - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)
//

`default_nettype none               // mandatory for Verilog sanity
`timescale 1ns/1ps                  // mandatory to shut up Icarus Verilog

`ifdef ICEBREAKER

`include "xosera_pkg.sv"

module spi_target(
    input  wire logic        spi_sck_i,          // SPI clock
    input  wire logic        spi_copi_i,         // SPI data from initiator
    output      logic        spi_cipo_o,         // SPI data to initiator
    input  wire logic        spi_cs_i,           // SPI target select

    output      logic        select_o,
    output      logic        receive_strobe_o,   // true for cycle when data available
    output      logic [7: 0] receive_byte_o,     // data read from initiator
    output      logic        transmit_strobe_o,  // data to send to initiator (on next initiator write)
    input  wire logic [7: 0] transmit_byte_i,    // data to send to initiator (on next initiator write)

    input  wire logic        reset_i,            // reset
    input  wire logic        clk                 // input clk (should be ~4x faster than SPI clock)
);

// input synchronizers (shifts left each cycle with bit 0 is set from inputs and bit 1 is acted on)
logic [1: 0] cs_r;
logic [1: 0] copi_r;
logic [2: 0] sck_r;        // an extra "previous bit" for detecting edges

logic sck_rise;
assign sck_rise = (sck_r[1: 0] == 2'b10);
logic sck_fall;
assign sck_fall = (sck_r[1: 0] == 2'b01);
logic copi;
assign copi = copi_r[0];
logic cs;
assign cs = cs_r[0];

assign select_o = cs;

logic [7: 0] data_byte;
logic [2: 0] bit_count;

logic sck_state;

assign spi_cipo_o = data_byte[7];

always @(posedge clk) begin
    if (reset_i) begin
        receive_strobe_o    <= 1'b0;
        transmit_strobe_o   <= 1'b0;
        receive_byte_o      <= 8'h00;
        data_byte           <= 8'h00;
        bit_count           <= 3'b000;
        cs_r                <= 2'b00;
        copi_r              <= 2'b00;
        sck_r               <= 3'b000;
        sck_state           <= 1'b0;
    end
    else begin
        // synchronize inputs (shift right)
        sck_r   <= { spi_sck_i, sck_r[2:1] };
        copi_r  <= { spi_copi_i, copi_r[1] };
        cs_r    <= { ~spi_cs_i, cs_r[1] };  // invert active low input

        receive_strobe_o    <= 1'b0;
        transmit_strobe_o   <= 1'b0;

        if (!cs) begin           // SS not selected
            transmit_strobe_o   <= 1'b1;
            data_byte           <= transmit_byte_i;
            bit_count           <= 3'b000;
            sck_state           <= 1'b0;
        end
        else begin              // SS selected
            if (~sck_state && sck_rise) begin
                sck_state   <= 1'b1;
                data_byte   <= { data_byte[6: 0], copi };
                if (bit_count == 3'b111) begin
                    receive_strobe_o    <= 1'b1;
                    receive_byte_o      <= { data_byte[6: 0], copi };
                end
            end
            else begin
                if (sck_state && sck_fall) begin
                    sck_state   <= 1'b0;
                    bit_count   <= bit_count + 1'b1;
                    if (bit_count == 3'b111) begin
                        transmit_strobe_o   <= 1'b1;
                        data_byte           <= transmit_byte_i;
                    end
                end
            end
        end
    end
end
endmodule

`endif
`default_nettype wire               // restore default

