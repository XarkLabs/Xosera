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

#define DELAY_TIME 100

#include "xosera_m68k_api.h"

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

#define RETURN_ON_KEYPRESS()                                                                                           \
    if (checkchar())                                                                                                   \
        return -1;                                                                                                     \
    else                                                                                                               \
        (void)0

#define BREAK_ON_KEYPRESS()                                                                                            \
    if (checkchar())                                                                                                   \
        break;                                                                                                         \
    else                                                                                                               \
        (void)0

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

#if defined(printf)        // printf macro interferes with gcc format attribute
#define _save_printf printf
#undef printf
#endif

void dprintf(const char * fmt, ...) __attribute__((format(printf, 1, 2)));

#if defined(_save_printf)        // retstore printf macro
#define printf _save_printf
#undef _save_printf
#endif

static char dprint_buff[4096];

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

void dprintf(const char * fmt, ...)
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

// VRAM test globals

// buffer for test pattern during test
uint16_t vram_buffer[64 * 1024];

#define MODE_TOGGLE_BIT 4        // toggle Xosera config every 4 iterations (power of two)

enum vram_test_flags
{
    // test type
    MODEFLAG_SLOW = (1 << 0),
    MODEFLAG_BYTE = (1 << 1),
    MODEFLAG_WORD = (1 << 2),
    MODEFLAG_LONG = (1 << 3),
    // test pattern
    MODEFLAG_LFSR = (1 << 4),
    MODEFLAG_ADDR = (1 << 5),
    // error severities
    MODEFLAG_BAD   = (1 << 6),
    MODEFLAG_WRITE = (1 << 7),
    MODEFLAG_READ  = (1 << 8),
    // video modes
    MODEFLAG_1BPP  = (1 << 9),
    MODEFLAG_2BPP  = (1 << 10),
    MODEFLAG_4BPP  = (1 << 11),
    MODEFLAG_8BPP  = (1 << 12),
    MODEFLAG_BLANK = (1 << 13)
};

struct vram_fail_info
{
    uint16_t addr;            // vram address of error
    uint16_t data;            // date read from vram
    uint16_t expected;        // expected data
    uint16_t flags;           // flags for test type, error serverity and video mode
    uint16_t count;           // number of errors at this address, data and expected data
};

#define MAX_ERROR_LOG 4096
#define MAX_TEST_FAIL 16
#define TEST_MODES    5
#define TEST_SPEEDS   4
struct vram_fail_info vram_fails[MAX_ERROR_LOG];
const char *          vram_mode_names[TEST_MODES] = {"1-BPP", "2-BPP", "4-BPP", "8-BPP", "blank"};
const char *          speed_names[TEST_SPEEDS]    = {"SLOW", "BYTE", "WORD", "LONG"};
const uint16_t        vram_modes[TEST_MODES]      = {0x0040, 0x0050, 0x0060, 0x0070, 0x0080};
const uint16_t        vram_mode_flags[TEST_MODES] = {MODEFLAG_1BPP,
                                              MODEFLAG_2BPP,
                                              MODEFLAG_4BPP,
                                              MODEFLAG_8BPP,
                                              MODEFLAG_BLANK};
int                   vram_test_fails;
int                   vram_next_fail;
int                   vram_test_count;
bool                  first_failure;

#define VRAM_WR_DELAY() mcBusywait(1)        // delay for "SLOW" write
#define VRAM_RD_DELAY() mcBusywait(1)        // delay for "SLOW" read

void add_fail(int addr, int data, int expected, int flags)
{
    struct vram_fail_info fi;

    fi.addr     = addr;
    fi.data     = data;
    fi.expected = expected;
    fi.flags    = flags;
    fi.count    = 1;

    int i = 0;
    for (i = 0; i < vram_next_fail; i++)
    {
        struct vram_fail_info * fip = &vram_fails[i];

        if (fi.addr == fip->addr)
        {
            if (fi.data == fip->data)
            {
                if (fi.expected == fip->expected)
                {
                    fip->flags |= fi.flags;
                    fip->count++;

                    return;
                }
            }
        }

        if (fi.addr > fip->addr)
        {
            break;
        }
    }

    if (vram_next_fail < MAX_ERROR_LOG)
    {
        for (int j = vram_next_fail - 1; j >= i; j--)
        {
            vram_fails[j + 1] = vram_fails[j];
        }
        vram_next_fail++;
        vram_fails[i] = fi;
    }
}

void fill_LFSR()
{
    uint16_t start_state;
    do
    {
        start_state = xm_getw(TIMER);
    } while (start_state == 0);
    uint16_t lfsr = start_state;

    for (uint32_t i = 0; i < 0xffff; i++)
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

int vram_retry(uint16_t addr, uint16_t baddata, bool LFSR, int mode, int speed)
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
                     MODEFLAG_READ | (LFSR ? MODEFLAG_LFSR : MODEFLAG_ADDR) | vram_mode_flags[mode] |
                         (1 << (speed & 0x3)));
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
                         MODEFLAG_WRITE | (LFSR ? MODEFLAG_LFSR : MODEFLAG_ADDR) | (vram_mode_flags[mode]) |
                             (1 << (speed & 0x3)));
                rc = 1;        // correctable write error
                break;
            }
        }
    }

    // if 10 tries fail, mark it as uncorrectable
    if (data != vram_buffer[addr])
    {
        add_fail(addr,
                 baddata,
                 vram_buffer[addr],
                 MODEFLAG_BAD | (LFSR ? MODEFLAG_LFSR : MODEFLAG_ADDR) | (vram_mode_flags[mode]) |
                     (1 << (speed & 0x3)));
        rc = -1;        // uncorrectable error
    }

    // log error
    vram_test_fails++;
    if (first_failure)
    {
        dprintf("FAILED!\n");
        first_failure = false;
    }
    dprintf("*** MISMATCH %s %s %s: VRAM[0x%04x]=0x%04x vs data[0x%04x]=0x%04x [Error #%u]\n",
            LFSR ? "LFSR" : "ADDR",
            speed_names[speed],
            rc < 0   ? "BAD! "
            : rc > 0 ? "WRITE"
                     : "READ ",
            addr,
            baddata,
            addr,
            vram_buffer[addr],
            vram_test_fails);

    // setup to continue trying
    xm_setw(RD_ADDR, addr + 1);
    xm_setw(WR_ADDR, addr + 1);

    return rc;
}

int verify_vram(bool LFSR, int mode, int speed)
{
    int vram_errs = 0;
    switch (speed)
    {
        case 0:
            // slow
            xm_setw(RD_INCR, 0x0000);

            for (int addr = 0; addr < 0x10000; addr++)
            {
                xm_setw(RD_ADDR, addr);
                VRAM_RD_DELAY();
                uint16_t data = xm_getw(DATA);
                if (data != vram_buffer[addr])
                {
                    vram_retry(addr, data, LFSR, mode, speed);
                    if (++vram_errs >= MAX_TEST_FAIL)
                    {
                        return vram_errs;
                    }
                }
            }
            break;

        case 1:
            // byte
            xm_setw(RD_INCR, 0x0001);
            xm_setw(RD_ADDR, 0x0000);

            for (int addr = 0; addr < 0x10000; addr++)
            {
                uint16_t data = ((xm_getbh(DATA) << 8) | xm_getbl(DATA));
                if (data != vram_buffer[addr])
                {
                    vram_retry(addr, data, LFSR, mode, speed);
                    if (++vram_errs >= MAX_TEST_FAIL)
                    {
                        return vram_errs;
                    }
                }
            }
            break;

        case 2:
            // word
            xm_setw(RD_INCR, 0x0001);
            xm_setw(RD_ADDR, 0x0000);

            for (int addr = 0; addr < 0x10000; addr++)
            {
                uint16_t data = xm_getw(DATA);
                if (data != vram_buffer[addr])
                {
                    vram_retry(addr, data, LFSR, mode, speed);
                    if (++vram_errs >= MAX_TEST_FAIL)
                    {
                        return vram_errs;
                    }
                }
            }
            break;

        case 3:
            // long
            xm_setw(RD_INCR, 0x0001);
            xm_setw(RD_ADDR, 0x0000);

            for (int addr = 0; addr < 0x10000; addr += 2)
            {
                uint32_t data = xm_getl(DATA);
                if (data != (((uint32_t)vram_buffer[addr] << 16) | (uint32_t)vram_buffer[addr + 1]))
                {
                    if (vram_buffer[addr] != (data >> 16))
                    {
                        vram_retry(addr, (data >> 16), LFSR, mode, speed);
                        if (++vram_errs >= MAX_TEST_FAIL)
                        {
                            return vram_errs;
                        }
                    }
                    if (vram_buffer[addr + 1] != (data & 0xffff))
                    {
                        vram_retry(addr + 1, (data & 0xffff), LFSR, mode, speed);
                        if (++vram_errs >= MAX_TEST_FAIL)
                        {
                            return vram_errs;
                        }
                    }
                }
            }
            break;

        default:
            break;
    }

    return vram_errs;
}

int test_vram(bool LFSR, int mode, int speed)
{
    int vram_errs = 0;
    first_failure = true;
    xv_prep();

    // set funky mode to show VRAM
    wait_vsync();
    xreg_setw(PA_DISP_ADDR, 0x0000);
    xreg_setw(PA_GFX_CTRL, vram_modes[mode]);        // bitmap + 8-bpp Hx2 Vx1
    xreg_setw(PA_LINE_LEN, 136);                     // ~65536/480 words per line

    dprintf("  > VRAM test=%s speed=%s mode=%s : ", LFSR ? "LFSR" : "ADDR", speed_names[speed], vram_mode_names[mode]);

    // generate vram_buffer data
    if (LFSR)
    {
        fill_LFSR();
    }
    else
    {
        fill_ADDR();
    }
    RETURN_ON_KEYPRESS();

    uint16_t start_time = xm_getw(TIMER);

    // fill VRAM with vram_buffer
    switch (speed)
    {
        case 0:
            // slow
            xm_setw(WR_INCR, 0x0000);

            for (int addr = 0; addr < 0x10000; addr++)
            {
                xm_setw(WR_ADDR, addr);
                xm_setw(DATA, vram_buffer[addr]);
                VRAM_WR_DELAY();
            }
            break;

        case 1:
            // byte
            xm_setw(WR_INCR, 0x0001);
            xm_setw(WR_ADDR, 0x0000);

            for (int addr = 0; addr < 0x10000; addr++)
            {
                xm_setbh(DATA, vram_buffer[addr] >> 8);
                xm_setbl(DATA, vram_buffer[addr] & 0xff);
            }
            break;
        case 2:
            // word
            xm_setw(WR_INCR, 0x0001);
            xm_setw(WR_ADDR, 0x0000);

            for (int addr = 0; addr < 0x10000; addr++)
            {
                xm_setw(DATA, vram_buffer[addr]);
            }
            break;

        case 3:
            // long
            xm_setw(WR_INCR, 0x0001);
            xm_setw(WR_ADDR, 0x0000);

            for (int addr = 0; addr < 0x10000; addr += 2)
            {
                xm_setl(DATA, ((vram_buffer[addr] << 16) | vram_buffer[addr + 1]));
            }
            break;

        default:
            break;
    }
    RETURN_ON_KEYPRESS();

    // verify write was correct
    vram_errs += verify_vram(LFSR, mode, speed);
    if (vram_errs >= 16)
    {
        dprintf("TEST CANCELLED (too many errors)!\n");
    }

    // scroll vram_buffer and vram
    for (int addr = 0; addr < 0x10000; addr++)
    {
        vram_buffer[(addr - 1) & 0xffff] = vram_buffer[addr];
    }

    switch (speed)
    {
        case 0:
            // slow
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
            break;
        case 1:
            // byte
            xm_setw(RD_INCR, 0x0001);
            xm_setw(RD_ADDR, 0x0000);
            xm_setw(WR_INCR, 0x0001);
            xm_setw(WR_ADDR, 0xffff);
            for (int addr = 0; addr < 0x10000; addr++)
            {
                uint8_t data_h = xm_getbh(DATA);
                uint8_t data_l = xm_getbl(DATA);
                xm_setbh(DATA, data_h);
                xm_setbl(DATA, data_l);
            }
            break;
        case 2:
            // word
            xm_setw(RD_INCR, 0x0001);
            xm_setw(RD_ADDR, 0x0000);
            xm_setw(WR_INCR, 0x0001);
            xm_setw(WR_ADDR, 0xffff);
            for (int addr = 0; addr < 0x10000; addr++)
            {
                uint16_t data = xm_getw(DATA);
                xm_setw(DATA, data);
            }
            break;
        case 3:
            // long
            xm_setw(RD_INCR, 0x0001);
            xm_setw(RD_ADDR, 0x0000);
            xm_setw(WR_INCR, 0x0001);
            xm_setw(WR_ADDR, 0xffff);
            for (int addr = 0; addr < 0x10000; addr += 2)
            {
                uint32_t data = xm_getl(DATA);
                xm_setl(DATA, data);
            }
            break;
        default:
            break;
    }
    RETURN_ON_KEYPRESS();

    // verify scroll was correct
    vram_errs += verify_vram(LFSR, mode, speed);
    if (vram_errs == 0)
    {
        uint16_t elapsed_time = xm_getw(TIMER) - start_time;
        dprintf("PASSED  (%3u.%1ums)\n", elapsed_time / 10, elapsed_time % 10);
    }

    return vram_errs;
}

extern void install_intr(void);
extern void remove_intr(void);

void xosera_test()
{
    // flush any input charaters to avoid instant exit
    while (checkchar())
    {
        readchar();
    }

    dprintf("Xosera_vramtest_m68k\n");

    uint8_t cur_xosera_config = 0;
    dprintf("\nxosera_init(%d)...", cur_xosera_config);
    bool success = xosera_init(cur_xosera_config);
    dprintf("%s (%dx%d)\n", success ? "succeeded" : "FAILED", xreg_getw(VID_HSIZE), xreg_getw(VID_VSIZE));

    if (delay_check(4000))
    {
        return;
    }

    // D'oh! Uses timer    rosco_m68k_CPUMHz();

    dprintf("Installing interrupt handler...");
    install_intr();
    dprintf("okay.\n");

    while (true)
    {
        // switch between configurations every few test iterations
        uint8_t new_config = (vram_test_count & MODE_TOGGLE_BIT) ? 1 : 0;
        if (new_config != cur_xosera_config)
        {
            cur_xosera_config = new_config;
            dprintf("\n [Switching to Xosera config #%d...", cur_xosera_config);
            success = xosera_init(cur_xosera_config);
            dprintf("%s (%dx%d). ]\n", success ? "succeeded" : "FAILED", xreg_getw(VID_HSIZE), xreg_getw(VID_VSIZE));
        }

        uint32_t t = XFrameCount;
        int      h = t / (60 * 60 * 60);
        int      m = t / (60 * 60) % 60;
        int      s = (t / 60) % 60;
        dprintf("\n>>> xosera_vramtest_m68k iteration: %u, running %u:%02u:%02u, errors: %u\n",
                vram_test_count++,
                h,
                m,
                s,
                vram_test_fails);

        uint16_t version   = xreg_getw(VERSION);
        uint32_t githash   = ((uint32_t)xreg_getw(GITHASH_H) << 16) | (uint32_t)xreg_getw(GITHASH_L);
        uint16_t monwidth  = xreg_getw(VID_HSIZE);
        uint16_t monheight = xreg_getw(VID_VSIZE);
        uint16_t monfreq   = xreg_getw(VID_VFREQ);

        dprintf("    Xosera v%1x.%02x #%08x Features:0x%02x %dx%d @%2x.%02xHz\n",
                (version >> 8) & 0xf,
                (version & 0xff),
                (unsigned int)githash,
                version >> 8,
                monwidth,
                monheight,
                monfreq >> 8,
                monfreq & 0xff);

        for (uint32_t i = 0; i < TEST_MODES; i++)
        {
            for (uint32_t j = 0; j < TEST_SPEEDS; j++)
            {
                test_vram(false, i, j);
                if (delay_check(DELAY_TIME))
                {
                    break;
                }
                test_vram(true, i, j);
                if (delay_check(DELAY_TIME))
                {
                    break;
                }
            }
        }
        BREAK_ON_KEYPRESS();

        if (vram_next_fail)
        {
            dprintf("Cummulative VRAM test errors:\n");
            for (int i = 0; i < vram_next_fail; i++)
            {
                struct vram_fail_info * fip = &vram_fails[i];

                dprintf("#%2u @ 0x%04x=0x%04x vs 0x%04x pat=%s%s\te=%s%s%s\tm=%s%s%s%s%s\tt=%s%s%s%s\n",
                        fip->count,
                        fip->addr,
                        fip->data,
                        fip->expected,
                        fip->flags & MODEFLAG_LFSR ? "LFSR " : "",
                        fip->flags & MODEFLAG_ADDR ? "ADDR " : "",
                        fip->flags & MODEFLAG_BAD ? "BAD!  " : "",
                        fip->flags & MODEFLAG_READ ? "R " : "",
                        fip->flags & MODEFLAG_WRITE ? "W " : "",
                        fip->flags & MODEFLAG_1BPP ? "1" : "",
                        fip->flags & MODEFLAG_2BPP ? "2" : "",
                        fip->flags & MODEFLAG_4BPP ? "4" : "",
                        fip->flags & MODEFLAG_8BPP ? "8" : "",
                        fip->flags & MODEFLAG_BLANK ? "B" : "",
                        fip->flags & MODEFLAG_SLOW ? "S" : "",
                        fip->flags & MODEFLAG_BYTE ? "B" : "",
                        fip->flags & MODEFLAG_WORD ? "W" : "",
                        fip->flags & MODEFLAG_LONG ? "L" : "");
            }
        }
    }
    wait_vsync();
    remove_intr();

    wait_vsync();
    xmem_setw(XR_COLOR_MEM, 0x000);
    xreg_setw(PA_GFX_CTRL, 0x0000);         // text mode
    xreg_setw(PA_TILE_CTRL, 0x000F);        // text mode
    xreg_setw(COPP_CTRL, 0x0000);           // disable copper
    xreg_setw(PA_LINE_LEN, 106);            // line len

    while (checkchar())
    {
        readchar();
    }
}
