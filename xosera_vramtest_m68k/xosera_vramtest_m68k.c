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

// VRAM test
uint16_t vram_buffer[64 * 1024];

#define MODEFLAG_FAST  0x8000
#define MODEFLAG_LFSR  0x4000
#define MODEFLAG_BAD   0x2000
#define MODEFLAG_WRITE 0x1000
#define MODEFLAG_READ  0x0800

int vram_test_fails = 0;

struct fail_info
{
    uint16_t addr;
    uint16_t data;
    uint16_t expected;
    uint16_t mode;
    uint16_t count;
};

int              next_fail;
struct fail_info fails[64 * 1024];

#define VRAM_WR_DELAY() mcBusywait(1)
#define VRAM_RD_DELAY() mcBusywait(1)

#define EXIT_ON_KEYPRESS()                                                                                             \
    do                                                                                                                 \
    {                                                                                                                  \
        if (checkchar())                                                                                               \
            return -1;                                                                                                 \
    } while (false)

void add_fail(uint16_t addr, uint16_t data, uint16_t expected, uint16_t mode_flags)
{
    struct fail_info fi;
    fi.addr     = addr;
    fi.data     = data;
    fi.expected = expected;
    fi.mode     = (xreg_getw(PA_GFX_CTRL) & 0x00ff) | mode_flags;
    fi.count    = 0;

    struct fail_info * fip;
    for (int i = 0; i < next_fail; i++)
    {
        fip = &fails[i];
        if (fi.addr == fip->addr && fi.data == fip->data && fi.expected == fip->expected && fi.mode == fip->mode)
        {
            fip->count++;
            return;
        }
    }
    fip  = &fails[next_fail++];
    *fip = fi;
}

void fill_LFSR()
{
    uint16_t start_state;
    do
    {
        start_state = xm_getw(TIMER);
    } while (start_state == 0);
    uint16_t lfsr = start_state;

    for (int i = 0; i < 0xffff; i++)
    {
        unsigned msb = (int16_t)lfsr < 0; /* Get MSB (i.e., the output bit). */
        lfsr <<= 1;                       /* Shift register */
        if (msb)                          /* If the output bit is 1, */
            lfsr ^= 0x002Du;              /*  apply toggle mask. */
        vram_buffer[i] = lfsr;
    }
    // swap last lfsr and zero (to keep zero in the mix)
    vram_buffer[0xffff] = vram_buffer[lfsr];
    vram_buffer[lfsr]   = 0;
}

void fill_ADDR()
{
    for (int addr = 0; addr < 0x10000; addr++)
    {
        vram_buffer[addr] = addr;
    }
}

int vram_retry(uint16_t addr, uint16_t baddata, bool LFSR, bool fast)
{
    int retries = 0;
    int rc      = 0;
    // see if slow read retry will read it correctly (if not, assume
    // it was a write error)
    uint16_t data = ~vram_buffer[addr];
    while (++retries < 10)
    {
        xm_setw(RD_ADDR, addr);
        VRAM_RD_DELAY();
        data = xm_getw(DATA);
        if (data == vram_buffer[addr])
        {
            add_fail(addr,
                     baddata,
                     vram_buffer[addr],
                     MODEFLAG_READ | (fast ? MODEFLAG_FAST : 0) | (LFSR ? MODEFLAG_LFSR : 0));
            rc = 0;        // read error
            break;
        }
    }

    // try to correct VRAM
    if (data != vram_buffer[addr])
    {
        for (retries = 0; retries < 10; retries++)
        {
            xm_setw(WR_ADDR, addr);
            xm_setw(DATA, vram_buffer[addr]);
            VRAM_WR_DELAY();
            xm_setw(RD_ADDR, addr);
            VRAM_RD_DELAY();
            data = xm_getw(DATA);
            if (data == vram_buffer[addr])
            {
                add_fail(addr,
                         baddata,
                         vram_buffer[addr],
                         MODEFLAG_WRITE | (fast ? MODEFLAG_FAST : 0) | (LFSR ? MODEFLAG_LFSR : 0));
                rc = 1;        // correctable write error
                break;
            }
        }
    }

    if (data != vram_buffer[addr])
    {
        add_fail(
            addr, baddata, vram_buffer[addr], MODEFLAG_BAD | (fast ? MODEFLAG_FAST : 0) | (LFSR ? MODEFLAG_LFSR : 0));
        rc = -1;        // uncorrectable error
    }

    if (++vram_test_fails <= 10)
    {
        dprintf("*** MISMATCH %s: VRAM[0x%04x]=0x%04x vs data[%04x]=0x%04x [Error #%d]\n",
                rc < 0   ? "BAD!"
                : rc > 0 ? "WRITE"
                         : "READ",
                addr,
                baddata,
                addr,
                vram_buffer[addr],
                vram_test_fails);
    }

    xm_setw(RD_ADDR, addr + 1);
    xm_setw(WR_ADDR, addr + 1);

    return rc;
}

int verify_vram(bool LFSR, bool fast)
{
    int vram_errs = 0;
    if (fast)
    {
        xm_setw(RD_INCR, 0x0001);
        xm_setw(RD_ADDR, 0x0000);

        for (int addr = 0; addr < 0x10000; addr++)
        {
            uint16_t data = xm_getw(DATA);
            if (data != vram_buffer[addr])
            {
                vram_retry(addr, data, LFSR, fast);
                vram_errs++;
            }
        }
    }
    else
    {
        xm_setw(RD_INCR, 0x0000);

        for (int addr = 0; addr < 0x10000; addr++)
        {
            xm_setw(RD_ADDR, addr);
            VRAM_RD_DELAY();
            uint16_t data = xm_getw(DATA);
            if (data != vram_buffer[addr])
            {
                vram_retry(addr, data, LFSR, fast);
                vram_errs++;
            }
        }
    }

    return vram_errs;
}

int test_vram(bool LFSR, bool fast)
{
    xv_prep();

    int vram_errs = 0;

    dprintf(
        "  > VRAM test (mode=0x%04x %s %s)\n", xreg_getw(PA_GFX_CTRL), LFSR ? "LFSR" : "ADDR", fast ? "Fast" : "Slow");

    // generate vram_buffer data
    if (LFSR)
    {
        fill_LFSR();
    }
    else
    {
        fill_ADDR();
    }
    EXIT_ON_KEYPRESS();

    // fill VRAM with vram_buffer
    if (fast)
    {
        xm_setw(WR_INCR, 0x0001);
        xm_setw(WR_ADDR, 0x0000);

        for (int addr = 0; addr < 0x10000; addr++)
        {
            xm_setw(DATA, vram_buffer[addr]);
        }
    }
    else
    {
        xm_setw(WR_INCR, 0x0000);

        for (int addr = 0; addr < 0x10000; addr++)
        {
            xm_setw(WR_ADDR, addr);
            xm_setw(DATA, vram_buffer[addr]);
            VRAM_WR_DELAY();
        }
    }
    EXIT_ON_KEYPRESS();

    // verify write was correct
    vram_errs += verify_vram(LFSR, fast);

    // scroll vram and vram_buffer
    for (int addr = 0; addr < 0x10000; addr++)
    {
        vram_buffer[(addr - 1) & 0xffff] = vram_buffer[addr];
    }

    if (fast)
    {
        xm_setw(RD_INCR, 0x0001);
        xm_setw(RD_ADDR, 0x0000);
        xm_setw(WR_INCR, 0x0001);
        xm_setw(WR_ADDR, 0xffff);
        for (int addr = 0; addr < 0x10000; addr++)
        {
            uint16_t data = xm_getw(DATA);
            xm_setw(DATA, data);
        }
    }
    else
    {
        xm_setw(RD_INCR, 0x0000);
        xm_setw(WR_INCR, 0x0000);
        for (int addr = 0; addr < 0x10000; addr++)
        {
            xm_setw(RD_ADDR, addr);
            VRAM_RD_DELAY();
            uint16_t data = xm_getw(DATA);
            xm_setw(WR_ADDR, (addr - 1) & 0xffff);
            xm_setw(DATA, data);
            VRAM_WR_DELAY();
        }
    }

    // verify scroll was correct
    vram_errs += verify_vram(LFSR, fast);

    if (vram_errs)
    {
        dprintf("*** FAILED! (errors: %d)\n", vram_errs);
    }
    else
    {
        dprintf("    PASSED!\n");
    }

    return vram_errs;
}

uint32_t test_count;

#define TEST_MODES 4

int      cur_mode;
uint16_t test_modes[TEST_MODES] = {0x0040, 0x0060, 0x0070, 0x0080};
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

    if (delay_check(4000))
    {
        return;
    }

    // D'oh! Uses timer    rosco_m68k_CPUMHz();

    dprintf("Installing interrupt handler...");
    install_intr();
    dprintf("okay.\n");

    printf("Checking for interrupt...");
    uint32_t t = XFrameCount;
    while (XFrameCount == t)
        ;
    printf("okay. Vsync interrupt detected.\n\n");

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

        dprintf("    Xosera v%1x.%02x #%08x Features:0x%02x %dx%d @%2x.%02xHz\n",
                (version >> 8) & 0xf,
                (version & 0xff),
                githash,
                version >> 8,
                monwidth,
                monheight,
                monfreq >> 8,
                monfreq & 0xff);


        // set funky mode to show most of VRAM
        wait_vsync();
        xreg_setw(PA_DISP_ADDR, 0x0000);
        xreg_setw(PA_GFX_CTRL, test_modes[cur_mode]);        // bitmap + 8-bpp Hx2 Vx1
        xreg_setw(PA_LINE_LEN, 0x100);

        //        uint16_t gfxctrl  = xreg_getw(PA_GFX_CTRL);
        //        uint16_t tilectrl = xreg_getw(PA_TILE_CTRL);
        //        uint16_t dispaddr = xreg_getw(PA_DISP_ADDR);
        //        uint16_t linelen  = xreg_getw(PA_LINE_LEN);
        //        uint16_t hvscroll = xreg_getw(PA_HV_SCROLL);

        //        dprintf("     Playfield A:\n");
        //        dprintf("     PA_GFX_CTRL : 0x%04x PA_TILE_CTRL: 0x%04x\n", gfxctrl, tilectrl);
        //        dprintf("     PA_DISP_ADDR: 0x%04x PA_LINE_LEN : 0x%04x\n", dispaddr, linelen);
        //        dprintf("     PA_HV_SCROLL: 0x%04x\n", hvscroll);

        all_errors += test_vram(false, false);
        if (delay_check(DELAY_TIME))
        {
            break;
        }
        all_errors += test_vram(false, true);
        if (delay_check(DELAY_TIME))
        {
            break;
        }
        all_errors += test_vram(true, false);
        if (delay_check(DELAY_TIME))
        {
            break;
        }
        all_errors += test_vram(true, true);
        if (delay_check(DELAY_TIME))
        {
            break;
        }

        if (all_errors)
        {
            dprintf("Cummulitive errors: %d\n", all_errors);
            for (int i = 0; i < next_fail; i++)
            {
                struct fail_info * fip = &fails[i];

                dprintf("ERR @ 0x%04x=0x%04x vs 0x%04x #%u mode=0x%02x %s %s%s%s\n",
                        fip->addr,
                        fip->data,
                        fip->expected,
                        fip->count,
                        fip->mode & 0xff,
                        fip->mode & MODEFLAG_FAST ? "FAST" : "SLOW ",
                        fip->mode & MODEFLAG_LFSR ? "LFSR " : "ADDR ",
                        fip->mode & MODEFLAG_READ ? "READ " : "",
                        fip->mode & MODEFLAG_WRITE ? "WRITE " : "",
                        fip->mode & MODEFLAG_BAD ? "BAD! " : "");
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
