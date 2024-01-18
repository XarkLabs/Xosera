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

#include <assert.h>
#include <ctype.h>
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

#define DEBUG 1

#include "rosco_m68k_support.h"
#include "xosera_mon_m68k.h"

extern void install_intr(void);
extern void remove_intr(void);

extern volatile uint32_t XFrameCount;

bool use_sd;

// Xosera default color palette
uint16_t def_colors[256] = {
    0x0000, 0x000a, 0x00a0, 0x00aa, 0x0a00, 0x0a0a, 0x0aa0, 0x0aaa, 0x0555, 0x055f, 0x05f5, 0x05ff, 0x0f55, 0x0f5f,
    0x0ff5, 0x0fff, 0x0213, 0x0435, 0x0546, 0x0768, 0x098a, 0x0bac, 0x0dce, 0x0313, 0x0425, 0x0636, 0x0858, 0x0a7a,
    0x0c8c, 0x0eae, 0x0413, 0x0524, 0x0635, 0x0746, 0x0857, 0x0a68, 0x0b79, 0x0500, 0x0801, 0x0a33, 0x0d55, 0x0f78,
    0x0fab, 0x0fde, 0x0534, 0x0756, 0x0867, 0x0a89, 0x0b9a, 0x0dbc, 0x0ecd, 0x0200, 0x0311, 0x0533, 0x0744, 0x0966,
    0x0b88, 0x0daa, 0x0421, 0x0532, 0x0643, 0x0754, 0x0864, 0x0a75, 0x0b86, 0x0310, 0x0630, 0x0850, 0x0a70, 0x0da3,
    0x0fd5, 0x0ff7, 0x0210, 0x0432, 0x0654, 0x0876, 0x0a98, 0x0cba, 0x0edc, 0x0321, 0x0431, 0x0541, 0x0763, 0x0985,
    0x0ba7, 0x0dc9, 0x0331, 0x0441, 0x0551, 0x0662, 0x0773, 0x0884, 0x0995, 0x0030, 0x0250, 0x0470, 0x06a0, 0x08c0,
    0x0bf3, 0x0ef5, 0x0442, 0x0664, 0x0775, 0x0997, 0x0aa8, 0x0cca, 0x0ddb, 0x0010, 0x0231, 0x0341, 0x0562, 0x0673,
    0x0895, 0x0ab7, 0x0130, 0x0241, 0x0351, 0x0462, 0x0573, 0x0694, 0x07a5, 0x0040, 0x0060, 0x0180, 0x03b2, 0x05e5,
    0x08f7, 0x0af9, 0x0120, 0x0342, 0x0453, 0x0675, 0x0897, 0x0ab9, 0x0dec, 0x0020, 0x0141, 0x0363, 0x0474, 0x0696,
    0x08b8, 0x0ad9, 0x0031, 0x0142, 0x0253, 0x0364, 0x0486, 0x0597, 0x06a8, 0x0033, 0x0054, 0x0077, 0x02a9, 0x04cc,
    0x07ff, 0x09ff, 0x0354, 0x0465, 0x0576, 0x0798, 0x08a9, 0x0acb, 0x0ced, 0x0011, 0x0022, 0x0244, 0x0366, 0x0588,
    0x0699, 0x08bb, 0x0035, 0x0146, 0x0257, 0x0368, 0x0479, 0x058a, 0x069b, 0x0018, 0x003b, 0x035d, 0x047f, 0x07af,
    0x09ce, 0x0cff, 0x0123, 0x0234, 0x0456, 0x0678, 0x089a, 0x0abc, 0x0cde, 0x0013, 0x0236, 0x0347, 0x0569, 0x078b,
    0x09ad, 0x0bcf, 0x0226, 0x0337, 0x0448, 0x0559, 0x066a, 0x077c, 0x088d, 0x0209, 0x041c, 0x063f, 0x085f, 0x0b7f,
    0x0eaf, 0x0fdf, 0x0446, 0x0557, 0x0779, 0x088a, 0x0aac, 0x0bbd, 0x0ddf, 0x0103, 0x0215, 0x0437, 0x0548, 0x076a,
    0x098d, 0x0baf, 0x0315, 0x0426, 0x0537, 0x0648, 0x085a, 0x096b, 0x0a7c, 0x0405, 0x0708, 0x092a, 0x0c4d, 0x0f6f,
    0x0f9f, 0x0fbf, 0x0000, 0x0111, 0x0222, 0x0333, 0x0444, 0x0555, 0x0666, 0x0777, 0x0888, 0x0999, 0x0aaa, 0x0bbb,
    0x0ccc, 0x0ddd, 0x0eee, 0x0fff};

xosera_info_t initinfo;
uint32_t      mem_buffer[128 * 1024];

// timer helpers
static uint32_t start_tick;

void timer_start()
{
    uint32_t ts = XFrameCount;
    uint32_t t;
    // this waits for a "fresh tick" to reduce timing jitter
    while ((t = XFrameCount) == ts)
        ;
    start_tick = t;
}

uint32_t timer_stop()
{
    uint32_t stop_tick = XFrameCount;

    return ((stop_tick - start_tick) * 1667) / 100;
}

static inline void wait_vblank_start()
{
    xv_prep();

    xwait_not_vblank();
    xwait_vblank();
}

static inline void check_vblank()
{
    xv_prep();

    if (!xm_getb_sys_ctrl(VBLANK) || xreg_getw(SCANLINE) > 520)
    {
        wait_vblank_start();
    }
}

void restore_colors()
{
    xv_prep();

    wait_vblank_start();
    xmem_setw_next_addr(XR_COLOR_ADDR);
    uint16_t * cp = def_colors;
    for (uint16_t i = 0; i < 256; i++)
    {
        xmem_setw_next(*cp++);
    }
    // set B colors to same, alpha 0x8 (with color 0 fully transparent)
    xmem_setw_next(0x0000);
    cp = def_colors + 1;
    for (uint16_t i = 1; i < 256; i++)
    {
        xmem_setw_next(0x8000 | *cp++);
    }
}

void reset_vid(void)
{
    xv_prep();

    remove_intr();

    wait_vblank_start();

    xreg_setw(VID_CTRL, 0x0008);
    xreg_setw(COPP_CTRL, 0x0000);        // disable copper
    xreg_setw(VID_LEFT, (xosera_vid_width() > 640 ? ((xosera_vid_width() - 640) / 2) : 0) + 0);
    xreg_setw(VID_RIGHT, (xosera_vid_width() > 640 ? (xosera_vid_width() - 640) / 2 : 0) + 640);
    xreg_setw(PA_GFX_CTRL, 0x0000);
    xreg_setw(PA_TILE_CTRL, 0x000F);
    xreg_setw(PA_DISP_ADDR, 0x0000);
    xreg_setw(PA_LINE_LEN, 80);        // line len
    xreg_setw(PA_HV_FSCALE, 0x0000);
    xreg_setw(PA_H_SCROLL, 0x0000);
    xreg_setw(PA_V_SCROLL, 0x0000);
    xreg_setw(PB_GFX_CTRL, 0x0080);

    restore_colors();

    printf("\033c");        // reset XANSI

    while (checkchar())
    {
        readchar();
    }
}

void bail()
{
    reset_vid();
    disable_sd_boot();
    _WARM_BOOT();
}

const char * val_name(const addr_range_t * ar, uint16_t v)
{
    static char str[64];
    for (int i = 0; ar[i].name != NULL; i++)
    {
        if (v >= ar[i].addr && v < ar[i].addr + ar[i].size)
        {
            if (ar[i].size > 1)
            {
                uint16_t o = v - ar[i].addr;
                sprintf(str, "%s+0x%x", ar[i].name, o);
            }
            else
            {
                sprintf(str, "%s", ar[i].name);
            }
            return str;
        }
    }

    return "";
}

int name_val(const addr_range_t * ar, const char * name)
{
    for (int i = 0; ar[i].name != NULL; i++)
    {
        if (strcmp(name, ar[i].name) == 0)
        {
            return (int)ar[i].addr;
        }
    }

    return -1;
}

void str_upper(char * str)
{
    char c;
    while (str && (c = *str) != '\0')
    {
        *str = toupper(*str);
        str++;
    }
}

void print_xr_reg(int xreg_num)
{
    xv_prep();
    xwait_mem_ready();
    uint16_t v = xmem_getw_wait(xreg_num);

    dprintf("%-13.13s= 0x%04x", val_name(xr_mem, xreg_num), v);
}

void print_xm_reg(int reg_num)
{
    xv_prep();

    dprintf("%-10.10s= ", val_name(xm_regs, reg_num));

    uint16_t v;
    uint32_t l;
    uint16_t prev;
    uint16_t read;
    uint16_t incr;

    (void)read;

    switch (reg_num << 2)
    {
        case XM_SYS_CTRL:
            v = xm_getw(SYS_CTRL);
            dprintf("0x%04x", v);
            for (int b = 7; b >= 0; b--)
            {
                if (v & (0x100 << b))
                {
                    dprintf(" %s", val_name(sys_ctrl_status, b));
                }
            }
            dprintf(" WM:%x%x%x%x", (v >> 3) & 1, (v >> 2) & 1, (v >> 1) & 1, (v >> 0) & 1);
            break;
        case XM_INT_CTRL:
            v = xm_getw(INT_CTRL);
            dprintf("0x%04x", v);
            dprintf(" IM:%s %s %s %s %s %s %s",
                    (v >> 14) & 1 ? "BL" : "- ",
                    (v >> 13) & 1 ? "TI" : "- ",
                    (v >> 12) & 1 ? "VI" : "- ",
                    (v >> 11) & 1 ? "A3" : "- ",
                    (v >> 10) & 1 ? "A2" : "- ",
                    (v >> 9) & 1 ? "A1" : "- ",
                    (v >> 8) & 1 ? "A0" : "- ");
            dprintf(" IP:%s %s %s %s %s %s %s",
                    (v >> 6) & 1 ? "BL" : "- ",
                    (v >> 5) & 1 ? "TI" : "- ",
                    (v >> 4) & 1 ? "VI" : "- ",
                    (v >> 3) & 1 ? "A3" : "- ",
                    (v >> 2) & 1 ? "A2" : "- ",
                    (v >> 1) & 1 ? "A1" : "- ",
                    (v >> 0) & 1 ? "A0" : "- ");
            break;
        case XM_TIMER:
            v = xm_getw(TIMER);
            dprintf("0x%04x [%d.%04d s]", v, v / (uint16_t)10000, v % (uint16_t)10000);
            break;
        case XM_RD_XADDR:
            v = xm_getw(RD_XADDR);
            dprintf("0x%04x %s", v, val_name(xr_mem, v));
            break;
        case XM_WR_XADDR:
            v = xm_getw(WR_XADDR);
            dprintf("0x%04x %s", v, val_name(xr_mem, v));
            break;
        case XM_XDATA:
            xwait_mem_ready();
            prev = xm_getw(RD_XADDR);
            v    = xm_getw(XDATA);
            dprintf("[0x%04x]", v);
            read = xm_getw(RD_XADDR);
            ASSERT(read == (prev + 1), "0x%04x vs 0x%04x + 1", read, prev);
            xm_setw(RD_XADDR, prev - 1);
            xwait_mem_ready();
            break;
        case XM_RD_INCR:
            v = xm_getw(RD_INCR);
            dprintf("0x%04x", v);
            break;
        case XM_RD_ADDR:
            v = xm_getw(RD_ADDR);
            dprintf("0x%04x", v);
            break;
        case XM_WR_INCR:
            v = xm_getw(WR_INCR);
            dprintf("0x%04x", v);
            break;
        case XM_WR_ADDR:
            v = xm_getw(WR_ADDR);
            dprintf("0x%04x", v);
            break;
        case XM_DATA:
            xwait_mem_ready();
            prev = xm_getw(RD_ADDR);
            incr = xm_getw(RD_INCR);
            v    = xm_getw(DATA);
            dprintf("[0x%04x]", v);
            read = xm_getw(RD_ADDR);
            ASSERT(read == (prev + incr), "0x%04x vs 0x%04x + 0x%04x", read, prev, incr);
            xm_setw(RD_ADDR, prev - incr);
            xwait_mem_ready();
            break;
        case XM_DATA_2:
            xwait_mem_ready();
            prev = xm_getw(RD_ADDR);
            incr = xm_getw(RD_INCR) << 1;
            l    = xm_getl(DATA);
            v    = (uint16_t)l;
            dprintf("[0x%04x]", v);
            read = xm_getw(RD_ADDR);
            ASSERT(read == (prev + incr), "0x%04x vs 0x%04x + 0x%04x", read, prev, incr);
            xm_setw(RD_ADDR, prev - incr);
            xwait_mem_ready();
            break;
        case XM_PIXEL_X:
            v = xm_getw(PIXEL_X);
            dprintf("0x%04x", v);
            break;
        case XM_PIXEL_Y:
            v = xm_getw(PIXEL_Y);
            dprintf("0x%04x", v);
            break;
        case XM_UART:
            v = xm_getw(UART);
            dprintf("0x%04x", v);
            break;
        case XM_FEATURE:
            v = xm_getw(FEATURE);
            dprintf("0x%04x", v);
            break;
    }
}

void print_xm_regs()
{
    for (int r = 0; r < 16; r++)
    {
        print_xm_reg(r);
        dprintf("\n");
    }
}

void print_xr_regs()
{
    for (int r = 0; r < 0x20; r++)
    {
        if (r >= XR_UNUSED_08 && r <= XR_UNUSED_0F)
            continue;
        print_xr_reg(r);
        dprintf("\n");
    }
}

char line[4096];

void xosera_mon()
{
    //    printf("\033c\033[?25l");        // ANSI reset, disable input cursor
    printf("\033c");        // ANSI reset, disable input cursor
    cpu_delay(1000);

    dprintf("Xosera_mon_m68k\n");

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

#if 0
    dprintf("Installing Xosera test interrupt handler...");
    install_intr();
    dprintf("done.\n");
#else
    dprintf("NOT Installing test interrupt handler\n");
#endif

    dprintf("\n");

    if (xosera_sync())
    {
        print_xm_regs();
    }
    else
    {
        dprintf("*** Xosera not responding.\n\n");
    }


    bool exit = false;
    do
    {
        dprintf("\n*");
        dreadline(line, sizeof(line));

        char * line_ptr = line;

        char * cmd = next_token(&line_ptr);

        if (strcmp(cmd, "xm") == 0)
        {
            char * reg = next_token(&line_ptr);

            if (*reg == '\0')
            {
                print_xm_regs();
            }
            else
            {
                str_upper(reg);
                int r = name_val(xm_regs, reg);
                if (r < 0 && isdigit(*reg))
                {
                    r = strtol(reg, NULL, 0);
                }
                else
                {
                    dprintf("Bad register: \"%s\"\n", reg);
                    continue;
                }

                print_xm_reg(r);

                char * assign = next_token(&line_ptr);
                if (assign[0] == '=')
                {
                    assign++;
                    if (*assign == '\0')
                    {
                        assign = next_token(&line_ptr);
                    }

                    if (isdigit(*assign))
                    {
                        int v = strtol(assign, NULL, 0);

                        dprintf(" = 0x%04x, ", v);

                        print_xm_reg(r);
                    }
                }
            }
        }
        else if (strcmp(cmd, "xr") == 0)
        {
            print_xr_regs();
        }
        else if (strcmp(cmd, "Z") == 0)
        {
            char * num = next_token(&line_ptr);
            int    n   = strtol(num, NULL, 0);
            if (!isdigit(*num))
            {
                n = -1;
            }
            bool result = xosera_init(n);
            dprintf("xosera_init(%d) %s\n", n, result ? "succeeded" : "failed");
        }
        else if (strcmp(cmd, "exit") == 0)
        {
            exit = true;
        }
    } while (!exit);

    dprintf("\nExit...\n");

    // exit test
    reset_vid();
}
