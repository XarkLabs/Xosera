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

#include "xosera_api.h"

// Define rosco_m68k Xosera board base address pointer (See
// https://github.com/rosco-m68k/hardware-projects/blob/feature/xosera/xosera/code/pld/decoder/ic3_decoder.pld#L25)
volatile xreg_t * const xosera_ptr = (volatile xreg_t * const)0xf80060;        // rosco_m68k Xosera base

bool use_sd;

// dummy global variable
uint32_t global;        // this is used to prevent the compiler from optimizing out tests

uint32_t mem_buffer[128 * 1024];

// timer helpers
static uint32_t start_tick;

void timer_start()
{
    uint32_t ts = _TIMER_100HZ;
    uint32_t t;
    // this waits for a "fresh tick" to reduce timing jitter
    while ((t = _TIMER_100HZ) == ts)
        ;
    start_tick = t;
}

uint32_t timer_stop()
{
    uint32_t stop_tick = _TIMER_100HZ;

    return (stop_tick - start_tick) * 10;
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

bool delay_check(int ms)
{
    while (ms > 0)
    {
        if (checkchar())
        {
            return true;
        }

        int d = ms;
        if (d > 100)
        {
            d = 100;
        }
        delay(d);
        ms -= d;
    }

    return false;
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

uint16_t screen_addr;
uint8_t  text_color = 0x02;        // dark green on black
uint8_t  text_columns;
uint8_t  text_rows;
int8_t   text_h;
int8_t   text_v;

static void get_textmode_settings()
{
    //    int mode = 0;               // xv_reg_getw(gfxctrl);
    //    bool v_dbl     = mode & 2;
    int tile_size = 16;         //((xv_reg_getw(fontctrl) & 0xf) + 1) << (v_dbl ? 1 : 0);
    screen_addr   = 0;          // xv_reg_getw(dispstart);
    text_columns  = 106;        // xv_reg_getw(dispwidth);
    //    text_rows      = (xv_reg_getw(vidheight) + (tile_size - 1)) / tile_size;
    text_rows = 480 / tile_size;
}

static void xcls()
{
    get_textmode_settings();
    xv_setw(wr_addr, screen_addr);
    xv_setw(wr_inc, 1);
    xv_setbh(data, text_color);
    for (uint16_t i = 0; i < (text_columns * text_rows); i++)
    {
        xv_setbl(data, ' ');
    }
    xv_setw(wr_addr, screen_addr);
}

static void xmsg(int x, int y, int color, const char * msg)
{
    xv_setw(wr_addr, (y * text_columns) + x);
    xv_setbh(data, color);
    char c;
    while ((c = *msg++) != '\0')
    {
        xv_setbl(data, c);
    }
}

void test_hello()
{
    static const char test_string[] = "Xosera is mostly running happily on rosco_m68k";
    static uint16_t   test_read[sizeof(test_string)];

    xcls();
    xmsg(0, 0, 0xa, "WROTE:");
    xv_setw(wr_inc, 1);                            // set write inc
    xv_setw(wr_addr, 0x0008);                      // set write address
    xv_setw(data, 0x0200 | test_string[0]);        // set full word
    for (size_t i = 1; i < sizeof(test_string) - 1; i++)
    {
        if (i == sizeof(test_string) - 5)
        {
            xv_setbh(data, 0x04);        // test setting bh only (saved, VRAM not altered)
        }
        xv_setbl(data, test_string[i]);        // set byte, will use continue using previous high byte (0x20)
    }

    // read test
    dprintf("Read VRAM test, with auto-increment.\n\n");
    dprintf(" Begin: rd_addr=0x0000, rd_inc=0x0001\n");
    xv_setw(rd_inc, 1);
    xv_setw(rd_addr, 0x0008);
    uint16_t * tp = test_read;
    for (uint16_t c = 0; c < (sizeof(test_string) - 1); c++)
    {
        *tp++ = xv_getw(data);
    }
    uint16_t end_addr = xv_getw(rd_addr);

    xmsg(0, 2, 0xa, "READ:");
    xv_setw(wr_inc, 1);                              // set write inc
    xv_setw(wr_addr, (text_columns * 2) + 8);        // set write address

    bool good = true;
    for (size_t i = 0; i < sizeof(test_string) - 1; i++)
    {
        uint16_t v = test_read[i];
        xv_setw(data, v);
        if ((v & 0xff) != test_string[i])
        {
            good = false;
        }
    }
    // incremented one extra, because data was already pre-read
    if (end_addr != sizeof(test_string) + 8)
    {
        good = false;
    }
    dprintf("   End: rd_addr=0x%04x.  Test: ", end_addr);
    dprintf("%s\n", good ? "good" : "BAD!");
}

void test_vram_speed()
{
    xcls();
    xv_setw(wr_inc, 1);
    xv_setw(wr_addr, 0x0000);
    xv_setw(rd_inc, 1);
    xv_setw(rd_addr, 0x0000);

    int vram_write = 0;
    int vram_read  = 0;
    int main_write = 0;
    int main_read  = 0;

    int reps = 16;        // just a few flashes for write test
    xmsg(0, 0, 0x02, "VRAM write     ");
    dprintf("VRAM write x %d\n", reps);
    uint32_t v = ((0x0f00 | 'G') << 16) | (0xf000 | 'o');
    timer_start();
    for (int loop = 0; loop < reps; loop++)
    {
        uint16_t count = 0x8000;        // VRAM long count
        do
        {
            xv_setl(data, v);
        } while (--count);
        v ^= 0xff00ff00;
    }
    vram_write = timer_stop();
    global     = v;        // save v so GCC doesn't optimize away test
    if (checkchar())
    {
        return;
    }
    reps = 32;        // main ram test (NOTE: I am not even incrementing pointer below - like "fake
                      // register" write)
    xmsg(0, 0, 0x02, "main RAM write ");
    dprintf("main RAM write x %d\n", reps);
    timer_start();
    for (int loop = 0; loop < reps; loop++)
    {
        uint32_t * ptr   = mem_buffer;
        uint16_t   count = 0x8000;        // VRAM long count
        do
        {
            //            *ptr++ = loop;    // GCC keeps trying to be clever, we want a fair test
            __asm__ __volatile__("move.l %[loop],(%[ptr])" : : [loop] "d"(loop), [ptr] "a"(ptr) :);
        } while (--count);
        v ^= 0xff00ff00;
    }
    main_write = timer_stop();
    global     = v;        // save v so GCC doesn't optimize away test
    if (checkchar())
    {
        return;
    }
    reps = 32;        // a bit longer read test (to show stable during read)
    xmsg(0, 0, 0x02, "VRAM read      ");
    dprintf("VRAM read x %d\n", reps);
    timer_start();
    for (int loop = 0; loop < reps; loop++)
    {
        uint16_t count = 0x8000;        // VRAM long count
        do
        {
            v = xv_getl(data);
        } while (--count);
    }
    vram_read = timer_stop();
    global    = v;        // save v so GCC doesn't optimize away test
    if (checkchar())
    {
        return;
    }
    reps = 32;        // main ram test (NOTE: I am not even incrementing pointer below - like "fake
                      // register" read)
    xmsg(0, 0, 0x02, "main RAM read  ");
    dprintf("main RAM read x %d\n", reps);
    timer_start();
    for (int loop = 0; loop < reps; loop++)
    {
        uint32_t * ptr   = mem_buffer;
        uint16_t   count = 0x8000;        // VRAM long count
        do
        {
            //            v += *ptr++;    // GCC keeps trying to be clever, we want a fair test
            __asm__ __volatile__("move.l (%[ptr]),%[v]" : [v] "+d"(v) : [ptr] "a"(ptr) :);
        } while (--count);
        v ^= 0xff00ff00;
    }
    main_read = timer_stop();
    global    = v;         // save v so GCC doesn't optimize away test
    reps      = 32;        // a bit longer read test (to show stable during read)
    xmsg(0, 0, 0x02, "VRAM slow read ");
    dprintf("VRAM slow read x %d\n", reps);
    timer_start();
    for (int loop = 0; loop < reps; loop++)
    {
        uint16_t count = 0x8000;        // VRAM long count
        do
        {
            xv_setw(rd_addr, 0);
            v = xv_getbl(data);
        } while (--count);
    }
    vram_read = timer_stop();
    global    = v;        // save v so GCC doesn't optimize away test
    if (checkchar())
    {
        return;
    }
    reps = 32;        // a bit longer read test (to show stable during read)
    xmsg(0, 0, 0x02, "VRAM slow read2");
    dprintf("VRAM slow read2 x %d\n", reps);
    timer_start();
    for (int loop = 0; loop < reps; loop++)
    {
        uint16_t count = 0x8000;        // VRAM long count
        do
        {
            xv_setw(rd_addr, count & 0xff);
            v = xv_getbl(data);
        } while (--count);
    }
    vram_read = timer_stop();
    global    = v;        // save v so GCC doesn't optimize away test
    if (checkchar())
    {
        return;
    }
    dprintf("done\n");

    dprintf("MOVEP.L VRAM write      128KB x 16 (2MB)    %d ms (%d KB/sec)\n",
            vram_write,
            (1000 * 128 * reps) / vram_write);
    dprintf(
        "MOVEP.L VRAM read       128KB x 16 (2MB)    %d ms (%d KB/sec)\n", vram_read, (1000 * 128 * reps) / vram_read);
    dprintf("MOVE.L  main RAM write  128KB x 16 (2MB)    %d ms (%d KB/sec)\n",
            main_write,
            (1000 * 128 * reps) / main_write);
    dprintf(
        "MOVE.L  main RAM read   128KB x 16 (2MB)    %d ms (%d KB/sec)\n", main_read, (1000 * 128 * reps) / main_read);
}

uint16_t rosco_m68k_CPUMHz()
{
    uint32_t count;
    uint32_t tv;
    __asm__ __volatile__(
        "   moveq.l #0,%[count]\n"
        "   move.w  _TIMER_100HZ+2.w,%[tv]\n"
        "0: cmp.w   _TIMER_100HZ+2.w,%[tv]\n"
        "   beq.s   0b\n"
        "   move.w  _TIMER_100HZ+2.w,%[tv]\n"
        "1: addq.w  #1,%[count]\n"                   //   4  cycles
        "   cmp.w   _TIMER_100HZ+2.w,%[tv]\n"        //  12  cycles
        "   beq.s   1b\n"                            // 10/8 cycles (taken/not)
        : [count] "=d"(count), [tv] "=&d"(tv)
        :
        :);
    uint16_t MHz = ((count * 26) + 500) / 1000;
    dprintf("rosco_m68k: m68k CPU speed %d.%d MHz (%d.%d BogoMIPS)\n",
            MHz / 10,
            MHz % 10,
            count * 3 / 10000,
            ((count * 3) % 10000) / 10);

    return (MHz + 5) / 10;
}

static void load_sd_bitmap(const char * filename)
{
    dprintf("Loading bitmap: \"%s\"", filename);
    void * file = fl_fopen(filename, "r");

    if (file != NULL)
    {
        int cnt   = 0;
        int vaddr = 0;

        while ((cnt = fl_fread(mem_buffer, 1, 512, file)) > 0)
        {
            /* period every 4KiB, does not noticeably affect speed */
            if (!(vaddr % 0x7))
            {
                dprintf(".");
            }

            uint16_t * maddr = (uint16_t *)mem_buffer;
            xv_setw(wr_addr, vaddr);
            for (int i = 0; i < (cnt >> 1); i++)
            {
                xv_setw(data, *maddr++);
            }
            vaddr += (cnt >> 1);
        }

        fl_fclose(file);
        dprintf("done!\n");
    }
    else
    {
        dprintf(" - FAILED\n");
    }
}

static void load_sd_palette(const char * filename)
{
    dprintf("Loading colormap: \"%s\"", filename);
    void * file = fl_fopen(filename, "r");

    if (file != NULL)
    {
        int cnt   = 0;
        int vaddr = 0;

        while ((cnt = fl_fread(mem_buffer, 1, 512, file)) > 0)
        {
            /* period every 4KiB, does not noticeably affect speed */
            if (!(vaddr % 0x7))
            {
                dprintf(".");
            }

            uint16_t * maddr = (uint16_t *)mem_buffer;
            xv_setw(aux_addr, XV_AUX_COLORMEM);
            for (int i = 0; i < (cnt >> 1); i++)
            {
                xv_setw(aux_data, *maddr++);
            }
            vaddr += (cnt >> 1);
        }

        fl_fclose(file);
        dprintf("done!\n");
    }
    else
    {
        dprintf(" - FAILED\n");
    }
}

uint32_t test_count;
void     xosera_test()
{
    // flush any input charaters to avoid instant exit
    while (checkchar())
    {
        readchar();
    }

    dprintf("Xosera_test_m68k\n");

    dprintf("\nxosera_init(1)...");
    // wait for monitor to unblank
    bool success = xosera_init(0);
    dprintf("%s (%dx%d)\n", success ? "succeeded" : "FAILED", xv_reg_getw(vidwidth), xv_reg_getw(vidheight));

    if (delay_check(4000))
    {
        return;
    }

    while (true)
    {
        xcls();
        dprintf("*** xosera_test_m68k iteration: %d\n", test_count++);
        rosco_m68k_CPUMHz();

        uint32_t githash   = (xv_reg_getw(githash_h) << 16) | xv_reg_getw(githash_l);
        uint16_t width     = xv_reg_getw(vidwidth);
        uint16_t height    = xv_reg_getw(vidheight);
        uint16_t features  = xv_reg_getw(features);
        uint16_t dispstart = xv_reg_getw(dispstart);
        uint16_t dispwidth = xv_reg_getw(dispwidth);
        uint16_t scrollxy  = xv_reg_getw(scrollxy);
        uint16_t gfxctrl   = xv_reg_getw(gfxctrl);

        dprintf("Xosera #%08x\n", githash);
        dprintf("Mode: %dx%d  Features:0x%04x\n", width, height, features);
        dprintf("dispstart:0x%04x dispwidth:0x%04x\n", dispstart, dispwidth);
        dprintf(" scrollxy:0x%04x   gfxctrl:0x%04x\n", scrollxy, gfxctrl);

        for (int y = 0; y < 30; y += 3)
        {
            xmsg(20, y, (y & 0xf) ? (y & 0xf) : 0xf0, ">>> Xosera rosco_m68k test utility <<<<");
        }

        if (delay_check(DELAY_TIME))
        {
            break;
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
        // 4/8 bpp test
        if (use_sd)
        {
            xv_reg_setw(gfxctrl, 0x00F5);        // enable + bitmap + 8-bpp + Hx2 + Vx2
            xv_reg_setw(dispwidth, 160);

            load_sd_palette("/xosera_r1_pal.raw");
            load_sd_bitmap("/xosera_r1.raw");
            if (delay_check(DELAY_TIME))
            {
                break;
            }
            xv_reg_setw(gfxctrl, 0x0080);
        }

        if (use_sd)
        {
            xv_reg_setw(gfxctrl, 0x00E5);        // enable + bitmap + 4-bpp + Hx2 + Vx2
            load_sd_palette("/ST_KingTut_Dpaint_16_pal.raw");
            load_sd_bitmap("/ST_KingTut_Dpaint_16.raw");
            if (delay_check(DELAY_TIME))
            {
                break;
            }
            xv_reg_setw(gfxctrl, 0x0080);
        }
        if (use_sd)
        {
            xv_reg_setw(gfxctrl, 0x00E5);        // enable + bitmap + 4-bpp + Hx2 + Vx2
            load_sd_palette("/escher-relativity_320x240_16_pal.raw");
            load_sd_bitmap("/escher-relativity_320x240_16.raw");
            if (delay_check(DELAY_TIME))
            {
                break;
            }
            xv_reg_setw(gfxctrl, 0x0080);
        }
        if (use_sd)
        {
            xv_reg_setw(gfxctrl, 0x00C0);        // enable + bitmap + 1-bpp + Hx1 + Vx1
            load_sd_bitmap("/space_shuttle_color_small.raw");
            if (delay_check(DELAY_TIME))
            {
                break;
            }
            xv_reg_setw(gfxctrl, 0x0080);
        }

        if (use_sd)
        {
            xv_reg_setw(gfxctrl, 0x00C0);        // enable + bitmap + 1-bpp + Hx1 + Vx1
            load_sd_bitmap("/mountains_mono_640x480w.raw");
            if (delay_check(DELAY_TIME))
            {
                break;
            }
            xv_reg_setw(gfxctrl, 0x0080);
        }

        if (use_sd)
        {
            xv_reg_setw(gfxctrl, 0x00C0);        // enable + bitmap + 1-bpp + Hx1 + Vx1
            load_sd_bitmap("/escher-relativity_640x480w.raw");
            if (delay_check(DELAY_TIME))
            {
                break;
            }
            xv_reg_setw(gfxctrl, 0x0080);
        }

        test_hello();
        if (delay_check(DELAY_TIME))
        {
            break;
        }

        test_vram_speed();
        if (delay_check(DELAY_TIME))
        {
            break;
        }
    }
    xv_reg_setw(gfxctrl, 0x0080);

    while (checkchar())
    {
        readchar();
    }
}
