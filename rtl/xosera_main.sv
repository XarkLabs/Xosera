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
           input  wire logic         clk,                    // pixel clock
           input  wire logic         bus_cs_n_i,             // register select strobe (active low)
           input  wire logic         bus_rd_nwr_i,           // 0 = write, 1 = read
           input  wire logic [3:0]   bus_reg_num_i,          // register number
           input  wire logic         bus_bytesel_i,          // 0 = even byte, 1 = odd byte
           input  wire logic [7:0]   bus_data_i,             // 8-bit data bus input
           output logic      [7:0]   bus_data_o,             // 8-bit data bus output
           output logic              bus_intr_o,             // Vertical blank (active high)
           output logic      [3:0]   red_o, green_o, blue_o, // RGB 4-bit color outputs
           output logic              hsync_o, vsync_o,       // horizontal and vertical sync
           output logic              dv_de_o,                // pixel visible (aka display enable)
           output logic              audio_l_o, audio_r_o,   // left and right audio PWM output
           output logic              reconfig_o,             // reconfigure iCE40 from flash
           output logic      [1:0]   boot_select_o,          // reconfigure congigureation number (0-3)
           input  wire logic         reset_i                 // reset signal
       );

logic        blit_vram_sel  /* verilator public */;     // blitter VRAM select
logic        blit_aux_sel   /* verilator public */;     // blitter AUX select
logic        blit_wr        /* verilator public */;     // blitter VRAM/AUX rite
logic  [3:0] blit_mask      /* verilator public */;     // 4 nibble write masks for vram

logic [15:0] blit_addr      /* verilator public */;     // blitter VRAM/AUX addr
logic [15:0] blit_data_in   /* verilator public */;     // blitter VRAM/AUX data read
logic [15:0] blit_data_out  /* verilator public */;     // blitter bus VRAM/AUX data write

logic blit_vgen_reg_sel     /* verilator public */;
logic blit_vgen_reg_wr      /* verilator public */;

logic blit_fontram_sel      /* verilator public */;
logic blit_fontram_wr       /* verilator public */;

logic blit_paletteram_sel   /* verilator public */;
logic blit_paletteram_wr    /* verilator public */;

logic blit_other_sel        /* verilator public */;
logic blit_other_wr         /* verilator public */;

assign  blit_vgen_reg_sel   = (blit_addr[15:14] == xv::AUX_VID[15:14]) && blit_aux_sel;
assign  blit_fontram_sel    = (blit_addr[15:14] == xv::AUX_FONTMEM[15:14]) && blit_aux_sel;
assign  blit_paletteram_sel = (blit_addr[15:14] == xv::AUX_COLORMEM[15:14]) && blit_aux_sel;
assign  blit_other_sel      = (blit_addr[15:14] == xv::AUX_OTHERMEM[15:14]) && blit_aux_sel;

assign  blit_vgen_reg_wr    = blit_vgen_reg_sel & blit_wr;
assign  blit_fontram_wr     = blit_fontram_sel & blit_wr;
assign  blit_paletteram_wr  = blit_paletteram_sel && blit_wr;
assign  blit_other_wr       = blit_paletteram_sel && blit_wr;

//  16x64K (128KB) video memory
logic        vram_sel       /* verilator public */;
logic        vram_wr        /* verilator public */;
logic  [3:0] vram_mask      /* verilator public */; // 4 nibble masks for vram write
logic [15:0] vram_addr      /* verilator public */; // 16-bit word address
logic [15:0] vram_data_in   /* verilator public */;
logic [15:0] vram_data_out  /* verilator public */;
logic        vgen_vram_load;
logic [15:0] vgen_vram_read;
logic        blit_vram_load;
logic [15:0] blit_vram_read;

logic vgen_ena;                 // enable text/bitmap generation
logic vgen_vram_sel;            // video vram select (read)
logic [15:0] vgen_vram_addr;    // video vram addr
logic [15:0] vgen_data_in;      // video vram read data
logic [15:0] vgen_reg_data_out; // video data out for blitter reg reads

logic dbug_cs_strobe;               // TODO debug ACK signal
logic dbug_drive_bus;               // TODO debug bus output signal

logic           fontram_rd_en       /* verilator public */;
logic [11:0]    fontram_addr        /* verilator public */; // 12-bit word address
logic [15:0]    fontram_data_out    /* verilator public */;

logic  [7:0]    pal_index       /* verilator public */;
logic [15:0]    pal_lookup      /* verilator public */;

logic           bus_intr_1;
logic           vsync_1;
logic           hsync_1;
logic           dv_de_1;

// audio generation (TODO)
assign audio_l_o = dbug_cs_strobe;                    // TODO: audio
assign audio_r_o = blit_aux_sel; //dbug_drive_bus;                    // TODO: audio

assign dbug_drive_bus = (bus_cs_n_i == xv::cs_ENABLED && bus_rd_nwr_i == xv::RnW_READ);

assign vram_sel     = vgen_vram_sel ? 1'b1              : blit_vram_sel;
assign vram_wr      = vgen_vram_sel ? 1'b0              : (blit_wr & blit_vram_sel);
assign vram_mask    = vgen_vram_sel ? 4'b0000           : blit_mask;
assign vram_addr    = vgen_vram_sel ? vgen_vram_addr    : blit_addr;
assign vram_data_in = blit_data_out;
assign blit_data_in = blit_vram_load ? vram_data_out    : blit_vram_read;
assign vgen_data_in = vgen_vram_load ? vram_data_out    : vgen_vram_read;
 
// save vgen value read from vram
always_ff @(posedge clk) begin
    if (vgen_vram_load) begin
        vgen_vram_read <= vram_data_out;
        vgen_vram_load <= 1'b0; 
    end
    if (vgen_vram_sel) begin
        vgen_vram_load <= 1'b1;
    end
end

// save blit value read from vram
always_ff @(posedge clk) begin
    if (blit_vram_load) begin
        blit_vram_read <= vram_data_out;
        blit_vram_load <= 1'b0; 
    end
    if (blit_vram_sel) begin
        blit_vram_load <= ~blit_wr;
    end
end

blitter blitter(
            .clk(clk),
            .bus_cs_n_i(bus_cs_n_i),            // register select strobe
            .bus_rd_nwr_i(bus_rd_nwr_i),        // 0 = write, 1 = read
            .bus_reg_num_i(bus_reg_num_i),      // register number
            .bus_bytesel_i(bus_bytesel_i),      // 0=even byte, 1=odd byte
            .bus_data_i(bus_data_i),            // 8-bit data bus input
            .bus_data_o(bus_data_o),            // 8-bit data bus output
            .vgen_sel_i(vgen_vram_sel),         // blitter or vgen vram access this cycle
            .vgen_ena_o(vgen_ena),              // enable video generation
            .blit_vram_sel_o(blit_vram_sel),    // blitter vram select
            .blit_aux_sel_o(blit_aux_sel),      // blitter aux memory select
            .blit_wr_o(blit_wr),                // blitter write
            .blit_mask_o(blit_mask),            // vram nibble masks
            .blit_addr_o(blit_addr),            // vram/aux address
            .blit_data_i(blit_data_in),         // 16-bit word read from aux/vram
            .blit_data_o(blit_data_out),        // 16-bit word write to aux/vram
            .aux_data_i(vgen_reg_data_out),
            .reconfig_o(reconfig_o),
            .boot_select_o(boot_select_o),
            .bus_ack_o(dbug_cs_strobe),            // TODO debug
            .reset_i(reset_i)
        );

//  video generation
video_gen video_gen(
    .clk(clk),
    .reset_i(reset_i),
    .enable_i(vgen_ena),
    .fontram_sel_o(fontram_rd_en),
    .fontram_addr_o(fontram_addr),
    .fontram_data_i(fontram_data_out),
    .vram_sel_o(vgen_vram_sel),
    .vram_addr_o(vgen_vram_addr),
    .vram_data_i(vgen_data_in),
    .vgen_reg_wr_i(blit_vgen_reg_wr),
    .vgen_reg_num_i(blit_addr[3:0]),
    .vgen_reg_data_o(vgen_reg_data_out),
    .vgen_reg_data_i(blit_data_out),
    .pal_index_o(pal_index),
    .bus_intr_o(bus_intr_1),
    .hsync_o(hsync_1),
    .vsync_o(vsync_1),
    .dv_de_o(dv_de_1)
);

vram vram(
    .clk(clk),
    .sel(vram_sel),
    .wr_en(vram_wr),
    .wr_mask(vram_mask),
    .address_in(vram_addr),
    .data_in(vram_data_in),
    .data_out(vram_data_out)
);

//  16-bit x 4KB font memory
fontram fontram(
    .clk(clk),
    .rd_en_i(fontram_rd_en),
    .rd_address_i(fontram_addr),
    .rd_data_o(fontram_data_out),
    .wr_clk(clk),
    .wr_en_i(blit_fontram_wr),
    .wr_address_i(blit_addr[11:0]),
    .wr_data_i(blit_data_out)
);

// video palette RAM
paletteram paletteram(
    .clk(clk),
    .rd_en_i(1'b1),
    .rd_address_i(pal_index),
    .rd_data_o(pal_lookup),
    .wr_clk(clk),
    .wr_en_i(blit_paletteram_wr),
    .wr_address_i(blit_addr[7:0]),
    .wr_data_i(blit_data_out)
);

// palette RAM lookup (delays video 1 cycle for BRAM)
always_ff @(posedge clk) begin
    bus_intr_o  <= bus_intr_1;
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
`default_nettype wire               // restore default
