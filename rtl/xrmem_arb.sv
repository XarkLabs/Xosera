// xrmem_arb.sv
//
// vim: set et ts=4 sw=4
//
// Copyright (c) 2021 Xark - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)
//

`default_nettype none               // mandatory for Verilog sanity
`timescale 1ns/1ps                  // mandatory to shut up Icarus Verilog

`include "xosera_pkg.sv"

module xrmem_arb
(
    // regs XR register/memory interface (read/write)
    input  wire logic                           xr_sel_i,
    output      logic                           xr_ack_o,
    input  wire logic                           xr_wr_i,
    input  wire logic [15:0]                    xr_addr_i,
    input  wire logic [15:0]                    xr_data_i,
    output      logic [15:0]                    xr_data_o,

    // copper XR register/memory interface (write-only)
    input  wire logic                           copp_xr_sel_i,
    output      logic                           copp_xr_ack_o,
    input  wire logic [15:0]                    copp_xr_addr_i,
    input  wire logic [15:0]                    copp_xr_data_i,

    // XR register bus (read/write)
    output      logic                           xreg_wr_o,
    output      logic [15:0]                    xreg_addr_o,
    input  wire logic [15:0]                    xreg_data_i,
    output      logic [15:0]                    xreg_data_o,

    // color lookup colormem A+B 2 x 16-bit bus (read-only)
    input  wire logic                           vgen_color_sel_i,
    input  wire logic [xv::COLOR_AWIDTH-1:0]    vgen_colorA_addr_i,
    input  wire logic [xv::COLOR_AWIDTH-1:0]    vgen_colorB_addr_i,
    output      logic [15:0]                    vgen_colorA_data_o,
    output      logic [15:0]                    vgen_colorB_data_o,

    // video generation tilemem bus (read-only)
    input  wire logic                           vgen_tile_sel_i,
    input  wire logic [xv::TILE_AWIDTH-1:0]     vgen_tile_addr_i,
    output      logic [15:0]                    vgen_tile_data_o,

    // copper program coppermem 32-bit bus (read-only)
    input  wire logic                           copp_prog_sel_i,
    input  wire logic [xv::COPPER_AWIDTH-1:0]   copp_prog_addr_i,
    output      logic [31:0]                    copp_prog_data_o,

    input  wire logic                           clk
);

// internal COLORMEM signals
logic                           color_rd_en     /* verilator public */;
logic                           color_wr_en     /* verilator public */;
logic [xv::COLOR_AWIDTH-1:0]    colorA_addr     /* verilator public */;
logic [xv::COLOR_AWIDTH-1:0]    colorB_addr     /* verilator public */;
logic [15:0]                    colorA_data_out /* verilator public */;
logic [15:0]                    colorB_data_out /* verilator public */;
logic                           color_B         /* verilator public */;

// internal TILEMEM signals
logic                           tile_rd_en      /* verilator public */;
logic                           tile_wr_en      /* verilator public */;
logic [xv::TILE_AWIDTH-1:0]     tile_addr       /* verilator public */;
logic [15:0]                    tile_data_out   /* verilator public */;

// internal COPPERMEM signals
logic                           copp_rd_en      /* verilator public */;
logic                           copp_wr_en      /* verilator public */;
logic [xv::COPPER_AWIDTH-1:0]   copp_addr       /* verilator public */;
logic [31:0]                    copp_data_out   /* verilator public */;
logic                           copp_odd;

// internal XR write signals
logic                           xr_wr_en        /* verilator public */;
logic [15:0]                    xr_addr         /* verilator public */;
logic [15:0]                    xr_data         /* verilator public */;

// combinatorial ack signals
logic           xr_ack_next;
logic           copp_xr_ack_next;
logic           color_ack_next;
logic           tile_ack_next;
logic           copp_ack_next;

// combinatorial decode signals
logic           xr_regs_sel         /* verilator public */;
logic           xr_color_sel        /* verilator public */;
logic           xr_tile_sel         /* verilator public */;
logic           xr_copp_sel         /* verilator public */;

logic           copp_xr_regs_sel    /* verilator public */;
logic           copp_xr_color_sel   /* verilator public */;
logic           copp_xr_tile_sel    /* verilator public */;
logic           copp_xr_copp_sel    /* verilator public */;

// assign read outputs
assign  vgen_colorA_data_o  = colorA_data_out;
assign  vgen_colorB_data_o  = colorB_data_out;
assign  vgen_tile_data_o    = tile_data_out;
assign  copp_prog_data_o    = copp_data_out;

// decode flags for XR interface
assign  xr_regs_sel     = (xr_addr_i[15] == 1'b0);
assign  xr_color_sel    = (xr_addr_i[15:13] == xv::XR_COLOR_MEM[15:13]);
assign  xr_tile_sel     = (xr_addr_i[15:13] == xv::XR_TILE_MEM[15:13]);
assign  xr_copp_sel     = (xr_addr_i[15:13] == xv::XR_COPPER_MEM[15:13]);

// decode flags for copper XR interface
assign  copp_xr_regs_sel    = (copp_xr_addr_i[15] == 1'b0);
assign  copp_xr_color_sel   = (copp_xr_addr_i[15:13] == xv::XR_COLOR_MEM[15:13]);
assign  copp_xr_tile_sel    = (copp_xr_addr_i[15:13] == xv::XR_TILE_MEM[15:13]);
assign  copp_xr_copp_sel    = (copp_xr_addr_i[15:13] == xv::XR_COPPER_MEM[15:13]);

// color read (vgen or reg XR memory)
always_comb begin
    color_ack_next  = 1'b0;
    color_rd_en     = 1'b0;
    colorA_addr     = vgen_colorA_addr_i;
    colorB_addr     = vgen_colorB_addr_i;
    color_B         = xr_addr_i[8];
    if (vgen_color_sel_i) begin
        color_rd_en     = 1'b1;
        colorA_addr     = vgen_colorA_addr_i;
        colorB_addr     = vgen_colorB_addr_i;
    end else if (xr_sel_i & ~ xr_ack_o) begin
        color_ack_next  = xr_color_sel;
        color_rd_en     = xr_color_sel;
        colorA_addr     = xr_addr_i[7:0];
        colorB_addr     = xr_addr_i[7:0];
    end
end

// tile read (vgen or reg XR memory)
always_comb begin
    tile_ack_next   = 1'b0;
    tile_rd_en      = 1'b0;
    tile_addr       = vgen_tile_addr_i;
    if (vgen_tile_sel_i) begin
        tile_rd_en      = 1'b1;
        tile_addr       = vgen_tile_addr_i;
    end else if (xr_sel_i & ~ xr_ack_o) begin
        tile_ack_next   = xr_tile_sel;
        tile_rd_en      = xr_tile_sel;
        tile_addr       = xr_addr_i[xv::TILE_AWIDTH-1:0];
    end
end

// copp program read
always_comb begin
    copp_ack_next   = 1'b0;
    copp_rd_en      = 1'b0;
    copp_addr       = copp_prog_addr_i;
    copp_odd        = xr_addr_i[0];
    if (copp_prog_sel_i) begin
        copp_rd_en     = 1'b1;
        copp_addr      = copp_prog_addr_i;
    end else if (xr_sel_i & ~xr_ack_o) begin
        copp_ack_next  = xr_copp_sel;
        copp_rd_en     = xr_copp_sel;
        copp_addr      = xr_addr_i[xv::COPPER_AWIDTH:1];
    end
end

// XR register bus read/write
always_comb begin
    xr_ack_next         = 1'b0;
    copp_xr_ack_next    = 1'b0;
    xreg_wr_o           = 1'b0;
    xreg_addr_o         = xr_addr;
    xreg_data_o         = xr_data;
    // copper write has priority
    if (copp_xr_sel_i & ~copp_xr_ack_o) begin
        copp_xr_ack_next    = 1'b1;
        xreg_wr_o           = copp_xr_regs_sel;  // copper only writes
    end else if (xr_sel_i & ~xr_ack_o) begin
        xr_ack_next         = 1'b1;
        xreg_wr_o           = xr_regs_sel & xr_wr_i;
    end
end

// XR memory interface select
always_comb begin
    color_wr_en = 1'b0;
    tile_wr_en  = 1'b0;
    copp_wr_en  = 1'b0;
    // copper write has priority
    xr_addr     = copp_xr_sel_i ? copp_xr_addr_i : xr_addr_i;
    xr_data     = copp_xr_sel_i ? copp_xr_data_i : xr_data_i;
    if (copp_xr_sel_i) begin
        color_wr_en = copp_xr_color_sel;
        tile_wr_en  = copp_xr_tile_sel;
        copp_wr_en  = copp_xr_copp_sel;
    end else if (xr_sel_i) begin
        color_wr_en = xr_color_sel;
        tile_wr_en  = xr_tile_sel;
        copp_wr_en  = xr_copp_sel;
    end
end

// XR read result select with ack
always_ff @(posedge clk) begin
    copp_xr_ack_o   <= copp_xr_ack_next;
    xr_ack_o        <= 1'b0;
    xr_data_o       <= xreg_data_i;
    if (xr_ack_next) begin
        xr_ack_o    <= 1'b1;
        xr_data_o   <= xreg_data_i;
    end
    if (color_ack_next) begin
        xr_ack_o    <= 1'b1;
        xr_data_o   <= !color_B ? colorA_data_out : colorB_data_out;
    end
    if (tile_ack_next) begin
        xr_ack_o    <= 1'b1;
        xr_data_o   <= tile_data_out;
    end
    if (copp_ack_next) begin
        xr_ack_o    <= 1'b1;
        xr_data_o   <= !copp_odd ? copp_data_out[31:16] : copp_data_out[15:0];
    end
end

//  playfield A color lookup RAM
colormem #(
    .AWIDTH(xv::COLOR_AWIDTH)
    ) colormem(
    .clk(clk),
    .rd_en_i(color_rd_en),
    .rd_address_i(colorA_addr),
    .rd_data_o(colorA_data_out),
    .wr_clk(clk),
    .wr_en_i(color_wr_en & ~xr_addr[8]),
    .wr_address_i(xr_addr[7:0]),
    .wr_data_i(xr_data)
);

//  playfield B color lookup RAM
colormem #(
    .AWIDTH(xv::COLOR_AWIDTH)
    ) colormem2(
    .clk(clk),
    .rd_en_i(color_rd_en),
    .rd_address_i(colorB_addr),
    .rd_data_o(colorB_data_out),
    .wr_clk(clk),
    .wr_en_i(color_wr_en & ~xr_addr[8]),
    .wr_address_i(xr_addr[7:0]),
    .wr_data_i(xr_data)
);

// tile RAM
tilemem #(
    .AWIDTH(xv::TILE_AWIDTH)
    )
    tilemem(
    .clk(clk),
    .rd_en_i(tile_rd_en),
    .rd_address_i(tile_addr),
    .rd_data_o(tile_data_out),
    .wr_clk(clk),
    .wr_en_i(tile_wr_en),
    .wr_address_i(xr_addr[xv::TILE_AWIDTH-1:0]),
    .wr_data_i(xr_data)
);

// copper RAM (even word)
coppermem #(
    .AWIDTH(xv::COPPER_AWIDTH)
    ) coppermem_e(
    .clk(clk),
    .rd_en_i(copp_rd_en),
    .rd_address_i(copp_addr),
    .rd_data_o(copp_data_out[31:16]),
    .wr_clk(clk),
    .wr_en_i(copp_wr_en & ~xr_addr[0]),
    .wr_address_i(xr_addr[xv::COPPER_AWIDTH:1]),
    .wr_data_i(xr_data)
);

// copper RAM (odd word)
coppermem #(
    .AWIDTH(xv::COPPER_AWIDTH)
    ) coppermem_o(
    .clk(clk),
    .rd_en_i(copp_rd_en),
    .rd_address_i(copp_addr),
    .rd_data_o(copp_data_out[15:0]),
    .wr_clk(clk),
    .wr_en_i(copp_wr_en & xr_addr[0]),
    .wr_address_i(xr_addr[xv::COPPER_AWIDTH:1]),
    .wr_data_i(xr_data)
);

endmodule
`default_nettype wire               // restore default
