// xosera_iceb.v - Top module for iCEBreaker Xosera
//
// vim: set noet ts=4 sw=4
//
// Copyright (c) 2020 Xark - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)
//
// For info about iCEBreaker: https://1bitsquared.com/products/icebreaker
//

`default_nettype none		 // mandatory for Verilog sanity

module xosera_iceb(
	input wire	CLK,							// 12Mhz clock
	input wire	BTN_N,						// reset button (active LOW)
	input wire	LEDR_N,						// RED LED, but as input FTDI SPI SS (aka CTS)
	output wire LEDG_N,						// GREEN LED
	input wire	RX,							// UART receive
	output wire	TX,							// UART transmit
	input wire	FLASH_SCK,					// SPI SCK
	input wire	FLASH_IO0,					// SPI COPI
	output wire	FLASH_IO1,					// SPI CIPO
	output wire	FLASH_SSB,					// SPI flash CS (drive high unless using SPI flash)
	output wire	P1A1, P1A2, P1A3, P1A4, P1A7, P1A8, P1A9, P1A10,	// PMOD 1A
  	output wire	P1B1, P1B2, P1B3, P1B4, P1B7, P1B8, P1B9, P1B10		// PMOD 1B
);

	`include "../rtl/xosera_clk_defs.vh"	// Xosera global Verilog definitions

	wire nreset = BTN_N;					// use iCEBreaker "UBUTTON" as reset

	wire pclk;								// clock output from PLL block
	wire pll_lock;							// indicates when PLL frequency has locked-on

`ifndef SIMULATE
	SB_PLL40_PAD
	#(
		.DIVR(PLL_DIVR),		// DIVR from video mode
		.DIVF(PLL_DIVF),		// DIVF from video mode
		.DIVQ(PLL_DIVQ),		// DIVQ from video mode
		.FEEDBACK_PATH("SIMPLE"),
		.FILTER_RANGE(3'b001),
		.PLLOUT_SELECT("GENCLK"),
	)
	pll_inst (
		.LOCK(pll_lock),		// signal indicates PLL lock
		.RESETB(1'b1),
		.BYPASS(1'b0),
		.PACKAGEPIN(CLK),		// input reference clock
		.PLLOUTGLOBAL(pclk)		// PLL output clock (via global buffer)
	);
`else	// for simulation just use 1:1 clock
	assign pll_lock = 1'b1;
	assign pclk = CLK;
`endif

	assign FLASH_SSB = 1'b1;	// prevent SPI flash interfering with other SPI/FTDI pins

	wire spi_sck = FLASH_SCK;
	wire spi_copi = FLASH_IO0;
	wire spi_cs = LEDR_N;		// iCEBreaker uses red LED pin as input for FTDI SPI SS (CTR)
	wire spi_cipo;
	assign FLASH_IO1 = spi_cipo;

	// reset generator waits for PLL lock & reset button released for > ~0.8ms
	reg [7:0] reset_cnt;
	reg reset = 1'b1;
	always @(posedge pclk)
	begin
		if (!pll_lock || !nreset) begin
			reset_cnt <= 0;
			reset <= 1'b1;
		end
		else
		begin
			if (!&reset_cnt) begin
				reset_cnt <= reset_cnt + 1;
				reset <= 1'b1;
			end
			else begin
				reset <= 1'b0;
			end
		end
	end

//	assign LEDR_N = ~reset;		// red LED used used as FTDI SPI CS input
	assign LEDG_N = reset;		// green LED when not in reset

	// xosera main module
	wire [3:0] r;
	wire [3:0] g;
	wire [3:0] b;
	wire vga_hs;
	wire vga_vs;
	wire vga_de;

`ifdef PMOD_1B2_DVI12
// 12-bit DVI using dual-PMOD https://1bitsquared.com/products/pmod-digital-video-interface
	assign 	{P1A1,   P1A2,   P1A3,   P1A4,   P1A7,   P1A8,   P1A9,   P1A10} = 
			{r[3],   r[1],   g[3],   g[1],   r[2],   r[0],   g[2],   g[0]};
	assign 	{P1B1,   P1B2,   P1B3,   P1B4,   P1B7,   P1B8,   P1B9,   P1B10} = 
			{b[3],   pclk, 	 b[0],   vga_hs, b[2],   b[1],   vga_de, vga_vs};
`elsif  PMOD_DIGILENT_VGA
// 12-bit VGA using dual-PMOD https://store.digilentinc.com/pmod-vga-video-graphics-array/
	assign	{P1A1,   P1A2,   P1A3,   P1A4,   P1A7,   P1A8,   P1A9,   P1A10} = 
			{r[0],   r[1],   r[2],   r[3],   b[0],   b[1],   b[2],   b[3]};
	assign	{P1B1,   P1B2,   P1B3,   P1B4,   P1B7,   P1B8,   P1B9,   P1B10} = 
			{g[0],   g[1],   g[2],   g[3],   vga_hs, vga_vs, 1'bx,	 1'bx};
`elsif PMOD_XESS_VGA
// 9-bit VGA using dual-PMOD http://www.xess.com/shop/product/stickit-vga/
	assign	{P1A1,   P1A2,   P1A3,   P1A4,   P1A7,   P1A8,   P1A9,   P1A10} = 
			{vga_vs,  g[3],   r[2],   b[2],   vga_hs, r[3],   b[3],   g[2]};
	assign	{P1B1,    P1B2,   P1B3,   P1B4,   P1B7,   P1B8,   P1B9,   P1B10} = 
			{1'b0,    1'b0,   1'b0,   1'b0,   r[1],   g[1],   b[1],	  1'b0};
`elsif PMOD_XESS_VGA_SINGLE
// 6-bit VGA using single-PMOD http://www.xess.com/shop/product/stickit-vga/ (only PMOD-1B)
	assign	{P1B1,   P1B2,   P1B3,   P1B4,   P1B7,   P1B8,   P1B9,   P1B10} =  
			{vga_vs, g[3],   r[2],   b[2],   vga_hs, r[3],   b[3],   g[2]};
`else
	$error("No video output set, see Makefile");
`endif

	xosera_main xosera_main(
		.clk(pclk),
		.reset(reset),
		.vga_r(r),
		.vga_g(g),
		.vga_b(b),
		.vga_vs(vga_vs),
		.vga_hs(vga_hs),
		.visible(vga_de),
		.spi_sck_i(spi_sck),	
		.spi_copi_i(spi_copi),	
		.spi_cipo_o(spi_cipo),	
		.spi_cs_i(spi_cs)	
	);

	assign TX = RX;
	
endmodule
