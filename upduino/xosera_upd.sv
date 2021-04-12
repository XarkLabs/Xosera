// xosera_upd.sv - Top module for Upduino v3.0 Xosera
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
// NOTE: Upduino 3.x needs the "OSC" jumper shorted to provide 12MHz clock to drive PLL

`default_nettype none   // mandatory for Verilog sanity

module xosera_upd(
            // left side (USB at top)
            input  logic    led_red,        // m68k bus select (RGB red, Upduino 3.0 needs jumper R28 cut)
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
            input  logic    gpio_20         // input 12MHz clock (Upduino 3.0 needs OSC jumper shorted)
       );

`include "../rtl/xosera_clk_defs.svh"       // Xosera global clock definitions
`include "../rtl/xosera_defs.svh"           // Xosera global definitions

assign spi_cs = 1'b1;                   // prevent SPI flash interfering with other SPI/FTDI pins

// gpio pin aliases
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
logic       dv_de;                      // DV display enable

// assign gpio pins to bus signals
assign bus_cs_n     = led_red;          // RGB red as active low select
assign bus_rd_nwr   = led_green;        // RGB green as read/not write
assign bus_bytesel  = led_blue;         // RGB blue for even/odd byte select
assign bus_reg_num  = { gpio_27, gpio_26, gpio_25, gpio_23 };   // gpio for register number
assign bus_data     = { gpio_28, gpio_38, gpio_42, gpio_36, gpio_43, gpio_34, gpio_37, gpio_31 };   // gpio for data bus

// assign audio output signals to gpio
assign gpio_32      = audio_l;           // left audio channel gpio
assign gpio_35      = audio_r;           // right audio channel gpio

// split tri-state data lines into in/out signals for inside FPGA
logic bus_out_ena;
logic [7:0] bus_data_out;
logic [7:0] bus_data_in;

// only set bus to output if Xosera is selected and read is selected
assign bus_out_ena = (bus_cs_n == cs_ENABLED && bus_rd_nwr == RnW_READ);

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
`else
assign bus_data     = bus_out_ena ? bus_data_out : 8'bZ;
assign bus_data_in  = bus_data;
`endif

// video output signals
`ifdef SYNTHESIS
// DV PMOD mode (but still works great for VGA)
// NOTE: Use SB_IO DDR to help assure clock arrives a bit before signal
//       Also register the other signals.
SB_IO #(
          .PIN_TYPE(6'b010000)   // PIN_OUTPUT_DDR
      ) dv_clk_sbio (
          .PACKAGE_PIN(gpio_2),
          //        .CLOCK_ENABLE(1'b1),    // ICE Technology Library recommends leaving unconnected when always enabled to save a LUT
          .OUTPUT_CLK(pclk),
          .D_OUT_0(1'b0),                   // output on rising edge
          .D_OUT_1(1'b1)                    // output on falling edge
      );

SB_IO #(
          .PIN_TYPE(6'b010100)   // PIN_OUTPUT_REGISTERED
      ) dv_signals_sbio [14: 0] (
          .PACKAGE_PIN({gpio_46, gpio_21, gpio_12, gpio_13, gpio_11, gpio_44, gpio_48, gpio_19, gpio_9, gpio_4, gpio_45, gpio_18, gpio_6, gpio_3, gpio_47}),
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

// PLL to derive proper video frequency from 12MHz oscillator (gpio_20 with OSC jumper shorted)
logic pclk;                  // video pixel clock output from PLL block
logic pll_lock;              // indicates when PLL frequency has locked-on

`ifdef SYNTHESIS
/* verilator lint_off PINMISSING */
SB_PLL40_CORE
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
        .REFERENCECLK(gpio_20), // input reference clock
        .PLLOUTGLOBAL(pclk)     // PLL output clock (via global buffer)
    );
/* verilator lint_on PINMISSING */

`else
// for simulation use 1:1 input clock (and testbench can simulate proper frequency)
assign pll_lock = 1'b1;
assign pclk = gpio_20;
`endif

// reset logic waits for PLL lock & reset button released (with small delay)
logic [7:0] reset_cnt;      // counter for reset delay (assures memories ready)
logic reset = 1'b1;         // default in reset state

always_ff @(posedge pclk) begin
    // reset count and stay in reset if pll_lock lost or bus_nreset
    if (!pll_lock) begin
        reset_cnt   <= 0;
        reset       <= 1'b1;
    end
    else begin
        if (!&reset_cnt) begin
            reset_cnt   <= reset_cnt + 1;
            reset       <= 1'b1;
        end
        else begin
            reset       <= 1'b0;
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
                .dv_de_o(dv_de),
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
