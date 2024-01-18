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

// from interrupt.asm
extern void              install_intr(void);
extern void              remove_intr(void);
extern volatile uint16_t NukeColor;
extern volatile uint32_t XFrameCount;

#define USE_ASM_LONG 1

#define DELAY_TIME 100

uint32_t elapsed_tenthms;        // Xosera elapsed timer
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

void wait_vblank_start()
{
    xv_prep();

    xwait_not_vblank();
    xwait_vblank();
}

// VRAM test globals
uint16_t pattern_buffer[64 * 1024];        // buffer for test pattern during test
uint16_t vram_buffer[64 * 1024];           // buffer to hold copy of VRAM data

#define MODE_TOGGLE_BIT 4        // toggle Xosera config every 4 iterations (power of two)

// test flags used for error summary
enum vram_test_flags
{
    // test type
    MODEFLAG_SLOW  = (1 << 0),
    MODEFLAG_BYTE  = (1 << 1),
    MODEFLAG_WORD  = (1 << 2),
    MODEFLAG_LONG  = (1 << 3),
    MODEFLAG_XRMEM = (1 << 4),
    // test pattern
    MODEFLAG_LFSR = (1 << 5),
    MODEFLAG_ADDR = (1 << 6),
    // error severities
    MODEFLAG_BAD   = (1 << 7),
    MODEFLAG_WRITE = (1 << 8),
    MODEFLAG_READ  = (1 << 9),
    // video modes
    MODEFLAG_1BPP  = (1 << 10),
    MODEFLAG_4BPP  = (1 << 11),
    MODEFLAG_8BPP  = (1 << 12),
    MODEFLAG_XBPP  = (1 << 13),
    MODEFLAG_BLANK = (1 << 14)
};

// error summary info
struct vram_fail_info
{
    uint16_t addr;            // vram address of error
    uint16_t data;            // date read from vram
    uint16_t expected;        // expected data
    uint16_t flags;           // flags for test type, error serverity and video mode
    uint16_t count;           // number of errors at this address, data and expected data
    uint16_t pass;            // test iteration pass when first occurred
};

#define MAX_ERROR_LOG 4096
#define MAX_TEST_FAIL 16
#define TEST_MODES    5
#define TEST_SPEEDS   5
int                   num_vram_fails;
struct vram_fail_info vram_fails[MAX_ERROR_LOG];
const char *          vram_mode_names[TEST_MODES] = {"1-BPP", "4-BPP", "8-BPP", "X-BPP", "blank"};
const char *          speed_names[TEST_SPEEDS]    = {"SLOW", "BYTE", "WORD", "LONG", "XMEM"};
const uint16_t        vram_modes[TEST_MODES]      = {0x0040, 0x0050, 0x0060, 0x0070, 0x0080};
const uint16_t        vram_mode_flags[TEST_MODES] = {MODEFLAG_1BPP,
                                                     MODEFLAG_4BPP,
                                                     MODEFLAG_8BPP,
                                                     MODEFLAG_XBPP,
                                                     MODEFLAG_BLANK};

int  vram_test_count;             // total number of test iterations
int  vram_test_fail_count;        // number of failed tests
bool first_failure;               // bool indicating first failure of current test (formatting)

#define VRAM_WR_DELAY() mcBusywait(1)        // delay for "SLOW" write
#define VRAM_RD_DELAY() mcBusywait(1)        // delay for "SLOW" read

// test display
static char     xr_dprint_buff[4096];
static uint16_t xr_screen_addr = XR_TILE_ADDR + 0x1000;
static uint8_t  xr_text_columns;
static uint8_t  xr_text_rows;
static uint8_t  xr_text_color = 0x07;        // white on gray
static uint8_t  xr_x;
static uint8_t  xr_y;

static void xr_cls()
{
    if (!has_PF_B)
    {
        return;
    }
    xv_prep();
    xr_x = 0;
    xr_y = 0;
    wait_vblank_start();
    xmem_setw_next_addr(xr_screen_addr);
    for (int i = 0; i < xr_text_columns * xr_text_rows; i++)
    {
        xmem_setw_next(' ');
    }
}

static void xr_textmode_pb()
{
    xr_text_columns = 28;
    xr_text_rows    = 20;

    if (!has_PF_B)
    {
        return;
    }

    wait_vblank_start();
    xv_prep();
    xreg_setw(PB_GFX_CTRL, 0x0080);
#if 1
    for (int i = 1; i < 256; i++)
    {
        uint16_t v = xmem_getw_wait(XR_COLOR_A_ADDR + i) & 0x0fff;
        xmem_setw(XR_COLOR_A_ADDR + i, v);
    }
#endif
    xr_cls();

    xmem_setw(XR_COLOR_B_ADDR + 0xf0, 0x0000);        // set write address and 1st color transparent
    for (int i = 1; i < 16; i++)
    {
        xmem_setw(XR_COLOR_B_ADDR + 0xf0 + i, 0xf202 | (i << 4));        // shades of opaque green
    }
    xmem_setw(XR_COLOR_B_ADDR, 0x0000);        // set write address

    xwait_vblank();
    xreg_setw(PB_GFX_CTRL, 0xF00A);         // colorbase = 0xF0 tiled + 1-bpp + Hx3 + Vx2
    xreg_setw(PB_TILE_CTRL, 0x0E07);        // tile=0x0C00,tile=tile_mem, map=tile_mem, 8x8 tiles
    xreg_setw(PB_LINE_LEN, xr_text_columns);
    xreg_setw(PB_DISP_ADDR, xr_screen_addr);
}

static void xr_msg_color(uint8_t c)
{
    xr_text_color = c;
}

static void xr_pos(int x, int y)
{
    xr_x = x;
    xr_y = y;
}

static void xr_putc(const char c)
{
    xv_prep();

    if (!has_PF_B)
    {
        return;
    }
    xmem_setw_next_addr(xr_screen_addr + (xr_y * xr_text_columns) + xr_x);
    if (c == '\n')
    {
        while (xr_x < xr_text_columns)
        {
            xmem_setw_next(' ');
            xr_x++;
        }
        xr_x = 0;
        xr_y += 1;
    }
    else if (c == '\r')
    {
        xr_x = 0;
    }
    else
    {
        xmem_setw_next((xr_text_color << 8) | (uint8_t)c);
        xr_x++;
        if (xr_x >= xr_text_columns)
        {
            xr_x = 0;
            xr_y++;
        }
    }
}

static void xr_print(const char * str)
{
    if (!has_PF_B)
    {
        return;
    }
    register char c;
    while ((c = *str++) != '\0')
    {
        xr_putc(c);
    }
}

static void xr_printf(const char * fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(xr_dprint_buff, sizeof(xr_dprint_buff), fmt, args);
    xr_print(xr_dprint_buff);
    va_end(args);
}

static void xr_printfxy(int x, int y, const char * fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    xr_pos(x, y);
    vsnprintf(xr_dprint_buff, sizeof(xr_dprint_buff), fmt, args);
    xr_print(xr_dprint_buff);
    va_end(args);
}

static void add_fail(int addr, int data, int expected, int flags)
{
    struct vram_fail_info fi;

    fi.addr     = (uint16_t)addr;
    fi.data     = (uint16_t)data;
    fi.expected = (uint16_t)expected;
    fi.flags    = (uint16_t)flags;
    fi.count    = 1;
    fi.pass     = (uint16_t)vram_test_count;

    int i = 0;
    for (i = 0; i < num_vram_fails; i++)
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

    if (num_vram_fails < MAX_ERROR_LOG)
    {
        for (int j = num_vram_fails - 1; j >= i; j--)
        {
            vram_fails[j + 1] = vram_fails[j];
        }
        num_vram_fails++;
        vram_fails[i] = fi;
    }
}

static _NOINLINE void fill_LFSR()
{
    static uint16_t salt = 42;
    uint32_t        start_state;
    uint16_t        lfsr;
    xv_prep();

    do
    {
        start_state = xm_getw(TIMER);
        start_state += salt++;
        if (start_state > 0xffff)
        {
            start_state++;
        }
        lfsr = start_state;
    } while (lfsr == 0);

    for (uint32_t i = 0; i < 0xffff; i++)
    {
        unsigned msb = (int16_t)lfsr < 0;     /* Get MSB (i.e., the output bit). */
        lfsr         = (uint16_t)(lfsr << 1); /* Shift register */
        if (msb)                              /* If the output bit is 1, */
            lfsr ^= 0x002Du;                  /*  apply toggle mask. */
        pattern_buffer[i] = lfsr;
    }
    // swap last lfsr and zero (to keep zero in the mix)
    pattern_buffer[0xffff] = pattern_buffer[lfsr];
    pattern_buffer[lfsr]   = 0;
}

static _NOINLINE void fill_ADDR()
{
    xv_prep();

    uint16_t start_value = xm_getw(TIMER);

    for (int addr = 0; addr < 0x10000; addr++)
    {
        pattern_buffer[addr] = start_value++;
    }
}

static int vram_retry(uint16_t addr, uint16_t baddata, bool LFSR, int mode, int speed)
{
    int retries = 0;
    int rc      = 0;
    xv_prep();

    // see if slow read retry will read it correctly (if not, assume
    // it was a write error)
    uint16_t data = (uint16_t)~pattern_buffer[addr];
    while (++retries < 10)
    {
        xm_setw(RD_ADDR, addr);
        VRAM_RD_DELAY();
        data = xm_getw(DATA);
        if (data == pattern_buffer[addr])
        {
            add_fail(addr,
                     baddata,
                     pattern_buffer[addr],
                     MODEFLAG_READ | (LFSR ? MODEFLAG_LFSR : MODEFLAG_ADDR) | vram_mode_flags[mode] |
                         (1 << (speed & 0x3)));
            rc = 0;        // read error
            break;
        }
    }

    // try to correct VRAM
    if (data != pattern_buffer[addr])
    {
        for (retries = 0; retries < 10; retries++)
        {
            xm_setw(WR_ADDR, addr);
            xm_setw(DATA, pattern_buffer[addr]);
            VRAM_WR_DELAY();
            xm_setw(RD_ADDR, addr);
            VRAM_RD_DELAY();
            data = xm_getw(DATA);
            if (data == pattern_buffer[addr])
            {
                add_fail(addr,
                         baddata,
                         pattern_buffer[addr],
                         MODEFLAG_WRITE | (LFSR ? MODEFLAG_LFSR : MODEFLAG_ADDR) | (vram_mode_flags[mode]) |
                             (1 << (speed & 0x3)));
                rc = 1;        // correctable write error
                break;
            }
        }
    }

    // if 10 tries fail, mark it as uncorrectable
    if (data != pattern_buffer[addr])
    {
        add_fail(addr,
                 baddata,
                 pattern_buffer[addr],
                 MODEFLAG_BAD | (LFSR ? MODEFLAG_LFSR : MODEFLAG_ADDR) | (vram_mode_flags[mode]) |
                     (1 << (speed & 0x3)));
        rc = -1;        // uncorrectable error
    }
    else
    {
        vram_buffer[addr] = data;
    }

    // log error
    vram_test_fail_count++;
    if (first_failure)
    {
        dprintf("FAILED!\n");
        first_failure = false;
    }
    dprintf("*** MISMATCH %s %s %s: VRAM[0x%04x]=0x%04x vs data[0x%04x]=0x%04x [Error #%d]\n",
            LFSR ? "LFSR" : "ADDR",
            speed_names[speed],
            rc < 0   ? "BAD! "
            : rc > 0 ? "WRITE"
                     : "READ ",
            addr,
            baddata,
            addr,
            pattern_buffer[addr],
            vram_test_fail_count);

    return rc;
}

static int verify_vram(bool LFSR, int mode, int speed)
{
    int vram_errs = 0;

    for (int addr = 0; addr < 0x10000; addr++)
    {
        uint16_t data = vram_buffer[addr];
        if (data != pattern_buffer[addr])
        {
            vram_retry((uint16_t)addr, data, LFSR, mode, speed);
            if (++vram_errs >= MAX_TEST_FAIL)
            {
                return vram_errs;
            }
        }
    }

    return vram_errs;
}

static void read_vram_buffer(int speed)
{
    xv_prep();

    // read VRAM back into vram_buffer
    switch (speed)
    {
        case 0:
            // slow
            xm_setw(RD_INCR, 0x0000);
            for (uint32_t addr = 0; addr < 0x10000; addr++)
            {
                xm_setw(RD_ADDR, (uint16_t)addr);
                VRAM_RD_DELAY();
                vram_buffer[addr] = xm_getw(DATA);
            }
            break;
        case 1:
            // byte
            xm_setw(RD_INCR, 0x0001);
            xm_setw(RD_ADDR, 0x0000);

            uint8_t * bp = (uint8_t *)&vram_buffer[0];
            for (int addr = 0; addr < 0x10000; addr++)
            {
                *bp++ = xm_getbh(DATA);
                *bp++ = xm_getbl(DATA);
            }
            break;

        case 2:
            // word
            xm_setw(RD_INCR, 0x0001);
            xm_setw(RD_ADDR, 0x0000);

            for (int addr = 0; addr < 0x10000; addr++)
            {
                uint16_t data     = xm_getw(DATA);
                vram_buffer[addr] = data;
            }
            break;

        case 3:
            // long
            xm_setw(RD_INCR, 0x0001);
            xm_setw(RD_ADDR, 0x0000);

            uint32_t * lp = (uint32_t *)&vram_buffer[0];
#if USE_ASM_LONG
            uint32_t tmp_u32;
            uint16_t cnt = 0x800 - 1;
            __asm__ __volatile__(
                    "0:    movep.l  " XM_STR(XM_DATA) "(%[xptr]),%[tmp]\n"
                    "      move.l  %[tmp],(%[dptr])+\n"
                    "      movep.l  " XM_STR(XM_DATA) "(%[xptr]),%[tmp]\n"
                    "      move.l  %[tmp],(%[dptr])+\n"
                    "      movep.l  " XM_STR(XM_DATA) "(%[xptr]),%[tmp]\n"
                    "      move.l  %[tmp],(%[dptr])+\n"
                    "      movep.l  " XM_STR(XM_DATA) "(%[xptr]),%[tmp]\n"
                    "      move.l  %[tmp],(%[dptr])+\n"
                    "      movep.l  " XM_STR(XM_DATA) "(%[xptr]),%[tmp]\n"
                    "      move.l  %[tmp],(%[dptr])+\n"
                    "      movep.l  " XM_STR(XM_DATA) "(%[xptr]),%[tmp]\n"
                    "      move.l  %[tmp],(%[dptr])+\n"
                    "      movep.l  " XM_STR(XM_DATA) "(%[xptr]),%[tmp]\n"
                    "      move.l  %[tmp],(%[dptr])+\n"
                    "      movep.l  " XM_STR(XM_DATA) "(%[xptr]),%[tmp]\n"
                    "      move.l  %[tmp],(%[dptr])+\n"
                    "      movep.l  " XM_STR(XM_DATA) "(%[xptr]),%[tmp]\n"
                    "      move.l  %[tmp],(%[dptr])+\n"
                    "      movep.l  " XM_STR(XM_DATA) "(%[xptr]),%[tmp]\n"
                    "      move.l  %[tmp],(%[dptr])+\n"
                    "      movep.l  " XM_STR(XM_DATA) "(%[xptr]),%[tmp]\n"
                    "      move.l  %[tmp],(%[dptr])+\n"
                    "      movep.l  " XM_STR(XM_DATA) "(%[xptr]),%[tmp]\n"
                    "      move.l  %[tmp],(%[dptr])+\n"
                    "      movep.l  " XM_STR(XM_DATA) "(%[xptr]),%[tmp]\n"
                    "      move.l  %[tmp],(%[dptr])+\n"
                    "      movep.l  " XM_STR(XM_DATA) "(%[xptr]),%[tmp]\n"
                    "      move.l  %[tmp],(%[dptr])+\n"
                    "      movep.l  " XM_STR(XM_DATA) "(%[xptr]),%[tmp]\n"
                    "      move.l  %[tmp],(%[dptr])+\n"
                    "      movep.l  " XM_STR(XM_DATA) "(%[xptr]),%[tmp]\n"
                    "      move.l  %[tmp],(%[dptr])+\n"
                    "      dbf     %[cnt],0b"
                    : [tmp] "=&d"(tmp_u32)
                    : [xptr] "a"(xosera_ptr), [dptr] "a"(lp), [cnt] "d"(cnt)
                    :);
#else

            for (int addr = 0; addr < 0x10000; addr += 2)
            {
                uint32_t data = xm_getl(DATA);
                *lp++         = data;
            }
#endif
            break;

        default:
            break;
    }
}

int test_vram(bool LFSR, int mode, int speed)
{
    int vram_errs = 0;
    first_failure = true;
    xv_prep();

    // set funky mode to show VRAM
    wait_vblank_start();
    xreg_setw(VID_CTRL, 0x0000);
    xreg_setw(PA_LINE_LEN, 136);        // ~65536/480 words per line
    xreg_setw(PA_DISP_ADDR, 0x0000);
    xreg_setw(PA_TILE_CTRL, 0x000F);                 // text mode
    xreg_setw(PA_GFX_CTRL, vram_modes[mode]);        // bitmap + 8-bpp Hx2 Vx1

    dprintf("  > VRAM test=%s speed=%s mode=%s : ", LFSR ? "LFSR" : "ADDR", speed_names[speed], vram_mode_names[mode]);

    // generate pattern_buffer data
    if (LFSR)
    {
        fill_LFSR();
    }
    else
    {
        fill_ADDR();
    }
    RETURN_ON_KEYPRESS();

    update_elapsed();
    uint32_t start_time;
    uint32_t check_time = elapsed_tenthms;
    do
    {
        update_elapsed();
        start_time = elapsed_tenthms;
    } while (start_time == check_time);

    // fill VRAM with pattern_buffer
    switch (speed)
    {
        case 0:
            // slow
            xm_setw(WR_INCR, 0x0000);

            for (uint32_t addr = 0; addr < 0x10000; addr++)
            {
                xm_setw(WR_ADDR, (uint16_t)addr);
                xm_setw(DATA, pattern_buffer[addr]);
                VRAM_WR_DELAY();
            }
            break;

        case 1:
            // byte
            xm_setw(WR_INCR, 0x0001);
            xm_setw(WR_ADDR, 0x0000);

            uint8_t * bp = (uint8_t *)&pattern_buffer[0];
            for (int addr = 0; addr < 0x10000; addr++)
            {
                xm_setbh(DATA, *bp++);
                xm_setbl(DATA, *bp++);
            }
            break;
        case 2:
            // word
            xm_setw(WR_INCR, 0x0001);
            xm_setw(WR_ADDR, 0x0000);

            for (int addr = 0; addr < 0x10000; addr++)
            {
                xm_setw(DATA, pattern_buffer[addr]);
            }
            break;

        case 3:
            // long
            xm_setw(WR_INCR, 0x0001);
            xm_setw(WR_ADDR, 0x0000);

            uint32_t * lp = (uint32_t *)&pattern_buffer[0];
#if USE_ASM_LONG
            uint32_t tmp_u32;
            uint16_t cnt = 0x800 - 1;
            __asm__ __volatile__(
                    "0:     move.l  (%[dptr])+,%[tmp]\n"
                    "       movep.l  %[tmp]," XM_STR(XM_DATA) "(%[xptr])\n"
                    "       move.l  (%[dptr])+,%[tmp]\n"
                    "       movep.l  %[tmp]," XM_STR(XM_DATA) "(%[xptr])\n"
                    "       move.l  (%[dptr])+,%[tmp]\n"
                    "       movep.l  %[tmp]," XM_STR(XM_DATA) "(%[xptr])\n"
                    "       move.l  (%[dptr])+,%[tmp]\n"
                    "       movep.l  %[tmp]," XM_STR(XM_DATA) "(%[xptr])\n"
                    "       move.l  (%[dptr])+,%[tmp]\n"
                    "       movep.l  %[tmp]," XM_STR(XM_DATA) "(%[xptr])\n"
                    "       move.l  (%[dptr])+,%[tmp]\n"
                    "       movep.l  %[tmp]," XM_STR(XM_DATA) "(%[xptr])\n"
                    "       move.l  (%[dptr])+,%[tmp]\n"
                    "       movep.l  %[tmp]," XM_STR(XM_DATA) "(%[xptr])\n"
                    "       move.l  (%[dptr])+,%[tmp]\n"
                    "       movep.l  %[tmp]," XM_STR(XM_DATA) "(%[xptr])\n"
                    "       move.l  (%[dptr])+,%[tmp]\n"
                    "       movep.l  %[tmp]," XM_STR(XM_DATA) "(%[xptr])\n"
                    "       move.l  (%[dptr])+,%[tmp]\n"
                    "       movep.l  %[tmp]," XM_STR(XM_DATA) "(%[xptr])\n"
                    "       move.l  (%[dptr])+,%[tmp]\n"
                    "       movep.l  %[tmp]," XM_STR(XM_DATA) "(%[xptr])\n"
                    "       move.l  (%[dptr])+,%[tmp]\n"
                    "       movep.l  %[tmp]," XM_STR(XM_DATA) "(%[xptr])\n"
                    "       move.l  (%[dptr])+,%[tmp]\n"
                    "       movep.l  %[tmp]," XM_STR(XM_DATA) "(%[xptr])\n"
                    "       move.l  (%[dptr])+,%[tmp]\n"
                    "       movep.l  %[tmp]," XM_STR(XM_DATA) "(%[xptr])\n"
                    "       move.l  (%[dptr])+,%[tmp]\n"
                    "       movep.l  %[tmp]," XM_STR(XM_DATA) "(%[xptr])\n"
                    "       move.l  (%[dptr])+,%[tmp]\n"
                    "       movep.l  %[tmp]," XM_STR(XM_DATA) "(%[xptr])\n"
                    "       dbf     %[cnt],0b"
                    : [tmp] "=&d"(tmp_u32)
                    : [xptr] "a"(xosera_ptr), [dptr] "a"(lp), [cnt] "d"(cnt)
                    :);
#else
            for (int addr = 0; addr < 0x10000; addr += 2)
            {
                xm_setl(DATA, *lp++);
            }
#endif
            break;

        default:
            break;
    }
    RETURN_ON_KEYPRESS();

    read_vram_buffer(speed);
    RETURN_ON_KEYPRESS();

    // verify write was correct
    vram_errs += verify_vram(LFSR, mode, speed);
    if (vram_errs >= 16)
    {
        dprintf("TEST CANCELLED (too many errors)!\n");
    }

    // scroll pattern_buffer and vram
    for (int addr = 0; addr < 0x10000; addr++)
    {
        pattern_buffer[(addr - 1) & 0xffff] = pattern_buffer[addr];
    }

    switch (speed)
    {
        case 0:
            // slow
            xm_setw(RD_INCR, 0x0000);
            xm_setw(WR_INCR, 0x0000);
            for (uint32_t addr = 0; addr < 0x10000; addr++)
            {
                xm_setw(RD_ADDR, (uint16_t)addr);
                VRAM_RD_DELAY();
                uint16_t data = xm_getw(DATA);
                xm_setw(WR_ADDR, (uint16_t)(addr - 1));
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

    read_vram_buffer(speed);
    RETURN_ON_KEYPRESS();

    // verify scroll was correct
    vram_errs += verify_vram(LFSR, mode, speed);
    if (vram_errs == 0)
    {
        update_elapsed();
        unsigned int elapsed_time = elapsed_tenthms - start_time;
        dprintf("PASSED  (%3u.%1ums)\n", elapsed_time / 10, elapsed_time % 10);
    }

    return vram_errs;
}

#define XR_TILEMAP (XR_TILE_ADDR + 0x1000)
#define XR_COLS    28
#define XR_ROWS    20

static int xmem_retry(uint16_t addr, uint16_t baddata, bool LFSR, int mode)
{
    int retries = 0;
    int rc      = 0;
    xv_prep();

    // see if slow read retry will read it correctly (if not, assume
    // it was a write error)
    uint16_t data = (uint16_t)~pattern_buffer[addr];
    while (++retries < 10)
    {
        data = xmem_getw_wait(addr);
        if (data == pattern_buffer[addr])
        {
            add_fail(addr,
                     baddata,
                     pattern_buffer[addr],
                     MODEFLAG_READ | (LFSR ? MODEFLAG_LFSR : MODEFLAG_ADDR) | vram_mode_flags[mode] | MODEFLAG_XRMEM);
            rc = 0;        // read error
            break;
        }
    }

    // try to correct VRAM
    if (data != pattern_buffer[addr])
    {
        for (retries = 0; retries < 10; retries++)
        {
            xmem_setw_wait(addr, pattern_buffer[addr]);
            data = xmem_getw_wait(addr);
            if (data == pattern_buffer[addr])
            {
                add_fail(addr,
                         baddata,
                         pattern_buffer[addr],
                         MODEFLAG_WRITE | (LFSR ? MODEFLAG_LFSR : MODEFLAG_ADDR) | (vram_mode_flags[mode]) |
                             MODEFLAG_XRMEM);
                rc = 1;        // correctable write error
                break;
            }
        }
    }

    // if 10 tries fail, mark it as uncorrectable
    if (data != pattern_buffer[addr])
    {
        add_fail(addr,
                 baddata,
                 pattern_buffer[addr],
                 MODEFLAG_BAD | (LFSR ? MODEFLAG_LFSR : MODEFLAG_ADDR) | (vram_mode_flags[mode]) | MODEFLAG_XRMEM);
        rc = -1;        // uncorrectable error
    }
    else
    {
        vram_buffer[addr] = data;
    }

    // log error
    vram_test_fail_count++;
    if (first_failure)
    {
        dprintf("FAILED!\n");
        first_failure = false;
    }
    dprintf("*** MISMATCH %s %s %s: XMEM[0x%04x]=0x%04x vs data[0x%04x]=0x%04x [Error #%d]\n",
            LFSR ? "LFSR" : "ADDR",
            speed_names[4],
            rc < 0   ? "BAD! "
            : rc > 0 ? "WRITE"
                     : "READ ",
            addr,
            baddata,
            addr,
            pattern_buffer[addr],
            vram_test_fail_count);

    return rc;
}

static int verify_xmem(bool LFSR, int mode)
{
    int xmem_errs = 0;

    // read XMEM back into vram_buffer
    for (int addr = XR_COLOR_ADDR; addr < (XR_COLOR_ADDR + colormem_size); addr++)
    {
        uint16_t data = vram_buffer[addr];
        if (data != pattern_buffer[addr])
        {
            xmem_retry((uint16_t)addr, data, LFSR, mode);
            if (++xmem_errs >= MAX_TEST_FAIL)
            {
                return xmem_errs;
            }
        }
    }
    for (int addr = XR_TILE_ADDR; addr < (XR_TILE_ADDR + XR_TILE_SIZE); addr++)
    {
        uint16_t data = vram_buffer[addr];
        if (data != pattern_buffer[addr])
        {
            xmem_retry((uint16_t)addr, data, LFSR, mode);
            if (++xmem_errs >= MAX_TEST_FAIL)
            {
                return xmem_errs;
            }
        }
    }
    for (int addr = XR_COPPER_ADDR; addr < (XR_COPPER_ADDR + XR_COPPER_SIZE); addr++)
    {
        uint16_t data = vram_buffer[addr];
        if (data != pattern_buffer[addr])
        {
            xmem_retry((uint16_t)addr, data, LFSR, mode);
            if (++xmem_errs >= MAX_TEST_FAIL)
            {
                return xmem_errs;
            }
        }
    }

    return xmem_errs;
}

static void read_xmem_buffer()
{
    xv_prep();

    // read XMEM back into vram_buffer
    xmem_getw_next_addr(XR_COLOR_ADDR);
    for (int addr = XR_COLOR_ADDR; addr < (XR_COLOR_ADDR + colormem_size); addr++)
    {
        uint16_t data     = xmem_getw_next_wait();
        vram_buffer[addr] = data;
    }
    xmem_getw_next_addr(XR_TILE_ADDR);
    for (int addr = XR_TILE_ADDR; addr < (XR_TILE_ADDR + XR_TILE_SIZE); addr++)
    {
        uint16_t data     = xmem_getw_next_wait();
        vram_buffer[addr] = data;
    }
    xmem_getw_next_addr(XR_COPPER_ADDR);
    for (int addr = XR_COPPER_ADDR; addr < (XR_COPPER_ADDR + XR_COPPER_SIZE); addr++)
    {
        uint16_t data     = xmem_getw_next_wait();
        vram_buffer[addr] = data;
    }
}

int test_xmem(bool LFSR, int mode)
{
    int xmem_errs = 0;
    first_failure = true;
    xv_prep();

    // set funky mode to show XMEM
    wait_vblank_start();
    xreg_setw(PA_GFX_CTRL, 0x0080);
    xreg_setw(VID_RIGHT, xosera_vid_width());        // restore border
    xmem_setw_next_addr(XR_TILEMAP);
    for (int i = 0; i < (XR_COLS * XR_ROWS); i++)
    {
        xmem_setw_next(i);
    }
    wait_vblank_start();
    xreg_setw(PA_GFX_CTRL, vram_modes[mode] & ~0x0040);        // text
    xreg_setw(PA_TILE_CTRL, 0x0207);                           // tile=0x0000,tile=tile_mem, map=tile_mem, 8x8 tiles
    xreg_setw(PA_LINE_LEN, XR_COLS);
    xreg_setw(PA_DISP_ADDR, XR_TILEMAP);


    unsigned int elapsed_time = 0;

    dprintf("  > XMEM test=%s speed=%s mode=%s : ", LFSR ? "LFSR" : "ADDR", speed_names[4], vram_mode_names[mode]);
    // fill XMEM with pattern_buffer
    wait_vblank_start();
    for (int r = 0; r < 16; r++)
    {
        // generate pattern_buffer data
        if (LFSR)
        {
            fill_LFSR();
        }
        else
        {
            fill_ADDR();
        }
        RETURN_ON_KEYPRESS();

        NukeColor = 0xffff;        // disable color cycle while testing COLOR mem

        update_elapsed();
        uint32_t start_time;
        uint32_t check_time = elapsed_tenthms;
        do
        {
            update_elapsed();
            start_time = elapsed_tenthms;
        } while (start_time == check_time);

        // word color mem
        xmem_setw_next_addr(XR_COLOR_ADDR);
        for (int addr = XR_COLOR_ADDR; addr < (XR_COLOR_ADDR + colormem_size); addr++)
        {
            xmem_setw_next(pattern_buffer[addr]);
        }

        // word tile mem
        xmem_setw_next_addr(XR_TILE_ADDR);
        for (int addr = XR_TILE_ADDR; addr < (XR_TILE_ADDR + XR_TILE_SIZE); addr++)
        {
            xmem_setw_next(pattern_buffer[addr]);
        }
        // word copper mem
        xmem_setw_next_addr(XR_COPPER_ADDR);
        for (int addr = XR_COPPER_ADDR; addr < (XR_COPPER_ADDR + XR_COPPER_SIZE); addr++)
        {
            xmem_setw_next(pattern_buffer[addr]);
        }

        RETURN_ON_KEYPRESS();

        read_xmem_buffer();

        NukeColor = 0;

        RETURN_ON_KEYPRESS();
        // verify write was correct
        xmem_errs += verify_xmem(LFSR, mode);
        if (xmem_errs >= 16)
        {
            dprintf("TEST CANCELLED (too many errors)!\n");
        }

        if (xmem_errs == 0)
        {
            update_elapsed();
            elapsed_time += elapsed_tenthms - start_time;
        }
    }

    xreg_setw(VID_RIGHT, xosera_vid_width() - 2);        // steal a few pixels for border
    xreg_setw(VID_CTRL, 0x0000);
    xmem_setw(XR_COLOR_A_ADDR, vram_test_fail_count ? 0x0C00 : 0x00C0);
    xmem_setw(XR_COLOR_B_ADDR, vram_test_fail_count ? 0x0C00 : 0x00C0);

    if (xmem_errs == 0)
    {
        dprintf("PASSED  (%3u.%1ums)\n", elapsed_time / 10, elapsed_time % 10);
    }

    return xmem_errs;
}

struct xosera_initdata
{
    char     name_version[28];
    uint32_t githash;
};

xosera_info_t initinfo;

void xosera_vramtest()
{
    dprintf("Xosera_vramtest_m68k\n");

    dprintf("Checking for Xosera XANSI firmware...");
    if (xosera_xansi_detect(true))        // check for XANSI (and disable input cursor if present)
    {
        dprintf("detected.\n");
    }
    else
    {
        dprintf(
            "\n\nXosera XANSI firmware was not detected!\n"
            "This program will likely trap without Xosera hardware.\n");
    }

    xv_prep();

    uint8_t saved_config      = xosera_cur_config();
    uint8_t cur_xosera_config = ~0;
    uint8_t new_config        = 0;

    while (true)
    {
        // switch between configurations every few test iterations
        if (vram_test_count & MODE_TOGGLE_BIT)
        {
            new_config = (new_config + 1) & 0x3;
        }

        if (new_config != cur_xosera_config)
        {
            update_elapsed();
            cur_xosera_config = new_config;
            dprintf("\n [Switching to Xosera config #%d...", cur_xosera_config);
            bool success = xosera_init(cur_xosera_config);
            xm_setw(TIMER, 0xffff);        // set to wrapping 16-bit counter
            last_timer_val = xm_getw(TIMER);
            dprintf("%s (%dx%d). ]\n", success ? "succeeded" : "FAILED", xosera_vid_width(), xosera_vid_height());
            xosera_get_info(&initinfo);
            xreg_setw(VID_RIGHT, xosera_vid_width() - 2);        // steal a few pixels for border
            xreg_setw(VID_CTRL, 0x0000);
            xmem_setw(XR_COLOR_A_ADDR, vram_test_fail_count ? 0x0C00 : 0x00C0);
            xmem_setw(XR_COLOR_B_ADDR, vram_test_fail_count ? 0x0C00 : 0x00C0);

            xreg_setw(PB_GFX_CTRL, 0x0080);
            has_PF_B = xreg_getw(PB_GFX_CTRL) & 0x0080;
            dprintf("  PF_B is %s testing COLOR_B XMEM\n", has_PF_B ? "present," : "disabled, not");
            colormem_size = has_PF_B ? (XR_COLOR_A_SIZE + XR_COLOR_B_SIZE) : XR_COLOR_A_SIZE;
        }

        update_elapsed();
        unsigned int t = elapsed_tenthms;
        unsigned int h = t / (10000 * 60 * 60);
        unsigned int m = t / (10000 * 60) % 60;
        unsigned int s = (t / 10000) % 60;

        dprintf("\n>>> xosera_vramtest_m68k iteration: %d, running %u:%02u:%02u, errors: %d\n",
                vram_test_count++,
                h,
                m,
                s,
                vram_test_fail_count);

        uint16_t feature   = xm_getw(FEATURE);        // TODO: this is feature codes
        uint16_t monwidth  = xosera_vid_width();
        uint16_t monheight = xosera_vid_height();

        if (initinfo.description_str[0])
        {
            dprintf("    ID: %.48s\n", initinfo.description_str);
        }
        else
        {
            dprintf("    ID: (no COPPER mem)\n");
        }

        dprintf("    Config #%d [%04x]   Res:%ux%u   Git:0x%08x\n",
                cur_xosera_config,
                (unsigned int)feature,
                (unsigned int)monwidth,
                (unsigned int)monheight,
                (unsigned int)initinfo.githash);

        install_intr();

        for (int i = 0; i < TEST_MODES; i++)
        {
            for (int j = 0; j < TEST_SPEEDS - 1; j++)
            {
                if (test_vram(false, i, j) || delay_check(DELAY_TIME))
                {
                    break;
                }
                if (test_vram(true, i, j) || delay_check(DELAY_TIME))
                {
                    break;
                }
            }
            {
                if (test_xmem(false, i) || delay_check(DELAY_TIME))
                {
                    break;
                };
                if (test_xmem(true, i) || delay_check(DELAY_TIME))
                {
                    break;
                }
            }
            BREAK_ON_KEYPRESS();
        }
        BREAK_ON_KEYPRESS();

        if (num_vram_fails)
        {
            dprintf("Cummulative VRAM test errors:\n");
            for (int i = 0; i < num_vram_fails; i++)
            {
                struct vram_fail_info * fip = &vram_fails[i];

                dprintf("pass %3u #%2u @ 0x%04x=0x%04x vs 0x%04x pat=%s%s\te=%s%s%s\tm=%s%s%s%s%s\tt=%s%s%s%s%s\n",
                        fip->pass,
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
                        fip->flags & MODEFLAG_4BPP ? "4" : "",
                        fip->flags & MODEFLAG_8BPP ? "8" : "",
                        fip->flags & MODEFLAG_XBPP ? "X" : "",
                        fip->flags & MODEFLAG_BLANK ? "B" : "",
                        fip->flags & MODEFLAG_SLOW ? "S" : "",
                        fip->flags & MODEFLAG_BYTE ? "B" : "",
                        fip->flags & MODEFLAG_WORD ? "W" : "",
                        fip->flags & MODEFLAG_LONG ? "L" : "",
                        fip->flags & MODEFLAG_XRMEM ? "XMEM" : "");
            }
        }
    }
    wait_vblank_start();
    remove_intr();

    // reset console
    xosera_init(saved_config);        // restore fonts, which were trashed
    xosera_xansi_restore();
    dprintf("\n\nExiting...\n");
}
