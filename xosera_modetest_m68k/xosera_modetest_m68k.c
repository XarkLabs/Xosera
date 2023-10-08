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

// #define DELAY_TIME 15000        // slow human speed
// #define DELAY_TIME 5000        // human speed
// #define DELAY_TIME 1000        // impatient human speed
#define DELAY_TIME 500        // machine speed

#if !defined(_NOINLINE)
#define _NOINLINE __attribute__((noinline))
#endif

#if !defined(_UNUSED)
#define _UNUSED __attribute__((unused))
#endif

#if !defined(NUM_ELEMENTS)
#define NUM_ELEMENTS(a) (sizeof(a) / sizeof(a[0]))
#endif

#include "xosera_m68k_api.h"

extern void install_intr(void);
extern void remove_intr(void);

extern volatile uint32_t XFrameCount;
extern volatile uint16_t NukeColor;

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

#if !defined(checkchar)        // newer rosco_m68k library addition, this is in case not present
bool checkchar()
{
    int rc;
    __asm__ __volatile__(
        "move.l #6,%%d1\n"        // CHECKCHAR
        "trap   #14\n"
        "move.b %%d0,%[rc]\n"
        "ext.w  %[rc]\n"
        "ext.l  %[rc]\n"
        : [rc] "=d"(rc)
        :
        : "d0", "d1");
    return rc != 0;
}
#endif

// resident _EFP_SD_INIT hook to disable SD loader upon next boot
static void disable_sd_boot()
{
    extern void resident_init();        // no SD boot resident setup
    resident_init();                    // install no SD hook next next warm-start
}

static void wait_vblank_start()
{
    xwait_not_vblank();
    xwait_vblank();
}

static inline void check_vblank()
{
    if (!xm_getb_sys_ctrl(VBLANK) || xreg_getw(SCANLINE) > 520)
    {
        wait_vblank_start();
    }
}

_NOINLINE void restore_def_colors()
{
    wait_vblank_start();
    xmem_setw_next_addr(XR_COLOR_A_ADDR);
    for (uint16_t i = 0; i < 256; i++)
    {
        xmem_setw_next(def_colors[i]);
    }
    // set B colors to same, alpha 0x8 (with color 0 fully transparent)
    xmem_setw(XR_COLOR_B_ADDR, 0x0000);
    for (uint16_t i = 1; i < 256; i++)
    {
        xmem_setw_next(0x8000 | def_colors[i]);
    }
}

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

static void hexdump(void * ptr, size_t bytes)
{
    uint8_t * p = (uint8_t *)ptr;
    for (size_t i = 0; i < bytes; i++)
    {
        if ((i & 0xf) == 0)
        {
            if (i)
            {
                dprintf("    ");
                for (size_t j = i - 16; j < i; j++)
                {
                    int c = p[j];
                    dprintf("%c", c >= ' ' && c <= '~' ? c : '_');
                    // dprintf("%c", isprint(c) ? c : '_');
                }
                dprintf("\n");
            }
            dprintf("%04x: ", i);
        }
        else
        {
            dprintf(", ");
        }
        dprintf("%02x", p[i]);
    }
    dprintf("\n");
}

static void reset_video(void)
{
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
    xreg_setw(PA_H_SCROLL, 0x0000);
    xreg_setw(PA_V_SCROLL, 0x0000);
    xreg_setw(PA_HV_FSCALE, 0x0000);
    xreg_setw(PB_GFX_CTRL, 0x0080);

    restore_def_colors();

    printf("\033c");        // reset XANSI

    char c = 0;
    while (checkchar())
    {
        c = readchar();
    }

#if 1        // handy for development to force Kermit upload
    if (c == '\x1b')
    {
        dprintf("Disabling SD on next boot...\n");
        disable_sd_boot();
    }
#endif
}

_NOINLINE void delay_check(int ms)
{
    while (ms--)
    {
        if (checkchar())
        {
            break;
        }
        uint16_t tms = 10;
        do
        {
            uint16_t tv = xm_getw(TIMER);
            while (tv == xm_getw(TIMER))
                ;
        } while (--tms);
    }
}

uint32_t font[16 * 7] = {
    // 0
    0x00ff0000,        // .#..
    0xff00ff00,        // #.#.
    0xff00ff00,        // #.#.
    0xff00ff00,        // #.#.
    0xff00ff00,        // #.#.
    0xff00ff00,        // #.#.
    0x00ff0000,        // .#..
    // 1
    0x00ff0000,        // .#..
    0xffff0000,        // ##..
    0x00ff0000,        // .#..
    0x00ff0000,        // .#..
    0x00ff0000,        // .#..
    0x00ff0000,        // .#..
    0xffffff00,        // ###.
    // 2
    0xffffff00,        // ###.
    0x0000ff00,        // ..#.
    0x0000ff00,        // ..#.
    0xffffff00,        // ###.
    0xff000000,        // #...
    0xff000000,        // #...
    0xffffff00,        // ###.
    // 3
    0xffffff00,        // ###.
    0x0000ff00,        // ..#.
    0x0000ff00,        // ..#.
    0xffffff00,        // ###.
    0x0000ff00,        // ..#.
    0x0000ff00,        // ..#.
    0xffffff00,        // ###.
    // 4
    0xff00ff00,        // #.#.
    0xff00ff00,        // #.#.
    0xff00ff00,        // #.#.
    0xffffff00,        // ###.
    0x0000ff00,        // ..#.
    0x0000ff00,        // ..#.
    0x0000ff00,        // ..#.
    // 5
    0xffffff00,        // ###.
    0xff000000,        // #...
    0xff000000,        // #...
    0xffffff00,        // ###.
    0x0000ff00,        // ..#.
    0x0000ff00,        // ..#.
    0xffffff00,        // ###.
    // 6
    0x00ffff00,        // ###.
    0xff000000,        // #...
    0xff000000,        // #...
    0xffffff00,        // ###.
    0xff00ff00,        // #.#.
    0xff00ff00,        // #.#.
    0xffffff00,        // ###.
    // 7
    0xffffff00,        // ###.
    0x0000ff00,        // ..#.
    0x0000ff00,        // ..#.
    0x0000ff00,        // ..#.
    0x0000ff00,        // ..#.
    0x0000ff00,        // ..#.
    0x0000ff00,        // ..#.
    // 8
    0xffffff00,        // ###.
    0xff00ff00,        // #.#.
    0xff00ff00,        // #.#.
    0xffffff00,        // ###.
    0xff00ff00,        // #.#.
    0xff00ff00,        // #.#.
    0xffffff00,        // ###.
    // 9
    0xffffff00,        // ###.
    0xff00ff00,        // #.#.
    0xff00ff00,        // #.#.
    0xffffff00,        // ###.
    0x0000ff00,        // ..#.
    0x0000ff00,        // ..#.
    0xffff0000,        // ###.
    // 8
    0x00ff0000,        // .#..
    0xff00ff00,        // #.#.
    0xff00ff00,        // #.#.
    0xffffff00,        // ###.
    0xff00ff00,        // #.#.
    0xff00ff00,        // #.#.
    0xff00ff00,        // #.#.
    // 8
    0xffff0000,        // ##..
    0xff00ff00,        // #.#.
    0xff00ff00,        // #.#.
    0xffff0000,        // ##..
    0xff00ff00,        // #.#.
    0xff00ff00,        // #.#.
    0xffff0000,        // ##..
    // 8
    0x00ffff00,        // .##.
    0xff000000,        // #...
    0xff000000,        // #...
    0xff000000,        // #...
    0xff000000,        // #...
    0xff000000,        // #...
    0x00ffff00,        // .##.
    // 8
    0xffff0000,        // ##..
    0xff00ff00,        // #.#.
    0xff00ff00,        // #.#.
    0xff00ff00,        // #.#.
    0xff00ff00,        // #.#.
    0xff00ff00,        // #.#.
    0xffff0000,        // ##..
    // 8
    0xffffff00,        // ###.
    0xff000000,        // #...
    0xff000000,        // #...
    0xffffff00,        // ###.
    0xff000000,        // #...
    0xff000000,        // #...
    0xffffff00,        // ###.
    // 8
    0xffffff00,        // ###.
    0xff000000,        // #...
    0xff000000,        // #...
    0xffff0000,        // ##..
    0xff000000,        // #...
    0xff000000,        // #...
    0xff000000         // #...
};

void print_digit(uint16_t off, uint16_t ll, uint16_t dig, uint16_t color)
{
    union lw
    {
        uint32_t l;
        uint16_t w[2];
    };

    union lw * lwp = (union lw *)&font[dig * 7];

    xm_setw(WR_INCR, 0x0001);        // set write inc
    for (uint16_t h = 0; h < 7; h++)
    {
        xm_setw(WR_ADDR, off + (h * ll));        // set write address
        xm_setbl(SYS_CTRL, (lwp->w[0] & 0x8000 ? 0xc : 0) | (lwp->w[0] & 0x0080 ? 0x3 : 0));
        xm_setw(DATA, lwp->w[0] & color);
        xm_setbl(SYS_CTRL, (lwp->w[1] & 0x8000 ? 0xc : 0) | (lwp->w[1] & 0x0080 ? 0x3 : 0));
        xm_setw(DATA, lwp->w[1] & color);
        lwp++;
    }
    xm_setbl(SYS_CTRL, 0xf);
}

void test_colormap()
{
    xwait_not_vblank();
    xwait_vblank();

    xreg_setw(VID_CTRL, 0x0005);
    xreg_setw(PA_GFX_CTRL, 0x0080);
    xreg_setw(PB_GFX_CTRL, 0x0080);

    xm_setw(WR_INCR, 0x0001);        // set write inc
    xm_setw(WR_ADDR, 0x0000);        // set write address

    for (int i = 0; i < 65536; i++)
    {
        xm_setw(DATA, 0x0000);
    }

    xwait_not_vblank();
    xwait_vblank();

    uint16_t linelen = 160;
    uint16_t w       = 10;
    uint16_t h       = 14;

    xreg_setw(VID_CTRL, 0x0000);
    xreg_setw(VID_LEFT, (xosera_vid_width() > 640 ? ((xosera_vid_width() - 640) / 2) : 0) + 0);
    xreg_setw(VID_RIGHT, (xosera_vid_width() > 640 ? (xosera_vid_width() - 640) / 2 : 0) + 640);
    xreg_setw(PA_GFX_CTRL, 0x0065);
    xreg_setw(PA_TILE_CTRL, 0x0C07);
    xreg_setw(PA_DISP_ADDR, 0x0000);
    xreg_setw(PA_LINE_LEN, linelen);        // line len
    xreg_setw(PA_H_SCROLL, 0x0000);
    xreg_setw(PA_V_SCROLL, 0x0000);
    xreg_setw(PA_HV_FSCALE, 0x0000);
    xreg_setw(PB_GFX_CTRL, 0x0080);

    xm_setw(WR_INCR, 0x0001);        // set write inc
    xm_setw(WR_ADDR, 0x0000);        // set write address

    uint16_t c = 0;

    for (uint16_t y = 0; y < 16; y++)
    {
        for (uint16_t yp = y * h; yp < ((y + 1) * h) - 2; yp++)
        {
            xm_setw(WR_ADDR, (linelen * (yp + 15)));
            c = y * 16;
            for (uint16_t x = 0; x < 16; x++)
            {
                for (uint16_t xp = x * w; xp < ((x + 1) * w) - 1; xp++)
                {
                    xm_setw(DATA, c << 8 | c);
                }
                xm_setw(DATA, 0x0000);
                c++;
            }
        }
    }

    c = 0;
    for (uint16_t y = 0; y < 16; y++)
    {
        for (uint16_t x = 0; x < 16; x++)
        {
            uint16_t col = xmem_getw_wait(XR_COLOR_A_ADDR + c);
            uint16_t off = (linelen * (h * y + 18)) + (x * w) + 2;
            print_digit(off, linelen, c / 100, ((col & 0x0880) == 0x880) ? 0x0000 : 0xffff);
            off += 2;
            print_digit(off, linelen, (c / 10) % 10, ((col & 0x0880) == 0x880) ? 0x0000 : 0xffff);
            off += 2;
            print_digit(off, linelen, c % 10, ((col & 0x0880) == 0x880) ? 0x0000 : 0xffff);
            c++;
        }
    }

    delay_check(DELAY_TIME * 3);

    c = 0;
    for (uint16_t y = 0; y < 16; y++)
    {
        for (uint16_t yp = y * h; yp < ((y + 1) * h) - 2; yp++)
        {
            xm_setw(WR_ADDR, (linelen * (yp + 15)));
            c = y * 16;
            for (uint16_t x = 0; x < 16; x++)
            {
                for (uint16_t xp = x * w; xp < ((x + 1) * w) - 1; xp++)
                {
                    xm_setw(DATA, c << 8 | c);
                }
                xm_setw(DATA, 0x0000);
                c++;
            }
        }
    }

    c = 0;
    for (uint16_t y = 0; y < 16; y++)
    {
        for (uint16_t x = 0; x < 16; x++)
        {
            uint16_t col = xmem_getw_wait(XR_COLOR_A_ADDR + c);
            uint16_t off = (linelen * (h * y + 18)) + (x * w) + 3;
            print_digit(off, linelen, c / 16, ((col & 0x0880) == 0x880) ? 0x0000 : 0xffff);
            off += 2;
            print_digit(off, linelen, c & 0xf, ((col & 0x0880) == 0x880) ? 0x0000 : 0xffff);
            c++;
        }
    }

    delay_check(DELAY_TIME * 3);
}

void xosera_modetest(void)
{
    printf("\033c\033[?25l");        // ANSI reset, disable input cursor

    dprintf("Xosera_modetest_m68k\n");

    cpu_delay(3000);

    while (checkchar())        // clear any queued input
    {
        readchar();
    }

    dprintf("\nCalling xosera_sync()...");
    bool syncok = xosera_sync();
    dprintf("%s\n\n", syncok ? "succeeded" : "FAILED");

    dprintf("\nCalling xosera_init(XINIT_CONFIG_640x480)...");
    bool success = xosera_init(XINIT_CONFIG_640x480);
    dprintf("%s (%dx%d)\n\n", success ? "succeeded" : "FAILED", xosera_vid_width(), xosera_vid_height());

    cpu_delay(1000);
    xosera_get_info(&initinfo);
    dprintf("xosera_get_info details:\n");
    xv_prep();
    xmem_getw_next_addr(XR_COPPER_ADDR);

    dprintf("\n");
    dprintf("Description : \"%s\"\n", initinfo.description_str);
    dprintf("Version BCD : %x.%02x\n", initinfo.version_bcd >> 8, initinfo.version_bcd & 0xff);
    dprintf("Git hash    : #%08x %s\n", initinfo.githash, initinfo.git_modified ? "[modified]" : "[clean]");

    cpu_delay(1000);

    dprintf("\nBegin...\n");

    while (!checkchar())
    {
        wait_vblank_start();

        restore_def_colors();
        test_colormap();
    }

    reset_video();
}
