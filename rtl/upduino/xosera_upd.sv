// xosera_upd.sv - Top module for UPduino v3.0 Xosera
//
// vim: set et ts=4 sw=4
//
// Copyright (c) 2020 Xark - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)
//
// For info about Updino v3.0: https://github.com/tinyvision-ai-inc/UPduino-v3.0
// It should be here on Tindie soon: https://www.tindie.com/stores/tinyvision_ai/
//
// NOTE: UPduino 3.x needs the "OSC" jumper shorted to provide 12MHz clock to drive PLL

`default_nettype none               // mandatory for Verilog sanity
`timescale 1ns/1ps                  // mandatory to shut up Icarus Verilog

`include "xosera_pkg.sv"

//                         UPduino v3.0 pinout for Xosera
//
//             [Xosera]       PCF  Pin#  _____  Pin#  PCF       [Xosera]
//                                ------| USB |------
//                          <GND> |  1   \___/   48 | spi_ssn   (16)
//                          <VIO> |  2           47 | spi_sck   (15)
//        [BUS_RESET_N]     <RST> |  3           46 | spi_mosi  (17)
//                         <DONE> |  4           45 | spi_miso  (14)
//           [BUS_CS_N]   led_red |  5           44 | gpio_20   <N/A w/OSC>
//         [BUS_RD_NWR] led_green |  6     U     43 | gpio_10   <INTERRUPT>
//        [BUS_BYTESEL]  led_blue |  7     P     42 | <GND>     <silkscreen errata>
//                          <+5V> |  8     D     41 | <12 MHz>  <silkscreen errata>
//                        <+3.3V> |  9     U     40 | gpio_12   [VGA_HS]
//                          <GND> | 10     I     39 | gpio_21   [VGA_VS]
//       [BUS_REG_NUM0]   gpio_23 | 11     N     38 | gpio_13   [VGA_R3]
//       [BUS_REG_NUM1]   gpio_25 | 12     O     37 | gpio_19   [VGA_G3]
//       [BUS_REG_NUM2]   gpio_26 | 13           36 | gpio_18   [VGA_B3]
//       [BUS_REG_NUM3]   gpio_27 | 14     V     35 | gpio_11   [VGA_R2]
//            [AUDIO_L]   gpio_32 | 15     3     34 | gpio_9    [VGA_G2]
// <out-only> [AUDIO_R]   gpio_35 | 16     .     33 | gpio_6    [VGA_B2]
//          [BUS_DATA0]   gpio_31 | 17     0     32 | gpio_44   [VGA_R1]
//          [BUS_DATA1]   gpio_37 | 18           31 | gpio_4    [VGA_G1]
//          [BUS_DATA2]   gpio_34 | 19           30 | gpio_3    [VGA_B1]
//          [BUS_DATA3]   gpio_43 | 20           29 | gpio_48   [VGA_R0]
//          [BUS_DATA4]   gpio_36 | 21           28 | gpio_45   [VGA_G0]
//          [BUS_DATA5]   gpio_42 | 22           27 | gpio_47   [VGA_B0]
//          [BUS_DATA6]   gpio_38 | 23           26 | gpio_46   [DV_DE]
//          [BUS_DATA7]   gpio_28 | 24           25 | gpio_2    [DV_CLK]
//                                -------------------
// NOTE: Xosera assumes 12MHz OSC jumper is shorted, and R28 RGB LED jumper is cut (using RGB for input)

module xosera_upd(
            // left side (USB at top)
            input  logic    led_red,        // m68k bus select (RGB red, UPduino 3.0 needs jumper R28 cut)
            input  logic    led_green,      // m68k bus read/not write (RGB green when output)
            input  logic    led_blue,       // m68k bus byte select (RGB blue when output)
            input  logic    gpio_23,        // m68k bus regnum 0
            input  logic    gpio_25,        // m68k bus regnum 1
            input  logic    gpio_26,        // m68k bus regnum 2
            input  logic    gpio_27,        // m68k bus regnum 3
            output logic    gpio_32,        // audio left output
            output logic    gpio_35,        // audio right output (NOTE: this gpio can't be input)
            inout  logic    gpio_31,        // m68k bus data 0
            inout  logic    gpio_37,        // m68k bus data 1
            inout  logic    gpio_34,        // m68k bus data 2
            inout  logic    gpio_43,        // m68k bus data 3
            inout  logic    gpio_36,        // m68k bus data 4
            inout  logic    gpio_42,        // m68k bus data 5
            inout  logic    gpio_38,        // m68k bus data 6
            inout  logic    gpio_28,        // m68k bus data 7
            // right side (USB at top)
            output logic    gpio_10,        // interrupt out (L->H edge)
            output logic    gpio_12,        // video hsync
            output logic    gpio_21,        // video vsync
            output logic    gpio_13,        // video R[3]
            output logic    gpio_19,        // video G[3]
            output logic    gpio_18,        // video B[3]
            output logic    gpio_11,        // video R[2]
            output logic    gpio_9,         // video G[2]
            output logic    gpio_6,         // video B[2]
            output logic    gpio_44,        // video R[1]
            output logic    gpio_4,         // video G[1]
            output logic    gpio_3,         // video B[1]
            output logic    gpio_48,        // video R[0]
            output logic    gpio_45,        // video G[0]
            output logic    gpio_47,        // video B[0]
            output logic    gpio_46,        // video enable for HDMI
            output logic    gpio_2,         // video clock for HDMI
            output logic    spi_cs,         // FPGA SPI flash CS (keep high unless using SPI flash)
            output logic    serial_txd,     // FPGA USB uart tx (shared with SPI flash MISO so keep CS high)
            input  logic    serial_rxd,     // FPGA USB uart rx (shared with SPI flash SCK so keep CS high)
            input  logic    gpio_20         // input 12MHz clock (UPduino 3.0 needs OSC jumper shorted)
       );

assign spi_cs = 1'b1;                   // prevent SPI flash interfering with other SPI/FTDI pins

// gpio pin aliases
logic       bus_cs_n;                   // bus select (active LOW)
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
logic       dv_de;                      // DV display enable
logic       bus_intr;                   // interrupt signal
logic       bus_intr_r;                 // registered signal, to improve timing
logic       reconfig;                   // set to 1 to force reconfigure of FPGA
logic       reconfig_r;                 // registered signal, to improve timing
logic [1:0] boot_select;                // two bit number for flash configuration to load on reconfigure
logic [1:0] boot_select_r;              // registered signal, to improve timing


// assign gpio pins to bus signals
assign bus_cs_n     = led_red;          // RGB red as active low select
assign bus_rd_nwr   = led_green;        // RGB green as read/not write
assign bus_bytesel  = led_blue;         // RGB blue for even/odd byte select
assign bus_reg_num  = { gpio_27, gpio_26, gpio_25, gpio_23 };   // gpio for register number
assign bus_data     = { gpio_28, gpio_38, gpio_42, gpio_36, gpio_43, gpio_34, gpio_37, gpio_31 };   // gpio for data bus

// assign audio output signals to gpio
assign gpio_32      = audio_l;          // left audio channel gpio
assign gpio_35      = audio_r;          // right audio channel gpio

assign gpio_10      = bus_intr_r;         // interrupt signal

// split tri-state data lines into in/out signals for inside FPGA
logic bus_out_ena;
logic [7:0] bus_data_out;               // bus out from Xosera
logic [7:0] bus_data_out_r;             // registered bus_data_out signal (experimental)
logic [7:0] bus_data_in;                // bus input to Xosera

// only set bus to output if Xosera is selected and read is selected
assign bus_out_ena  = (bus_cs_n == xv::CS_ENABLED && bus_rd_nwr == xv::RnW_READ);

`ifdef SYNTHESIS
// NOTE: Use iCE40 SB_IO primitive to control tri-state properly here
/* verilator lint_off PINMISSING */
SB_IO #(
    .PIN_TYPE(6'b101001)    //PIN_OUTPUT_TRISTATE|PIN_INPUT
) bus_tristate [7:0] (
    .PACKAGE_PIN(bus_data),
    .INPUT_CLK(pclk),
    .OUTPUT_CLK(pclk),
    //        .CLOCK_ENABLE(1'b1),    // ICE Technology Library recommends leaving unconnected when always enabled to save a LUT
    .OUTPUT_ENABLE(bus_out_ena),
    .D_OUT_0(bus_data_out_r),
    .D_IN_0(bus_data_in)
);
/* verilator lint_on PINMISSING */
`else
// NOTE: Using the registered ("_r") signal may be a win for <posedge pclk> -> async
//        timing on bus_data_out signals (but might cause issues?)
assign bus_data     = bus_out_ena ? bus_data_out_r  : 8'bZ;
assign bus_data_in  = bus_data;
`endif

// update registered signals each clock
always_ff @(posedge pclk) begin
    bus_data_out_r  <= bus_data_out;
    bus_intr_r      <= bus_intr;
    reconfig_r      <= reconfig;
    boot_select_r   <= boot_select;
end

// PLL to derive proper video frequency from 12MHz oscillator (gpio_20 with OSC jumper shorted)
logic pclk;                  // video pixel clock output from PLL block
logic pll_lock;              // indicates when PLL frequency has locked-on

`ifdef SYNTHESIS
/* verilator lint_off PINMISSING */
SB_PLL40_CORE #(
    .DIVR(xv::PLL_DIVR),        // DIVR from video mode
    .DIVF(xv::PLL_DIVF),        // DIVF from video mode
    .DIVQ(xv::PLL_DIVQ),        // DIVQ from video mode
    .FEEDBACK_PATH("SIMPLE"),
    .FILTER_RANGE(3'b001),
    .PLLOUT_SELECT("GENCLK")
) pll_inst(
    .LOCK(pll_lock),        // signal indicates PLL lock
    .RESETB(1'b1),
    .BYPASS(1'b0),
    .REFERENCECLK(gpio_20), // input reference clock
    .PLLOUTGLOBAL(pclk)     // PLL output clock (via global buffer)
);
/* verilator lint_on PINMISSING */

`else
// for simulation use 1:1 input clock (and testbench can simulate proper frequency)
assign pll_lock = 1'b1;
assign pclk     = gpio_20;
`endif

// video output signals
`ifdef SYNTHESIS
// DV PMOD mode (but still works great for VGA)
// NOTE: Use SB_IO DDR to help assure clock arrives a bit before signal
//       Also register the other signals.
SB_IO #(
    .PIN_TYPE(6'b010000)   // PIN_OUTPUT_DDR
) dv_clk_sbio(
`ifdef PMOD_DIGILENT_VGA
    //             CK*
    .PACKAGE_PIN(gpio_2),
`endif
`ifdef PMOD_MUSE_VGA
    //             CK*
    .PACKAGE_PIN(gpio_19),
`endif
`ifdef PMOD_1B2_DVI12
    //             CK
    .PACKAGE_PIN(gpio_4),
`endif
    //        .CLOCK_ENABLE(1'b1),    // ICE Technology Library recommends leaving unconnected when always enabled to save a LUT
    .OUTPUT_CLK(pclk),
    .D_OUT_0(1'b0),                   // output on rising edge
    .D_OUT_1(1'b1)                    // output on falling edge
);

SB_IO #(
    .PIN_TYPE(6'b010100)   // PIN_OUTPUT_REGISTERED
) dv_signals_sbio [14: 0](
`ifdef PMOD_DIGILENT_VGA
    //              DE*      VS       HS       R3       R2       R1       R0       G3       G2      G1        G0       B3       B2       B1       B0
    .PACKAGE_PIN({gpio_46, gpio_21, gpio_12, gpio_13, gpio_11, gpio_44, gpio_48, gpio_19, gpio_9, gpio_4,   gpio_45, gpio_18, gpio_6,  gpio_3,  gpio_47}),
`endif
`ifdef PMOD_MUSE_VGA
    //              DE*      VS       HS       R3       R2       R1       R0       G3       G2       G1       G0       B3       B2       B1       B0
    .PACKAGE_PIN({gpio_9,  gpio_4,  gpio_45, gpio_18, gpio_6,  gpio_3,  gpio_47, gpio_2,  gpio_46, gpio_21, gpio_12, gpio_13, gpio_11, gpio_44, gpio_48}),
`endif
`ifdef PMOD_1B2_DVI12
    //              DE       VS       HS       R3       R2       R1       R0       G3       G2       G1       G0       B3       B2       B1       B0
    .PACKAGE_PIN({gpio_46, gpio_2,  gpio_19, gpio_48, gpio_47, gpio_44, gpio_3,  gpio_11, gpio_6,  gpio_13, gpio_18, gpio_45, gpio_12, gpio_21, gpio_9}),
`endif
    //        .CLOCK_ENABLE(1'b1),    // ICE Technology Library recommends leaving unconnected when always enabled to save a LUT
    .OUTPUT_CLK(pclk),
    .D_OUT_0({dv_de, vga_vs, vga_hs, vga_r, vga_g, vga_b}),
    /* verilator lint_off PINCONNECTEMPTY */
    .D_OUT_1()
    /* verilator lint_on PINCONNECTEMPTY */
);
`else
// Generic VGA mode (for simulation)
assign { gpio_46,  gpio_12,  gpio_21,  gpio_13,  gpio_19,  gpio_18,  gpio_11,  gpio_9,   gpio_6   } =
       { dv_de,    vga_hs,   vga_vs,   vga_r[3], vga_g[3], vga_b[3], vga_r[2], vga_g[2], vga_b[2] };
assign { gpio_44,  gpio_4,   gpio_3,   gpio_48,  gpio_45,  gpio_47  } =
       { vga_r[1], vga_g[1], vga_b[1], vga_r[0], vga_g[0], vga_b[0] };

assign gpio_2   = pclk;    // output HDMI clk
`endif

`ifdef SYNTHESIS
SB_WARMBOOT boot(
    .BOOT(reconfig_r),
    .S0(boot_select_r[0]),
    .S1(boot_select_r[1])
);
`else
always @* begin
    if (reconfig_r) begin
        $display("XOSERA REBOOT: To flash config #0x%x", boot_select_r);
        $finish;
    end
end
`endif

// reset logic waits for PLL lock
logic reset;

always_ff @(posedge pclk) begin
    // reset if pll_lock lost
    if (!pll_lock) begin
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
    .serial_txd_o(serial_txd),
    .serial_rxd_i(serial_rxd),
    .reconfig_o(reconfig),
    .boot_select_o(boot_select),
    .reset_i(reset),
    .clk(pclk)
);
endmodule
