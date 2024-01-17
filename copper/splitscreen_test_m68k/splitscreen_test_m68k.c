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
// #define DELAY_TIME 1000        // impatient human speed
// #define DELAY_TIME 100        // machine speed

#include "xosera_m68k_api.h"

bool use_sd;

// Copper list
uint16_t copper_list[] = {
    COP_MOVER(0x0055, PA_GFX_CTRL),             //  0: First half of screen in 4-bpp + Hx2 + Vx2 //
    COP_MOVER(0x0ec6, COLOR_ADDR + 0xf),        //  2: Palette entry 0xf from tut bitmap
    COP_VPOS(240),                              //  4: Wait for 640-8, 240
    COP_MOVER(0x0040, PA_GFX_CTRL),             //  5: 1-bpp + Hx1 + Vx1
    COP_MOVER(0x3e80, PA_LINE_ADDR),            //  7: Line start now at 16000
    COP_MOVER(0x0fff, COLOR_ADDR + 0xf),        //  9: Palette entry 0xf to white for 1bpp bitmap
    COP_END()                                   // 11:Wait for next frame
};

uint32_t file_buffer[512];

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
#if 0

void wait_vblank_start()
{
    xwait_not_vblank();
    xwait_vblank();
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
    text_rows            = (uint8_t)(((xosera_vid_height() / vx) + (tile_height - 1)) / tile_height);
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
#endif

static xosera_info_t initinfo;

void dump_xosera_regs(void)
{
    xv_prep();

    uint16_t feature     = xm_getw(FEATURE);
    uint16_t monwidth    = xosera_vid_width();
    uint16_t monheight   = xosera_vid_height();
    uint16_t maxhpos     = xosera_max_hpos();
    uint16_t maxvpos     = xosera_max_vpos();
    uint16_t audchannels = xosera_aud_channels();

    uint16_t sysctrl = xm_getw(SYS_CTRL);
    uint16_t intctrl = xm_getw(INT_CTRL);

    uint16_t vidctrl  = xreg_getw(VID_CTRL);
    uint16_t coppctrl = xreg_getw(COPP_CTRL);
    uint16_t audctrl  = xreg_getw(AUD_CTRL);
    uint16_t vidleft  = xreg_getw(VID_LEFT);
    uint16_t vidright = xreg_getw(VID_RIGHT);

    uint16_t pa_gfxctrl  = xreg_getw(PA_GFX_CTRL);
    uint16_t pa_tilectrl = xreg_getw(PA_TILE_CTRL);
    uint16_t pa_dispaddr = xreg_getw(PA_DISP_ADDR);
    uint16_t pa_linelen  = xreg_getw(PA_LINE_LEN);
    uint16_t pa_hscroll  = xreg_getw(PA_H_SCROLL);
    uint16_t pa_vscroll  = xreg_getw(PA_V_SCROLL);
    uint16_t pa_hvfscale = xreg_getw(PA_HV_FSCALE);

    uint16_t pb_gfxctrl  = xreg_getw(PB_GFX_CTRL);
    uint16_t pb_tilectrl = xreg_getw(PB_TILE_CTRL);
    uint16_t pb_dispaddr = xreg_getw(PB_DISP_ADDR);
    uint16_t pb_linelen  = xreg_getw(PB_LINE_LEN);
    uint16_t pb_hscroll  = xreg_getw(PB_H_SCROLL);
    uint16_t pb_vscroll  = xreg_getw(PB_V_SCROLL);
    uint16_t pb_hvfscale = xreg_getw(PB_HV_FSCALE);

    dprintf("Xosera state:\n");
    dprintf("DESCRIPTION : \"%s\"\n", initinfo.description_str);
    dprintf("VERSION BCD : %x.%02x\n", initinfo.version_bcd >> 8, initinfo.version_bcd & 0xff);
    dprintf("GIT HASH    : #%08x %s\n", initinfo.githash, initinfo.git_modified ? "[modified]" : "[clean]");
    dprintf("FEATURE     : 0x%04x\n", feature);
    dprintf(
        "MONITOR RES : %dx%d MAX H/V POS : %d/%d AUDIO CHANS : %d\n", monwidth, monheight, maxhpos, maxvpos, audchannels);
    dprintf("\nConfig:\n");
    dprintf("SYS_CTRL    : 0x%04x  INT_CTRL    : 0x%04x\n", sysctrl, intctrl);
    dprintf("VID_CTRL    : 0x%04x  COPP_CTRL   : 0x%04x\n", vidctrl, coppctrl);
    dprintf("AUD_CTRL    : 0x%04x\n", audctrl);
    dprintf("VID_LEFT    : 0x%04x  VID_RIGHT   : 0x%04x\n", vidleft, vidright);
    dprintf("\nPlayfield A:                                Playfield B:\n");
    dprintf("PA_GFX_CTRL : 0x%04x  PA_TILE_CTRL: 0x%04x  PB_GFX_CTRL : 0x%04x  PB_TILE_CTRL: 0x%04x\n",
            pa_gfxctrl,
            pa_tilectrl,
            pb_gfxctrl,
            pb_tilectrl);
    dprintf("PA_DISP_ADDR: 0x%04x  PA_LINE_LEN : 0x%04x  PB_DISP_ADDR: 0x%04x  PB_LINE_LEN : 0x%04x\n",
            pa_dispaddr,
            pa_linelen,
            pb_dispaddr,
            pb_linelen);
    dprintf("PA_H_SCROLL : 0x%04x  PA_V_SCROLL : 0x%04x  PB_H_SCROLL : 0x%04x  PB_V_SCROLL : 0x%04x\n",
            pa_hscroll,
            pa_vscroll,
            pb_hscroll,
            pb_vscroll);
    dprintf("PA_HV_FSCALE: 0x%04x                        PB_HV_FSCALE: 0x%04x\n", pa_hvfscale, pb_hvfscale);
    dprintf("\n\n");
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

        while ((cnt = fl_fread(file_buffer, 1, 512, file)) > 0)
        {
            if ((vaddr & 0xFFF) == 0)
            {
                dprintf(".");
            }

            uint16_t * maddr = (uint16_t *)file_buffer;
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

        while ((cnt = fl_fread(file_buffer, 1, 512, file)) > 0)
        {
            if ((vaddr & 0x7) == 0)
            {
                dprintf(".");
            }

            uint16_t * maddr = (uint16_t *)file_buffer;
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

void xosera_splitscreen_test()
{
    xv_prep();

    dprintf("Xosera_test_m68k\n");
    dprintf("Checking for Xosera XANSI firmware...");
    if (xosera_xansi_detect(true))
    {
        dprintf("detected.\n");
    }
    else
    {
        dprintf(
            "\n\nXosera XANSI firmware was not detected!\n"
            "This program will likely trap without Xosera hardware.\n");
    }
    dprintf("xosera_init(XINIT_CONFIG_640x480)...");
    bool success = xosera_init(XINIT_CONFIG_640x480);
    dprintf("%s (%dx%d)\n", success ? "succeeded" : "FAILED", xosera_vid_width(), xosera_vid_height());
    xosera_get_info(&initinfo);
    dump_xosera_regs();

    dprintf("Loading copper list...\n");
    xmem_setw_next_addr(XR_COPPER_ADDR);
    uint16_t * wp = copper_list;
    for (uint8_t i = 0; i < sizeof(copper_list) / sizeof(copper_list[0]); i++)
    {
        xmem_setw_next(*wp++);
    }

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
        xwait_not_vblank();
        xwait_vblank();

        xmem_setw_next_addr(XR_COPPER_ADDR + 4);
        if (up)
        {
            current += 1;
            uint16_t op = COP_VPOS(current);
            xmem_setw_next(op);
            if (current >= 300)
            {
                up = false;
            }
        }
        else
        {
            current -= 1;
            uint16_t op = COP_VPOS(current);
            xmem_setw_next(op);
            if (current <= 200)
            {
                up = true;
            }
        }
    }

    // disable Copper
    xreg_setw(COPP_CTRL, 0x0000);

    // restore text mode
    xosera_xansi_restore();
    dprintf("Exit\n");
}
