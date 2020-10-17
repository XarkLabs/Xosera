// blitter.v
//
// vim: set noet ts=4 sw=4
//
// Copyright (c) 2020 Xark - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)
//

`default_nettype none		 	// mandatory for Verilog sanity


module blitter(
	input wire 			clk,
	input wire			reset_i,
	input wire			blit_cycle_i,
	output reg			video_ena_o,
	output reg			blit_sel_o,
	output reg			blit_wr_o,
	output reg [15:0]	blit_addr_o,
	input wire [15:0]	blit_data_i,
	output reg [15:0]	blit_data_o
);

// this is mostly a test and proof-of-concept currently

`include "xosera_defs.vh"		// Xosera global Verilog definitions

localparam	INIT		=	0;
localparam	CLEAR		=	1;
localparam	TOP			=	2;
localparam	TOPLOOP		=	3;
localparam	BOTTOM		=	4;
localparam	BOTTOMLOOP	=	5;
localparam	BOTTOM1		=	6;
localparam	BOTTOMLOOP1	=	7;
localparam	LEFT		=	8;
localparam	LEFTLOOP	=	9;
localparam	RIGHT		=	10;
localparam	RIGHTLOOP	=	11;
localparam	FONT		=	12;
localparam	FONTLOOP	=	13;
localparam	FONTDONE	=	14;
localparam	CHAR_X		=	15;
localparam	CHAR_o		=	16;
localparam	CHAR_s		=	17;
localparam	CHAR_e		=	18;
localparam	CHAR_r		=	19;
localparam	CHAR_a		=	20;

localparam	DONE		=	21;

reg	[5:0] init_state;
reg [15:0] blit_count;
reg [15:0] blit_incr;
reg [7:0] font_char;

always @(posedge clk) begin
	if (reset_i) begin
		video_ena_o		<= 1'b0;
		blit_sel_o		<= 1'b0;
		blit_wr_o		<= 1'b0;
		font_char		<= 8'h00;
		blit_addr_o		<= 16'h0000;
		blit_data_o		<= 16'h0000;
		blit_count		<= 16'h0000;
		blit_incr		<= 16'h0000;
		init_state		<= INIT;
	end
	else begin
		blit_sel_o	<=	1'b0;
		blit_wr_o	<=	1'b0;
		if (blit_count != 0) begin
			blit_count	<= blit_count - 1;
			blit_addr_o	<= blit_addr_o + blit_incr;
		end
		if (blit_cycle_i) begin
			case (init_state)
				INIT:	begin
					video_ena_o		<= 1'b0;
					blit_sel_o 		<= 1'b1;
					blit_wr_o 		<= 1'b1;
					blit_incr		<= 16'hffff;
					blit_count		<= 16'hffff;
					blit_data_o		<= 16'h1F20;
					init_state		<= CLEAR;
				end
				CLEAR:	begin
					blit_sel_o 		<= 1'b1;
					blit_wr_o 		<= 1'b1;
					blit_incr		<= 16'hffff;
					blit_data_o		<= 16'h1F20;
					init_state		<= (blit_count == 0) ? TOP : CLEAR;
				end
				TOP: begin
					blit_addr_o		<= (0 * CHARS_WIDE + 0)-1;
					blit_count		<= CHARS_WIDE;
					blit_incr		<= 16'h0001;
					init_state		<= TOPLOOP;
				end
				TOPLOOP: begin
					blit_sel_o 			<= 1'b1;
					blit_wr_o 			<= 1'b1;
					blit_data_o			<= { blit_count[3:0] ^ 4'b1111, blit_count[3:0], "*"};
					init_state			<= (blit_count == 0) ? BOTTOM : TOPLOOP;
				end
				BOTTOM: begin
					blit_addr_o		<= ((CHARS_HIGH-1) * CHARS_WIDE - 1)-1;
					blit_count		<= CHARS_WIDE;
					blit_incr		<= 16'h0001;
					init_state		<= BOTTOMLOOP;
				end
				BOTTOMLOOP: begin
					blit_sel_o 			<= 1'b1;
					blit_wr_o 			<= 1'b1;
					blit_data_o			<= { blit_count[3:0] ^ 4'b1111, blit_count[3:0], "*"};
					init_state			<= (blit_count == 0) ? BOTTOM1 : BOTTOMLOOP;
				end
				BOTTOM1: begin
					blit_addr_o		<= ((CHARS_HIGH) * CHARS_WIDE)-1;
					blit_count		<= CHARS_WIDE;
					blit_incr		<= 16'h0001;
					init_state		<= BOTTOMLOOP1;
				end
				BOTTOMLOOP1: begin
					blit_sel_o 			<= 1'b1;
					blit_wr_o 			<= 1'b1;
					blit_data_o			<= 16'h0000;
					init_state			<= (blit_count == 0) ? LEFT : BOTTOMLOOP1;
				end
				LEFT: begin
					blit_addr_o		<= (0 * CHARS_WIDE + 0)-CHARS_WIDE;
					blit_count		<= CHARS_HIGH;
					blit_incr		<= CHARS_WIDE;
					init_state		<= LEFTLOOP;
				end
				LEFTLOOP: begin
					blit_sel_o 			<= 1'b1;
					blit_wr_o 			<= 1'b1;
					blit_data_o			<= { blit_count[3:0] ^ 4'b1111, blit_count[3:0], "*" };
					init_state			<= (blit_count == 0) ? RIGHT : LEFTLOOP;
				end
				RIGHT: begin
					blit_addr_o		<= (0 * CHARS_WIDE + CHARS_WIDE-1)-CHARS_WIDE;
					blit_count		<= CHARS_HIGH;
					blit_incr		<= CHARS_WIDE;
					init_state		<= RIGHTLOOP;
				end
				RIGHTLOOP: begin
					blit_sel_o 			<= 1'b1;
					blit_wr_o 			<= 1'b1;
					blit_data_o			<= { blit_count[3:0] ^ 4'b1111, blit_count[3:0], "*" };
					init_state			<= (blit_count == 0) ? FONT : RIGHTLOOP;
				end
				FONT: begin
					blit_addr_o		<= (CHARS_WIDE*3) - 10;
					blit_count		<= 16'h0100;
					blit_incr		<= 2;
					blit_data_o		<= 16'h0FFF;
					init_state		<= FONTLOOP;
				end
				FONTLOOP: begin
					blit_sel_o 			<= 1'b1;
					blit_wr_o 			<= 1'b1;
					blit_data_o			<= { 8'h1f, font_char };
					font_char			<= font_char + 1;
					if (font_char[4:0] == 0) begin
						blit_addr_o		<= blit_addr_o + (CHARS_WIDE*2 - 64 + 2);
					end
					init_state			<= (blit_count == 0) ? FONTDONE : FONTLOOP;
				end
				FONTDONE: begin
					blit_sel_o 			<= 1'b0;
					blit_wr_o 			<= 1'b0;
					init_state			<= CHAR_X;
				end
				CHAR_X: begin
					blit_sel_o 			<= 1'b1;
					blit_wr_o 			<= 1'b1;
					blit_addr_o	 		<= (2 * CHARS_WIDE + 2 + 0);
					blit_data_o			<= { 8'h1F, "X" };
					init_state			<= CHAR_o;
				end
				CHAR_o: begin
					blit_sel_o 			<= 1'b1;
					blit_wr_o 			<= 1'b1;
					blit_addr_o	 		<= (2 * CHARS_WIDE + 2 + 1);
					blit_data_o			<= { 8'h1e, "o" };
					init_state			<= CHAR_s;
				end
				CHAR_s: begin
					blit_sel_o 			<= 1'b1;
					blit_wr_o 			<= 1'b1;
					blit_addr_o	 		<= (2 * CHARS_WIDE + 2 + 2);
					blit_data_o			<= { 8'h14, "s" };
					init_state			<= CHAR_e;
				end
				CHAR_e: begin
					blit_sel_o 			<= 1'b1;
					blit_wr_o 			<= 1'b1;
					blit_addr_o	 		<= (2 * CHARS_WIDE + 2 + 3);
					blit_data_o			<= { 8'h12, "e"};
					init_state			<= CHAR_r;
				end
				CHAR_r: begin
					blit_sel_o 			<= 1'b1;
					blit_wr_o 			<= 1'b1;
					blit_addr_o	 		<= (2 * CHARS_WIDE + 2 + 4);
					blit_data_o			<= { 8'h18, "r" };
					init_state			<= CHAR_a;
				end
				CHAR_a: begin
					blit_sel_o 			<= 1'b1;
					blit_wr_o 			<= 1'b1;
					blit_addr_o	 		<= (2 * CHARS_WIDE + 2 + 5);
					blit_data_o			<= { 8'h15, "a" };
					init_state			<= DONE;
				end
				default: begin
					video_ena_o		<= 1'b1;
					blit_data_o		<= 16'hd3d3;	// DEBUG
					blit_addr_o		<= 16'ha3a3;	// DEBUG
					init_state		<= DONE;
				end
			endcase
		end
	end
end

endmodule