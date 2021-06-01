/*
 * vim: set et ts=4 sw=4
 *------------------------------------------------------------
 *                                  ___ ___ _
 *  ___ ___ ___ ___ ___       _____|  _| . | |_
 * |  _| . |_ -|  _| . |     |     | . | . | '_|
 * |_| |___|___|___|___|_____|_|_|_|___|___|_,_|
 *                     |_____|
 * ------------------------------------------------------------
 * Copyright (c) 2021 Xark
 * MIT License
 *
 * Test and tech-demo for Xosera FPGA "graphics card"
 * ------------------------------------------------------------
 */

#include <machine.h>
#include <stdio.h>

extern void xosera_demo();

void kmain()
{
    mcDelaymsec10(200);        // wait a bit for terminal window
    printf("\n\f\nxosera_demo_m68k started...\n\n");

    xosera_demo();

    printf("xosera_demo_m68k exiting.\n");
}
