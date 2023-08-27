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

#include "xosera_m68k_api.h"

bool use_sd;

uint8_t mem_buffer[512];

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

void wait_vblank_start()
{
    xwait_not_vblank();
    xwait_vblank();
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
            xmem_set_addr(XR_COLOR_ADDR);
            for (int i = 0; i < (cnt >> 1); i++)
            {
                xmem_setw_next(*maddr++);
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

void line_draw(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color)
{
    int16_t x = x1, y = y1;
    int16_t d;
    int16_t a = x2 - x1, b = y2 - y1;
    int16_t dx_diag, dy_diag;
    int16_t dx_nondiag, dy_nondiag;
    int16_t inc_nondiag, inc_diag;
    int16_t temp;

    xv_prep();

    if (a < 0)
    {
        a       = -a;
        dx_diag = -1;
    }
    else
    {
        dx_diag = 1;
    }

    if (b < 0)
    {
        b       = -b;
        dy_diag = -1;
    }
    else
    {
        dy_diag = 1;
    }

    if (a < b)
    {
        temp       = a;
        a          = b;
        b          = temp;
        dx_nondiag = 0;
        dy_nondiag = dy_diag;
    }
    else
    {
        dx_nondiag = dx_diag;
        dy_nondiag = 0;
    }

    d           = b + b - a;
    inc_nondiag = b + b;
    inc_diag    = b + b - a - a;

    for (int16_t i = 0; i < a; i++)
    {
        xm_setw(PIXEL_X, x);
        xm_setw(PIXEL_Y, y);
        xm_setw(DATA, color);

        if (d < 0)
        {
            x += dx_nondiag;
            y += dy_nondiag;
            d += inc_nondiag;
        }
        else
        {
            x += dx_diag;
            y += dy_diag;
            d += inc_diag;
        }
    }
}

void line_test()
{
    uint16_t color = 0;

    for (int16_t x = 0; x < 320; x += 7)
    {
        line_draw(x, 0, 319 - x, 239, color);
        color = (color == 0xffff) ? 0 : color + 0x1111;
    }
    for (int16_t y = 0; y < 240; y += 7)
    {
        line_draw(0, y, 319, 239 - y, color);
        color = (color == 0xffff) ? 0 : color + 0x1111;
    }
}

uint32_t test_count;
void     xosera_pointer_test()
{
    printf("\033c\033[?25l");        // ANSI reset, disable input cursor

    dprintf("Xosera_test_m68k\n");
    dprintf("\nxosera_init(0)...");
    bool success = xosera_init(0);
    dprintf("%s (%dx%d)\n", success ? "succeeded" : "FAILED", xosera_vid_width(), xosera_vid_height());

    uint8_t  feature   = xm_getbh(FEATURE);
    uint16_t monwidth  = xosera_vid_width();
    uint16_t monheight = xosera_vid_height();

    uint16_t gfxctrl  = xreg_getw(PA_GFX_CTRL);
    uint16_t tilectrl = xreg_getw(PA_TILE_CTRL);
    uint16_t dispaddr = xreg_getw(PA_DISP_ADDR);
    uint16_t linelen  = xreg_getw(PA_LINE_LEN);
    uint16_t hvscroll = xreg_getw(PA_HV_SCROLL);
    uint16_t hvfscale = xreg_getw(PA_HV_FSCALE);

    dprintf("Xosera - FEATURE: 0x%04x\n", feature);
    dprintf("Monitor Mode: %dx%d\n", monwidth, monheight);
    dprintf("\nPlayfield A:\n");
    dprintf("PA_GFX_CTRL : 0x%04x  PA_TILE_CTRL: 0x%04x\n", gfxctrl, tilectrl);
    dprintf("PA_DISP_ADDR: 0x%04x  PA_LINE_LEN : 0x%04x\n", dispaddr, linelen);
    dprintf("PA_HV_SCROLL: 0x%04x  PA_HV_FSCALE: 0x%04x\n", hvscroll, hvfscale);

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
    if (!load_sd_colors("/pacbox-320x240_pal.raw"))
    {
        return;
    }

    if (!load_sd_bitmap("/pacbox-320x240.raw", 0))
    {
        return;
    }

#if 1
    // set corners of default pointer for testing
    xmem_setw(XR_POINTER_ADDR + 7, 0x000f);
    xmem_setw(XR_POINTER_ADDR + 31 * 8, 0xf000);
    xmem_setw(XR_POINTER_ADDR + 31 * 8 + 7, 0x000f);
#else
    // box for testing
    xmem_set_addr(XR_POINTER_ADDR);
    for (uint8_t v = 0; v < 32; v++)
    {
        if (v == 0 || v == 31)
        {
            for (uint8_t h = 0; h < (32 / 4); h++)
            {
                xmem_setw_next(0xffff);
            }
        }
        else
        {
            xmem_setw_next(0xf000);
            for (uint8_t h = 0; h < ((32 - 8) / 4); h++)
            {
                xmem_setw_next(0x0000);
            }
            xmem_setw_next(0x000f);
        }
    }
#endif

    /* For manual testing tut, if copper disabled */
    xreg_setw(PA_GFX_CTRL, 0x0055);

    // Set line len here, if the two res had different the copper
    // would handle this instead...
    xreg_setw(PA_LINE_LEN, 320 / 4);

    int32_t px = 300, py = 200;
    bool    done = false;

    xm_setw(WR_INCR, 0);
    uint16_t color = 0x1111;

    // init
    xm_setw(PIXEL_X, 0x0000);         // base VRAM address
    xm_setw(PIXEL_Y, 320 / 4);        // words per line
    xm_setbh(FEATURE, 0x00);          // init pixel address generator

    line_test();

    while (!done)
    {
        if (checkchar())
        {
            char c = readchar();

            if (c == 'r')
            {
                px = 640 / 2;
                py = 480 / 2;
            }
            else if (c == 'z')
                py += 0x1;
            else if (c == 'a')
                py -= 0x1;
            else if (c == ',')
                px -= 0x1;
            else if (c == '.')
                px += 0x1;
            else if (c == ' ')
            {
                if (color == 0xffff)
                {
                    color = 0x0000;
                }
                else
                {
                    color += 0x1111;
                }
            }
            else if (c == '\x1b' || c == 'k' || c == 'K')
                done = true;

            xm_setw(PIXEL_X, px >> 1);        // set X coordinate (convert from 640 -> 320)
            xm_setw(PIXEL_Y, py >> 1);        // set Y coordinate (convert from 480 -> 240)
            xm_setw(DATA, color);             // plot color (write mask set to isolate pixel)
        }

        wait_vblank_start();
        xosera_set_pointer(px, py, 0xF000);
    }

    xreg_setw(PA_GFX_CTRL, 0x0000);        // un-blank screen
    xm_setbl(SYS_CTRL, 0x000F);            // restore write mask
    print("\033c");                        // reset & clear
}
