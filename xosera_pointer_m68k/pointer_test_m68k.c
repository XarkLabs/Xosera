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

uint16_t mem_buffer[256];

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
    xv_prep();

    xwait_not_vblank();
    xwait_vblank();
}

static bool load_sd_bitmap(const char * filename, uint16_t base_address)
{
    xv_prep();

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
    xv_prep();

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
            xmem_setw_next_addr(XR_COLOR_ADDR);
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

// original version translated from Pascal
// from https://ia801900.us.archive.org/0/items/byte-magazine-1988-03/byte-magazine-1988-03.pdf
// "Better Bit-mapped Lines" pg. 249
void line_draw_orig(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color)
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

    for (int16_t i = 0; i <= a; i++)
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

// slightly optimized version ~20% faster
void line_draw(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color)
{
    int16_t x = x1, y = y1;
    int16_t d;
    int16_t a = x2 - x1, b = y2 - y1;
    int16_t dx_diag, dy_diag;
    int16_t dx_nondiag, dy_nondiag;
    int16_t inc_nondiag, inc_diag;

    xv_prep();

    // always draw first pixel, and this sets up full data word and latch
    xm_setw(PIXEL_X, x);
    xm_setw(PIXEL_Y, y);
    xm_setw(DATA, color);

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

    // instead of swapping for one loop, have x major and y major loops
    // remove known zero terms
    if (a < b)
    {
        dy_nondiag = dy_diag;

        d           = a + a - b;
        inc_nondiag = a + a;
        inc_diag    = a + a - b - b;

        // start count at one (drew one pixel already )
        for (int16_t i = 1; i <= b; i++)
        {
            if (d < 0)
            {
                y += dy_nondiag;
                d += inc_nondiag;
                // x not changing, don't need to set it
                xm_setw(PIXEL_Y, y);
                xm_setbl(DATA, color);        // we can get away with bl here, since upper byte is latched
            }
            else
            {
                x += dx_diag;
                y += dy_diag;
                d += inc_diag;
                xm_setw(PIXEL_X, x);
                xm_setw(PIXEL_Y, y);
                xm_setbl(DATA, color);        // we can get away with bl here, since upper byte is latched
            }
        }
    }
    else
    {
        dx_nondiag = dx_diag;

        d           = b + b - a;
        inc_nondiag = b + b;
        inc_diag    = b + b - a - a;

        // start count at one (drew one pixel already )
        for (int16_t i = 1; i <= a; i++)
        {
            if (d < 0)
            {
                x += dx_nondiag;
                d += inc_nondiag;
                // y not changing, don't need to set it
                xm_setw(PIXEL_X, x);
                xm_setbl(DATA, color);        // we can get away with bl here, since upper byte is latched
            }
            else
            {
                x += dx_diag;
                y += dy_diag;
                d += inc_diag;
                xm_setw(PIXEL_X, x);
                xm_setw(PIXEL_Y, y);
                xm_setbl(DATA, color);        // we can get away with bl here, since upper byte is latched
            }
        }
    }
}

void line_test_orig()
{
    uint16_t color = 0;

    xv_prep();

    uint16_t start_time;
    uint16_t check_time = xm_getw(TIMER);
    do
    {
        start_time = xm_getw(TIMER);
    } while (start_time == check_time);

    for (int16_t x = 0; x < 320; x += 3)
    {
        line_draw_orig(x, 0, 319 - x, 239, color);
        color = (color == 0xffff) ? 0 : color + 0x1111;
    }
    for (int16_t y = 0; y < 240; y += 3)
    {
        line_draw_orig(0, y, 319, 239 - y, color);
        color = (color == 0xffff) ? 0 : color + 0x1111;
    }

    uint16_t elapsed_time = xm_getw(TIMER) - start_time;
    dprintf("line_test original  (%3u.%1ums)\n", elapsed_time / 10, elapsed_time % 10);
}


void line_test()
{
    uint16_t color = 0;

    xv_prep();

    uint16_t start_time;
    uint16_t check_time = xm_getw(TIMER);
    do
    {
        start_time = xm_getw(TIMER);
    } while (start_time == check_time);

    for (int16_t x = 0; x < 320; x += 3)
    {
        line_draw(x, 0, 319 - x, 239, color);
        color = (color == 0xffff) ? 0 : color + 0x1111;
    }
    for (int16_t y = 0; y < 240; y += 3)
    {
        line_draw(0, y, 319, 239 - y, color);
        color = (color == 0xffff) ? 0 : color + 0x1111;
    }

    uint16_t elapsed_time = xm_getw(TIMER) - start_time;
    dprintf("line_test optimized (%3u.%1ums)\n", elapsed_time / 10, elapsed_time % 10);
}

// pointer sprite
// clang-format off
uint16_t eks[256] = {
    0xf100, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x001f,
    0x1f10, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x01f1,
    0x01f1, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1f10,
    0x001f, 0x1000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0001, 0xf100,
    0x0001, 0xf100, 0x0000, 0x0000, 0x0000, 0x0000, 0x001f, 0x1000,
    0x0000, 0x1f10, 0x0000, 0x0000, 0x0000, 0x0000, 0x01f1, 0x0000,
    0x0000, 0x01f1, 0x0000, 0x0000, 0x0000, 0x0000, 0x1f10, 0x0000,
    0x0000, 0x001f, 0x1000, 0x0000, 0x0000, 0x0001, 0xf100, 0x0000,
    0x0000, 0x0001, 0xf100, 0x0000, 0x0000, 0x001f, 0x1000, 0x0000,
    0x0000, 0x0000, 0x1f10, 0x0000, 0x0000, 0x01f1, 0x0000, 0x0000,
    0x0000, 0x0000, 0x01f1, 0x0000, 0x0000, 0x1f10, 0x0000, 0x0000,
    0x0000, 0x0000, 0x001f, 0x1000, 0x0001, 0xf100, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0001, 0xf100, 0x001f, 0x1000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x1f10, 0x01f1, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x01f1, 0x1f10, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x01f1, 0x1f10, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x1f10, 0x01f1, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0001, 0xf100, 0x001f, 0x1000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x001f, 0x1000, 0x0001, 0xf100, 0x0000, 0x0000,
    0x0000, 0x0000, 0x01f1, 0x0000, 0x0000, 0x1f10, 0x0000, 0x0000,
    0x0000, 0x0000, 0x1f10, 0x0000, 0x0000, 0x01f1, 0x0000, 0x0000,
    0x0000, 0x0001, 0xf100, 0x0000, 0x0000, 0x001f, 0x1000, 0x0000,
    0x0000, 0x001f, 0x1000, 0x0000, 0x0000, 0x0001, 0xf100, 0x0000,
    0x0000, 0x01f1, 0x0000, 0x0000, 0x0000, 0x0000, 0x1f10, 0x0000,
    0x0000, 0x1f10, 0x0000, 0x0000, 0x0000, 0x0000, 0x01f1, 0x0000,
    0x0001, 0xf100, 0x0000, 0x0000, 0x0000, 0x0000, 0x001f, 0x1000,
    0x001f, 0x1000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0001, 0xf100,
    0x01f1, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1f10,
    0x1f10, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x01f1,
    0xf100, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x001f
};
// clang-format on

uint32_t test_count;
void     xosera_pointer_test()
{
    xv_prep();

    dprintf("pointer_test_m68k\n");

    dprintf("Checking for Xosera XANSI firmware...");
    if (xosera_xansi_detect(true))        // check for XANSI (and disable input cursor if present)
    {
        dprintf("detected.\n");
    }
    else
    {
        dprintf(
            "\n\nXosera XANSI firmware was not detected!\n"
            "This program will likely trap without Xosera hardware.\n");
    }
    dprintf("Calling xosera_init(XINIT_CONFIG_640x480)...");
    bool success = xosera_init(XINIT_CONFIG_640x480);
    dprintf("%s (%dx%d)\n", success ? "succeeded" : "FAILED", xosera_vid_width(), xosera_vid_height());

    uint8_t  feature   = xm_getbh(FEATURE);
    uint16_t monwidth  = xosera_vid_width();
    uint16_t monheight = xosera_vid_height();

    uint16_t gfxctrl  = xreg_getw(PA_GFX_CTRL);
    uint16_t tilectrl = xreg_getw(PA_TILE_CTRL);
    uint16_t dispaddr = xreg_getw(PA_DISP_ADDR);
    uint16_t linelen  = xreg_getw(PA_LINE_LEN);
    uint16_t hscroll  = xreg_getw(PA_H_SCROLL);
    uint16_t vscroll  = xreg_getw(PA_V_SCROLL);
    uint16_t hvfscale = xreg_getw(PA_HV_FSCALE);

    dprintf("Xosera - FEATURE: 0x%04x\n", feature);
    dprintf("Monitor Mode: %dx%d\n", monwidth, monheight);
    dprintf("\nPlayfield A:\n");
    dprintf("PA_GFX_CTRL : 0x%04x  PA_TILE_CTRL: 0x%04x\n", gfxctrl, tilectrl);
    dprintf("PA_DISP_ADDR: 0x%04x  PA_LINE_LEN : 0x%04x\n", dispaddr, linelen);
    dprintf("PA_H_SCROLL : 0x%04x  PA_V_SCROLL : 0x%04x\n", hscroll, vscroll);
    dprintf("PA_HV_FSCALE: 0x%04x\n", hvfscale);

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
    // X for testing
    xmem_setw_next_addr(XR_POINTER_ADDR);
    for (uint16_t v = 0; v < XR_POINTER_SIZE; v++)
    {
        xmem_setw_next(eks[v]);
    }

#elif 0
    // set corners of default pointer for testing
    xmem_setw(XR_POINTER_ADDR + 7, 0x000f);
    xmem_setw(XR_POINTER_ADDR + 31 * 8, 0xf000);
    xmem_setw(XR_POINTER_ADDR + 31 * 8 + 7, 0x000f);
#else
    // box for testing
    xmem_setw_next_addr(XR_POINTER_ADDR);
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
    xm_setbh(SYS_CTRL, 0x00);         // set PIXEL_BASE and PIXEL_WIDTH for 4-bpp

    dprintf("\nLine benchmark:\n");
    line_test_orig();
    cpu_delay(3000);
    line_test();

    dprintf("\n\nFor cheesy painting:\n\n");
    dprintf("    A / Z  move pointer up / down\n");
    dprintf("    , / .  move pointer left / right\n");
    dprintf("    SPACE  cycles color\n");
    dprintf("    r      resets to center\n");
    dprintf("    ESC    exit (or Kermit)\n");

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
        xosera_set_pointer(px - 15, py - 15, 0xF000);
    }

    xosera_set_pointer(-32, 0, 0xF000);        // hide pointer
    xosera_xansi_restore();
}
