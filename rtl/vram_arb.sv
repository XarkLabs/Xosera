// vram_arb.sv
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

module vram_arb
(
    // video generation access (read-only)
    input  wire logic           vgen_sel_i,
    input  wire logic [15:0]    vgen_addr_i,

    // register interface access (read/write)
    input  wire logic           regs_sel_i        /* verilator public */,
    output      logic           regs_ack_o        /* verilator public */,
    input  wire logic           regs_wr_i        /* verilator public */,
    input  wire logic  [3:0]    regs_wr_mask_i,
    input  wire logic [15:0]    regs_addr_i        /* verilator public */,
    input  wire logic [15:0]    regs_data_i        /* verilator public */,

    // TODO: 2D blit access (read/write)
    input  wire logic           blit_sel_i,
    output      logic           blit_ack_o,
    input  wire logic           blit_wr_i,
    input  wire logic  [3:0]    blit_wr_mask_i,
    input  wire logic [15:0]    blit_addr_i,
    input  wire logic [15:0]    blit_data_i,

    // TODO: polygon draw access (read/write)
    input  wire logic           draw_sel_i,
    output      logic           draw_ack_o,
    input  wire logic           draw_wr_i,
    input  wire logic  [3:0]    draw_wr_mask_i,
    input  wire logic [15:0]    draw_addr_i,
    input  wire logic [15:0]    draw_data_i,

    // common VRAM data output
    output      logic [15:0]    vram_data_o        /* verilator public */,

    input  wire logic           clk
);

// internal VRAM signals
logic           vram_sel        /* verilator public */;
logic           vram_wr         /* verilator public */;
logic  [3:0]    vram_wr_mask    /* verilator public */;
logic [15:0]    vram_addr       /* verilator public */;
logic [15:0]    vram_data_in    /* verilator public */;

// ack signals
logic           regs_ack_next;
logic           blit_ack_next;
logic           draw_ack_next;

always_comb begin
    regs_ack_next   = 1'b0;
    blit_ack_next   = 1'b0;
    draw_ack_next   = 1'b0;
    vram_sel        = 1'b0;
    vram_wr         = 1'b0;
    vram_addr       = regs_addr_i;
    vram_wr_mask    = regs_wr_mask_i;
    vram_data_in    = regs_data_i;
    if (vgen_sel_i) begin
        vram_sel        = 1'b1;
        vram_wr         = 1'b0;             // no vgen write
        vram_addr       = vgen_addr_i;
        vram_wr_mask    = regs_wr_mask_i;   // not really used (no write)
        vram_data_in    = regs_data_i;      // not really used (no write)
    end else if (regs_sel_i & ~regs_ack_o) begin
        regs_ack_next   = 1'b1;
        vram_sel        = 1'b1;
        vram_wr         = regs_wr_i;
        vram_addr       = regs_addr_i;
        vram_wr_mask    = regs_wr_mask_i;
        vram_data_in    = regs_data_i;
    end else if (blit_sel_i & ~blit_ack_o) begin
        blit_ack_next   = 1'b1;
        vram_sel        = 1'b1;
        vram_wr         = blit_wr_i;
        vram_addr       = blit_addr_i;
        vram_wr_mask    = blit_wr_mask_i;
        vram_data_in    = blit_data_i;
    end else if (draw_sel_i & ~draw_ack_o) begin
        draw_ack_next   = 1'b1;
        vram_sel        = 1'b1;
        vram_wr         = draw_wr_i;
        vram_addr       = draw_addr_i;
        vram_wr_mask    = draw_wr_mask_i;
        vram_data_in    = draw_data_i;
    end
end

always_ff @(posedge clk) begin
    regs_ack_o  <= regs_ack_next;
    blit_ack_o  <= blit_ack_next;
    draw_ack_o  <= draw_ack_next;
end

vram vram(
    .clk(clk),
    .sel(vram_sel),
    .wr_en(vram_wr),
    .wr_mask(vram_wr_mask),
    .address_in(vram_addr),
    .data_in(vram_data_in),
    .data_out(vram_data_o)
);

endmodule
`default_nettype wire               // restore default
