// xosera_upd.v - Top module for Upduino Xosera
//
// vim: set noet ts=4 sw=4
//
// Copyright (c) 2020 Xark - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)
//
// For info about Updino v3.0: https://github.com/tinyvision-ai-inc/UPduino-v3.0
// It should be here on Tindie soon: https://www.tindie.com/stores/tinyvision_ai/
//
// NOTE: Upduino 2.x should be the same as 3.x except for the clock input.
//		 However, it is known to suffer from significant problems when using the
//		 PLL (without board modifications, e.g., adding a capacitor to PLL VCC).  See:
//		 https://tinyvision.ai/blogs/processing-at-the-edge/ground-trampolines-and-phase-locked-loops
//       See -DEXTLK35 option below to enable external clock with a wire from J8/12Mhz to gpio_35
//
// NOTE: Upduino 3.x needs either the "OSC" jumper shorted (recommended, but dedicates gpio_10 as a clock)
//		 or add the -DCLKGPIO35 option below and a wire from 12Mhz pin to gpio_35

`default_nettype none		 // mandatory for Verilog sanity

`include "../rtl/xosera_defs.vh"		// Xosera global Verilog definitions

`ifdef SIMULATE					// no PLL when simulating
`define	NOPLL
`endif

// depending on the external clock input pin, slightly different
// PLL primitives versions are needed for "reasons" (internal FPGA architecture)
`ifdef CLKGPIO35
`define	CLK_PIN		gpio_35		// external clock (NOTE: wire clock to gpio_35, from J8/12Mhz or other clock)
`define	PLL_PRIM	SB_PLL40_PAD
`define PLL_CLKIN	PACKAGEPIN
`else
`define	CLK_PIN		gpio_20		// 12Mhz clock (NOTE: "OSC" jumper on Upduino V3.x must be shorted)
`define	PLL_PRIM	SB_PLL40_CORE
`define PLL_CLKIN	REFERENCECLK
`endif

module xosera_upd(
	input wire `CLK_PIN,						// input clock
	input wire gpio_28,						// reset button (active LOW) 
	output wire gpio_12,					// hsync
	output wire gpio_21,					// vsync
	output wire gpio_13,					// R[3]
	output wire gpio_19,					// G[3]
	output wire gpio_18,					// B[3]
	output wire gpio_11,					// R[2]
	output wire gpio_9,						// G[2]
	output wire gpio_6,						// B[2]
	output wire gpio_44,					// R[1]
	output wire gpio_4,						// G[1]
	output wire gpio_3,						// B[1]
	output wire gpio_48,					// R[0]
	output wire gpio_45,					// G[0]
	output wire gpio_47,					// B[0]
	input wire 	spi_sck,					// SPI sck (also serial_rxd)
	input wire	spi_copi,					// SPI copi
	output wire spi_cipo,					// SPI cipo (also serial_txd)
	output wire spi_cs,						// SPI flash CS (drive high unless using SPI flash)
	input wire  led_red,					// R input from FTDI CTS/SPI SS (TP11 <-330 Ohm-> R)
	output wire led_green, led_blue			// Upduino GB LED
`ifdef SPI_DEBUG
	,
	output wire gpio_35, gpio_31, gpio_37, gpio_34, gpio_43, gpio_36, gpio_42, gpio_38	// DEBUG
`endif
);

	wire nreset = gpio_28;					// use gpio_28 as active LOW reset button
	wire pclk;								// clock output from PLL block
	wire pll_lock;							// indicates when PLL frequency has locked-on

// NOTE: For "reasons" (not sure) different PLL primitives are needed for gpio_20 vs gpio_35

`ifndef NOPLL
	`PLL_PRIM
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
		.`PLL_CLKIN(`CLK_PIN),	// input reference clock
		.PLLOUTGLOBAL(pclk)		// PLL output clock (via global buffer)
	);
`else	// for simulation just use 1:1 clock
	assign pll_lock = 1'b1;
	assign pclk = `CLK_PIN;
`endif

	assign spi_cs = 1'b1;	// prevent SPI flash interfering with other SPI/FTDI pins
	wire spi_sel = 1'b0;//led_red;		// Upduino 3.x needs 330 ohm resistor placed between R and TP11 (closest to R)

`ifdef SPI_DEBUG
	// DEBUG
	assign gpio_43 = spi_cipo;
	assign gpio_36 = spi_copi;
	assign gpio_42 = spi_sck;
	assign gpio_38 = spi_sel;
`endif

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

//	assign led_red = ~reset;		// red LED used used as FTDI SPI CS input
	assign led_green = reset;		// green LED when not in reset
	assign led_blue = 1'b1;			// blue LED off

	// xosera main module
	wire [3:0] r;
	wire [3:0] g;
	wire [3:0] b;
	wire vga_hs;
	wire vga_vs;
	wire vga_de;

	assign	{ gpio_12, gpio_21, gpio_13, gpio_19, gpio_18, gpio_11, gpio_9, gpio_6 } =
			{ vga_hs,  vga_vs,  r[3],    g[3],    b[3],    r[2],    g[2],   b[2] };
	assign	{ gpio_44, gpio_4, gpio_3, gpio_48, gpio_45, gpio_47 } =
			{ r[1],    g[1],   b[1],   r[0],    g[0],    b[0] };

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
		.spi_cs_i(spi_sel)
`ifdef SPI_DEBUG
		,
		.dbg_receive(gpio_34),
		.dbg_transmit(gpio_37),
		.dbg_sck_rise(gpio_31),
		.dbg_sck_fall(gpio_35)
`endif
	);

endmodule
