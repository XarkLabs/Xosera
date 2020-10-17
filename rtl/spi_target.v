// spi_target.v
//
// vim: set noet ts=4 sw=4
//
// Copyright (c) 2020 Xark - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)
//

`default_nettype none		 	// mandatory for Verilog sanity


module spi_target(
	input	wire		clk,				// input clk (should be ~4x faster than SPI clock)
	input	wire		reset_i,			// reset

	input	wire		spi_sck_i,			// SPI clock
	input	wire		spi_copi_i,			// SPI data from initiator
	output	reg			spi_cipo_o,			// SPI data to initiator
	input	wire		spi_cs_i,			// SPI target select

	output	reg			receive_strobe_o,	// true for cycle when data available
	output	reg [7:0]	receive_byte_o,		// data read from initiator
	output	reg			transmit_strobe_o,	// data to send to initiator (on next initiator write)
	input	wire [7:0]	transmit_byte_i		// data to send to initiator (on next initiator write)
`ifdef SPI_DEBUG
	,
	output	reg			dbg_sck_rise,
	output	reg			dbg_sck_fall
`endif
);

	// input synchronizers (shifts left each cycle with bit 0 is set from inputs and bit 1 is acted on)
	reg [1:0]	cs_r;
	reg [1:0]	copi_r;
	reg [2:0]	sck_r;		// an extra "previous bit" for detecting edges

	wire sck_rise	= (sck_r[2:1] == 2'b01);
	wire sck_fall	= (sck_r[2:1] == 2'b10);

`ifdef SPI_DEBUG
	assign dbg_sck_rise = sck_rise;
	assign dbg_sck_fall = sck_fall;
`endif

	reg [7:0] data_byte;
	reg [2:0] bit_count;

	reg sck_state;

	assign spi_cipo_o = data_byte[7];

	always @(posedge clk) begin
		if (reset_i) begin
			receive_byte_o	<= 8'h00;
			data_byte		<= 8'h00;
			bit_count		<= 3'b000;
			cs_r			<= 2'b00;
			copi_r			<= 2'b00;
			sck_r			<= 3'b000;
			sck_state		<= 1'b0;
		end
		else begin
			// synchronize inputs
			sck_r	<= { sck_r[1:0], spi_sck_i };
			copi_r	<= { copi_r[0], spi_copi_i };
			cs_r	<= { cs_r[0], spi_cs_i };

			receive_strobe_o	<= 1'b0;
			transmit_strobe_o	<= 1'b0;

			if (cs_r[1]) begin		// SS not selected
				transmit_strobe_o	<= 1'b1;
				data_byte			<= transmit_byte_i;
				bit_count			<= 3'b000;
				sck_state			<= 1'b0;
			end
			else begin					// SS selected
				if (~sck_state && sck_rise) begin
					sck_state	<= 1'b1;
					data_byte	<= { data_byte[6:0], copi_r[1] };
					if (bit_count == 3'b111) begin
						receive_strobe_o	<= 1'b1;
						receive_byte_o		<= { data_byte[6:0], copi_r[1] };
					end
				end
				else begin
					if (sck_state && sck_fall) begin
						sck_state	<= 1'b0;
						bit_count <= bit_count + 1'b1;
						if (bit_count == 3'b111) begin
							transmit_strobe_o	<= 1'b1;
							data_byte			<= transmit_byte_i;
						end
					end
				end
			end
		end
	end
endmodule
