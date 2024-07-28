/*
 * vim: set et ts=4 sw=4
 *------------------------------------------------------------
 *                                  ___ ___ _
 *  ___ ___ ___ ___ ___       _____|  _| . | |_
 * |  _| . |_ -|  _| . |     |     | . | . | '_|
 * |_| |___|___|___|___|_____|_|_|_|___|___|_,_|
 *                     |_____|
 * ------------------------------------------------------------
 * Copyright (c) 2024 Xark
 * MIT License
 *
 * Test and example for Xosera filled rectangle
 * ------------------------------------------------------------
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <basicio.h>
#include <machine.h>

extern void xosera_rectangle();

void kmain()
{
    delay(1000 * 500);          // wait a bit for terminal window/serial
    while (checkinput())        // clear any queued input
    {
        inputchar();
    }
    xosera_rectangle();
}
