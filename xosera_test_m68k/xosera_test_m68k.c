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

// Define rosco_m68k Xosera board address pointer (See
// https://github.com/rosco-m68k/hardware-projects/blob/feature/xosera/xosera/code/pld/decoder/ic3_decoder.pld#L25)
xosera_regs_t * xosera_ptr = (xosera_regs_t *)(0xf80060 - 4);        // rosco_m68k Xosera register base (minus 4)

// timer helpers
static uint32_t start_tick;

uint32_t global;

void timer_start()
{
    uint32_t ts = _TIMER_100HZ;
    uint32_t t;
    while ((t = _TIMER_100HZ) == ts)
        ;
    start_tick = t;
}

uint32_t timer_stop()
{
    uint32_t stop_tick = _TIMER_100HZ;

    return (stop_tick - start_tick) * 10;
}

void test_hello()
{
    printf(">>> %s\n", __FUNCTION__);
    xv_setw(wr_inc, 1);
    xv_setw(wr_addr, 0x0000);
    xv_setw(data, 0x0200 | 'X');
    xv_setbl(data, 'o');
    xv_setbl(data, 's');
    xv_setbl(data, 'e');
    xv_setbl(data, 'r');
    xv_setbl(data, 'a');
    xv_setbl(data, ' ');
    xv_setw(data, 0x0400 | '6');
    xv_setbl(data, '8');
    xv_setbl(data, 'k');

    // read test
    xv_setw(rd_inc, 0x0001);
    xv_setw(rd_addr, 0x0000);

    printf("Read back rd_addr= 0x0000, rd_inc=0x0001\n");
    printf("[ '%c", xv_getw(data) & 0xff);
    printf(" '%c", xv_getw(data) & 0xff);
    printf(" '%c", xv_getw(data) & 0xff);
    printf(" '%c", xv_getw(data) & 0xff);
    printf(" '%c", xv_getw(data) & 0xff);
    printf(" '%c", xv_getw(data) & 0xff);
    printf(" '%c", xv_getw(data) & 0xff);
    printf(" '%c", xv_getw(data) & 0xff);
    printf(" '%c", xv_getw(data) & 0xff);
    printf(" '%c ]", xv_getw(data) & 0xff);
    printf(" rd_addr = 0x%04x\n", xv_getw(rd_addr));
}

uint32_t mem_buffer[32768];

void test_vram_speed()
{
    printf(">>> %s\n", __FUNCTION__);
    xv_setw(wr_addr, 0x0000);
    xv_setw(wr_inc, 1);

    uint32_t v    = ((0x2f00 | 'G') << 16) | (0x4f00 | 'o');
    uint16_t loop = 16;
    timer_start();
    do
    {
        uint16_t count = 0x8000;        // VRAM long count
        do
        {
            xv_setl(data, v);
        } while (--count);
        v ^= 0xff00ff00;
    } while (--loop);
    uint32_t elapsed = timer_stop();
    printf("MOVEP.L time to write 128KB x 16 (2MB) is %d ms, %d KB/sec\n", elapsed, (1000 * 128 * 16) / elapsed);
    loop = 16;
    timer_start();
    do
    {
        uint16_t count = 0x8000;        // VRAM long count
        do
        {
            v = xv_getl(data);
        } while (--count);
    } while (--loop);
    global  = v;
    elapsed = timer_stop();
    printf("MOVEP.L time to read 128KB x 16 (2MB) is %d ms, %d KB/sec\n", elapsed, (1000 * 128 * 16) / elapsed);
    loop = 16;
    timer_start();
    do
    {
        uint16_t   count = 0x8000;        // VRAM long count
        uint32_t * ptr   = mem_buffer;
        do
        {
            *ptr++ = count;
        } while (--count);
    } while (--loop);
    elapsed = timer_stop();
    printf("MOVE.L time to write main memory (2MB) is %d ms, %d KB/sec\n", elapsed, (1000 * 128 * 16) / elapsed);
}

bool check_key()
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

bool delay_check(int ms)
{
    while (ms > 0)
    {
        if (check_key())
        {
            return true;
        }

        delay(100);
        ms -= 100;
    }

    return false;
}

uint32_t test_count;
void     xosera_test()
{
    while (true)
    {
        printf("\n*** xosera_test_m68k interation: %d\n", test_count++);

        printf("xosera_init(0)...");
        if (xosera_init(0))
        {
            printf("success.  Resolution %dx%d, features: 0x%04x\n",
                   xv_reg_getw(vidwidth),
                   xv_reg_getw(vidheight),
                   xv_reg_getw(features));
        }
        else
        {
            printf("Failed!\n");
            if (delay_check(5000))        // extra time for monitor to sync
            {
                break;
            }
            continue;
        }

        if (delay_check(5000))        // extra time for monitor to sync
        {
            break;
        }

        test_hello();
        if (delay_check(3000))
        {
            break;
        }

        test_vram_speed();
        if (delay_check(3000))
        {
            break;
        }
    }
}
