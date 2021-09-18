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

#define DELAY_TIME 500

#include "xosera_m68k_api.h"

extern void install_intr(void);
extern void remove_intr(void);

extern volatile uint32_t XFrameCount;

uint16_t vram_buffer[64 * 1024];

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

void wait_vsync()
{
    while (xreg_getw(SCANLINE) >= 0x8000)
        ;
    while (xreg_getw(SCANLINE) < 0x8000)
        ;
}
#define LEFT
int test_vram_LFSR_fast()
{
    int      LFSR_errors = 0;
    uint16_t start_state = 0xACE1u; /* Any nonzero start state will work. */
    uint16_t lfsr        = start_state;

    dprintf("FAST LFSR VRAM scroll test .\n");
    LFSR_errors = 0;

    dprintf(" ... FAST Filling VRAM with LFSR pattern\n");
    xm_setw(WR_INCR, 0x0001);
    xm_setw(WR_ADDR, 0x0000);
    for (int addr = 0; addr < 0x10000; addr++)
    {
#ifndef LEFT
        unsigned lsb = lfsr & 1u; /* Get LSB (i.e., the output bit). */
        lfsr >>= 1;               /* Shift register */
        if (lsb)                  /* If the output bit is 1, */
            lfsr ^= 0xB400u;      /*  apply toggle mask. */
#else
        unsigned msb = (int16_t)lfsr < 0; /* Get MSB (i.e., the output bit). */
        lfsr <<= 1;                       /* Shift register */
        if (msb)                          /* If the output bit is 1, */
            lfsr ^= 0x002Du;              /*  apply toggle mask. */
#endif
        vram_buffer[addr] = lfsr;
        xm_setw(DATA, lfsr);
    }

    if (checkchar())
    {
        return 0;
    }

    dprintf(" ... FAST Verifying all of VRAM matches original LFSR pattern\n");
    xm_setw(RD_INCR, 0x0000);
    int retries = 0;
    for (int addr = 0; addr < 0x10000; addr++)
    {
        uint16_t data;
        do
        {
            xm_setw(RD_ADDR, addr);
            data = xm_getw(DATA);

            if (vram_buffer[addr] != data)
            {
                xm_setw(RD_ADDR, addr);
                if (++retries < 10)
                {
                    continue;
                }
            }
            break;
        } while (true);
        if (retries)
        {
            if (++LFSR_errors < 10)
            {
                dprintf("*** FAST %s MISMATCH:  VRAM[0x%04x] has 0x%04x, LFSR[%04x] is 0x%04x [Error %d]\n",
                        retries >= 10 ? "WRITE" : "READ",
                        addr,
                        data,
                        addr,
                        vram_buffer[addr],
                        LFSR_errors);
            }
            retries = 0;
        }
    }
    dprintf(" ... FAST VRAM LFSR pattern verified.\n");

    if (checkchar())
    {
        return 0;
    }

    dprintf(" ... FAST Scrolling all of VRAM\n");

    xm_setw(RD_INCR, 0x0001);
    xm_setw(RD_ADDR, 0x0000);
    xm_setw(WR_INCR, 0x0001);
    xm_setw(WR_ADDR, 0xffff);
    for (int addr = 0; addr < 0x10000; addr++)
    {
        uint16_t data = xm_getw(DATA);
        xm_setw(DATA, data);
    }

    if (checkchar())
    {
        return 0;
    }

#if 0        // trust, yet verify
    xm_setw(WR_ADDR, 1234);
    xm_setw(DATA, 7);
#endif

    dprintf(" ... FAST Verifying all of VRAM matches original LFSR pattern\n");

    xm_setw(RD_INCR, 0x0000);
    retries = 0;
    for (int addr = 0; addr < 0x10000; addr++)
    {
        uint16_t data;
        do
        {
            xm_setw(RD_ADDR, (addr - 1) & 0xffff);
            data = xm_getw(DATA);
            if (vram_buffer[addr] != data)
            {
                if (++retries < 10)
                {
                    continue;
                }
            }
            break;
        } while (true);
        if (retries)
        {
            if (++LFSR_errors < 10)
            {
                dprintf("*** FAST %s MISMATCH:  VRAM[0x%04x] has 0x%04x, LFSR[0x%04x] is 0x%04x [Error %d]\n",
                        retries >= 10 ? "WRITE" : "READ",
                        ((addr - 1)) & 0xffff,
                        data,
                        addr,
                        vram_buffer[addr],
                        LFSR_errors);
            }
            retries = 0;
        }
    }

    if (LFSR_errors)
    {
        dprintf(" BAD FAST LFSR VRAM scroll testFAILED: %d errors.\n", LFSR_errors);
    }
    else
    {
        dprintf(" Ok! FAST VRAM LFSR pattern verified after scroll.\n");
    }

    return LFSR_errors;
}

#define VRAM_WR_DELAY() mcBusywait(10)
#define VRAM_RD_DELAY() mcBusywait(10)

int test_vram_LFSR()
{
    int      LFSR_errors = 0;
    uint16_t start_state = 0xACE1u; /* Any nonzero start state will work. */
    uint16_t lfsr        = start_state;

    dprintf("LFSR VRAM scroll test.\n");
    LFSR_errors = 0;

    dprintf(" ... Filling VRAM with LFSR pattern\n");
    xm_setw(WR_INCR, 0x0000);
    xm_setw(WR_ADDR, 0x0000);
    for (int addr = 0; addr < 0x10000; addr++)
    {
#ifndef LEFT
        unsigned lsb = lfsr & 1u; /* Get LSB (i.e., the output bit). */
        lfsr >>= 1;               /* Shift register */
        if (lsb)                  /* If the output bit is 1, */
            lfsr ^= 0xB400u;      /*  apply toggle mask. */
#else
        unsigned msb = (int16_t)lfsr < 0; /* Get MSB (i.e., the output bit). */
        lfsr <<= 1;                       /* Shift register */
        if (msb)                          /* If the output bit is 1, */
            lfsr ^= 0x002Du;              /*  apply toggle mask. */
#endif
        vram_buffer[addr] = lfsr;
        xm_setw(WR_ADDR, addr);
        xm_setw(DATA, lfsr);
        VRAM_WR_DELAY();
    }

    if (checkchar())
    {
        return 0;
    }

    dprintf(" ... Verifying all of VRAM matches original LFSR pattern\n");
    xm_setw(RD_INCR, 0x0000);
    int retries = 0;
    for (int addr = 0; addr < 0x10000; addr++)
    {
        uint16_t data;
        do
        {
            xm_setw(RD_ADDR, addr);
            VRAM_RD_DELAY();
            data = xm_getw(DATA);
            if (vram_buffer[addr] != data)
            {
                xm_setw(RD_ADDR, addr);
                VRAM_RD_DELAY();
                if (++retries < 10)
                {
                    continue;
                }
            }
            break;
        } while (true);
        if (retries)
        {
            if (++LFSR_errors < 10)
            {
                dprintf("*** %s MISMATCH:  VRAM[0x%04x] has 0x%04x, LFSR[0x%04x] was 0x%04x [Error %d]\n",
                        retries >= 10 ? "WRITE" : "READ",
                        addr,
                        data,
                        addr,
                        vram_buffer[addr],
                        LFSR_errors);
            }
            retries = 0;
        }
    }
    dprintf(" ... VRAM LFSR pattern verified.\n");

    if (checkchar())
    {
        return 0;
    }

    dprintf(" ... Scrolling all of VRAM\n");

    xm_setw(RD_INCR, 0x0000);
    xm_setw(WR_INCR, 0x0001);
    xm_setw(WR_ADDR, 0xffff);
    for (int addr = 0; addr < 0x10000; addr++)
    {
        xm_setw(RD_ADDR, addr);
        VRAM_RD_DELAY();
        uint16_t data = xm_getw(DATA);
        xm_setw(DATA, data);
        VRAM_WR_DELAY();
    }

    if (checkchar())
    {
        return 0;
    }

    dprintf(" ... Verifying all of VRAM matches original LFSR pattern\n");

    xm_setw(RD_INCR, 0x0000);
    retries = 0;
    for (int addr = 0; addr < 0x10000; addr++)
    {
        uint16_t data;
        do
        {
            xm_setw(RD_ADDR, (addr - 1) & 0xffff);
            VRAM_RD_DELAY();
            data = xm_getw(DATA);
            if (vram_buffer[addr] != data)
            {
                if (++retries < 10)
                {
                    continue;
                }
            }
            break;
        } while (true);
        if (retries)
        {
            if (++LFSR_errors < 10)
            {
                dprintf("*** %s MISMATCH:  VRAM[0x%04x] has 0x%04x, LFSR[0x%04x] was 0x%04x [Error %d]\n",
                        retries >= 10 ? "WRITE" : "READ",
                        ((addr - 1)) & 0xffff,
                        data,
                        addr,
                        vram_buffer[addr],
                        LFSR_errors);
            }
            retries = 0;
        }
    }

    if (LFSR_errors)
    {
        dprintf(" BAD LFSR VRAM scroll test FAILED: %d errors.\n", LFSR_errors);
    }
    else
    {
        dprintf(" Ok! VRAM LFSR pattern verified after scroll.\n");
    }

    return LFSR_errors;
}

uint32_t test_count;

#define TEST_MODES 4

int      cur_mode;
uint16_t test_modes[TEST_MODES] = {0x0080, 0x0040, 0x0060, 0x0070};
uint32_t total_LFSR_fast_errors[TEST_MODES];
uint32_t total_LFSR_errors[TEST_MODES];
uint32_t all_errors;

void xosera_test()
{
    // flush any input charaters to avoid instant exit
    while (checkchar())
    {
        readchar();
    }

    dprintf("Xosera_vramtest_m68k\n");

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
        dprintf("\n>>> xosera_vramtest_m68k iteration: %u, running %u:%02u:%02u, errs: %u\n",
                test_count++,
                h,
                m,
                s,
                all_errors);

        uint16_t version   = xreg_getw(VERSION);
        uint32_t githash   = ((uint32_t)xreg_getw(GITHASH_H) << 16) | (uint32_t)xreg_getw(GITHASH_L);
        uint16_t monwidth  = xreg_getw(VID_HSIZE);
        uint16_t monheight = xreg_getw(VID_VSIZE);
        uint16_t monfreq   = xreg_getw(VID_VFREQ);

        dprintf("     Xosera v%1x.%02x #%08x Features:0x%02x %dx%d @%2x.%02xHz\n",
                (version >> 8) & 0xf,
                (version & 0xff),
                githash,
                version >> 8,
                monwidth,
                monheight,
                monfreq >> 8,
                monfreq & 0xff);


        // set funky 8-bpp mode to show all VRAM
        xreg_setw(PA_DISP_ADDR, 0x0000);
        xreg_setw(PA_GFX_CTRL, test_modes[cur_mode]);        // bitmap + 8-bpp Hx2 Vx1
        xreg_setw(PA_LINE_LEN, 0x100);

        uint16_t gfxctrl  = xreg_getw(PA_GFX_CTRL);
        uint16_t tilectrl = xreg_getw(PA_TILE_CTRL);
        uint16_t dispaddr = xreg_getw(PA_DISP_ADDR);
        uint16_t linelen  = xreg_getw(PA_LINE_LEN);
        uint16_t hvscroll = xreg_getw(PA_HV_SCROLL);

        dprintf("     Playfield A:\n");
        dprintf("     PA_GFX_CTRL : 0x%04x PA_TILE_CTRL: 0x%04x\n", gfxctrl, tilectrl);
        dprintf("     PA_DISP_ADDR: 0x%04x PA_LINE_LEN : 0x%04x\n", dispaddr, linelen);
        dprintf("     PA_HV_SCROLL: 0x%04x\n", hvscroll);

        int errs = test_vram_LFSR_fast();
        all_errors += errs;
        total_LFSR_fast_errors[cur_mode] += errs;

        if (delay_check(DELAY_TIME))
        {
            break;
        }

        errs = test_vram_LFSR();
        all_errors += errs;
        total_LFSR_errors[cur_mode] += errs;

        if (delay_check(DELAY_TIME))
        {
            break;
        }

        for (int i = 0; i < TEST_MODES; i++)
        {
            if (total_LFSR_fast_errors[i])
            {
                dprintf("ERRORS gfx_ctrl:0x%04x = %d (fast)\n", test_modes[i], total_LFSR_fast_errors[i]);
            }
            if (total_LFSR_errors[i])
            {
                dprintf("ERRORS gfx_ctrl:0x%04x = %d\n", test_modes[i], total_LFSR_errors[i]);
            }
        }

        cur_mode = (cur_mode + 1) & 0x3;
    }
    wait_vsync();
    remove_intr();

    xreg_setw(PA_GFX_CTRL, 0x0000);                           // text mode
    xreg_setw(PA_TILE_CTRL, 0x000F);                          // text mode
    xreg_setw(COPP_CTRL, 0x0000);                             // disable copper
    xreg_setw(PA_LINE_LEN, xreg_getw(VID_HSIZE) >> 3);        // line len

    while (checkchar())
    {
        readchar();
    }
}
