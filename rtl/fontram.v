// fontram.v
//
// vim: set noet ts=4 sw=4
//
// Copyright (c) 2020 Xark - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)
//

`default_nettype none		 	// mandatory for Verilog sanity

`include "xosera_defs.vh"		// Xosera global Verilog definitions

module fontram(
	input wire rd_clk,
	input wire [12:0] rd_address,
	output reg [7:0] data_out,
	input wire wr_clk,
	input wire wr_en,
	input wire [12:0] wr_address,
	input wire [7:0] data_in
);
	// infer 8x8KB font BRAM
	reg [7:0] bram[8191:0];
`ifndef SHOW		// yosys show command doesn't like "too long" init string
	initial
`ifndef FONT_MEM
		$readmemb("../fonts/font_8x16.mem", bram, 0, 4095);
`else
		$readmemb(`STRINGIFY(`FONT_MEM), bram, 0, 4095);
`endif
`endif

	always @(posedge wr_clk) begin
		if (wr_en)
			bram[wr_address] <= data_in;
	end

	always @(posedge rd_clk) begin
		data_out <= bram[rd_address];
	end
endmodule
