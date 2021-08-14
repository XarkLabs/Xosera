// xosera_iceb.sv - Top module for iCEBreaker Xosera
//
// vim: set et ts=4 sw=4
//
// Copyright (c) 2020 Xark - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)
//
// For info about iCEBreaker: https://1bitsquared.com/products/icebreaker
`default_nettype none               // mandatory for Verilog sanity
`timescale 1ns/1ps                  // mandatory to shut up Icarus Verilog

`include "xosera_pkg.sv"

`ifdef PMOD_1B2_DVI12
`elsif PMOD_DIGILENT_VGA
`elsif PMOD_XESS_VGA
`elsif PMOD_XESS_VGA_SINGLE
`else
`define PMOD_1B2_DVI12    // default
`endif

module xosera_iceb(
        input  logic BTN_N,                         // iCEBreaker uButton (active LOW reset)
        output logic LEDG_N,                        // iCEBreaker board green status LED
        input  logic RX,                            // UART receive
        output logic TX,                            // UART transmit
`ifndef SPI_INTERFACE   // use all pins for bus
        input  logic LED_RED_N,                     // BUS_CS_N (open-collector LED used as input)
        input  logic LED_GRN_N,                     // BUS_RD_nWR (open-collector LED used as input)
        input  logic LED_BLU_N,                     // BUS_BYTESEL (open-collector LED used as input)
        input  logic FLASH_IO0,                     // BUS_REG_NUM0
        input  logic FLASH_IO1,                     // BUS_REG_NUM1
        input  logic FLASH_IO2,                     // BUS_REG_NUM2
        input  logic FLASH_IO3,                     // BUS_REG_NUM3
	    output wire  FLASH_SCK,					    // AUDIO_L
        output logic LEDR_N,                        // iCEBreaker board red status LED
`else                   // emulate bus signals via SPI peripheral
	    input  wire  FLASH_SCK,					    // SPI SCK
	    input  wire  FLASH_IO0,					    // SPI COPI
	    output wire  FLASH_IO1,					    // SPI CIPO
        output logic FLASH_IO2,                     // AUDIO_L
        output logic FLASH_IO3,                     // AUDIO_R
        input  logic LEDR_N,                        // RED LED, but as input FTDI SPI SS (aka FTDI GPIO CTS)
`endif
        output logic P1A1, P1A2, P1A3, P1A4, P1A7, P1A8, P1A9, P1A10,   // PMOD 1A
        output logic P1B1, P1B2, P1B3, P1B4, P1B7, P1B8, P1B9, P1B10,   // PMOD 1B
`ifdef SPI_INTERFACE
// DBUG        output logic P2_1, P2_2, P2_3, P2_4, P2_7, P2_8, P2_9, P2_10,   // PMOD 2 (8-bit bi-dir data bus)
`else
        inout  logic P2_1, P2_2, P2_3, P2_4, P2_7, P2_8, P2_9, P2_10,   // PMOD 2 (8-bit bi-dir data bus)
`endif
        output logic FLASH_SSB,                     // SPI flash CS (drive high unless using SPI flash)
        input  logic CLK                            // 12Mhz clock
    );

assign FLASH_SSB    = 1'b1;             // prevent SPI flash interfering with other SPI/FTDI pins
assign LEDG_N       = reset;            // green LED on when not in reset (active LOW LED)
assign TX = RX;                         // loopback serial

// gpio pin aliases
logic       nreset;                     // user button as reset (active LOW button)
logic       bus_cs_n;                   // bus select (active LOW)
logic       bus_rd_nwr;                 // bus read not write (write LOW, read HIGH)
logic       bus_bytesel;                // bus even/odd byte select (even LOW, odd HIGH)
logic [3:0] bus_reg_num;                // bus 4-bit register index number (16-bit registers)
logic [7:0] bus_data;                   // bus 8-bit bidirectional data I/O
logic       audio_l;                    // left audio PWM
logic       audio_r;                    // right audio PWM
logic       bus_intr;                   // bus interrupt signal output (not used)
logic [3:0] vga_r;                      // vga red (4-bit)
logic [3:0] vga_g;                      // vga green (4-bits)
logic [3:0] vga_b;                      // vga blue (4-bits)
logic       vga_hs;                     // vga hsync
logic       vga_vs;                     // vga vsync
logic       dv_de;                      // DV display enable
`ifdef SPI_INTERFACE    // SPI target interface (FPGA is the peripheral)
logic       spi_sck;                    // SPI clock from controller
logic       spi_copi;                   // SPI controller out/peripheral in
logic       spi_cipo;                   // SPI controller in/peripheral out
logic       spi_cs_n;                   // SPI CS for FPGA from controller
`endif

logic       spi_reset   = 1'b0;         // SPI "soft" reset (if SPI_INTERFACE)
assign      nreset      = BTN_N;        // active LOW reset button

// split tri-state data lines into in/out signals for inside FPGA
logic bus_out_ena;
logic [7:0] bus_data_out;
logic [7:0] bus_data_in;
// only set bus to output if CS enabled and read
assign bus_out_ena = (bus_cs_n == xv::cs_ENABLED && bus_rd_nwr == xv::RnW_READ);

`ifdef SPI_INTERFACE   // SPU interface to drive bus signals
    // assign SPI GPIO for SPI interface (to emulate bus interface)
    assign spi_cs_n     = LEDR_N;
    assign spi_sck      = FLASH_SCK;
    assign spi_copi     = FLASH_IO0;
    assign FLASH_IO1    = spi_cipo;
    assign FLASH_IO2    = spi_receive_strobe;   // TODO audio_l;
    assign FLASH_IO3    = bus_cs_n; // TODO audio_r;
`else   // direct bus interface
    // tri-state data bus unless Xosera is both selected and bus is reading
    `ifdef SYNTHESIS
        // NOTE: Need to use iCE40 SB_IO primitive to control tri-state properly here
        SB_IO #(
            .PIN_TYPE(6'b101001)
        ) bus_tristate [7:0] (
            .PACKAGE_PIN(bus_data),
            //        .CLOCK_ENABLE(1'b1),    // ICE Technology Library recommends leaving unconnected when always enabled to save a LUT
            .OUTPUT_ENABLE(bus_out_ena),
            .D_OUT_0(bus_data_out),
            .D_IN_0(bus_data_in)
        );
    `else   // !SYNTHESIS
        // Use normal tristate in simulation
        assign bus_data     = bus_out_ena ? bus_data_out : 8'bZ;
        assign bus_data_in  = bus_data;
    `endif
    assign bus_cs_n     = LED_RED_N;        // RGB LED red as Xosera select=cs_ENABLED (UP_nCS)
    assign bus_rd_nwr   = LED_GRN_N;        // RGB LED green as RnW_WRITE=0, RnW_READ=1, read= (UP_RnW)
    assign bus_bytesel  = LED_BLU_N;        // RGB LED blue for word byte select (UP_bytesel)
    assign bus_reg_num  = { FLASH_IO3, FLASH_IO2, FLASH_IO1, FLASH_IO0 };       // gpio for register number (UP_R0-UP_R3)
    assign bus_data     = { P2_1, P2_2, P2_3, P2_4, P2_7, P2_8, P2_9, P2_10 };  // gpio for data bus
    assign FLASH_SCK    = audio_l;
    assign LEDR_N       = bus_cs_n;         // show bus activity on red LED
`endif

// video output signals
`ifdef PMOD_1B2_DVI12
// 12-bit DVI using dual-PMOD https://1bitsquared.com/products/pmod-digital-video-interface
`ifdef SYNTHESIS
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
          .D_OUT_0({dv_de, vga_vs, vga_hs, vga_r, vga_g, vga_b}),
          /* verilator lint_off PINCONNECTEMPTY */
          .D_OUT_1()
          /* verilator lint_on PINCONNECTEMPTY */
      );
`else
// Generic VGA mode (for simulation)
assign {P1A1, P1A2, P1A3, P1A4, P1A7, P1A8, P1A9, P1A10} =
       {vga_r[3], vga_r[1], vga_g[3], vga_g[1], vga_r[2], vga_r[0], vga_g[2], vga_g[0]};
assign {P1B1, P1B2, P1B3, P1B4, P1B7, P1B8, P1B9, P1B10} =
       {vga_b[3], pclk, vga_b[0], vga_hs, vga_b[2], vga_b[1], dv_de, vga_vs};

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

`ifdef SYNTHESIS
/* verilator lint_off PINMISSING */
SB_PLL40_PAD
    #(
        .DIVR(xv::PLL_DIVR),        // DIVR from video mode
        .DIVF(xv::PLL_DIVF),        // DIVF from video mode
        .DIVQ(xv::PLL_DIVQ),        // DIVQ from video mode
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
assign pclk     = CLK;
`endif

logic           reconfig;                   // set to 1 to force reconfigure of FPGA
logic   [1:0]   boot_select;                // two bit number for flash configuration to load on reconfigure

`ifdef SYNTHESIS
SB_WARMBOOT boot(
                .BOOT(reconfig),
                .S0(boot_select[0]),
                .S1(boot_select[1])
            );
`else
always @* begin
    if (reconfig) begin
        $display("XOSERA REBOOT: To flash config #0x%x", boot_select);
        $finish;
    end
end
`endif

// reset logic waits for PLL lock & reset button released (with small delay)
logic reset = 1'b1;         // default in reset state

always_ff @(posedge pclk) begin
    // reset if pll_lock lost, or reset button or SPI reset
    if (!pll_lock || !nreset || spi_reset) begin
        reset       <= 1'b1;
    end
    else begin
        reset       <= 1'b0;
    end
end

// xosera main module
xosera_main xosera_main(
            .red_o(vga_r),
            .green_o(vga_g),
            .blue_o(vga_b),
            .bus_intr_o(bus_intr),
            .vsync_o(vga_vs),
            .hsync_o(vga_hs),
            .dv_de_o(dv_de),
            .bus_cs_n_i(bus_cs_n),
            .bus_rd_nwr_i(bus_rd_nwr),
            .bus_reg_num_i(bus_reg_num),
            .bus_bytesel_i(bus_bytesel),
            .bus_data_i(bus_data_in),
            .bus_data_o(bus_data_out),
            .audio_l_o(audio_l),
            .audio_r_o(audio_r),
            .reconfig_o(reconfig),
            .boot_select_o(boot_select),
            .reset_i(reset),
            .clk(pclk)
);
`ifdef SPI_INTERFACE
// operate Xosera bus interface via SPI commands

logic   spi_select;
logic   spi_receive_strobe;
logic   spi_transmit_strobe;
logic   [7:0] spi_receive_data;
logic   [7:0] spi_transmit_data;

spi_target  spi_target(
            .spi_sck_i(spi_sck),
            .spi_copi_i(spi_copi),
            .spi_cipo_o(spi_cipo),
            .spi_cs_i(spi_cs_n),
            .select_o(spi_select),
            .receive_strobe_o(spi_receive_strobe),
            .receive_byte_o(spi_receive_data),
            .transmit_strobe_o(spi_transmit_strobe),
            .transmit_byte_i(spi_transmit_data),
            .reset_i(reset),
            .clk(pclk)
);
// SPI cmd byte (all active HIGH):
//  7  6  5  4  3  2  1  0
// CS WR RS BS R3 R2 R1 R0
logic [7:0] spi_cmd_byte        = 8'h00;
logic [7:0] spi_data_byte       = 8'h00;
logic       spi_payload_byte    = 1'b0;   // true on 2nd byte (payload byte) of packet
logic       spi_cs_hold0        = 1'b0;   // saved CS from spi_cmd_byte (held for two cycles)
logic       spi_cs_hold1        = 1'b0;   // saved CS from spi_cmd_byte (held for two cycles)
logic       spi_rs_hold0        = 1'b0;   // saved CS from spi_cmd_byte (held for two cycles)
logic       spi_rs_hold1        = 1'b0;   // saved CS from spi_cmd_byte (held for two cycles)

assign bus_cs_n             = ~spi_cs_hold0;                            // CS bit
assign bus_rd_nwr           = ~spi_cmd_byte[6];                         // WR bit
assign bus_bytesel          = spi_cmd_byte[4];                          // BS bit
assign spi_reset            = spi_cmd_byte[5];                          // RS bit
assign bus_reg_num          = spi_cmd_byte[3:0];                        // register bits
assign bus_data_in          = spi_data_byte;                            // bus data to write
assign spi_transmit_data    = spi_payload_byte ? bus_data_out : 8'hCB;  // bus data to read

// DBUG assign { P2_1, P2_2, P2_3, P2_4, P2_7, P2_8, P2_9, P2_10 } = spi_cmd_byte;  // TODO debug

always_ff @(posedge pclk) begin
    spi_cs_hold0        <= spi_cs_hold1;                 // clear held CS
    spi_cs_hold1        <= 1'b0;                        // clear held CS
    spi_cmd_byte[5]     <= 1'b0;                        // clear RS bit
    if (!spi_select) begin                              // if SPI de-selected
        spi_payload_byte    <= 1'b0;                    // next byte is command byte
    end
    if (spi_receive_strobe) begin                       // if an SPI byte received
        if (!spi_payload_byte) begin                    // if not a payload byte (aka is a command byte)
            spi_cmd_byte        <= spi_receive_data;    // save command byte
            spi_payload_byte    <= 1'b1;
        end
        else begin                                      // else payload byte
            spi_data_byte       <= spi_receive_data;    // put data byte on bus
            spi_cs_hold0        <= spi_cmd_byte[7];     // hold CS for next cycle
            spi_cs_hold1        <= spi_cmd_byte[7];     // hold CS for next cycle
            spi_payload_byte    <= 1'b0;                // next byte is command byte
        end
    end
end

`endif

endmodule
