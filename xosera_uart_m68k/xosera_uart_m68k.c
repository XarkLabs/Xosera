/*
 * vim: set et ts=4 sw=4
 *------------------------------------------------------------
 *                                  ___ ___ _
 *  ___ ___ ___ ___ ___       _____|  _| . | |_
 * |  _| . |_ -|  _| . |     |     | . | . | '_|
 * |_| |___|___|___|___|_____|_|_|_|___|___|_,_|
 *                     |_____|
 * ------------------------------------------------------------
 * Portions Copyright (c) 2021 Ross Bamford
 * Portions Copyright (c) 2021 Xark
 * MIT License
 *
 * Test and tech-demo for Xosera FPGA "graphics card"
 * Crop test with copper.
 * ------------------------------------------------------------
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <basicio.h>
#include <machine.h>
#include <sdfat.h>


#include "xosera_m68k_api.h"

const char blurb[] =
    "\n"
    "\n"
    "Xosera is an FPGA based video adapter designed with the rosco_m68k retro\n"
    "computer in mind. Inspired in concept by it's \"namesake\" the Commander X16's\n"
    "VERA, Xosera is an original open-source video adapter design, built with open-\n"
    "source tools and is tailored with features generally appropriate for a\n"
    "Motorola 68K era retro computer like the rosco_m68k (or even an 8-bit CPU).\n"
    "\n"
    "\n"
    "  \xf9  Uses low-cost FPGA instead of expensive semiconductor fabrication :)\n"
    "  \xf9  128KB of embedded video VRAM (16-bit words at 25/33 MHz)\n"
    "  \xf9  VGA output at 640x480 or 848x480 16:9 wide-screen (both @ 60Hz)\n"
    "  \xf9  Register based interface using 16 direct 16-bit registers\n"
    "  \xf9  Additional indirect read/write registers for easy configuration\n"
    "  \xf9  Read/write VRAM with programmable read/write address increment\n"
    "  \xf9  Fast 8-bit bus interface (using MOVEP) for rosco_m68k (by Ross Bamford)\n"
    "  \xf9  Dual video planes (playfields) with alpha color blending and priority\n"
    "  \xf9  Dual 256 color palettes with 12-bit RGB (4096 colors) and 4-bit \"alpha\"\n"
    "  \xf9  Read/write tile memory for an additional 10KB of tiles or tilemap\n"
    "  \xf9  Text mode with up to 8x16 glyphs and 16 forground & background colors\n"
    "  \xf9  Graphic tile modes with 1024 8x8 glyphs, 16/256 colors and H/V tile mirror\n"
    "  \xf9  Bitmap modes with 1 (plus attribute colors), 4 or 8 bits per pixel\n"
    "  \xf9  Fast 2-D \"blitter\" unit with transparency, masking, shifting and logic ops\n"
    "  \xf9  Screen synchronized \"copper\" to change colors and registers mid-screen\n"
    "  \xf9  Pixel H/V repeat of 1x, 2x, 3x or 4x (e.g. for 424x240 or 320x240)\n"
    "  \xf9  Fractional H/V repeat scaling (e.g. for 320x200 or 512x384 retro modes)\n"
    "  \xf9  Wavetable 8-bit stereo audio with 4 channels (2 with dual playfield)\n"
    "\n"
    "\n";

static void msg(char * msg)
{
    char * s = msg;
    char   c;
    while ((c = *s++) != '\0')
    {
        sendchar(c);
    }
    sendchar('\r');
    sendchar('\n');
}

void xosera_uart_test()
{
    xv_prep();

    printf("\033c\033[?25l");        // ANSI reset, disable input cursor

    msg("copper crop_test - set Xosera to 640x480");
    msg("");

    const char * bp = blurb;

    while (1)
    {
        if (xuart_is_get_ready())
        {
            uint8_t c = xuart_get_byte();
            sendchar(c);        // echo to rosco UART
        }
        if (xuart_is_send_ready())
        {
            xuart_send_byte(*bp++);
            if (*bp == '\0')
            {
                bp = blurb;
            }
        }
    }
}
