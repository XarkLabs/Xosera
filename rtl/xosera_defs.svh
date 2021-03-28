// xosera_defs.svh
//
// vim: set et ts=4 sw=4
//
// Copyright (c) 2020 Xark - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)
//

`ifdef MODE_640x400    // 25.175 MHz (requested), 25.125 MHz (achieved)
`elsif MODE_640x480    // 25.175 MHz (requested), 25.125 MHz (achieved)
`elsif MODE_720x400    // 28.322 MHz (requested), 28.500 MHz (achieved)
`elsif MODE_848x480    // 33.750 MHz (requested), 33.750 MHz (achieved)
`elsif MODE_800x600    // 40.000 MHz (requested), 39.750 MHz (achieved) [tight timing]
`elsif MODE_1024x768    // 65.000 MHz (requested), 65.250 MHz (achieved) [fails timing]
`elsif MODE_1280x720    // 74.176 MHz (requested), 73.500 MHz (achieved) [fails timing]
`else
`define MODE_640x480    // default
`endif

`ifdef    MODE_640x400
// VGA mode 640x480 @ 60Hz (pixel clock 25.175Mhz)
parameter VISIBLE_WIDTH    = 640;                            // horizontal active pixels
parameter VISIBLE_HEIGHT    = 400;                            // vertical active lines
parameter H_FRONT_PORCH    = 16;                            // H pre-sync (front porch) pixels
parameter H_SYNC_PULSE        = 96;                            // H sync pulse pixels
parameter H_BACK_PORCH        = 48;                            // H post-sync (back porch) pixels
parameter V_FRONT_PORCH    = 12;                            // V pre-sync (front porch) lines
parameter V_SYNC_PULSE        = 2;                            // V sync pulse lines
parameter V_BACK_PORCH        = 35;                            // V post-sync (back porch) lines
parameter H_SYNC_POLARITY    = 1'b0;                            // H sync pulse active level
parameter V_SYNC_POLARITY    = 1'b1;                            // V sync pulse active level

`elsif    MODE_640x480
// VGA mode 640x480 @ 60Hz (pixel clock 25.175Mhz)
parameter VISIBLE_WIDTH    = 640;                            // horizontal active pixels
parameter VISIBLE_HEIGHT    = 480;                            // vertical active lines
parameter H_FRONT_PORCH    = 16;                            // H pre-sync (front porch) pixels
parameter H_SYNC_PULSE        = 96;                            // H sync pulse pixels
parameter H_BACK_PORCH        = 48;                            // H post-sync (back porch) pixels
parameter V_FRONT_PORCH    = 10;                            // V pre-sync (front porch) lines
parameter V_SYNC_PULSE        = 2;                            // V sync pulse lines
parameter V_BACK_PORCH        = 33;                            // V post-sync (back porch) lines
parameter H_SYNC_POLARITY    = 1'b0;                            // H sync pulse active level
parameter V_SYNC_POLARITY    = 1'b0;                            // V sync pulse active level

`elsif    MODE_720x400
// VGA mode 720x400 @ 70Hz (pixel clock 28.322Mhz)
parameter VISIBLE_WIDTH    = 720;                            // horizontal active pixels
parameter VISIBLE_HEIGHT    = 400;                            // vertical active lines
parameter H_FRONT_PORCH    = 18;                            // H pre-sync (front porch) pixels
parameter H_SYNC_PULSE        = 108;                            // H sync pulse pixels
parameter H_BACK_PORCH        = 54;                            // H post-sync (back porch) pixels
parameter V_FRONT_PORCH    = 12;                            // V pre-sync (front porch) lines
parameter V_SYNC_PULSE        = 2;                            // V sync pulse lines
parameter V_BACK_PORCH        = 35;                            // V post-sync (back porch) lines
parameter H_SYNC_POLARITY    = 1'b0;                            // H sync pulse active level
parameter V_SYNC_POLARITY    = 1'b1;                            // V sync pulse active level

`elsif    MODE_848x480
// VGA mode 848x480 @ 60Hz (pixel clock 33.750Mhz)
parameter VISIBLE_WIDTH    = 848;                            // horizontal active pixels
parameter VISIBLE_HEIGHT    = 480;                            // vertical active lines
parameter H_FRONT_PORCH    = 16;                            // H pre-sync (front porch) pixels
parameter H_SYNC_PULSE        = 112;                            // H sync pulse pixels
parameter H_BACK_PORCH        = 112;                            // H post-sync (back porch) pixels
parameter V_FRONT_PORCH    = 6;                            // V pre-sync (front porch) lines
parameter V_SYNC_PULSE        = 8;                            // V sync pulse lines
parameter V_BACK_PORCH        = 23;                            // V post-sync (back porch) lines
parameter H_SYNC_POLARITY    = 1'b1;                            // H sync pulse active level
parameter V_SYNC_POLARITY    = 1'b1;                            // V sync pulse active level

`elsif    MODE_800x600
// VGA mode 800x600 @ 60Hz (pixel clock 40.000Mhz)
parameter VISIBLE_WIDTH    = 800;                            // horizontal active pixels
parameter VISIBLE_HEIGHT    = 600;                            // vertical active lines
parameter H_FRONT_PORCH    = 40;                            // H pre-sync (front porch) pixels
parameter H_SYNC_PULSE        = 128;                            // H sync pulse pixels
parameter H_BACK_PORCH        = 88;                            // H post-sync (back porch) pixels
parameter V_FRONT_PORCH    = 1;                            // V pre-sync (front porch) lines
parameter V_SYNC_PULSE        = 4;                            // V sync pulse lines
parameter V_BACK_PORCH        = 23;                            // V post-sync (back porch) lines
parameter H_SYNC_POLARITY    = 1'b1;                            // H sync pulse active level
parameter V_SYNC_POLARITY    = 1'b1;                            // V sync pulse active level

`elsif    MODE_1024x768
// VGA mode 1024x768 @ 60Hz (pixel clock 65.000Mhz)
parameter VISIBLE_WIDTH    = 1024;                            // horizontal active pixels
parameter VISIBLE_HEIGHT    = 768;                            // vertical active lines
parameter H_FRONT_PORCH    = 24;                            // H pre-sync (front porch) pixels
parameter H_SYNC_PULSE        = 136;                            // H sync pulse pixels
parameter H_BACK_PORCH        = 160;                            // H post-sync (back porch) pixels
parameter V_FRONT_PORCH    = 3;                            // V pre-sync (front porch) lines
parameter V_SYNC_PULSE        = 6;                            // V sync pulse lines
parameter V_BACK_PORCH        = 29;                            // V post-sync (back porch) lines
parameter H_SYNC_POLARITY    = 1'b0;                            // H sync pulse active level
parameter V_SYNC_POLARITY    = 1'b0;                            // V sync pulse active level

`elsif    MODE_1280x720
// VGA mode 1280x720 @ 60Hz (pixel clock 74.250Mhz)
parameter VISIBLE_WIDTH    = 1280;                            // horizontal active pixels
parameter VISIBLE_HEIGHT    = 720;                            // vertical active lines
parameter H_FRONT_PORCH    = 110;                            // H pre-sync (front porch) pixels
parameter H_SYNC_PULSE        = 40;                            // H sync pulse pixels
parameter H_BACK_PORCH        = 220;                            // H post-sync (back porch) pixels
parameter V_FRONT_PORCH    = 5;                            // V pre-sync (front porch) lines
parameter V_SYNC_PULSE        = 5;                            // V sync pulse lines
parameter V_BACK_PORCH        = 20;                            // V post-sync (back porch) lines
parameter H_SYNC_POLARITY    = 1'b1;                            // H sync pulse active level
parameter V_SYNC_POLARITY    = 1'b1;                            // V sync pulse active level

`endif

// calculated video mode parametereters
parameter TOTAL_WIDTH        = H_FRONT_PORCH + H_SYNC_PULSE + H_BACK_PORCH + VISIBLE_WIDTH;
parameter TOTAL_HEIGHT        = V_FRONT_PORCH + V_SYNC_PULSE + V_BACK_PORCH + VISIBLE_HEIGHT;
parameter OFFSCREEN_WIDTH    = TOTAL_WIDTH - VISIBLE_WIDTH;
parameter OFFSCREEN_HEIGHT    = TOTAL_HEIGHT - VISIBLE_HEIGHT;

// character font related constants
parameter FONT_WIDTH       = 8;                            // 8 pixels wide character tiles (1 byte)
parameter FONT_HEIGHT        = 16;                            // up to 16 pixels high character tiles
parameter FONT_CHARS        = 256;                            // number of character tiles per font
parameter CHARS_WIDE        = (VISIBLE_WIDTH/FONT_WIDTH);
parameter CHARS_HIGH        = (VISIBLE_HEIGHT/FONT_HEIGHT);
parameter FONT_SIZE        = (FONT_CHARS * FONT_HEIGHT);    // bytes per font (up to 8x16 character tiles)

//`endif    // `ifndef XOSERA_DEFS_VH
