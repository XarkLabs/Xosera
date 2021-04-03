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

`default_nettype none             // mandatory for Verilog sanity
`timescale 1ns/1ps

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

`include "xosera_defs.svh"        // Xosera global Verilog definitions

logic        bus_write_strobe;      // strobe when a word of data written
logic        bus_read_strobe;       // strobe when a word of data written
logic        bus_cs_strobe;       // strobe when a bus selected
logic  [3:0] bus_reg_num;           // bus register on bus
logic [15:0] bus_data_write;        // word written to bus

bus_interface bus(
                  .clk(clk),                            // input clk (should be > 2x faster than bus signals)
                  .bus_cs_n_i(bus_cs_n_i),              // register select strobe
                  .bus_rd_nwr_i(bus_rd_nwr_i),          // 0 = write, 1 = read
                  .bus_reg_num_i(bus_reg_num_i),        // register number
                  .bus_bytesel_i(bus_bytesel_i),        // 0=even byte, 1=odd byte
                  .bus_data_i(bus_data_i),              // 8-bit data bus input
                  .bus_data_o(bus_data_o),              // 8-bit data bus output
                  .write_strobe_o(bus_write_strobe),    // strobe for vram access
                  .read_strobe_o(bus_read_strobe),      // strobe for vram access
                  .cs_strobe_o(bus_cs_strobe),          // strobe for bus selected
                  .reg_num_o(bus_reg_num),              // register number read/written
                  .reg_data_o(bus_data_write),          // word written to register
                  .reg_data_i(blit_to_bus),             // word to read from register
                  .reset_i(reset_i)                     // reset
              );

logic blit_vram_cycle;          // cycle is for blitter (vs video)
logic blit_vram_sel;            // blitter vram select
logic blit_vram_wr;             // blitter vram write
logic [15:0] blit_vram_addr;    // blitter vram addr
logic [15:0] blit_to_vram   /* verilator public */; // blitter bus VRAM data write
logic [15:0] blit_to_bus    /* verilator public */; // blitter bus register read

blitter blitter(
            .clk(clk),
            .blit_cycle_i(blit_vram_cycle),
            .video_ena_o(video_ena),
            .blit_vram_sel_o(blit_vram_sel),
            .blit_vram_wr_o(blit_vram_wr),
            .blit_vram_addr_o(blit_vram_addr),
            .blit_vram_data_i(vram_data_out),
            .blit_vram_data_o(blit_to_vram),
            .reg_write_strobe_i(bus_write_strobe),  // strobe for register write
            .reg_num_i(bus_reg_num),                // register number read/written
            .reg_data_i(bus_data_write),            // word to write to register
            .reg_data_o(blit_to_bus),               // word to read from register
            .reset_i(reset_i)
        );

logic video_ena;            // enable text/bitmap generation
logic vgen_sel;             // video vram select
logic [15:0] vgen_addr;     // video vram addr

//  video generation
video_gen video_gen(
              .clk(clk),
              .reset_i(reset_i),
              .enable_i(video_ena),
              .blit_cycle_o(blit_vram_cycle),
              .fontram_sel_o(fontram_sel),
              .fontram_addr_o(fontram_addr),
              .fontram_data_i(fontram_data_out),
              .vram_sel_o(vgen_sel),
              .vram_addr_o(vgen_addr),
              .vram_data_i(vram_data_out),
              .red_o(red_o),
              .green_o(green_o),
              .blue_o(blue_o),
              .hsync_o(hsync_o),
              .vsync_o(vsync_o),
              .dv_de_o(dv_de_o)
          );

// audio generation (TODO)
assign audio_l_o = bus_cs_strobe;                    // TODO: audio
assign audio_r_o = x_bus_output;                     // TODO: audio

logic x_bus_output;
assign x_bus_output = (bus_cs_n_i == cs_ENABLED && bus_rd_nwr_i == RnW_READ);

//  16x64K (128KB) video memory
logic        vram_sel        /* verilator public */;
logic        vram_wr         /* verilator public */;
logic [15:0] vram_addr       /* verilator public */; // 16-bit word address
logic [15:0] vram_data_out   /* verilator public */;
logic [15:0] vram_data_in    /* verilator public */;

always_comb vram_sel        = blit_vram_cycle ? blit_vram_sel     : vgen_sel;
always_comb vram_wr         = blit_vram_cycle ? blit_vram_wr      : 1'b0;
always_comb vram_addr       = blit_vram_cycle ? blit_vram_addr    : vgen_addr;
always_comb vram_data_in    = blit_to_vram;

    vram vram(
        .clk(clk),
        .sel(vram_sel),
        .wr_en(vram_wr),
        .address_in(vram_addr),
        .data_in(vram_data_in),
        .data_out(vram_data_out)
    );

//  8x8KB font memory
// TODO: Make font memory 16-bits wide
logic        fontram_sel        /* verilator public */;
logic        fontram_wr         /* verilator public */;
logic [12:0] fontram_addr       /* verilator public */; // 13-bit byte address
logic [7:0]  fontram_data_out   /* verilator public */;

fontram fontram(
            .clk(clk),
            .rd_en_i(fontram_sel),
            .rd_address_i(fontram_addr),
            .rd_data_o(fontram_data_out),
            .wr_en_i(1'b0),
            .wr_address_i(13'h0),
            .wr_data_i(8'h00)
        );
endmodule
