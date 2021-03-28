// xosera_iceb.sv - Top module for iCEBreaker Xosera
//
// vim: set et ts=4 sw=4
//
// Copyright (c) 2020 Xark - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)
//
// For info about iCEBreaker: https://1bitsquared.com/products/icebreaker

//

`default_nettype none   // mandatory for Verilog sanity
`timescale 1ns/1ns

`ifdef PMOD_1B2_DVI12
`elsif PMOD_DIGILENT_VGA
`elsif PMOD_XESS_VGA
`elsif PMOD_XESS_VGA_SINGLE
`else
`define PMOD_1B2_DVI12    // default
`endif

module xosera_iceb(
           input  logic CLK,                            // 12Mhz clock
           input  logic BTN_N,                          // reset button (active LOW)
           input  logic LEDR_N,                         // RED LED, but as input FTDI SPI SS (aka CTS)
           output logic LEDG_N,                         // GREEN LED
           input  logic RX,                             // UART receive
           output logic TX,                             // UART transmit
           output logic FLASH_SSB,                      // SPI flash CS (drive high unless using SPI flash)
           input  logic LED_RED_N,LED_GRN_N,LED_BLU_N,
           output logic P1A1, P1A2, P1A3, P1A4, P1A7, P1A8, P1A9, P1A10,        // PMOD 1A
           output logic P1B1, P1B2, P1B3, P1B4, P1B7, P1B8, P1B9, P1B10,        // PMOD 1B
           inout  logic P2_1, P2_2, P2_3, P2_4, P2_7, P2_8, P2_9, P2_10,        // PMOD 2 (68K BUS DATA8-15)
           input  logic FLASH_IO0, FLASH_IO1, FLASH_IO2, FLASH_IO3
       );

`include "../rtl/xosera_clk_defs.svh"    // Xosera global Verilog definitions

assign FLASH_SSB    = 1'b1;     // prevent SPI flash interfering with other SPI/FTDI pins
assign LEDG_N       = ~reset;   // green LED when not in reset
assign TX = RX;                 // loopback serial

// gpio pin aliases
logic       nreset;                     // user button as reset
logic       bus_cs_n;                  // bus select (active LOW)
logic       bus_rd_nwr;                 // bus read not write (write LOW, read HIGH)
logic       bus_bytesel;                // bus even/odd byte select (even LOW, odd HIGH)
logic [3:0] bus_reg_num;                // bus 4-bit register index number (16-bit registers)
logic [7:0] bus_data;                   // bus 8-bit bidirectional data I/O
logic       audio_l;                    // left audio PWM
logic       audio_r;                    // right audio PWM
logic [3:0] vga_r;                      // vga red (4-bit)
logic [3:0] vga_g;                      // vga green (4-bits)
logic [3:0] vga_b;                      // vga blue (4-bits)
logic       vga_hs;                     // vga hsync
logic       vga_vs;                     // vga vsync
logic       dvi_de;                     // HDMI display enable

// assign input signals to pins
assign nreset       = BTN_N;        // active LOW reset
assign bus_cs_n    = LED_GRN_N;         // RGB red as select input (UP_nCS)
assign bus_rd_nwr   = 1'b0;              // RGB blue as read/not write (always write on iCEBreaker)
assign bus_bytesel  = LED_BLU_N;         // gpio for word byte select
assign bus_reg_num  = { FLASH_IO0, FLASH_IO1, FLASH_IO2, FLASH_IO3 };   // gpio for register number
assign bus_data     = { P2_1, P2_2, P2_3, P2_4, P2_7, P2_8, P2_9, P2_10 };   // gpio for data bus

// split tri-state data lines into in/out signals for inside FPGA
typedef enum { RnW_WRITE, RnW_READ } RnW_t;
typedef enum { cs_ENABLED, cs_DISABLED } cs_n_t;
logic [7:0] bus_data_out;
logic [7:0] bus_data_in;
// tri-state data bus unless Xosera is both selected and bus is reading
assign bus_data = (bus_cs_n == cs_ENABLED && bus_rd_nwr == RnW_READ) ? bus_data_out : 8'bZ;
assign bus_data_in = bus_data;


// video output signals
`ifdef PMOD_1B2_DVI12
// 12-bit DVI using dual-PMOD https://1bitsquared.com/products/pmod-digital-video-interface
`ifndef SIMULATE
// NOTE: Use SB_IO DDR to help assure clock arrives a bit before signal
//       Also register the other signals.
SB_IO #(
          .PIN_TYPE(6'b010000)   // PIN_OUTPUT_DDR
      ) dvi_clk_sbio (
          .PACKAGE_PIN(P1B2),
          //        .CLOCK_ENABLE(1'b1),    // ICE Technology Library recommends leaving unconnected when always enabled to save a LUT
          .OUTPUT_CLK(pclk),
          .D_OUT_0(1'b0),                   // output on rising edge
          .D_OUT_1(1'b1)                    // output on falling edge
      );

SB_IO #(
          .PIN_TYPE(6'b010100)   // PIN_OUTPUT_REGISTERED
      ) dvi_signals_sbio [14: 0] (
          .PACKAGE_PIN({P1B9, P1B10, P1B4, P1A1, P1A7, P1A2, P1A8, P1A3, P1A9, P1A4, P1A10, P1B1, P1B7, P1B8, P1B3}),
          //        .CLOCK_ENABLE(1'b1),    // ICE Technology Library recommends leaving unconnected when always enabled to save a LUT
          .OUTPUT_CLK(pclk),
          .D_OUT_0({dvi_de, vga_vs, vga_hs, vga_r, vga_g, vga_b}),
          /* verilator lint_off PINCONNECTEMPTY */
          .D_OUT_1()
          /* verilator lint_on PINCONNECTEMPTY */
      );
`else
// Generic VGA mode (for simulation)
assign {P1A1, P1A2, P1A3, P1A4, P1A7, P1A8, P1A9, P1A10} =
       {vga_r[3], vga_r[1], vga_g[3], vga_g[1], vga_r[2], vga_r[0], vga_g[2], vga_g[0]};
assign {P1B1, P1B2, P1B3, P1B4, P1B7, P1B8, P1B9, P1B10} =
       {vga_b[3], pclk, vga_b[0], vga_hs, vga_b[2], vga_b[1], dvi_de, vga_vs};

`endif
`elsif PMOD_DIGILENT_VGA
// 12-bit VGA using dual-PMOD https://store.digilentinc.com/pmod-vga-video-graphics-array/
assign {P1A1, P1A2, P1A3, P1A4, P1A7, P1A8, P1A9, P1A10} =
       {vga_r[0], vga_r[1], vga_r[2], vga_r[3], vga_b[0], vga_b[1], vga_b[2], vga_b[3]};
assign {P1B1, P1B2, P1B3, P1B4, P1B7, P1B8, P1B9, P1B10} =
       {vga_g[0], vga_g[1], vga_g[2], vga_g[3], vga_hs, vga_vs, 1'bx, 1'bx};
`elsif PMOD_XESS_VGA
// 9-bit VGA using dual-PMOD http://www.xess.com/shop/product/stickit-vga/
assign {P1A1, P1A2, P1A3, P1A4, P1A7, P1A8, P1A9, P1A10} =
       {vga_vs, vga_g[3], vga_r[2], vga_b[2], vga_hs, vga_r[3], vga_b[3], vga_g[2]};
assign {P1B1, P1B2, P1B3, P1B4, P1B7, P1B8, P1B9, P1B10} =
       {1'b0, 1'b0, 1'b0, 1'b0, vga_r[1], vga_g[1], vga_b[1], 1'b0};
`elsif PMOD_XESS_VGA_SINGLE
// 6-bit VGA using single-PMOD http://www.xess.com/shop/product/stickit-vga/ (only PMOD-1B)
assign {P1B1, P1B2, P1B3, P1B4, P1B7, P1B8, P1B9, P1B10} =
       {vga_vs, vga_g[3], vga_r[2], vga_b[2], vga_hs, vga_r[3], vga_b[3], vga_g[2]};
`endif

// PLL to derive proper video frequency from 12MHz oscillator
logic pclk;                  // video pixel clock output from PLL block
logic pll_lock;              // indicates when PLL frequency has locked-on

`ifndef SIMULATE
/* verilator lint_off PINMISSING */
SB_PLL40_PAD
    #(
        .DIVR(PLL_DIVR),        // DIVR from video mode
        .DIVF(PLL_DIVF),        // DIVF from video mode
        .DIVQ(PLL_DIVQ),        // DIVQ from video mode
        .FEEDBACK_PATH("SIMPLE"),
        .FILTER_RANGE(3'b001),
        .PLLOUT_SELECT("GENCLK")
    )
    pll_inst (
        .LOCK(pll_lock),        // signal indicates PLL lock
        .RESETB(1'b1),
        .BYPASS(1'b0),
        .PACKAGEPIN(CLK),       // input reference clock
        .PLLOUTGLOBAL(pclk)     // PLL output clock (via global buffer)
    );
/* verilator lint_on PINMISSING */
`else
// for simulation use 1:1 input clock (and testbench can simulate proper frequency)
assign pll_lock = 1'b1;
assign pclk = CLK;
`endif

// reset logic waits for PLL lock & reset button released (with small delay)
logic [7:0] reset_cnt;      // counter for reset delay (assures memories ready)
logic reset = 1'b1;

always_ff @(posedge pclk) begin
    // reset count and stay in reset if pll_lock lost or bus_nreset
    if (!pll_lock || !nreset) begin
        reset_cnt   <= 0;
        reset      <= 1'b1;
    end
    else begin
        if (!&reset_cnt) begin
            reset_cnt   <= reset_cnt + 1;
            reset      <= 1'b1;
        end
        else begin
            reset      <= 1'b0;
        end
    end
end

// xosera main module
xosera_main xosera_main(
                .clk(pclk),
                .red_o(vga_r),
                .green_o(vga_g),
                .blue_o(vga_b),
                .vsync_o(vga_vs),
                .hsync_o(vga_hs),
                .visible_o(dvi_de),
                .bus_cs_n_i(bus_cs_n),
                .bus_rd_nwr_i(bus_rd_nwr),
                .bus_reg_num_i(bus_reg_num),
                .bus_bytesel_i(bus_bytesel),
                .bus_data_i(bus_data_in),
                .bus_data_o(bus_data_out),
                .audio_l_o(audio_l),
                .audio_r_o(audio_r),
                .reset_i(reset)
            );
endmodule
