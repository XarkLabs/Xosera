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
 * Split-screen multi-resolution test with copper.
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

#define DELAY_TIME 5000        // human speed
//#define DELAY_TIME 1000        // impatient human speed
//#define DELAY_TIME 100        // machine speed

#include "xosera_m68k_api.h"

bool              use_sd;
volatile uint32_t optguard;

// NOTE: 8 pixels before EOL is good spot to change GFX_CTRL settings for next line
// Copper list
uint32_t copper_list[] = {
    COP_MOVER(0x55, PA_GFX_CTRL),           // mover 0055, PA_GFX_CTRL    First half of screen in 4-bpp + Hx2 + Vx2 //
    COP_MOVEP(0x0ec6, 0xf),                 // movep 0x0ec6, 0xf          Palette entry 0xf from tut bitmap
    COP_WAIT_V(240),                        // wait  632, 240             Wait for 640-8, 240
    COP_MOVER(0x0040, PA_GFX_CTRL),         // mover 0x0040, PA_GFX_CTRL  1-bpp + Hx1 + Vx1
    COP_MOVER(0x3e80, PA_LINE_ADDR),        // mover 0x3e80, PA_LINE_ADDR Line start now at 16000
    COP_MOVEP(0x0fff, 0xf),                 // movep 0x0fff, 0xf          Palette entry 0xf to white for 1bpp bitmap
    COP_END()                               // nextf                      Wait for next frame
};

uint32_t mem_buffer[128 * 1024];

static void dputc(char c)
{
#ifndef __INTELLISENSE__
    __asm__ __volatile__(
        "move.w %[chr],%%d0\n"
        "move.l #2,%%d1\n"        // SENDCHAR
        "trap   #14\n"
        :
        : [chr] "d"(c)
        : "d0", "d1");
#endif
}

static void dprint(const char * str)
{
    register char c;
    while ((c = *str++) != '\0')
    {
        if (c == '\n')
        {
            dputc('\r');
        }
        dputc(c);
    }
}

static char dprint_buff[4096];
static void dprintf(const char * fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(dprint_buff, sizeof(dprint_buff), fmt, args);
    dprint(dprint_buff);
    va_end(args);
}

uint16_t screen_addr;
uint8_t  text_color = 0x02;        // dark green on black
uint8_t  text_columns;
uint8_t  text_rows;
int8_t   text_h;
int8_t   text_v;

static void get_textmode_settings()
{
    uint16_t vx          = (xreg_getw(PA_GFX_CTRL) & 3) + 1;
    uint16_t tile_height = (xreg_getw(PA_TILE_CTRL) & 0xf) + 1;
    screen_addr          = xreg_getw(PA_DISP_ADDR);
    text_columns         = (uint8_t)xreg_getw(PA_LINE_LEN);
    text_rows            = (uint8_t)(((xreg_getw(VID_VSIZE) / vx) + (tile_height - 1)) / tile_height);
}

static void xcls()
{
    get_textmode_settings();
    xm_setw(WR_ADDR, screen_addr);
    xm_setw(WR_INCR, 1);
    xm_setbh(DATA, text_color);
    for (uint16_t i = 0; i < (text_columns * text_rows); i++)
    {
        xm_setbl(DATA, ' ');
    }
    xm_setw(WR_ADDR, screen_addr);
}

static bool load_sd_bitmap(const char * filename, uint16_t base_address)
{
    dprintf("Loading bitmap: \"%s\"", filename);
    void * file = fl_fopen(filename, "r");

    if (file != NULL)
    {
        int cnt   = 0;
        int vaddr = base_address;

        xm_setw(WR_INCR, 0x0001);        // needed to be set

        while ((cnt = fl_fread(mem_buffer, 1, 512, file)) > 0)
        {
            if ((vaddr & 0xFFF) == 0)
            {
                dprintf(".");
            }

            uint16_t * maddr = (uint16_t *)mem_buffer;
            xm_setw(WR_ADDR, vaddr);
            for (int i = 0; i < (cnt >> 1); i++)
            {
                xm_setw(DATA, *maddr++);
            }
            vaddr += (cnt >> 1);
        }

        fl_fclose(file);
        dprintf("done!\n");
        return true;
    }
    else
    {
        dprintf(" - FAILED\n");
        return false;
    }
}

static bool load_sd_colors(const char * filename)
{
    dprintf("Loading colormap: \"%s\"", filename);
    void * file = fl_fopen(filename, "r");

    if (file != NULL)
    {
        int cnt   = 0;
        int vaddr = 0;

        while ((cnt = fl_fread(mem_buffer, 1, 512, file)) > 0)
        {
            if ((vaddr & 0x7) == 0)
            {
                dprintf(".");
            }

            uint16_t * maddr = (uint16_t *)mem_buffer;
            xm_setw(XR_ADDR, XR_COLOR_MEM);
            for (int i = 0; i < (cnt >> 1); i++)
            {
                xm_setw(XR_DATA, *maddr++);
            }
            vaddr += (cnt >> 1);
        }

        fl_fclose(file);
        dprintf("done!\n");
        return true;
    }
    else
    {
        dprintf(" - FAILED\n");
        return false;
    }
}

uint32_t test_count;
void     xosera_test()
{
    dprintf("Xosera_test_m68k\n");

    if (checkchar())
    {
        readchar();
    }

    // wait for monitor to unblank
    dprintf("\nxosera_init(0)...");
    bool success = xosera_init(0);
    dprintf("%s (%dx%d)\n", success ? "succeeded" : "FAILED", xreg_getw(VID_HSIZE), xreg_getw(VID_VSIZE));

    dprintf("Loading copper list...\n");

    xm_setw(XR_ADDR, XR_COPPER_MEM);
    uint16_t * wp = (uint16_t *)copper_list;
    for (uint8_t i = 0; i < sizeof(copper_list) / sizeof(uint32); i++)
    {
        xm_setw(XR_DATA, *wp++);
        xm_setw(XR_DATA, *wp++);
    }

    uint16_t version   = xreg_getw(VERSION);
    uint32_t githash   = ((uint32_t)xreg_getw(GITHASH_H) << 16) | (uint32_t)xreg_getw(GITHASH_L);
    uint16_t monwidth  = xreg_getw(VID_HSIZE);
    uint16_t monheight = xreg_getw(VID_VSIZE);
    uint16_t monfreq   = xreg_getw(VID_VFREQ);

    uint16_t gfxctrl  = xreg_getw(PA_GFX_CTRL);
    uint16_t tilectrl = xreg_getw(PA_TILE_CTRL);
    uint16_t dispaddr = xreg_getw(PA_DISP_ADDR);
    uint16_t linelen  = xreg_getw(PA_LINE_LEN);
    uint16_t hvscroll = xreg_getw(PA_HV_SCROLL);

    dprintf("Xosera v%1x.%02x #%08x Features:0x%1x\n", (version >> 8) & 0xf, (version & 0xff), githash, version >> 8);
    dprintf("Monitor Mode: %dx%d@%2x.%02xHz\n", monwidth, monheight, monfreq >> 8, monfreq & 0xff);
    dprintf("\nPlayfield A:\n");
    dprintf("PA_GFX_CTRL : 0x%04x PA_TILE_CTRL: 0x%04x\n", gfxctrl, tilectrl);
    dprintf("PA_DISP_ADDR: 0x%04x PA_LINE_LEN : 0x%04x\n", dispaddr, linelen);
    dprintf("PA_HV_SCROLL: 0x%04x\n", hvscroll);

    if (SD_check_support())
    {
        dprintf("SD card supported: ");

        if (SD_FAT_initialize())
        {
            dprintf("SD card ready\n");
            use_sd = true;
        }
        else
        {
            dprintf("no SD card\n");
            use_sd = false;
        }
    }
    else
    {
        dprintf("No SD card support.\n");
    }

    if (!use_sd)
    {
        dprintf("No SD support. Cannot continue\n");
        return;
    }


    // load palette, and images into vram
    dprintf("Loading data...\n");
#if 0
    if (!load_sd_colors("/ST_KingTut_Dpaint_16_pal.raw"))
    {
        return;
    }

    if (!load_sd_bitmap("/ST_KingTut_Dpaint_16.raw", 0))
    {
        return;
    }
#else
    if (!load_sd_colors("/pacbox-320x240_pal.raw"))
    {
        return;
    }

    if (!load_sd_bitmap("/pacbox-320x240.raw", 0))
    {
        return;
    }
#endif

    if (!load_sd_bitmap("/mountains_mono_640x480w.raw", 16000))
    {
        return;
    }

    /* For manual testing tut, if copper disabled */
    // xreg_setw(PA_GFX_CTRL, 0x0065);

    // Set line len here, if the two res had different the copper
    // would handle this instead...
    xreg_setw(PA_LINE_LEN, 80);

    dprintf("Ready - enabling copper...\n");
    xreg_setw(COPP_CTRL, 0x8000);

    /* For manual testing mountain if copper disabled... */
    // xreg_setw(PA_GFX_CTRL, 0x0040);
    // xreg_setw(PA_LINE_LEN, 80);
    // xreg_setw(PA_DISP_ADDR, 0x3e80);

    bool     up      = false;
    uint16_t current = 240;

    while (!checkchar())
    {
        for (int i = 0; i < 100000; i++)        // TODO: 10x slower to show bug
        {
            optguard = i;
        }

        xm_setw(XR_ADDR, XR_COPPER_MEM + 4);
        if (up)
        {
            xm_setw(XR_DATA, ++current);
            if (current == 300)
            {
                up = false;
            }
        }
        else
        {
            xm_setw(XR_DATA, --current);
            if (current == 200)
            {
                up = true;
            }
        }
    }

    // disable Copper
    xreg_setw(COPP_CTRL, 0x0000);

    // restore text mode
    xosera_init(1);
    xreg_setw(PA_GFX_CTRL, 0x0000);        // un-blank screen
    print("\033c");                        // reset & clear
}
