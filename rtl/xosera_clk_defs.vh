// xosera_defs.vh
//
// vim: set et ts=4 sw=4
//
// Copyright (c) 2020 Xark - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)
//

`ifdef MODE_640x400     // 25.175 MHz (requested), 25.125 MHz (achieved)
`elsif MODE_640x480     // 25.175 MHz (requested), 25.125 MHz (achieved)
`elsif MODE_720x400     // 28.322 MHz (requested), 28.500 MHz (achieved)
`elsif MODE_848x480     // 33.750 MHz (requested), 33.750 MHz (achieved)
`elsif MODE_800x600     // 40.000 MHz (requested), 39.750 MHz (achieved) [tight timing]
`elsif MODE_1024x768    // 65.000 MHz (requested), 65.250 MHz (achieved) [fails timing]
`elsif MODE_1280x720    // 74.176 MHz (requested), 73.500 MHz (achieved) [fails timing]
`else
`define MODE_640x480    // default
`endif

`ifdef    MODE_640x400    // 25.175 MHz (requested), 25.125 MHz (achieved)
parameter PIXEL_FREQ        = 25_175_000;   // pixel clock in Hz
`elsif    MODE_640x480    // 25.175 MHz (requested), 25.125 MHz (achieved)
parameter PIXEL_FREQ        = 25_175_000;   // pixel clock in Hz
`elsif    MODE_720x400    // 28.322 MHz (requested), 28.500 MHz (achieved)
parameter PIXEL_FREQ        = 28_322_000;   // pixel clock in Hz
`elsif    MODE_848x480    // 33.750 MHz (requested), 33.750 MHz (achieved)
parameter PIXEL_FREQ        = 33_750_000;   // pixel clock in Hz
`elsif    MODE_800x600    // 40.000 MHz (requested), 39.750 MHz (achieved) [tight timing]
parameter PIXEL_FREQ        = 40_000_000;   // pixel clock in Hz
`elsif MODE_1024x768    // 65.000 MHz (requested), 65.250 MHz (achieved) [fails timing]
parameter PIXEL_FREQ        = 65_000_000;   // pixel clock in Hz
`elsif MODE_1280x720    // 74.176 MHz (requested), 73.500 MHz (achieved) [fails timing]
parameter PIXEL_FREQ        = 74_250_000;   // pixel clock in Hz
`endif

`ifdef ICE40UP5K    // iCE40UltraPlus5K specific
// Lattice/SiliconBlue PLL "magic numbers" to derive pixel clock from 12Mhz oscillator (from "icepll" utility)
`ifdef    MODE_640x400    // 25.175 MHz (requested), 25.125 MHz (achieved)

parameter PLL_DIVR    =    4'b0000;     // DIVR =  0
parameter PLL_DIVF    =    7'b1000010;  // DIVF = 66
parameter PLL_DIVQ    =    3'b101;      // DIVQ =  5
`elsif    MODE_640x480    // 25.175 MHz (requested), 25.125 MHz (achieved)
parameter PLL_DIVR    =    4'b0000;     // DIVR =  0
parameter PLL_DIVF    =    7'b1000010;  // DIVF = 66
parameter PLL_DIVQ    =    3'b101;      // DIVQ =  5
`elsif    MODE_720x400    // 28.322 MHz (requested), 28.500 MHz (achieved)
parameter PLL_DIVR    =    4'b0000;     // DIVR =  0
parameter PLL_DIVF    =    7'b1001011;  // DIVF = 75
parameter PLL_DIVQ    =    3'b101;      // DIVQ =  5
`elsif    MODE_848x480    // 33.750 MHz (requested), 33.750 MHz (achieved)
parameter PLL_DIVR    =    4'b0000;     // DIVR =  0
parameter PLL_DIVF    =    7'b0101100;  // DIVF = 44
parameter PLL_DIVQ    =    3'b100;      // DIVQ =  4
`elsif    MODE_800x600    // 40.000 MHz (requested), 39.750 MHz (achieved) [tight timing]
parameter PLL_DIVR    =    4'b0000;     // DIVR =  0
parameter PLL_DIVF    =    7'b0110100;  // DIVF = 52
parameter PLL_DIVQ    =    3'b100;      // DIVQ =  4
`elsif MODE_1024x768    // 65.000 MHz (requested), 65.250 MHz (achieved) [fails timing]
parameter PLL_DIVR    =    4'b0000;     // DIVR =  0
parameter PLL_DIVF    =    7'b1010110;  // DIVF = 86
parameter PLL_DIVQ    =    3'b100;      // DIVQ =  4
`elsif MODE_1280x720    // 74.176 MHz (requested), 73.500 MHz (achieved) [fails timing]
parameter PLL_DIVR    =    4'b0000;     // DIVR =  0
parameter PLL_DIVF    =    7'b0110000;  // DIVF = 48
parameter PLL_DIVQ    =    3'b011;      // DIVQ =  3
`endif
`endif
