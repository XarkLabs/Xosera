// xosera_tb.v
//
// vim: set noet ts=4 sw=4
//
// Copyright (c) 2020 Xark - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)
//
`default_nettype none		 	// mandatory for Verilog sanity

`timescale 1ns/1ps

module xosera_tb();

//`include "xosera_defs.vh"		// Xosera global Verilog definitions

	reg		clk;
	reg		reset;
	reg		spi_sck_i;
	reg		spi_copi_i;
	reg		spi_cs_i;
	reg		vga_vs;

	integer	 frame;
	reg		 last_vs;


	xosera_main xosera(
	.clk(clk),					// pixel clock
	.reset(reset),					// reset signal
	.vga_vs(vga_vs),
	.spi_sck_i(spi_sck_i),				// SPI clock
	.spi_copi_i(spi_copi_i),				// SPI data from initiator
	.spi_cs_i(spi_cs_i)				// SPI target select
	);

	`include "xosera_defs.vh"		// Xosera global Verilog definitions

	initial begin
		$dumpfile("xosera_tb.vcd");
		$dumpvars(0, xosera);

		frame	  = 0;
		clk		 = 1'b0;
		reset	   = 1'b1;
		spi_sck_i   = 1'b0;
		spi_copi_i  = 1'b0;
		spi_cs_i	= 1'b1;

		#((1000000000.0 / PIXEL_FREQ) * 10)
		@(posedge clk) begin
			reset = 1'b0;
		end

//		#(840100 * 3)
//			$finish;

	end

	 // toggle clock source at pixel clock frequency
	always begin
		#(1000000000.0 / PIXEL_FREQ) clk = ~clk;
	end

	always @(posedge clk) begin
		if (last_vs != vga_vs && vga_vs != V_SYNC_POLARITY) begin
			frame = frame + 1;
			if (frame > 3) begin
				$finish;
			end
			$display("Finished rendering frame %d %d", frame, V_SYNC_POLARITY, H_SYNC_POLARITY);
		end
		last_vs = vga_vs;
	end
endmodule
