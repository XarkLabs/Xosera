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
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <basicio.h>
#include <machine.h>
#include <sdfat.h>

//#define DELAY_TIME 15000        // slow human speed
#define DELAY_TIME 5000        // human speed
//#define DELAY_TIME 1000        // impatient human speed
//#define DELAY_TIME 100        // machine speed

#define COPPER_TEST 1

#if !defined(NUM_ELEMENTS)
#define NUM_ELEMENTS(a) (sizeof(a) / sizeof(a[0]))
#endif

#include "xosera_m68k_api.h"

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

#if COPPER_TEST
// Copper list
const uint16_t copper_list[] = {
    0xb000,
    0x0000,        // movep 0, 0x0F00          ; Make color 0 red
                   //
    30 * 1,
    0x0002,        // wait  0, 160, 0b000010   ; Wait for line 160, ignore X position
    0xb000,
    0x0111,        // movep 0, 0x00F0          ; Make color 0 green
    //
    30 * 2,
    0x0002,        // wait  0, 160, 0b000010   ; Wait for line 160, ignore X position
    0xb000,
    0x0222,        // movep 0, 0x00F0          ; Make color 0 green
    //
    30 * 3,
    0x0002,        // wait  0, 160, 0b000010   ; Wait for line 160, ignore X position
    0xb000,
    0x0333,        // movep 0, 0x00F0          ; Make color 0 green
    //
    30 * 4,
    0x0002,        // wait  0, 160, 0b000010   ; Wait for line 160, ignore X position
    0xb000,
    0x0444,        // movep 0, 0x00F0          ; Make color 0 green
    //
    30 * 5,
    0x0002,        // wait  0, 160, 0b000010   ; Wait for line 160, ignore X position
    0xb000,
    0x0555,        // movep 0, 0x00F0          ; Make color 0 green
    //
    30 * 6,
    0x0002,        // wait  0, 160, 0b000010   ; Wait for line 160, ignore X position
    0xb000,
    0x0666,        // movep 0, 0x00F0          ; Make color 0 green
    //
    30 * 7,
    0x0002,        // wait  0, 160, 0b000010   ; Wait for line 160, ignore X position
    0xb000,
    0x0777,        // movep 0, 0x00F0          ; Make color 0 green
    //
    30 * 8,
    0x0002,        // wait  0, 160, 0b000010   ; Wait for line 160, ignore X position
    0xb000,
    0x0888,        // movep 0, 0x00F0          ; Make color 0 green
    //
    30 * 9,
    0x0002,        // wait  0, 160, 0b000010   ; Wait for line 160, ignore X position
    0xb000,
    0x0999,        // movep 0, 0x00F0          ; Make color 0 green
    //
    30 * 10,
    0x0002,        // wait  0, 160, 0b000010   ; Wait for line 160, ignore X position
    0xb000,
    0x0AAA,        // movep 0, 0x00F0          ; Make color 0 green
    //
    30 * 11,
    0x0002,        // wait  0, 160, 0b000010   ; Wait for line 160, ignore X position
    0xb000,
    0x0BBB,        // movep 0, 0x00F0          ; Make color 0 green
    //
    30 * 12,
    0x0002,        // wait  0, 160, 0b000010   ; Wait for line 160, ignore X position
    0xb000,
    0x0ccc,        // movep 0, 0x00F0          ; Make color 0 green
    //
    30 * 13,
    0x0002,        // wait  0, 160, 0b000010   ; Wait for line 160, ignore X position
    0xb000,
    0x0ddd,        // movep 0, 0x00F0          ; Make color 0 green
    //
    30 * 14,
    0x0002,        // wait  0, 160, 0b000010   ; Wait for line 160, ignore X position
    0xb000,
    0x0eee,        // movep 0, 0x00F0          ; Make color 0 green
    //
    30 * 15,
    0x0002,        // wait  0, 160, 0b000010   ; Wait for line 160, ignore X position
    0xb000,
    0x0fff,        // movep 0, 0x00F0          ; Make color 0 green
    //
    30 * 16,
    0x0002,        // wait  0, 160, 0b000010   ; Wait for line 160, ignore X position
    0xb000,
    0x0000,        // movep 0, 0x00F0          ; Make color 0 green

    // end
    0x0000,
    0x0003        // wait  0, 0, 0b000011     ; Wait for next frame
};
const uint16_t copper_list_len = NUM_ELEMENTS(copper_list);

static_assert(NUM_ELEMENTS(copper_list) < 1024, "copper list too long");
#endif

// dummy global variable
uint32_t global;        // this is used to prevent the compiler from optimizing out tests

uint32_t mem_buffer[128 * 1024];

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

_NOINLINE bool delay_check(int ms)
{
    while (ms--)
    {
        if (checkchar())
        {
            return true;
        }

        uint16_t tms = 10;
        do
        {
            uint8_t tvb = xm_getbl(TIMER);
            while (tvb == xm_getbl(TIMER))
                ;
        } while (--tms);
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
uint8_t  text_columns;
uint8_t  text_rows;
int8_t   text_h;
int8_t   text_v;
uint8_t  text_color = 0x02;        // dark green on black

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

static void xmsg(int x, int y, int color, const char * msg)
{
    xm_setw(WR_ADDR, (y * text_columns) + x);
    xm_setbh(DATA, color);
    char c;
    while ((c = *msg++) != '\0')
    {
        xm_setbl(DATA, c);
    }
}

void wait_vsync()
{
    while (xreg_getw(SCANLINE) >= 0x8000)
        ;
    while (xreg_getw(SCANLINE) < 0x8000)
        ;
}

_NOINLINE void restore_colors()
{
    xm_setw(XR_ADDR, XR_COLOR_MEM);
    uint16_t * cp = def_colors;
    for (uint16_t i = 256; i != 0; i--)
    {
        xm_setw(XR_DATA, *cp++);
    };
}

void test_hello()
{
    static const char test_string[] = "Xosera is mostly running happily on rosco_m68k";
    static uint16_t   test_read[sizeof(test_string)];

    xcls();
    xmsg(0, 0, 0xa, "WROTE:");
    xm_setw(WR_INCR, 1);                           // set write inc
    xm_setw(WR_ADDR, 0x0008);                      // set write address
    xm_setw(DATA, 0x0200 | test_string[0]);        // set full word
    for (size_t i = 1; i < sizeof(test_string) - 1; i++)
    {
        if (i == sizeof(test_string) - 5)
        {
            xm_setbh(DATA, 0x04);        // test setting bh only (saved, VRAM not altered)
        }
        xm_setbl(DATA, test_string[i]);        // set byte, will use continue using previous high byte (0x20)
    }

    // read test
    dprintf("Read VRAM test, with auto-increment.\n\n");
    dprintf(" Begin: rd_addr=0x0000, rd_inc=0x0001\n");
    xm_setw(RD_INCR, 1);
    xm_setw(RD_ADDR, 0x0008);
    uint16_t * tp = test_read;
    for (uint16_t c = 0; c < (sizeof(test_string) - 1); c++)
    {
        *tp++ = xm_getw(DATA);
    }
    uint16_t end_addr = xm_getw(RD_ADDR);

    xmsg(0, 2, 0xa, "READ:");
    xm_setw(WR_INCR, 1);                             // set write inc
    xm_setw(WR_ADDR, (text_columns * 2) + 8);        // set write address

    bool good = true;
    for (size_t i = 0; i < sizeof(test_string) - 1; i++)
    {
        uint16_t v = test_read[i];
        xm_setw(DATA, v);
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
    xv_prep();
    xm_setw(WR_INCR, 1);
    xm_setw(WR_ADDR, 0x0000);
    xm_setw(RD_INCR, 1);
    xm_setw(RD_ADDR, 0x0000);

    uint32_t vram_write = 0;
    uint32_t vram_read  = 0;
    uint32_t main_write = 0;
    uint32_t main_read  = 0;

    uint16_t reps = 16;        // just a few flashes for write test
    xmsg(0, 0, 0x02, "VRAM write     ");
    dprintf("VRAM write x %d\n", reps);
    uint32_t v = ((0x0f00 | 'G') << 16) | (0xf000 | 'o');
    timer_start();
    for (int loop = 0; loop < reps; loop++)
    {
        uint16_t count = 0x8000;        // VRAM long count
        do
        {
            xm_setl(DATA, v);
        } while (--count);
        v ^= 0xff00ff00;
    }
    vram_write = timer_stop();
    global     = v;        // save v so GCC doesn't optimize away test
    if (checkchar())
    {
        return;
    }
    reps = 16;        // main ram test (NOTE: I am not even incrementing pointer below - like "fake
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
    reps = 16;        // a bit longer read test (to show stable during read)
    xmsg(0, 0, 0x02, "VRAM read      ");
    dprintf("VRAM read x %d\n", reps);
    timer_start();
    for (int loop = 0; loop < reps; loop++)
    {
        uint16_t count = 0x8000;        // VRAM long count
        do
        {
            v = xm_getl(DATA);
        } while (--count);
    }
    vram_read = timer_stop();
    global    = v;        // save v so GCC doesn't optimize away test
    if (checkchar())
    {
        return;
    }
    reps = 16;        // main ram test (NOTE: I am not even incrementing pointer below - like "fake
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
            xm_setw(RD_ADDR, 0);
            v = xm_getbl(DATA);
        } while (--count);
    }
    vram_read = timer_stop();
    global    = v;        // save v so GCC doesn't optimize away test
    if (checkchar())
    {
        return;
    }
    reps = 16;        // a bit longer read test (to show stable during read)
    xmsg(0, 0, 0x02, "VRAM slow read2");
    dprintf("VRAM slow read2 x %d\n", reps);
    timer_start();
    for (int loop = 0; loop < reps; loop++)
    {
        uint16_t count = 0x8000;        // VRAM long count
        do
        {
            xm_setw(RD_ADDR, count & 0xff);
            v = xm_getbl(DATA);
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
            (1000U * 128 * reps) / vram_write);
    dprintf(
        "MOVEP.L VRAM read       128KB x 16 (2MB)    %u ms (%u KB/sec)\n", vram_read, (1000U * 128 * reps) / vram_read);
    dprintf("MOVE.L  main RAM write  128KB x 16 (2MB)    %u ms (%u KB/sec)\n",
            main_write,
            (1000U * 128 * reps) / main_write);
    dprintf(
        "MOVE.L  main RAM read   128KB x 16 (2MB)    %u ms (%u KB/sec)\n", main_read, (1000U * 128 * reps) / main_read);
}

#if 0        // TODO: needs recalibrating for VSync timer
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
    uint16_t MHz = (uint16_t)(((count * 26U) + 500U) / 1000U);
    dprintf("rosco_m68k: m68k CPU speed %d.%d MHz (%d.%d BogoMIPS)\n",
            MHz / 10,
            MHz % 10,
            count * 3 / 10000,
            ((count * 3) % 10000) / 10);

    return (uint16_t)((MHz + 5U) / 10U);
}
#endif

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
    }
    else
    {
        dprintf(" - FAILED\n");
    }
}

static void load_sd_colors(const char * filename)
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

    dprintf("\nxosera_init(0)...");
    bool success = xosera_init(0);
    dprintf("%s (%dx%d)\n", success ? "succeeded" : "FAILED", xreg_getw(VID_HSIZE), xreg_getw(VID_VSIZE));

    // D'oh! Uses timer    rosco_m68k_CPUMHz();

    dprintf("Installing interrupt handler...");
    install_intr();
    dprintf("okay.\n");

    printf("Checking for interrupt...");
    uint32_t t = XFrameCount;
    while (XFrameCount == t)
        ;
    printf("okay. Vsync interrupt detected.\n\n");

#if COPPER_TEST
    dprintf("Loading copper list...");

    xm_setw(XR_ADDR, XR_COPPER_MEM);

    for (uint16_t i = 0; i < copper_list_len; i++)
    {
        xm_setw(XR_DATA, copper_list[i]);
    }

    dprintf("okay\n");
#endif

    if (delay_check(4000))
    {
        return;
    }

    while (true)
    {
        uint32_t t = XFrameCount;
        uint32_t h = t / (60 * 60 * 60);
        uint32_t m = t / (60 * 60) % 60;
        uint32_t s = (t / 60) % 60;
        dprintf("*** xosera_test_m68k iteration: %u, running %u:%02u:%02u\n", test_count++, h, m, s);

        xcls();
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

        dprintf(
            "Xosera v%1x.%02x #%08x Features:0x%02x\n", (version >> 8) & 0xf, (version & 0xff), githash, version >> 8);
        dprintf("Monitor Mode: %dx%d@%2x.%02xHz\n", monwidth, monheight, monfreq >> 8, monfreq & 0xff);
        dprintf("\nPlayfield A:\n");
        dprintf("PA_GFX_CTRL : 0x%04x PA_TILE_CTRL: 0x%04x\n", gfxctrl, tilectrl);
        dprintf("PA_DISP_ADDR: 0x%04x PA_LINE_LEN : 0x%04x\n", dispaddr, linelen);
        dprintf("PA_HV_SCROLL: 0x%04x\n", hvscroll);

#if COPPER_TEST
        if (test_count & 1)
        {
            dprintf("Copper enabled this interation.\n");
            wait_vsync();
            restore_colors();
            xreg_setw(COPP_CTRL, 0x8000);
        }
        else
        {
            dprintf("Copper disabled this iteration.\n");
            wait_vsync();
            restore_colors();
            xreg_setw(COPP_CTRL, 0x0000);
        }

#endif

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
            wait_vsync();
            xreg_setw(PA_GFX_CTRL, 0x0075);        // bitmap + 8-bpp + Hx2 + Vx2
            xreg_setw(PA_LINE_LEN, 160);

            load_sd_colors("/xosera_r1_pal.raw");
            load_sd_bitmap("/xosera_r1.raw");
            if (delay_check(DELAY_TIME))
            {
                break;
            }
        }

        // 4/8 bpp test
        if (use_sd)
        {
            wait_vsync();
            xreg_setw(PA_GFX_CTRL, 0x0075);        // bitmap + 8-bpp + Hx2 + Vx2
            xreg_setw(PA_LINE_LEN, 160);

            load_sd_colors("/color_cube_320x240_256_pal.raw");
            load_sd_bitmap("/color_cube_320x240_256.raw");
            if (delay_check(DELAY_TIME))
            {
                break;
            }
        }

        if (use_sd)
        {
            wait_vsync();
            xreg_setw(PA_GFX_CTRL, 0x0065);        // bitmap + 4-bpp + Hx2 + Vx2
            xreg_setw(PA_LINE_LEN, 80);

            load_sd_colors("/ST_KingTut_Dpaint_16_pal.raw");
            load_sd_bitmap("/ST_KingTut_Dpaint_16.raw");
            if (delay_check(DELAY_TIME))
            {
                break;
            }
        }
        if (use_sd)
        {
            wait_vsync();
            xreg_setw(PA_GFX_CTRL, 0x0065);        // bitmap + 4-bpp + Hx2 + Vx2
            xreg_setw(PA_LINE_LEN, 80);

            load_sd_colors("/escher-relativity_320x240_16_pal.raw");
            load_sd_bitmap("/escher-relativity_320x240_16.raw");
            if (delay_check(DELAY_TIME))
            {
                break;
            }
        }
        wait_vsync();
        restore_colors();
        if (use_sd)
        {
            wait_vsync();
            xreg_setw(PA_GFX_CTRL, 0x0040);        // bitmap + 1-bpp + Hx1 + Vx1
            xreg_setw(PA_LINE_LEN, 80);

            load_sd_bitmap("/space_shuttle_color_small.raw");
            if (delay_check(DELAY_TIME))
            {
                break;
            }
        }

        if (use_sd)
        {
            wait_vsync();
            xreg_setw(PA_GFX_CTRL, 0x0040);        // bitmap + 1-bpp + Hx1 + Vx1
            xreg_setw(PA_LINE_LEN, 80);

            load_sd_bitmap("/mountains_mono_640x480w.raw");
            if (delay_check(DELAY_TIME))
            {
                break;
            }
        }

        if (use_sd)
        {
            wait_vsync();
            xreg_setw(PA_GFX_CTRL, 0x0040);        // bitmap + 1-bpp + Hx1 + Vx1
            xreg_setw(PA_LINE_LEN, 80);

            load_sd_bitmap("/escher-relativity_640x480w.raw");
            if (delay_check(DELAY_TIME))
            {
                break;
            }
        }

        wait_vsync();
        xreg_setw(PA_GFX_CTRL, 0x0000);
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
    wait_vsync();

    xreg_setw(PA_GFX_CTRL, 0x0000);                           // text mode
    xreg_setw(PA_TILE_CTRL, 0x000F);                          // text mode
    xreg_setw(COPP_CTRL, 0x0000);                             // disable copper
    xreg_setw(PA_LINE_LEN, xreg_getw(VID_HSIZE) >> 3);        // line len
    restore_colors();
    remove_intr();
    xcls();
    xmsg(0, 0, 0x02, "Exited.");


    while (checkchar())
    {
        readchar();
    }
}
