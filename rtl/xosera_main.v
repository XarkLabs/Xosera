// xosera_main.v
//
// vim: set noet ts=4 sw=4
//
// Copyright (c) 2020 Xark - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)
//
// This project would not be possible without learning from the following
// open projects (and many others, no doubt):
//
// YaGraphCon		- http://www.frank-buss.de/yagraphcon/
// yavga			- https://opencores.org/projects/yavga
// f32c				- https://github.com/f32c
// up5k_vga			- https://github.com/emeb/up5k_vga
// icestation-32	- https://github.com/dan-rodrigues/icestation-32
// ice40-playground	- https://github.com/smunaut/ice40-playground
//
// Also the following web sites:
// Hamsterworks		- https://web.archive.org/web/20190119005744/http://hamsterworks.co.nz/mediawiki/index.php/Main_Page
//					  (Archived, but not forgotten - Thanks Mike Fields)
// John's FPGA Page	- http://members.optushome.com.au/jekent/FPGA.htm
// FPGA4Fun 		- https://www.fpga4fun.com/
// Nandland			- https://www.nandland.com/
// Alchrity			- https://alchitry.com/
// Time to Explore	- https://timetoexplore.net/
//
// 1BitSquared Discord server has also been welcoming and helpful - https://1bitsquared.com/pages/chat
//
// Special thanks to everyone involved with the IceStorm/Yosys/NextPNR (etc.) open source FPGA projects.
// Consider supporting open source FPGA tool development: https://www.patreon.com/fpga_dave

`default_nettype none		 	// mandatory for Verilog sanity

`include "xosera_defs.vh"		// Xosera global Verilog definitions

module xosera_main(
	input wire			clk,					// pixel clock
	input wire			reset,					// reset signal
	output reg [3:0]	vga_r, vga_g, vga_b,	// red, green and blue
	output reg			vga_hs, vga_vs,			// horizontal and vertical sync					
	output reg			visible,				// visible (aka display enable)
	input	wire		spi_sck_i,				// SPI clock
	input	wire		spi_copi_i,				// SPI data from initiator
	output	reg			spi_cipo_o,				// SPI data to initiator
	input	wire		spi_cs_i				// SPI target select
`ifdef SPI_DEBUG
	,
	output	wire		dbg_receive,			// debug output for logic analyzer (SPI byte received finished)
	output	wire		dbg_transmit,			// debug output for logic analyzer (SPI byte transmit start)
	output	wire		dbg_sck_rise,			// debug output for logic analyzer (SPI clock rising edge)
	output	wire		dbg_sck_fall			// debug output for logic analyzer (SPI clock falling edge)
`endif
);
	wire spi_receive_strobe;
	wire [7:0] spi_receive_data;
	wire spi_transmit_strobe;
	reg [7:0] spi_transmit_data;
	reg [12:0] test_addr;

	// simple test to allow writing received SPI data to font RAM

	always @(posedge clk) begin
		if (reset) begin
			spi_transmit_data	<= 8'h81;
			test_addr			<= 0;
		end
		else begin
			if (spi_receive_strobe) begin
				spi_transmit_data <= spi_receive_data;	// echo previous byte received
				test_addr			<= test_addr + 1;
			end
		end
	end

	spi_target spi_target(
		.clk(clk),
		.reset_i(reset),
		.spi_sck_i(spi_sck_i),
		.spi_copi_i(spi_copi_i),
		.spi_cipo_o(spi_cipo_o),
		.spi_cs_i(spi_cs_i),
		.receive_strobe_o(spi_receive_strobe),
		.receive_byte_o(spi_receive_data),
		.transmit_strobe_o(spi_transmit_strobe),
		.transmit_byte_i(spi_transmit_data)
`ifdef SPI_DEBUG		
		,
		.dbg_sck_rise(dbg_sck_rise),
		.dbg_sck_fall(dbg_sck_fall)
`endif
	);

`ifdef SPI_DEBUG
	assign dbg_receive = spi_receive_strobe;	// high for cycle when SPI byte received at spi_receive_data
	assign dbg_transmit = spi_transmit_strobe;	// high for cycle when SPI byte transmit begins (mostly debug)
`endif

	wire blit_cycle;			// cycle is for blitter (vs video)
	wire blit_sel;				// blitter vram select
	wire blit_wr;				// blitter vram write
	wire [15:0] blit_addr;		// blitter vram addr
	wire [15:0] blit_data_out;	// blitter write data

	blitter blitter(
		.clk(clk),
		.reset_i(reset),
		.blit_cycle_i(blit_cycle),
		.video_ena_o(video_ena),
		.blit_sel_o(blit_sel),
		.blit_wr_o(blit_wr),
		.blit_addr_o(blit_addr),
		.blit_data_i(vram_data_out),
		.blit_data_o(blit_data_out)
	);

	wire video_ena;				// enable text/bitmap generation
	wire vgen_sel;				// video vram select
	wire [15:0] vgen_addr;		// video vram addr

	//  video generation
	video_gen video_gen(
		.clk(clk),
		.reset_i(reset),
		.enable_i(video_ena),
		.blit_cycle_o(blit_cycle),
		.fontram_sel_o(fontram_sel),
		.fontram_addr_o(fontram_addr),
		.fontram_data_i(fontram_data_out),
		.vram_sel_o(vgen_sel),
		.vram_addr_o(vgen_addr),
		.vram_data_i(vram_data_out),
		.red_o(vga_r),
		.green_o(vga_g),
		.blue_o(vga_b),
		.hsync_o(vga_hs),
		.vsync_o(vga_vs),
		.visible_o(visible)
	);

	//  16x64K (128KB) video memory
	wire		vram_sel		= blit_cycle ? blit_sel	 : vgen_sel;
	wire		vram_wr			= blit_cycle ? blit_wr	 : 1'b0;
	wire [15:0] vram_addr		= blit_cycle ? blit_addr : vgen_addr;	// 64KB 16-bit word address
	wire [15:0]	vram_data_out;

	vram vram(
		.clk(clk),
		.sel(vram_sel),
		.wr_en(vram_wr),
		.address_in(vram_addr),
		.data_in(blit_data_out),
		.data_out(vram_data_out)
	);

	//  8x8K font memory
	wire		fontram_sel;
	wire		fontram_wr;
	wire [12:0] fontram_addr;	// 8KBx8-bit address
	wire [7:0]	fontram_data_out;

	fontram fontram(
		.rd_clk(clk),
		.rd_address(fontram_addr),
		.data_out(fontram_data_out),
		.wr_clk(clk),
		.wr_en(spi_receive_strobe),
		.wr_address(test_addr),
		.data_in(spi_receive_data)
	);
endmodule
