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

#include <rosco_m68k/machine.h>
#include <rosco_m68k/xosera.h>

#include "rosco_m68k_support.h"

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

extern void install_intr(void);
extern void remove_intr(void);

extern volatile uint32_t XFrameCount;
extern volatile uint16_t NukeColor;

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

// resident _EFP_SD_INIT hook to disable SD loader upon next boot
static void disable_sd_boot()
{
    extern void resident_init();        // no SD boot resident setup
    resident_init();                    // install no SD hook next next warm-start
}

static void wait_vblank_start()
{
    xv_prep();
    xwait_not_vblank();
    xwait_vblank();
}

static inline void check_vblank()
{
    xv_prep();
    if (!xm_getb_sys_ctrl(VBLANK) || xreg_getw(SCANLINE) > 520)
    {
        wait_vblank_start();
    }
}

_NOINLINE void restore_def_colors()
{
    wait_vblank_start();
    xv_prep();
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

static void reset_video(void)
{
    remove_intr();

    wait_vblank_start();

    xv_prep();
    xreg_setw(VID_CTRL, MAKE_VID_CTRL(0, 0x08));        // set border grey
    xreg_setw(COPP_CTRL, MAKE_COPP_CTRL(0));            // disable copper
    xreg_setw(VID_LEFT, 0);
    xreg_setw(VID_RIGHT, xosera_vid_width());
    xreg_setw(PA_GFX_CTRL, 0x0000);
    xreg_setw(PA_TILE_CTRL, 0x000F);
    xreg_setw(PA_DISP_ADDR, 0x0000);
    xreg_setw(PA_LINE_LEN, xosera_vid_width() / 8);        // line len
    xreg_setw(PA_H_SCROLL, 0x0000);
    xreg_setw(PA_V_SCROLL, 0x0000);
    xreg_setw(PA_HV_FSCALE, 0x0000);
    xreg_setw(PB_GFX_CTRL, 0x0080);

    restore_def_colors();

    xosera_xansi_restore();

    char c = 0;
    while (mcCheckInput())
    {
        c = mcInputchar();
    }

#if 1        // handy for development to force Kermit upload
    if (c == '\x1b')
    {
        debug_printf("Disabling SD on next boot...\n");
        disable_sd_boot();
    }
#endif
}

_NOINLINE void delay_check(int ms)
{
    xv_prep();
    while (ms--)
    {
        if (mcCheckInput())
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

// clang-format off
uint8_t fontm[2 * 8 * 16] = {
    // 0
    0b0011, 0b0000,        // .#..
    0b1100, 0b1100,        // #.#.
    0b1100, 0b1100,        // #.#.
    0b1100, 0b1100,        // #.#.
    0b1100, 0b1100,        // #.#.
    0b1100, 0b1100,        // #.#.
    0b0011, 0b0000,        // .#..
    0b0000, 0b0000,        // ....
    // 1
    0b0011, 0b0000,        // .#..
    0b1111, 0b0000,        // ##..
    0b0011, 0b0000,        // .#..
    0b0011, 0b0000,        // .#..
    0b0011, 0b0000,        // .#..
    0b0011, 0b0000,        // .#..
    0b1111, 0b1100,        // ###.
    0b0000, 0b0000,        // ....
    // 2
    0b1111, 0b1100,        // ###.
    0b0000, 0b1100,        // ..#.
    0b0000, 0b1100,        // ..#.
    0b1111, 0b1100,        // ###.
    0b1100, 0b0000,        // #...
    0b1100, 0b0000,        // #...
    0b1111, 0b1100,        // ###.
    0b0000, 0b0000,        // ....
    // 3
    0b1111, 0b1100,        // ###.
    0b0000, 0b1100,        // ..#.
    0b0000, 0b1100,        // ..#.
    0b1111, 0b1100,        // ###.
    0b0000, 0b1100,        // ..#.
    0b0000, 0b1100,        // ..#.
    0b1111, 0b1100,        // ###.
    0b0000, 0b0000,        // ....
    // 4
    0b1100, 0b1100,        // #.#.
    0b1100, 0b1100,        // #.#.
    0b1100, 0b1100,        // #.#.
    0b1111, 0b1100,        // ###.
    0b0000, 0b1100,        // ..#.
    0b0000, 0b1100,        // ..#.
    0b0000, 0b1100,        // ..#.
    0b0000, 0b0000,        // ....
    // 5
    0b1111, 0b1100,        // ###.
    0b1100, 0b0000,        // #...
    0b1100, 0b0000,        // #...
    0b1111, 0b1100,        // ###.
    0b0000, 0b1100,        // ..#.
    0b0000, 0b1100,        // ..#.
    0b1111, 0b1100,        // ###.
    0b0000, 0b0000,        // ....
    // 6
    0b0011, 0b1100,        // ###.
    0b1100, 0b0000,        // #...
    0b1100, 0b0000,        // #...
    0b1111, 0b1100,        // ###.
    0b1100, 0b1100,        // #.#.
    0b1100, 0b1100,        // #.#.
    0b1111, 0b1100,        // ###.
    0b0000, 0b0000,        // ....
    // 7
    0b1111, 0b1100,        // ###.
    0b0000, 0b1100,        // ..#.
    0b0000, 0b1100,        // ..#.
    0b0000, 0b1100,        // ..#.
    0b0000, 0b1100,        // ..#.
    0b0000, 0b1100,        // ..#.
    0b0000, 0b1100,        // ..#.
    0b0000, 0b0000,        // ....
    // 8
    0b1111, 0b1100,        // ###.
    0b1100, 0b1100,        // #.#.
    0b1100, 0b1100,        // #.#.
    0b1111, 0b1100,        // ###.
    0b1100, 0b1100,        // #.#.
    0b1100, 0b1100,        // #.#.
    0b1111, 0b1100,        // ###.
    0b0000, 0b0000,        // ....
    // 9
    0b1111, 0b1100,        // ###.
    0b1100, 0b1100,        // #.#.
    0b1100, 0b1100,        // #.#.
    0b1111, 0b1100,        // ###.
    0b0000, 0b1100,        // ..#.
    0b0000, 0b1100,        // ..#.
    0b1111, 0b0000,        // ###.
    0b0000, 0b0000,        // ....
    // A
    0b0011, 0b0000,        // .#..
    0b1100, 0b1100,        // #.#.
    0b1100, 0b1100,        // #.#.
    0b1111, 0b1100,        // ###.
    0b1100, 0b1100,        // #.#.
    0b1100, 0b1100,        // #.#.
    0b1100, 0b1100,        // #.#.
    0b0000, 0b0000,        // ....
    // B
    0b1111, 0b0000,        // ##..
    0b1100, 0b1100,        // #.#.
    0b1100, 0b1100,        // #.#.
    0b1111, 0b0000,        // ##..
    0b1100, 0b1100,        // #.#.
    0b1100, 0b1100,        // #.#.
    0b1111, 0b0000,        // ##..
    0b0000, 0b0000,        // ....
    // C
    0b0011, 0b1100,        // .##.
    0b1100, 0b0000,        // #...
    0b1100, 0b0000,        // #...
    0b1100, 0b0000,        // #...
    0b1100, 0b0000,        // #...
    0b1100, 0b0000,        // #...
    0b0011, 0b1100,        // .##.
    0b0000, 0b0000,        // ....
    // D
    0b1111, 0b0000,        // ##..
    0b1100, 0b1100,        // #.#.
    0b1100, 0b1100,        // #.#.
    0b1100, 0b1100,        // #.#.
    0b1100, 0b1100,        // #.#.
    0b1100, 0b1100,        // #.#.
    0b1111, 0b0000,        // ##..
    0b0000, 0b0000,        // ....
    // E
    0b1111, 0b1100,        // ###.
    0b1100, 0b0000,        // #...
    0b1100, 0b0000,        // #...
    0b1111, 0b1100,        // ###.
    0b1100, 0b0000,        // #...
    0b1100, 0b0000,        // #...
    0b1111, 0b1100,        // ###.
    0b0000, 0b0000,        // ....
    // F
    0b1111, 0b1100,        // ###.
    0b1100, 0b0000,        // #...
    0b1100, 0b0000,        // #...
    0b1111, 0b0000,        // ##..
    0b1100, 0b0000,        // #...
    0b1100, 0b0000,        // #...
    0b1100, 0b0000         // #...
};
// clang-format on

#if 0
void print_digit(uint16_t off, uint16_t ll, uint16_t dig, uint16_t color)
{
    union lw
    {
        uint32_t l;
        uint16_t w[2];
    };

    union lw * lwp = (union lw *)&font[dig * 7];

    xv_prep();
    xm_setw(WR_INCR, 0x0001);        // set write inc
    for (uint16_t h = 0; h < 7; h++)
    {
        xm_setw(WR_ADDR, off + (h * ll));        // set write address
        xm_set_vram_mask((lwp->w[0] & 0x8000 ? 0xc : 0) | (lwp->w[0] & 0x0080 ? 0x3 : 0));
        xm_setw(DATA, lwp->w[0] & color);
        xm_set_vram_mask((lwp->w[1] & 0x8000 ? 0xc : 0) | (lwp->w[1] & 0x0080 ? 0x3 : 0));
        xm_setw(DATA, lwp->w[1] & color);
        lwp++;
    }
    xm_set_vram_mask(0x0F);        // no VRAM write masking
}
#else
void print_digit(uint16_t off, uint16_t ll, uint16_t dig, uint16_t color)
{
    uint8_t * lwp = &fontm[dig * (8 * 2)];

    xv_prep();
    xm_setw(WR_INCR, 0x0001);        // set write inc
    for (uint16_t h = 0; h < 7; h++)
    {
        xm_setw(WR_ADDR, off + (h * ll));        // set write address
        xm_set_vram_mask(*lwp++);
        xm_setw(DATA, color);        // optimization: latch upper byte
        xm_set_vram_mask(*lwp++);
        xm_setbl(DATA, color & 0xff);        // optimization: upper byte latched, so only write lower byte
    }
    xm_set_vram_mask(0x0F);        // no VRAM write masking
}

inline void print_digit_xy(volatile xmreg_t * const xosera_ptr, uint16_t x, uint16_t y, uint16_t dig, uint16_t color)
{
    uint8_t * lwp = &fontm[dig * (8 * 2)];

    for (uint16_t h = 0; h < 7; h++)
    {
        xm_set_vram_mask(*lwp++);
        xm_set_pixel_data(x << 1, y + h, color);        // optimization: latch upper byte
        xm_set_vram_mask(*lwp++);
        xm_setbl(DATA, (uint8_t)(color & 0xff));        // optimization: upper byte latched, so only write lower byte
    }
    xm_set_vram_mask(0xf);        // no VRAM write masking
}

#endif

void test_colormap()
{
    xv_prep();

    xwait_not_vblank();
    xwait_vblank();

    uint16_t linelen = 160;

    xreg_setw(VID_CTRL, MAKE_VID_CTRL(0, 0x00));        // set border to black
    xreg_setw(VID_LEFT, (xosera_vid_width() > 640 ? ((xosera_vid_width() - 640) / 2) : 0) + 0);
    xreg_setw(VID_RIGHT, (xosera_vid_width() > 640 ? (xosera_vid_width() - 640) / 2 : 0) + 640);
    xreg_setw(PA_GFX_CTRL, MAKE_GFX_CTRL(0x00, GFX_VISIBLE, GFX_8_BPP, GFX_BITMAP, GFX_2X, GFX_2X));
    xreg_setw(PA_TILE_CTRL, 0x0C07);
    xreg_setw(PA_DISP_ADDR, 0x0000);
    xreg_setw(PA_LINE_LEN, linelen);        // line len
    xreg_setw(PA_H_SCROLL, 0x0000);
    xreg_setw(PA_V_SCROLL, 0x0000);
    xreg_setw(PA_HV_FSCALE, 0x0000);
    xreg_setw(PB_GFX_CTRL, 0x0080);

    for (uint16_t pass = 0; pass < 2; pass++)
    {
        xm_setw(WR_INCR, 0x0001);        // set write inc
        xm_setw(WR_ADDR, 0x0000);        // set write address

        uint16_t c = 0;
        xm_setup_pixel_addr(0x0000, linelen, 1, 1);
        xm_setw(PIXEL_X, 0);
        for (uint16_t y = 0; y < 240; y += (240 / 16))
        {
            xm_setl(DATA, 0);
            for (uint16_t bx = 0; bx < 10 - 1; bx++)
            {
                xm_setbl(DATA, 0);
            }

            for (uint16_t iy = 1; iy < 14; iy++)
            {
                uint32_t ic = (c << 24) | (c << 16) | (c << 8) | c;
                xm_setw(PIXEL_Y, y + iy);
                for (uint16_t x = 0; x < 320; x += (320 / 16))
                {
                    xm_setl(DATA, ic & 0xffffff);
                    xm_setl(DATA, ic);
                    xm_setl(DATA, ic);
                    xm_setl(DATA, ic);
                    xm_setl(DATA, ic & 0xffffff00);
                    ic += 0x01010101;
                }
                xm_setl(DATA, 0);
                for (uint16_t bx = 0; bx < 10 - 1; bx++)
                {
                    xm_setbl(DATA, 0);
                }
            }
            c += 16;
        }
        c = 0;
        for (uint16_t y = 4; y < 240; y += (240 / 16))
        {
            for (uint16_t x = 4; x < 320; x += (320 / 16))
            {
                uint16_t color = 0xffff;
                if ((def_colors[c] & 0x0880) == 0x880)
                {
                    color = 0x0000;
                }
                if (pass)
                {
                    uint16_t dig = c >> 4;
                    print_digit_xy(xosera_ptr, x + 2, y, dig, color);
                    dig = c & 0xf;
                    print_digit_xy(xosera_ptr, x + 6, y, dig, color);
                }
                else
                {
                    uint16_t dig = c / 100;
                    if (dig)
                    {
                        print_digit_xy(xosera_ptr, x, y, dig, color);
                    }
                    uint16_t dig2 = (c / 10) % 10;
                    if (dig || dig2)
                    {
                        print_digit_xy(xosera_ptr, x + 4, y, dig2, color);
                    }
                    dig = c % 10;
                    print_digit_xy(xosera_ptr, x + 8, y, dig, color);
                }
                c++;
            }
        }

        delay_check(DELAY_TIME * 3);
    }
}

int main(void)
{
    mcBusywait(1000 * 500);        // wait a bit for terminal window/serial
    while (mcCheckInput())         // clear any queued input
    {
        mcInputchar();
    }

    debug_printf("Xosera_modetest_m68k\n");

    debug_printf("Checking for Xosera XANSI firmware...");
    if (xosera_xansi_detect(true))        // check for XANSI (and disable input cursor if present)
    {
        debug_printf("detected.\n");
    }
    else
    {
        debug_printf(
            "\n\nXosera XANSI firmware was not detected!\n"
            "This program will likely trap without Xosera hardware.\n");
    }

    debug_printf("\nCalling xosera_sync()...");

    xv_prep();
    bool syncok = xosera_sync();
    debug_printf("%s\n\n", syncok ? "succeeded" : "FAILED");

    debug_printf("\nCalling xosera_init(XINIT_CONFIG_640x480)...");
    bool success = xosera_init(XINIT_CONFIG_640x480);
    debug_printf("%s (%dx%d)\n\n", success ? "succeeded" : "FAILED", xosera_vid_width(), xosera_vid_height());
    xosera_get_info(&initinfo);
    xwait_not_vblank();
    xwait_vblank();

    xreg_setw(VID_CTRL, MAKE_VID_CTRL(0, 0x05));        // set border to 5
    xreg_setw(PA_GFX_CTRL, MAKE_GFX_CTRL(0, GFX_BLANKED, GFX_1_BPP, GFX_TILEMAP, GFX_1X, GFX_1X));
    xreg_setw(PB_GFX_CTRL, MAKE_GFX_CTRL(0, GFX_BLANKED, GFX_1_BPP, GFX_TILEMAP, GFX_1X, GFX_1X));

    xm_setw(WR_INCR, 0x0001);        // set write inc
    xm_setw(WR_ADDR, 0x0000);        // set write address

    for (uint16_t i = 0; i < 32768; i++)
    {
        xm_setl(DATA, 0x0000);
    }

    cpu_delay(1000);
    debug_printf("xosera_get_info details:\n");
    xmem_getw_next_addr(XR_COPPER_ADDR);

    debug_printf("\n");
    debug_printf("Description : \"%s\"\n", initinfo.description_str);
    debug_printf("Version BCD : %x.%02x\n", initinfo.version_bcd >> 8, initinfo.version_bcd & 0xff);
    debug_printf("Git hash    : #%08x %s\n", initinfo.githash, initinfo.git_modified ? "[modified]" : "[clean]");

    cpu_delay(1000);

    debug_printf("\nBegin...\n");

    while (!mcCheckInput())
    {
        wait_vblank_start();

        restore_def_colors();
        test_colormap();
    }

    reset_video();
}
