// xosera_main.sv
//
// vim: set et ts=4 sw=4
//
// Copyright (c) 2020 Xark - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)
//
// This project would not be possible without learning from the following
// open projects (and many others, no doubt):
//
// YaGraphCon       - http://www.frank-buss.de/yagraphcon/
// yavga            - https://opencores.org/projects/yavga
// f32c             - https://github.com/f32c
// up5k_vga         - https://github.com/emeb/up5k_vga
// icestation-32    - https://github.com/dan-rodrigues/icestation-32Tanger
// ice40-playground - https://github.com/smunaut/ice40-playground
// Project-F        - https://github.com/projf/projf-explore
//
// Also the following web sites:
// Hamsterworks     - https://web.archive.org/web/20190119005744/http://hamsterworks.co.nz/mediawiki/index.php/Main_Page
//                    (Archived, but not forgotten - Thanks Mike Fields)
// John's FPGA Page - http://members.optushome.com.au/jekent/FPGA.htm
// FPGA4Fun         - https://www.fpga4fun.com/
// Nandland         - https://www.nandland.com/
// Project-F        - https://projectf.io/
// Alchrity         - https://alchitry.com/
//
// 1BitSquared Discord server has also been welcoming and helpful - https://1bitsquared.com/pages/chat
//
// Special thanks to everyone involved with the IceStorm/Yosys/NextPNR (etc.) open source FPGA projects.
// Consider supporting open source FPGA tool development: https://www.patreon.com/fpga_dave

`default_nettype none               // mandatory for Verilog sanity
`timescale 1ns/1ps                  // mandatory to shut up Icarus Verilog

`include "xosera_pkg.sv"

module xosera_main(
           input  logic         clk,                    // pixel clock
           input  logic         bus_cs_n_i,             // register select strobe (active low)
           input  logic         bus_rd_nwr_i,           // 0 = write, 1 = read
           input  logic [3:0]   bus_reg_num_i,          // register number
           input  logic         bus_bytesel_i,          // 0 = even byte, 1 = odd byte
           input  logic [7:0]   bus_data_i,             // 8-bit data bus input
           output logic [7:0]   bus_data_o,             // 8-bit data bus output
           output logic [3:0]   red_o, green_o, blue_o, // RGB 4-bit color outputs
           output logic         hsync_o, vsync_o,       // horizontal and vertical sync
           output logic         dv_de_o,                // pixel visible (aka display enable)
           output logic         audio_l_o, audio_r_o,   // left and right audio PWM output
           input  logic         reset_i                 // reset signal
       );

logic blit_vram_cycle;          // cycle is for blitter (vs video)
logic blit_vram_sel;            // blitter vram select
logic blit_aux_sel;
logic blit_wr;

logic [15:0] blit_addr;    // blitter vram addr
logic [15:0] blit_data_in   /* verilator public */; // blitter VRAM/AUX data read
logic [15:0] blit_data_out   /* verilator public */; // blitter bus VRAM/AUX data write
logic [15:0] blit_to_bus    /* verilator public */; // blitter bus register read

blitter blitter(
            .clk(clk),
            .bus_cs_n_i(bus_cs_n_i),              // register select strobe
            .bus_rd_nwr_i(bus_rd_nwr_i),          // 0 = write, 1 = read
            .bus_reg_num_i(bus_reg_num_i),        // register number
            .bus_bytesel_i(bus_bytesel_i),        // 0=even byte, 1=odd byte
            .bus_data_i(bus_data_i),              // 8-bit data bus input
            .bus_data_o(bus_data_o),              // 8-bit data bus output
            .blit_cycle_i(blit_vram_cycle),
            .vgen_ena_o(vgen_ena),
            .blit_vram_sel_o(blit_vram_sel),
            .blit_aux_sel_o(blit_aux_sel),
            .blit_wr_o(blit_wr),
            .blit_addr_o(blit_addr),
            .blit_data_i(blit_data_in),
            .blit_data_o(blit_data_out),
            .aux_data_i(vgen_data_out),
            .bus_ack_o(dbug_cs_strobe),            // TODO debug
            .reset_i(reset_i)
        );

logic vgen_ena;            // enable text/bitmap generation
logic vgen_sel;             // video vram select
logic [15:0] vgen_addr;     // video vram addr
logic [15:0] vgen_data_out; // video reg reads

logic vgen_reg_wr;
assign vgen_reg_wr = (blit_addr[15:14] == xv::AUX_VID_W_DISPSTART[15:14]) && blit_aux_sel && blit_wr;

//  video generation
video_gen video_gen(
    .clk(clk),
    .reset_i(reset_i),
    .enable_i(vgen_ena),
    .blit_cycle_o(blit_vram_cycle),
    .fontram_sel_o(fontram_rd_en),
    .fontram_addr_o(fontram_addr),
    .fontram_data_i(fontram_data_out),
    .vram_sel_o(vgen_sel),
    .vram_addr_o(vgen_addr),
    .vram_data_i(blit_data_in),
    .vgen_reg_wr_i(vgen_reg_wr),
    .vgen_reg_num_i(blit_addr[2:0]),
    .vgen_reg_data_o(vgen_data_out),
    .vgen_reg_data_i(blit_data_out),
    .pal_index_o(pal_index),
    .hsync_o(hsync_1),
    .vsync_o(vsync_1),
    .dv_de_o(dv_de_1)
);

// audio generation (TODO)
assign audio_l_o = dbug_cs_strobe;                    // TODO: audio
assign audio_r_o = dbug_drive_bus;                    // TODO: audio

logic dbug_cs_strobe;               // TODO debug ACK signal
logic dbug_drive_bus;               // TODO debug bus output signal
assign dbug_drive_bus = (bus_cs_n_i == xv::cs_ENABLED && bus_rd_nwr_i == xv::RnW_READ);

//  16x64K (128KB) video memory
logic        vram_sel       /* verilator public */;
logic        vram_wr        /* verilator public */;
logic [15:0] vram_addr      /* verilator public */; // 16-bit word address
logic [15:0] vram_data_in   /* verilator public */;
logic [15:0] vram_data_out  /* verilator public */;

always_comb vram_sel        = blit_vram_cycle ? blit_vram_sel             : vgen_sel    ;
always_comb vram_wr         = blit_vram_cycle ? (blit_wr & blit_vram_sel) : 1'b0;
always_comb vram_addr       = blit_vram_cycle ? blit_addr                 : vgen_addr;
always_comb vram_data_in    = blit_data_out;
always_comb blit_data_in    = vram_data_out;

vram vram(
    .clk(clk),
    .sel(vram_sel),
    .wr_en(vram_wr),
    .address_in(vram_addr),
    .data_in(vram_data_in),
    .data_out(vram_data_out)
);

//  8x8KB font memory
// TODO: Make font memory 16-bits wide?
logic           fontram_rd_en       /* verilator public */;
logic [12:0]    fontram_addr        /* verilator public */; // 13-bit byte address
logic  [7:0]    fontram_data_out    /* verilator public */;
logic           fontram_wr_en       /* verilator public */;
assign          fontram_wr_en = (blit_addr[15:14] == xv::AUX_FONT[15:14]) && blit_aux_sel && blit_wr;

fontram fontram(
    .clk(clk),
    .rd_en_i(fontram_rd_en),
    .rd_address_i(fontram_addr),
    .rd_data_o(fontram_data_out),
    .wr_clk(clk),
    .wr_en_i(fontram_wr_en),
    .wr_address_i(blit_addr[12:0]),
    .wr_data_i(blit_data_out[7:0])
);

// video palette RAM
logic  [3:0]    pal_index       /* verilator public */;
logic [15:0]    pal_lookup      /* verilator public */;
logic           palette_wr_en   /* verilator public */;
assign          palette_wr_en = (blit_addr[15:14] == xv::AUX_COLORTBL[15:14]) && blit_aux_sel && blit_wr;

paletteram paletteram(
    .clk(clk),
    .rd_en_i(1'b1),
    .rd_address_i({ 4'h0, pal_index}),
    .rd_data_o(pal_lookup),
    .wr_clk(clk),
    .wr_en_i(palette_wr_en),
    .wr_address_i(blit_addr[7:0]),
    .wr_data_i(blit_data_out)
);

// palette RAM lookup (delays video 1 cycle for BRAM)
logic           vsync_1;
logic           hsync_1;
logic           dv_de_1;

always_ff @(posedge clk) begin
    vsync_o     <= vsync_1;
    hsync_o     <= hsync_1;
    dv_de_o     <= dv_de_1;
    red_o       <= 4'h0;
    green_o     <= 4'h0;
    blue_o      <= 4'h0;
    if (dv_de_1) begin
        red_o       <= pal_lookup[11:8];
        green_o     <= pal_lookup[7:4];
        blue_o      <= pal_lookup[3:0];
    end
end

endmodule
