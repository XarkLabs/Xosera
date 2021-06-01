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

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <basicio.h>
#include <machine.h>

#include "xosera_api.h"

// Define rosco_m68k Xosera board base address pointer (See
// https://github.com/rosco-m68k/hardware-projects/blob/feature/xosera/xosera/code/pld/decoder/ic3_decoder.pld#L25)
volatile xreg_t * const xosera_ptr = (volatile xreg_t * const)0xf80060;        // rosco_m68k Xosera base

// dummy global variable
uint32_t global;        // this is used to prevent the compiler from optimizing out tests

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
        "move.l #6,%%d1\n"
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

void test_hello()
{
    static const char test_string[] = "Xosera on rosco_m68k";

    printf(">>> %s\n", __FUNCTION__);
    xv_setw(wr_inc, 1);                            // set write inc
    xv_setw(wr_addr, 0x0000);                      // set write address
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
    xv_setw(rd_inc, 0x0001);
    xv_setw(rd_addr, 0x0000);

    printf("Read back rd_addr= 0x0000, rd_inc=0x0001 [");
    bool     good = true;
    uint16_t ub   = 0x0200;
    for (size_t i = 0; i < sizeof(test_string) - 1; i++)
    {
        if (i == sizeof(test_string) - 5)
        {
            ub = 0x0400;
        }
        uint16_t v = xv_getw(data);
        if (v == (ub | test_string[i]))
        {
            //            printf("%c", v & 0xff);
        }
        else
        {
            //            printf("<bad:%04x>", v);
            good = false;
        }
    }
    //    printf("], ending rd_addr = 0x%04x\n", xv_getw(rd_addr));
    printf("%s] Ending rd_addr = 0x%04x\n", good ? "Good" : "bad", xv_getw(rd_addr));
}

uint32_t mem_buffer[1];

void test_vram_speed()
{
    printf(">>> %s\n", __FUNCTION__);
    xv_setw(wr_addr, 0x0000);
    xv_setw(wr_inc, 1);

    const int reps = 16;

    uint32_t v = ((0x2f00 | 'G') << 16) | (0x4f00 | 'o');
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
    uint32_t elapsed = timer_stop();
    global           = v;        // save v so GCC doesn't optimize away test
    printf("MOVEP.L VRAM write      128KB x 16 (2MB)    %d ms (%d KB/sec)\n", elapsed, (1000 * 128 * reps) / elapsed);
    if (checkchar())
    {
        return;
    }
    timer_start();
    for (int loop = 0; loop < reps; loop++)
    {
        uint16_t count = 0x8000;        // VRAM long count
        do
        {
            v = xv_getl(data);
        } while (--count);
        v ^= 0xff00ff00;
    }
    elapsed = timer_stop();
    global  = v;        // save v so GCC doesn't optimize away test
    printf("MOVEP.L VRAM read       128KB x 16 (2MB)    %d ms (%d KB/sec)\n", elapsed, (1000 * 128 * reps) / elapsed);
    if (checkchar())
    {
        return;
    }
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
    elapsed = timer_stop();
    global  = v;        // save v so GCC doesn't optimize away test
    printf("MOVE.L  main RAM write  128KB x 16 (2MB)    %d ms (%d KB/sec)\n", elapsed, (1000 * 128 * reps) / elapsed);
    if (checkchar())
    {
        return;
    }
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
    elapsed = timer_stop();
    global  = v;        // save v so GCC doesn't optimize away test
    printf("MOVE.L  main RAM read   128KB x 16 (2MB)    %d ms (%d KB/sec)\n", elapsed, (1000 * 128 * reps) / elapsed);
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
    printf("rosco_m68k: m68k CPU speed %d.%d MHz (BogoMIPS %d @ 26 cyc/loop estimated)\n", MHz / 10, MHz % 10, count);

    return MHz / 10;
}

uint32_t test_count;
void     xosera_test()
{
    while (true)
    {
        rosco_m68k_CPUMHz();
        printf("\n*** xosera_test_m68k iteration: %d\n", test_count++);

        printf("xosera_init(0)...");
        if (xosera_init(0))
        {
            printf("success.\n");
        }
        else
        {
            printf("Failed!\n");
            if (delay_check(5000))
            {
                break;
            }
            continue;
        }

        uint32_t githash  = (xv_reg_getw(githash_h) << 16) | xv_reg_getw(githash_l);
        uint16_t width    = xv_reg_getw(vidwidth);
        uint16_t height   = xv_reg_getw(vidheight);
        uint16_t features = xv_reg_getw(features);
        xv_reg_setw(dispstart, test_count);
        uint16_t dispstart = xv_reg_getw(dispstart);
        uint16_t dispwidth = xv_reg_getw(dispwidth);
        uint16_t scrollxy  = xv_reg_getw(scrollxy);
        uint16_t gfxctrl   = xv_reg_getw(gfxctrl);

        printf("Xosera #%08x\n", githash);
        printf("Mode: %dx%d  Features:0x%04x\n", width, height, features);
        printf("dispstart:0x%04x dispwidth:0x%04x\n", dispstart, dispwidth);
        printf(" scrollxy:0x%04x   gfxctrl:0x%04x\n", scrollxy, gfxctrl);

        if (delay_check(5000))        // extra time for monitor to sync
        {
            break;
        }

        test_hello();
        if (delay_check(2000))
        {
            break;
        }

        test_vram_speed();
        if (delay_check(2000))
        {
            break;
        }
    }
}
