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

#include <rosco_m68k/machine.h>
#include <rosco_m68k/xosera.h>

#include "rosco_m68k_support.h"

#define DELAY_TIME 5000        // human speed
// #define DELAY_TIME 1000        // impatient human speed
// #define DELAY_TIME 100        // machine speed

// Copper list
uint16_t copper_list[] = {
    COP_MOVER(MAKE_GFX_CTRL(0x00, GFX_VISIBLE, GFX_4_BPP, GFX_BITMAP, GFX_2X, GFX_2X),        //  0: 4-bpp + Hx2 + Vx2
              PA_GFX_CTRL),
    COP_MOVER(0x0ec6, COLOR_ADDR + 0xf),        //  2: Palette entry 0xf from tut bitmap
    COP_VPOS(240),                              //  4: Wait for 640-8, 240
    COP_MOVER(MAKE_GFX_CTRL(0x00, GFX_VISIBLE, GFX_1_BPP, GFX_BITMAP, GFX_1X, GFX_1X),        //  5: 1-bpp + Hx1 + Vx1
              PA_GFX_CTRL),
    COP_MOVER(0x3e80, PA_LINE_ADDR),            //  7: Line start now at 16000
    COP_MOVER(0x0fff, COLOR_ADDR + 0xf),        //  9: Palette entry 0xf to white for 1bpp bitmap
    COP_END()                                   // 11:Wait for next frame
};

uint32_t file_buffer[512];

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

    debug_printf("Xosera state:\n");
    debug_printf("DESCRIPTION : \"%s\"\n", initinfo.description_str);
    debug_printf("VERSION BCD : %x.%02x\n", initinfo.version_bcd >> 8, initinfo.version_bcd & 0xff);
    debug_printf("GIT HASH    : #%08x %s\n", initinfo.githash, initinfo.git_modified ? "[modified]" : "[clean]");
    debug_printf("FEATURE     : 0x%04x\n", feature);
    debug_printf("MONITOR RES : %dx%d MAX H/V POS : %d/%d AUDIO CHANS : %d\n",
                 monwidth,
                 monheight,
                 maxhpos,
                 maxvpos,
                 audchannels);
    debug_printf("\nConfig:\n");
    debug_printf("SYS_CTRL    : 0x%04x  INT_CTRL    : 0x%04x\n", sysctrl, intctrl);
    debug_printf("VID_CTRL    : 0x%04x  COPP_CTRL   : 0x%04x\n", vidctrl, coppctrl);
    debug_printf("AUD_CTRL    : 0x%04x\n", audctrl);
    debug_printf("VID_LEFT    : 0x%04x  VID_RIGHT   : 0x%04x\n", vidleft, vidright);
    debug_printf("\nPlayfield A:                                Playfield B:\n");
    debug_printf("PA_GFX_CTRL : 0x%04x  PA_TILE_CTRL: 0x%04x  PB_GFX_CTRL : 0x%04x  PB_TILE_CTRL: 0x%04x\n",
                 pa_gfxctrl,
                 pa_tilectrl,
                 pb_gfxctrl,
                 pb_tilectrl);
    debug_printf("PA_DISP_ADDR: 0x%04x  PA_LINE_LEN : 0x%04x  PB_DISP_ADDR: 0x%04x  PB_LINE_LEN : 0x%04x\n",
                 pa_dispaddr,
                 pa_linelen,
                 pb_dispaddr,
                 pb_linelen);
    debug_printf("PA_H_SCROLL : 0x%04x  PA_V_SCROLL : 0x%04x  PB_H_SCROLL : 0x%04x  PB_V_SCROLL : 0x%04x\n",
                 pa_hscroll,
                 pa_vscroll,
                 pb_hscroll,
                 pb_vscroll);
    debug_printf("PA_HV_FSCALE: 0x%04x                        PB_HV_FSCALE: 0x%04x\n", pa_hvfscale, pb_hvfscale);
    debug_printf("\n\n");
}

static bool load_sd_bitmap(const char * filename, uint16_t base_address)
{
    xv_prep();

    debug_printf("Loading bitmap: \"%s\"", filename);
    FILE * file = fopen(filename, "r");

    if (file != NULL)
    {
        int cnt   = 0;
        int vaddr = base_address;

        xm_setw(WR_INCR, 0x0001);        // needed to be set

        while ((cnt = fread(file_buffer, 1, 512, file)) > 0)
        {
            if ((vaddr & 0xFFF) == 0)
            {
                debug_printf(".");
            }

            uint16_t * maddr = (uint16_t *)file_buffer;
            xm_setw(WR_ADDR, vaddr);
            for (int i = 0; i < (cnt >> 1); i++)
            {
                xm_setw(DATA, *maddr++);
            }
            vaddr += (cnt >> 1);
        }

        fclose(file);
        debug_printf("done!\n");
        return true;
    }
    else
    {
        debug_printf(" - FAILED\n");
        return false;
    }
}

static bool load_sd_colors(const char * filename)
{
    xv_prep();

    debug_printf("Loading colormap: \"%s\"", filename);
    FILE * file = fopen(filename, "r");

    if (file != NULL)
    {
        int cnt   = 0;
        int vaddr = 0;

        while ((cnt = fread(file_buffer, 1, 512, file)) > 0)
        {
            if ((vaddr & 0x7) == 0)
            {
                debug_printf(".");
            }

            uint16_t * maddr = (uint16_t *)file_buffer;
            xmem_setw_next_addr(XR_COLOR_ADDR);
            for (int i = 0; i < (cnt >> 1); i++)
            {
                xmem_setw_next(*maddr++);
            }
            vaddr += (cnt >> 1);
        }

        fclose(file);
        debug_printf("done!\n");
        return true;
    }
    else
    {
        debug_printf(" - FAILED\n");
        return false;
    }
}

// xosera_splitscreen_test()
int main()
{
    mcBusywait(1000 * 500);        // wait a bit for terminal window/serial
    while (mcCheckInput())         // clear any queued input
    {
        mcInputchar();
    }
    xv_prep();

    printf("\033cXosera_splitscreen_test\n");
    debug_printf("Checking for Xosera XANSI firmware...");
    if (xosera_xansi_detect(true))
    {
        debug_printf("detected.\n");
    }
    else
    {
        debug_printf(
            "\n\nXosera XANSI firmware was not detected!\n"
            "This program will likely trap without Xosera hardware.\n");
    }
    debug_printf("xosera_init(XINIT_CONFIG_640x480)...");
    bool success = xosera_init(XINIT_CONFIG_640x480);
    debug_printf("%s (%dx%d)\n", success ? "succeeded" : "FAILED", xosera_vid_width(), xosera_vid_height());
    xosera_get_info(&initinfo);
    dump_xosera_regs();

    debug_printf("Loading copper list...\n");
    xmem_setw_next_addr(XR_COPPER_ADDR);
    uint16_t * wp = copper_list;
    for (uint8_t i = 0; i < sizeof(copper_list) / sizeof(copper_list[0]); i++)
    {
        xmem_setw_next(*wp++);
    }

    // load palette, and images into vram
    debug_printf("Loading data...\n");
#if 0
    if (!load_sd_colors("/sd/ST_KingTut_Dpaint_16_pal.raw"))
    {
        return;
    }

    if (!load_sd_bitmap("/sd/ST_KingTut_Dpaint_16.raw", 0))
    {
        return;
    }
#else
    if (!load_sd_colors("/sd/pacbox-320x240_pal.raw"))
    {
        return EXIT_FAILURE;
    }

    if (!load_sd_bitmap("/sd/pacbox-320x240.raw", 0))
    {
        return EXIT_FAILURE;
    }
#endif

    if (!load_sd_bitmap("/sd/mountains_mono_640x480w.raw", 16000))
    {
        return EXIT_FAILURE;
    }

    // Set line len here, if the two res had different the copper
    // would handle this instead...
    xreg_setw(PA_LINE_LEN, 80);

    debug_printf("Ready - enabling copper...\n");
    xreg_setw(COPP_CTRL, MAKE_COPP_CTRL(1));

    bool     up      = false;
    uint16_t current = 240;

    while (!mcCheckInput())
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
    mcInputchar();

    // disable Copper
    xreg_setw(COPP_CTRL, MAKE_COPP_CTRL(0));

    // restore text mode
    xosera_xansi_restore();
    debug_printf("Exit\n");
}
