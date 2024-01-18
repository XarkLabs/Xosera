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

#include "rosco_m68k_support.h"

#include "xosera_m68k_api.h"

#define DELAY_TIME 100

uint32_t elapsed_tenthms;        // alternate timer since interrupts not reliable
uint16_t last_timer_val;

bool     has_PF_B;
uint16_t colormem_size;

static void update_elapsed()
{
    xv_prep();
    uint16_t new_timer_val = xm_getw(TIMER);
    uint16_t delta         = (uint16_t)(new_timer_val - last_timer_val);
    last_timer_val         = new_timer_val;
    elapsed_tenthms += delta;
}

#define RETURN_ON_KEYPRESS()                                                                                           \
    update_elapsed();                                                                                                  \
    if (checkchar())                                                                                                   \
        return -1;                                                                                                     \
    else                                                                                                               \
        (void)0

#define BREAK_ON_KEYPRESS()                                                                                            \
    update_elapsed();                                                                                                  \
    if (checkchar())                                                                                                   \
        break;                                                                                                         \
    else                                                                                                               \
        (void)0

_NOINLINE static bool delay_check(int ms)
{
    xv_prep();

    while (ms--)
    {
        if (checkchar())
        {
            return true;
        }

        uint16_t tms = 10;
        do
        {
            update_elapsed();
            uint16_t tv = xm_getw(TIMER);
            while (tv == xm_getw(TIMER))
                ;
        } while (--tms);
    }

    return false;
}


#define WAIT_VBLANK_START()                                                                                            \
    do                                                                                                                 \
    {                                                                                                                  \
        xwait_not_vblank();                                                                                            \
        xwait_vblank();                                                                                                \
    } while (0)

// VRAM test globals
uint16_t pattern_buffer[64 * 1024];        // buffer for test pattern during test
uint16_t vram_buffer[64 * 1024];           // buffer to hold copy of VRAM data

#define TEST_WORDS 16

// byte write
static void write_vram_byte(void)
{
    xv_prep();

    xm_setw(WR_INCR, 0x0001);
    xm_setw(WR_ADDR, 0x0000);

    //    xwait_mem_ready();

    uint8_t * bp = (uint8_t *)pattern_buffer;
    for (uint16_t i = 0; i < TEST_WORDS; i++)
    {
        xm_setbh(DATA, *bp++);
        xm_setbh(DATA, *bp++);
    }

    //    xwait_mem_ready();
}

// word write
static void write_vram_word(void)
{
    xv_prep();

    xm_setw(WR_INCR, 0x0001);
    xm_setw(WR_ADDR, 0x0000);

    //    xwait_mem_ready();

    uint16_t * wp = pattern_buffer;
    for (uint16_t i = 0; i < TEST_WORDS; i++)
    {
        xm_setw(DATA, *wp++);
    }

    //    xwait_mem_ready();
}

// byte read
static void read_vram_byte(void)
{
    xv_prep();

    xm_setw(RD_INCR, 0x0001);
    xm_setw(RD_ADDR, 0x0000);

    //    xwait_mem_ready();

    uint8_t * bp = (uint8_t *)vram_buffer;
    for (uint16_t i = 0; i < TEST_WORDS; i++)
    {
        *bp++ = xm_getbh(DATA);
        *bp++ = xm_getbl(DATA);
    }

    //    xwait_mem_ready();
}

// word read
static void read_vram_word(void)
{
    xv_prep();

    xm_setw(RD_INCR, 0x0001);
    xm_setw(RD_ADDR, 0x0000);

    //    xwait_mem_ready();

    uint16_t * wp = vram_buffer;
    for (uint16_t i = 0; i < TEST_WORDS; i++)
    {
        *wp++ = xm_getw(DATA);
    }

    //    xwait_mem_ready();
}

struct xosera_initdata
{
    char     name_version[28];
    uint32_t githash;
};

xosera_info_t initinfo;

void xosera_memdiag()
{
    xv_prep();

    cpu_delay(1000);

    dprintf("\nXosera_memdiag_m68k\n");

    dprintf("\n [Switching to Xosera config #%d...", 0);
    bool success   = xosera_init(XINIT_CONFIG_640x480);
    last_timer_val = xm_getw(TIMER);
    dprintf("%s (%dx%d). ]\n", success ? "succeeded" : "FAILED", xreg_getw(VID_HSIZE), xreg_getw(VID_VSIZE));
    xosera_get_info(&initinfo);

    xreg_setw(PA_GFX_CTRL, 0x0000);
    printf("\f\033[?25l");
    dprintf("Press any key to start testing...\n");
    readchar();

    for (uint16_t i = 0; i < TEST_WORDS; i++)
    {
        pattern_buffer[i] = (0x40 | i) << 8 | i;
    }
    delay(100);

    while (true)
    {
        write_vram_word();
        delay(10);
        read_vram_word();
        delay(20);

        write_vram_byte();
        delay(10);
        read_vram_byte();

        bool bad = false;

        for (uint16_t i = 0; i < TEST_WORDS; i++)
        {
            if (pattern_buffer[i] != vram_buffer[i])
            {
                bad = true;
                xm_setw(UNUSED_0F, 0xFFFF);
                break;
            }
        }

        if (bad)
        {
            for (uint16_t i = 0; i < TEST_WORDS; i++)
            {
                dprintf("buffer[%d]=0x%04x != vram[%d]=0x%04x\n", i, pattern_buffer[i], i, vram_buffer[i]);
            }

            readchar();
        }

        delay(100);
    }

    while (checkchar())
    {
        readchar();
    }
}
