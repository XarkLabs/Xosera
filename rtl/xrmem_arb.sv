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

module xrmem_arb(
    // regs XR register/memory interface (read/write)
    input  wire logic                   xr_sel_i,
    output      logic                   xr_ack_o,
    input  wire logic                   xr_wr_i,
    input  wire addr_t                  xr_addr_i,
    input  wire word_t                  xr_data_i,
    output      word_t                  xr_data_o,

`ifdef EN_COPP
    // copper XR register/memory interface (write-only)
    input  wire logic                   copp_xr_sel_i,
    output      logic                   copp_xr_ack_o,
    input  wire addr_t                  copp_xr_addr_i,
    input  wire word_t                  copp_xr_data_i,
`endif

    // XR register bus (read/write)
    output      logic                   xreg_wr_o,
    output      logic  [6:0]            xreg_addr_o,
    input  wire word_t                  xreg_data_i,
    output      word_t                  xreg_data_o,

    // color lookup colormem A+B 2 x 16-bit bus (read-only)
    input  wire logic                   vgen_color_sel_i,
    input  wire color_t                 vgen_colorA_addr_i,
    output      argb_t                  vgen_colorA_data_o,
`ifdef EN_PF_B
    input  wire color_t                 vgen_colorB_addr_i,
    output      argb_t                  vgen_colorB_data_o,
`endif

`ifdef EN_POINTER
    input  wire pointer_t               pointer_addr_i,
    output      word_t                  pointer_data_o,
`endif

    // video generation tilemem bus (read-only)
    input  wire logic                   vgen_tile_sel_i,
    input  wire tile_addr_t             vgen_tile_addr_i,
    output      word_t                  vgen_tile_data_o,

`ifdef EN_COPP
    // copper program memory (read-only)
    input  wire logic                   copp_prog_sel_i,
    input  wire copp_addr_t             copp_prog_addr_i,
    output      word_t                  copp_prog_data_o,
`endif

    input  wire logic                   clk
);

// internal COLORMEM signals
logic                           color_wr_en;
logic [xv::COLOR_W-1:0]         colorA_addr;
argb_t                          colorA_data_out ;
`ifdef EN_PF_B
logic [xv::COLOR_W-1:0]         colorB_addr;
argb_t                          colorB_data_out;
`endif
`ifdef EN_POINTER
word_t                          pointer_data_out;
`endif

// internal TILEMEM signals
logic                           tile_wr_en;
tile_addr_t                     tile_addr;
tile_addr_t                     tile_addr_next;
word_t                          tile_data_out;
word_t                          tile2_data_out;

`ifdef EN_COPP
// internal COPPERMEM signals
logic                           copp_wr_en;
copp_addr_t                     copp_addr;
copp_addr_t                     copp_addr_next;
word_t                          copp_data_out;
word_t                          copp2_data_out;
`endif

// internal XR write signals
addr_t                          xr_addr;
word_t                          xr_write_data;

// combinatorial write ack signals
`ifdef EN_COPP
logic           copp_wr_ack_next;
`endif
logic           xr_wr_ack_next;

// combinatorial read ack signals
logic           xreg_rd_ack_next;
logic           color_rd_ack_next;
logic           tile_rd_ack_next;
`ifdef EN_COPP
logic           copp_rd_ack_next;
`endif

// combinatorial XR decode signals
logic           xr_regs_sel;
logic           xr_color_sel;
logic           xr_tile_sel;
`ifdef EN_COPP
logic           xr_copp_sel;
`endif

`ifdef EN_COPP
// combinatorial copper XR decode signals
logic           copp_xr_regs_sel;
logic           copp_xr_color_sel;
logic           copp_xr_tile_sel;
logic           copp_xr_copp_sel;
`endif

// assign read outputs
assign  vgen_colorA_data_o  = colorA_data_out;
`ifdef EN_PF_B
assign  vgen_colorB_data_o  = colorB_data_out;
`endif
assign  vgen_tile_data_o    = !tile_addr[xv::TILE_W-1] ? tile_data_out : tile2_data_out;
`ifdef EN_COPP
assign  copp_prog_data_o    = !copp_addr[xv::COPP_W-1] ? copp_data_out : copp2_data_out; ;
`endif
`ifdef EN_POINTER
assign  pointer_data_o      = pointer_data_out;
`endif


// decode flags for XR interface
assign  xr_regs_sel     = (xr_addr_i[15:14] == xv::XR_CONFIG_REGS[15:14]);
assign  xr_tile_sel     = (xr_addr_i[15:14] == xv::XR_TILE_ADDR[15:14]);
assign  xr_color_sel    = (xr_addr_i[15:14] == xv::XR_COLOR_ADDR[15:14]);

`ifdef EN_COPP
assign  xr_copp_sel     = (xr_addr_i[15:14] == xv::XR_COPPER_ADDR[15:14]);

// decode flags for copper XR interface
assign  copp_xr_regs_sel    = (copp_xr_addr_i[15:14] == xv::XR_CONFIG_REGS[15:14]);
assign  copp_xr_tile_sel    = (copp_xr_addr_i[15:14] == xv::XR_TILE_ADDR[15:14]);
assign  copp_xr_color_sel   = (copp_xr_addr_i[15:14] == xv::XR_COLOR_ADDR[15:14]);
assign  copp_xr_copp_sel    = (copp_xr_addr_i[15:14] == xv::XR_COPPER_ADDR[15:14]);
`endif

`ifdef EN_COPP
// select addr and write data from XR or copper XR write
assign xr_addr          = copp_xr_sel_i ? copp_xr_addr_i : xr_addr_i;
assign xr_write_data    = copp_xr_sel_i ? copp_xr_data_i : xr_data_i;
`else
// select addr and write data from XR
assign xr_addr          = xr_addr_i;
assign xr_write_data    = xr_data_i;
`endif

// XR memory interface write select (copper / regs)
always_comb begin
    xr_wr_ack_next      = 1'b0;
    color_wr_en         = 1'b0;
    tile_wr_en          = 1'b0;
`ifdef EN_COPP
    copp_wr_ack_next    = 1'b0;
    copp_wr_en          = 1'b0;
    // copper write has priority
    if (copp_xr_sel_i & ~copp_xr_ack_o) begin
        copp_wr_ack_next    = 1'b1;
        color_wr_en         = copp_xr_color_sel;
        tile_wr_en          = copp_xr_tile_sel;
        copp_wr_en          = copp_xr_copp_sel;
    end
`endif
    if (
`ifdef EN_COPP
        ~copp_xr_sel_i &
`endif
        xr_sel_i & xr_wr_i & ~xr_ack_o) begin
        xr_wr_ack_next      = 1'b1;
        color_wr_en         = xr_color_sel;
        tile_wr_en          = xr_tile_sel;
`ifdef EN_COPP
        copp_wr_en          = xr_copp_sel;
`endif
    end
end

// XR read result select
always_comb begin
    xr_data_o       = xreg_data_i;
    if (xr_color_sel) begin
`ifdef EN_PF_B
        xr_data_o   = !xr_addr_i[xv::COLOR_W] ? colorA_data_out : colorB_data_out;
`else
        xr_data_o   = colorA_data_out;
`endif
    end
    if (xr_tile_sel) begin
        xr_data_o   = !xr_addr_i[xv::TILE_W-1] ? tile_data_out : tile2_data_out;
    end
`ifdef EN_COPP
    if (xr_copp_sel) begin
        xr_data_o   = !xr_addr_i[xv::COPP_W-1] ? copp_data_out : copp2_data_out; ;
    end
`endif
end

// XR register bus (XR read/write or copper XR write only)
always_comb begin
    xreg_rd_ack_next    = 1'b0;
    xreg_wr_o           = 1'b0;
    xreg_addr_o         = xr_addr[6:0];
    xreg_data_o         = xr_write_data;
`ifdef EN_COPP
    // copper write has priority
    if (copp_xr_sel_i & ~copp_xr_ack_o) begin
        xreg_wr_o           = copp_xr_regs_sel;     // copper only writes
    end
`endif
    if (
`ifdef EN_COPP
        ~copp_xr_sel_i &                           // avoid read interference from copper write
`endif
        xr_sel_i & ~xr_ack_o) begin
        xreg_rd_ack_next    = xr_regs_sel & ~xr_wr_i;
        xreg_wr_o           = xr_regs_sel & xr_wr_i;
    end
end

// color mem read (vgen or reg XR memory)
always_comb begin
    color_rd_ack_next   = 1'b0;
    colorA_addr         = vgen_colorA_addr_i;
`ifdef EN_PF_B
    colorB_addr         = vgen_colorB_addr_i;
`endif
    if (vgen_color_sel_i) begin
        colorA_addr         = vgen_colorA_addr_i;
`ifdef EN_PF_B
        colorB_addr         = vgen_colorB_addr_i;
`endif
    end else if (xr_sel_i & ~xr_ack_o) begin
        color_rd_ack_next   = xr_color_sel & ~xr_wr_i;;
        colorA_addr         = $bits(colorA_addr)'(xr_addr_i);
`ifdef EN_PF_B
        colorB_addr         = $bits(colorB_addr)'(xr_addr_i);
`endif
    end
end

// tile mem read (vgen or reg XR memory)
always_comb begin
    tile_rd_ack_next        = 1'b0;
    tile_addr_next          = vgen_tile_addr_i;
    if (vgen_tile_sel_i) begin
        tile_addr_next      = vgen_tile_addr_i;
    end else if (xr_sel_i & ~xr_ack_o) begin
        tile_rd_ack_next    = xr_tile_sel & ~xr_wr_i;
        tile_addr_next      = $bits(tile_addr_next)'(xr_addr_i);
    end
end

`ifdef EN_COPP
// copp mem read (copper or reg XR memory)
always_comb begin
    copp_rd_ack_next        = 1'b0;
    copp_addr_next          = copp_prog_addr_i;
    if (copp_prog_sel_i) begin
        copp_addr_next      = copp_prog_addr_i;
    end else if (xr_sel_i & ~xr_ack_o) begin
        copp_rd_ack_next    = xr_copp_sel & ~xr_wr_i;;
        copp_addr_next      = $bits(copp_addr_next)'(xr_addr_i);
    end
end
`endif

// update acknowledge signals
always_ff @(posedge clk) begin

    tile_addr       <= tile_addr_next;  // need last cycle's read addr to determine tile or tile2 result output
`ifdef EN_COPP
    copp_addr       <= copp_addr_next;  // need last cycle's read addr to determine copp or copp2 result output
    copp_xr_ack_o   <= copp_wr_ack_next;
`endif
    xr_ack_o        <= xr_wr_ack_next | xreg_rd_ack_next | color_rd_ack_next | tile_rd_ack_next
`ifdef EN_COPP
                        | copp_rd_ack_next
`endif
                        ;
end

// playfield A color lookup RAM
colormem #(
    .AWIDTH(xv::COLOR_W),
    .PLAYFIELD("A")
) colormem_A(
    .clk(clk),
    .rd_address_i(colorA_addr),
    .rd_data_o(colorA_data_out),
    .wr_clk(clk),
    .wr_en_i(color_wr_en & ~xr_addr[xv::COLOR_W]
`ifdef EN_POINTER
    & ~xr_addr[xv::COLOR_W+1]
`endif
    ),
    .wr_address_i(xr_addr[xv::COLOR_W-1:0]),
    .wr_data_i(xr_write_data)
);

// playfield B color lookup RAM
`ifdef EN_PF_B
colormem #(
    .AWIDTH(xv::COLOR_W),
    .PLAYFIELD("B")
) colormem_B(
    .clk(clk),
    .rd_address_i(colorB_addr),
    .rd_data_o(colorB_data_out),
    .wr_clk(clk),
    .wr_en_i(color_wr_en & xr_addr[xv::COLOR_W]
`ifdef EN_POINTER
    & ~xr_addr[xv::COLOR_W+1]
`endif
    ),
    .wr_address_i(xr_addr[xv::COLOR_W-1:0]),
    .wr_data_i(xr_write_data)
);
`endif

`ifdef EN_POINTER
// pointer image RAM (32x32 4-bpp)
pointermem pointermem(
    .clk(clk),
    .rd_address_i(pointer_addr_i),
    .rd_data_o(pointer_data_out),
    .wr_clk(clk),
    .wr_en_i(color_wr_en & xr_addr[xv::COLOR_W+1]),
    .wr_address_i(xr_addr[xv::POINTER_W-1:0]),
    .wr_data_i(xr_write_data)
);
`endif

// tile RAM
tilemem #(
    .AWIDTH(xv::TILE_W-1)
) tilemem(
    .clk(clk),
    .rd_address_i(tile_addr_next[xv::TILE_W-2:0]),
    .rd_data_o(tile_data_out),
    .wr_clk(clk),
    .wr_en_i(tile_wr_en & ~xr_addr[xv::TILE_W-1]),
    .wr_address_i(xr_addr[xv::TILE_W-2:0]),
    .wr_data_i(xr_write_data)
);

// tile RAM additional 1K words
tilemem #(
    .AWIDTH(xv::TILE2_W)
) tilemem_2(
    .clk(clk),
    .rd_address_i(tile_addr_next[xv::TILE2_W-1:0]),
    .rd_data_o(tile2_data_out),
    .wr_clk(clk),
    .wr_en_i(tile_wr_en & xr_addr[xv::TILE_W-1]),
    .wr_address_i(xr_addr[xv::TILE2_W-1:0]),
    .wr_data_i(xr_write_data)
);

`ifdef EN_COPP
// copper RAM 1K words
coppermem #(
    .AWIDTH(xv::COPP_W-1),
    .ADDINFO("N")
) coppermem(
    .clk(clk),
    .rd_address_i(copp_addr_next[xv::COPP_W-2:0]),
    .rd_data_o(copp_data_out),
    .wr_clk(clk),
    .wr_en_i(copp_wr_en & ~xr_addr[xv::COPP_W-1]),
    .wr_address_i(xr_addr[xv::COPP_W-2:0]),
    .wr_data_i(xr_write_data)
);

// copper RAM additional 512 words
coppermem #(
    .AWIDTH(xv::COPP2_W),
    .ADDINFO("Y")
) coppermem_2(
    .clk(clk),
    .rd_address_i(copp_addr_next[xv::COPP2_W-1:0]),
    .rd_data_o(copp2_data_out),
    .wr_clk(clk),
    .wr_en_i(copp_wr_en & xr_addr[xv::COPP_W-1]),
    .wr_address_i(xr_addr[xv::COPP2_W-1:0]),
    .wr_data_i(xr_write_data)
);

`endif

endmodule
`default_nettype wire               // restore default
