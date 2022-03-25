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
//#define DELAY_TIME 5000        // human speed
#define DELAY_TIME 1000        // impatient human speed
//#define DELAY_TIME 500        // machine speed

#define COPPER_TEST    1
#define LR_MARGIN_TEST 0

#define BLIT_TEST_PIC      0
#define TUT_PIC            1
#define SHUTTLE_PIC        2
#define TRUECOLOR_TEST_PIC 3
#define SELF_PIC           4

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

// 32x16 nibble test sprite "programmer art"
uint8_t moto_m[] = {
    0x33, 0x30, 0x00, 0x00, 0x00, 0x11, 0x11, 0x11, 0x11, 0x11, 0x00, 0x00, 0x00, 0x00, 0x03, 0x33, 0x30, 0x00, 0x00,
    0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x11, 0x11, 0x11, 0xFF,
    0x11, 0x11, 0x11, 0xFF, 0x11, 0x11, 0x11, 0x00, 0x00, 0x00, 0x00, 0x11, 0x11, 0x11, 0x11, 0xFF, 0x11, 0x11, 0x11,
    0xFF, 0x11, 0x11, 0x11, 0x11, 0x00, 0x00, 0x00, 0x11, 0x11, 0x11, 0x11, 0xFF, 0x11, 0x11, 0x11, 0xFF, 0x11, 0x11,
    0x11, 0x11, 0x00, 0x00, 0x11, 0x11, 0x11, 0x11, 0xFF, 0xFF, 0xFF, 0x11, 0xFF, 0xFF, 0xFF, 0x11, 0x11, 0x11, 0x11,
    0x00, 0x11, 0x11, 0x11, 0x11, 0xFF, 0xFF, 0xFF, 0x11, 0xFF, 0xFF, 0xFF, 0x11, 0x11, 0x11, 0x11, 0x00, 0x11, 0x11,
    0x11, 0x11, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x11, 0x11, 0x11, 0x11, 0x00, 0x11, 0x11, 0x11, 0xFF, 0xFF,
    0x11, 0xFF, 0xFF, 0xFF, 0x11, 0xFF, 0xFF, 0x11, 0x11, 0x11, 0x00, 0x11, 0x11, 0x11, 0xFF, 0x11, 0x11, 0x11, 0xFF,
    0x11, 0x11, 0x11, 0xFF, 0x11, 0x11, 0x11, 0x00, 0x00, 0x11, 0x11, 0xFF, 0x11, 0x11, 0x11, 0xFF, 0x11, 0x11, 0x11,
    0xFF, 0x11, 0x11, 0x00, 0x00, 0x00, 0x11, 0x11, 0xFF, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0xFF, 0x11, 0x11,
    0x00, 0x00, 0x00, 0x00, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x00, 0x00, 0x00, 0x00,
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x30, 0x11, 0x22, 0x33,
    0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xF3, 0x33, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x33};


#if COPPER_TEST
// Copper list
const uint32_t copper_list[] = {COP_WAIT_V(30 * 0),  COP_MOVEP(0x000, 0),
                                COP_WAIT_V(30 * 1),  COP_MOVEP(0x111, 0),
                                COP_WAIT_V(30 * 2),  COP_MOVEP(0x222, 0),
                                COP_WAIT_V(30 * 3),  COP_MOVEP(0x333, 0),
                                COP_WAIT_V(30 * 4),  COP_MOVEP(0x444, 0),
                                COP_WAIT_V(30 * 5),  COP_MOVEP(0x555, 0),
                                COP_WAIT_V(30 * 6),  COP_MOVEP(0x666, 0),
                                COP_WAIT_V(30 * 7),  COP_MOVEP(0x777, 0),
                                COP_WAIT_V(30 * 8),  COP_MOVEP(0x888, 0),
                                COP_WAIT_V(30 * 9),  COP_MOVEP(0x999, 0),
                                COP_WAIT_V(30 * 10), COP_MOVEP(0xaaa, 0),
                                COP_WAIT_V(30 * 11), COP_MOVEP(0xbbb, 0),
                                COP_WAIT_V(30 * 12), COP_MOVEP(0xccc, 0),
                                COP_WAIT_V(30 * 13), COP_MOVEP(0xddd, 0),
                                COP_WAIT_V(30 * 14), COP_MOVEP(0xeee, 0),
                                COP_WAIT_V(30 * 15), COP_MOVEP(0xfff, 0),
                                COP_WAIT_V(30 * 16), COP_END()};

const uint16_t copper_list_len = NUM_ELEMENTS(copper_list);

static_assert(NUM_ELEMENTS(copper_list) < 1024, "copper list too long");

// 320x200 copper
// Copper list
uint32_t copper_320x200[] = {
    COP_WAIT_V(40),                        // wait  0, 40                   ; Wait for line 40, H position ignored
    COP_MOVER(0x0065, PA_GFX_CTRL),        // mover 0x0065, PA_GFX_CTRL     ; Set to 8-bpp + Hx2 + Vx2
    COP_MOVER(0x0065, PB_GFX_CTRL),        // mover 0x0065, PA_GFX_CTRL     ; Set to 8-bpp + Hx2 + Vx2
    COP_WAIT_V(440),                       // wait  0, 440                  ; Wait for line 440, H position ignored
    COP_MOVER(0x00E5, PA_GFX_CTRL),        // mover 0x00E5, PA_GFX_CTRL     ; Set to Blank + 8-bpp + Hx2 + Vx2
    //    COP_MOVER(0x00E5, PB_GFX_CTRL),         // mover 0x00E5, PA_GFX_CTRL     ; Set to Blank + 8-bpp + Hx2 + Vx2
    COP_MOVER((XR_TILE_ADDR + 0x1000), PB_LINE_ADDR),
    COP_MOVER(0xF009, PB_GFX_CTRL),         // mover 0x00E5, PA_GFX_CTRL     ; Set to Blank + 8-bpp + Hx2 + Vx2
    COP_MOVER(0x0E07, PB_TILE_CTRL),        // mover 0x00E5, PA_GFX_CTRL     ; Set to Blank + 8-bpp + Hx2 + Vx2
    COP_MOVER(28, PB_LINE_LEN),
    COP_WAIT_V(480),        // wait  0, 440                  ; Wait for line 440, H position ignored
    COP_MOVER(160, PB_LINE_LEN),
    COP_MOVER(0x000F, PB_TILE_CTRL),
    COP_MOVER(0x00E5, PA_GFX_CTRL),        // mover 0x00E5, PA_GFX_CTRL     ; Set to Blank + 8-bpp + Hx2 + Vx2
    COP_MOVER(0x00E5, PB_GFX_CTRL),        // mover 0x00E5, PA_GFX_CTRL     ; Set to Blank + 8-bpp + Hx2 + Vx2
    COP_END()                              // nextf
};

#endif

// dummy global variable
uint32_t global;        // this is used to prevent the compiler from optimizing out tests

uint8_t xosera_initdata[32];

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

// resident _EFP_SD_INIT hook to disable SD loader upon next boot
static void disable_sd_boot()
{
    extern void resident_init();        // no SD boot resident setup
    resident_init();                    // install no SD hook next next warm-start
}

static inline void wait_vsync()
{
    while (xreg_getw(SCANLINE) < 0x8000)
        ;
}

void wait_not_vsync()
{
    while (xreg_getw(SCANLINE) < 0x8000)
        ;
}

void wait_vsync_start()
{
    while (xreg_getw(SCANLINE) >= 0x8000)
        ;
    while (xreg_getw(SCANLINE) < 0x8000)
        ;
}

static inline void check_vsync()
{
    while (xreg_getw(SCANLINE) < 0x8000)
        ;
    while ((xreg_getw(SCANLINE) & 0x3ff) > 520)
        ;
}

static inline void wait_memory()
{
    while (xm_getbl(SYS_CTRL) & 0x80)
        ;
}

_NOINLINE void restore_colors()
{
    wait_vsync_start();
    xm_setw(XR_ADDR, XR_COLOR_ADDR);
    uint16_t * cp = def_colors;
    for (uint16_t i = 0; i < 256; i++)
    {
        xm_setw(XR_DATA, *cp++);
    };
    // set B colors to same, alpha 0x8 (with color fully transparent)
    xm_setw(XR_DATA, 0x0000);
    cp = def_colors + 1;
    for (uint16_t i = 1; i < 256; i++)
    {
        xm_setw(XR_DATA, 0x8000 | *cp++);
    };
}

_NOINLINE void restore_colors2(uint8_t alpha)
{
    wait_vsync_start();
    xm_setw(XR_ADDR, XR_COLOR_B_ADDR);
    uint16_t * cp = def_colors;
    for (uint16_t i = 0; i < 256; i++)
    {
        uint16_t w = *cp++;
        if (i)
        {
            w = ((alpha & 0xf) << 12) | (w & 0xfff);
        }
        else
        {
            w = 0;
        }
        xm_setw(XR_DATA, w);
    };
}

// sets test blend palette
_NOINLINE void restore_colors3()
{
    wait_vsync_start();
    xm_setw(XR_ADDR, XR_COLOR_B_ADDR);
    uint16_t * cp = def_colors;
    for (uint16_t i = 0; i < 256; i++)
    {
        uint16_t w = *cp++;
        if (i)
        {
            w = ((i & 0x3) << 14) | (w & 0xfff);
        }
        else
        {
            w = 0;
        }
        xm_setw(XR_DATA, w);
    };
}

_NOINLINE void dupe_colors(int alpha)
{
    wait_vsync_start();
    uint16_t a = (alpha & 0xf) << 12;
    for (uint16_t i = 0; i < 256; i++)
    {

        wait_memory();

        uint16_t v = (xmem_getw_wait(XR_COLOR_A_ADDR + i) & 0xfff) | a;

        xmem_setw(XR_COLOR_B_ADDR + i, v);
        wait_memory();
    };
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

static uint16_t screen_addr;
static uint8_t  text_columns;
static uint8_t  text_rows;
static uint8_t  text_color = 0x02;        // dark green on black

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
    xm_setw(WR_INCR, 1);
    xm_setw(WR_ADDR, screen_addr);
    xm_setbh(DATA, text_color);
    for (uint16_t i = 0; i < (text_columns * text_rows); i++)
    {
        xm_setbl(DATA, ' ');
    }
    xm_setw(WR_ADDR, screen_addr);
}

static const char * xmsg(int x, int y, int color, const char * msg)
{
    xm_setw(WR_ADDR, (y * text_columns) + x);
    xm_setbh(DATA, color);
    char c;
    while ((c = *msg) != '\0')
    {
        msg++;
        if (c == '\n')
        {
            break;
        }

        xm_setbl(DATA, c);
    }
    return msg;
}

static void reset_vid(void)
{
    remove_intr();

    wait_vsync_start();

    xreg_setw(PA_GFX_CTRL, 0x0000);
    xreg_setw(PA_TILE_CTRL, 0x000F);
    xreg_setw(PB_GFX_CTRL, 0x0080);
    xreg_setw(VID_LEFT, 0x0000);
    xreg_setw(VID_RIGHT, xreg_getw(VID_HSIZE));
    xreg_setw(PA_HV_SCROLL, 0x0000);
    xreg_setw(PA_HV_FSCALE, 0x0000);
    xreg_setw(COPP_CTRL, 0x0000);                             // disable copper
    xreg_setw(PA_LINE_LEN, xreg_getw(VID_HSIZE) >> 3);        // line len

    restore_colors();

    printf("\033c");        // reset XANSI

    while (checkchar())
    {
        readchar();
    }

#if 1        // handy for development to force Kermit upload
    dprintf("Disabling SD on next boot...\n");
    disable_sd_boot();
#endif
}

static inline void checkbail()
{
    if (checkchar())
    {
        reset_vid();
        _WARM_BOOT();
    }
}

_NOINLINE void delay_check(int ms)
{
    while (ms--)
    {
        checkbail();
        uint16_t tms = 10;
        do
        {
            uint16_t tv = xm_getw(TIMER);
            while (tv == xm_getw(TIMER))
                ;
        } while (--tms);
    }
}

static uint16_t xr_screen_addr = XR_TILE_ADDR + 0x1000;
static uint8_t  xr_text_columns;
static uint8_t  xr_text_rows;
static uint8_t  xr_text_color = 0x07;        // white on gray
static uint8_t  xr_x;
static uint8_t  xr_y;

static void xr_cls()
{
    xv_prep();

    xm_setw(XR_ADDR, xr_screen_addr);
    for (int i = 0; i < xr_text_columns * xr_text_rows; i++)
    {
        xm_setw(XR_DATA, ' ');
    }
    xr_x = 0;
    xr_y = 0;
}

static void xr_textmode_pb()
{
    xv_prep();

    xr_text_columns = 28;
    xr_text_rows    = 20;

    wait_vsync_start();
    xreg_setw(PB_GFX_CTRL, 0x0080);
    for (int i = 1; i < 256; i++)
    {
        uint16_t c = xmem_getw_wait(XR_COLOR_A_ADDR + i) & 0x0fff;
        xm_setw(XR_DATA, 0x0000 | c);
    }
    xr_cls();
    xmem_setw(XR_COLOR_B_ADDR + 0xf0, 0x0000);        // set write address
    for (int i = 1; i < 16; i++)
    {
        xmem_setw(XR_COLOR_B_ADDR + 0xf0 + i, 0xf202 | i << 4);
    }
    xmem_setw(XR_COLOR_B_ADDR, 0x0000);        // set write address

    wait_vsync();
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
    xm_setw(XR_ADDR, xr_screen_addr + (xr_y * xr_text_columns) + xr_x);
    if (c == '\n')
    {
        while (xr_x < xr_text_columns)
        {
            xm_setw(XR_DATA, ' ');
            xr_x++;
        }
        xr_x = 0;
        xr_y += 1;
    }
    else
    {
        xm_setw(XR_DATA, (xr_text_color << 8) | c);
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
    vsnprintf(dprint_buff, sizeof(dprint_buff), fmt, args);
    xr_print(dprint_buff);
    va_end(args);
}

static void xr_printfxy(int x, int y, const char * fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    xr_pos(x, y);
    vsnprintf(dprint_buff, sizeof(dprint_buff), fmt, args);
    xr_print(dprint_buff);
    va_end(args);
}

static void install_copper()
{
    dprintf("Loading copper list...");

    wait_vsync_start();
    xm_setw(XR_ADDR, XR_COPPER_ADDR);

#if 0        // copper torture test
    for (uint16_t i = 0; i < 1024; i++)
    {
        xm_setw(XR_DATA, 0xA000);
        xm_setw(XR_DATA, i << 2);
    }
#else
    for (uint16_t i = 0; i < copper_list_len; i++)
    {
        xm_setw(XR_DATA, copper_list[i] >> 16);
        xm_setw(XR_DATA, copper_list[i] & 0xffff);
    }
#endif

    dprintf("okay\n");
}

enum TEST_MODE
{
    BM_MONO_ATTR,
    BM_4_BIT,
    BM_4_BIT_RETRO,
    BM_8_BIT,
    BM_8_BIT_RETRO,
    BM_12_BIT
};

typedef struct _test_image
{
    uint16_t   mode;
    uint16_t   num_colors;
    uint16_t   size;
    char       name[64];
    uint8_t *  data;
    uint16_t * color;
} test_image;

#define MAX_IMAGES 16

static uint16_t   num_images;
static test_image test_images[MAX_IMAGES];

static long filesize(void * f)
{
    if (f == NULL)
    {
        dprintf("%s(%d): NULL fileptr\n");
        return -1;
    }

    if (fl_fseek(f, 0, SEEK_END) != 0)
    {
        dprintf("%s(%d):fl_fseek end error\n", __FILE__, __LINE__);
        return -1;
    }

    long fsize = fl_ftell(f);

    if (fl_fseek(f, 0, SEEK_SET) != 0)
    {
        dprintf("%s(%d):fl_fseek beg error\n", __FILE__, __LINE__);
        return -1;
    }

    return fsize;
}

static bool load_test_audio(const char * filename, uint8_t ** out, int * size)
{
    void * file  = fl_fopen(filename, "r");
    int    fsize = (int)filesize(file);

    if (fsize <= 0 || fsize > (128 * 1024))
    {
        dprintf("Bad size %ld for \"%s\"\n", fsize, filename);
        return false;
    }

    uint8_t * data = malloc(fsize);
    if (data == NULL)
    {
        dprintf("Allocating %ld for \"%s\" failed\n", fsize, filename);
        return false;
    }
    *out = data;

    int cnt   = 0;
    int rsize = 0;
    while ((cnt = fl_fread(data, 1, 512, file)) > 0)
    {
        if ((rsize & 0xFFF) == 0)
        {
            dprintf("\rReading \"%s\": %d KB ", filename, rsize >> 10);
        }

        data += cnt;
        rsize += cnt;
        checkbail();
    }
    dprintf("\rLoaded \"%s\": %dKB (%d bytes).  \n", filename, rsize >> 10, rsize);

    if (rsize != fsize)
    {
        dprintf("\nSize mismatch: ftell %ld vs read %ld\n", fsize, rsize);
    }
    *size = fsize;

    fl_fclose(file);

    return true;
}

static bool load_test_image(int mode, const char * filename, const char * colorname)
{
    if (num_images >= MAX_IMAGES)
        return false;

    test_image * ti = &test_images[num_images++];

    void * file  = fl_fopen(filename, "r");
    int    fsize = (int)filesize(file);

    if (fsize <= 0 || fsize > (128 * 1024))
    {
        dprintf("Bad size %ld for \"%s\"\n", fsize, filename);
        return false;
    }

    uint8_t * data = malloc(fsize);
    if (data == NULL)
    {
        dprintf("Allocating %ld for \"%s\" failed\n", fsize, filename);
        return false;
    }

    ti->data  = data;
    int cnt   = 0;
    int rsize = 0;
    while ((cnt = fl_fread(data, 1, 512, file)) > 0)
    {
        if ((rsize & 0xFFF) == 0)
        {
            dprintf("\rReading \"%s\": %d KB ", filename, rsize >> 10);
        }

        data += cnt;
        rsize += cnt;
        checkbail();
    }
    dprintf("\rLoaded \"%s\": %dKB (%d bytes).  \n", filename, rsize >> 10, rsize);

    if (rsize != fsize)
    {
        dprintf("\nSize mismatch: ftell %ld vs read %ld\n", fsize, rsize);
    }
    ti->size = fsize >> 1;

    fl_fclose(file);

    do
    {
        if (colorname == NULL)
        {
            break;
        }

        file = fl_fopen(colorname, "r");

        int csize = (int)filesize(file);
        if (csize <= 0 || csize > (512 * 2))
        {
            dprintf("Bad size %ld for \"%s\"\n", csize, colorname);
            break;
        }

        uint8_t * cdata = malloc(csize);
        if (cdata == NULL)
        {
            dprintf("Allocating %ld for \"%s\" failed\n", csize, colorname);
            break;
        }


        int       cnt   = 0;
        int       rsize = 0;
        uint8_t * rdata = cdata;
        while ((cnt = fl_fread(rdata, 1, 512, file)) > 0)
        {
            rdata += cnt;
            rsize += cnt;
        }
        if (rsize != csize)
        {
            dprintf("Color read failed.\n");
            free(cdata);
            break;
        }
        dprintf("Loaded colors %d colors from \"%s\".  \n", rsize >> 1, colorname);
        ti->color      = (uint16_t *)cdata;
        ti->num_colors = rsize >> 1;

    } while (false);

    ti->mode = mode;

    return true;
}

void show_test_pic(int pic_num, uint16_t addr)
{
    if (pic_num >= num_images)
    {
        return;
    }

    test_image * ti = &test_images[pic_num];

    uint16_t gfx_ctrl  = 0;
    uint16_t gfx_ctrlb = 0x0080;
    uint16_t wpl       = 640 / 8;
    uint16_t wplb      = 0;
    uint16_t frac      = 0;

    switch (ti->mode)
    {
        case BM_MONO_ATTR:
            gfx_ctrl = 0x0040;
            wpl      = (640 / 8);
            break;
        case BM_4_BIT:
            gfx_ctrl = 0x0055;
            wpl      = 320 / 4;
            break;
        case BM_4_BIT_RETRO:
            gfx_ctrl = 0x0055;
            wpl      = 320 / 4;
            frac     = 5;
            break;
        case BM_8_BIT:
            gfx_ctrl = 0x0065;
            wpl      = 320 / 2;
            break;
        case BM_8_BIT_RETRO:
            gfx_ctrl = 0x0065;
            wpl      = 320 / 2;
            frac     = 5;
            break;
        case BM_12_BIT:
            gfx_ctrl  = 0x0065;
            gfx_ctrlb = 0x0055;
            wpl       = 320 / 2;
            wplb      = 320 / 4;
            break;
        default:
            break;
    }

    wait_vsync_start();
    xreg_setw(PA_GFX_CTRL, 0x0080);        // blank screen
    xreg_setw(PB_GFX_CTRL, 0x0080);
    xreg_setw(VID_CTRL, 0x0000);        // set write address
    xmem_setw(XR_COLOR_A_ADDR, 0x0000);
    xreg_setw(VID_RIGHT, xreg_getw_wait(VID_HSIZE));        // set write address

    xm_setw(WR_INCR, 0x0001);
    xm_setw(WR_ADDR, addr);
    uint16_t * wp = (uint16_t *)ti->data;
    for (int w = 0; w < ti->size; w++)
    {
        xm_setw(DATA, *wp++);
    }

    if (ti->color)
    {
        xm_setw(XR_ADDR, XR_COLOR_A_ADDR);
        wp = (uint16_t *)ti->color;
        for (int w = 0; w < ti->num_colors; w++)
        {
            xm_setw(XR_DATA, *wp++);
        }
    }
    else
    {
        restore_colors();
    }


    xreg_setw(PA_TILE_CTRL, 0x000F);
    xreg_setw(PA_DISP_ADDR, addr);
    xreg_setw(PA_LINE_LEN, wpl + wplb);
    xreg_setw(PA_HV_FSCALE, frac);

    if (wplb)
    {
        xreg_setw(PB_TILE_CTRL, 0x000F);
        xreg_setw(PB_DISP_ADDR, addr + wpl);
        xreg_setw(PB_LINE_LEN, wpl + wplb);
        xreg_setw(PB_HV_FSCALE, frac);
    }

    wait_vsync();
    if (wplb == 0)
    {
        xreg_setw(PA_GFX_CTRL, gfx_ctrl);
        xr_textmode_pb();
    }
    else
    {
        xreg_setw(PA_GFX_CTRL, gfx_ctrl);
        xreg_setw(PB_GFX_CTRL, gfx_ctrlb);
    }
}

static void load_sd_bitmap(const char * filename, int vaddr)
{
    dprintf("Loading bitmap: \"%s\"", filename);
    void * file = fl_fopen(filename, "r");

    if (file != NULL)
    {
        int cnt = 0;

        while ((cnt = fl_fread(mem_buffer, 1, 512, file)) > 0)
        {
            if ((vaddr & 0xFFF) == 0)
            {
                dprintf(".");
            }

            uint16_t * maddr = (uint16_t *)mem_buffer;
            xm_setw(WR_INCR, 1);
            xm_setw(WR_ADDR, vaddr);
            for (int i = 0; i < (cnt >> 1); i++)
            {
                xm_setw(DATA, *maddr++);
            }
            vaddr += (cnt >> 1);
            checkbail();
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

        while ((cnt = fl_fread(mem_buffer, 1, 256 * 2 * 2, file)) > 0)
        {
            if ((vaddr & 0x7) == 0)
            {
                dprintf(".");
            }

            uint16_t * maddr = (uint16_t *)mem_buffer;
            wait_vsync();
            xm_setw(XR_ADDR, XR_COLOR_ADDR);
            for (int i = 0; i < (cnt >> 1); i++)
            {
                uint16_t v = *maddr++;
                xm_setw(XR_DATA, v);
            }
            vaddr += (cnt >> 1);
            checkbail();
        }

        fl_fclose(file);
        dprintf("done!\n");
    }
    else
    {
        dprintf(" - FAILED\n");
    }
}

#define DRAW_WIDTH  ((uint16_t)320)
#define DRAW_HEIGHT ((uint16_t)240)
#define DRAW_WORDS  ((uint16_t)DRAW_WIDTH / 2)

void draw8bpp_h_line(unsigned int base, uint8_t color, int x, int y, int len)
{
    if (len < 1)
    {
        return;
    }
    uint16_t addr = base + (uint16_t)(y * DRAW_WORDS) + (uint16_t)(x >> 1);
    uint16_t word = (color << 8) | color;
    xm_setw(WR_INCR, 1);           // set write inc
    xm_setw(WR_ADDR, addr);        // set write address
    if (x & 1)
    {
        xm_setbl(SYS_CTRL, 0x3);
        xm_setw(DATA, word);        // set left edge word
        len -= 1;
        xm_setbl(SYS_CTRL, 0xf);
    }
    while (len >= 2)
    {
        xm_setw(DATA, word);        // set full word
        len -= 2;
    }
    if (len)
    {
        xm_setbl(SYS_CTRL, 0xc);
        xm_setw(DATA, word);        // set right edge word
        xm_setbl(SYS_CTRL, 0xf);
    }
}

void draw8bpp_v_line(uint16_t base, uint8_t color, int x, int y, int len)
{
    if (len < 1)
    {
        return;
    }
    uint16_t addr = base + (uint16_t)(y * DRAW_WORDS) + (uint16_t)(x >> 1);
    uint16_t word = (color << 8) | color;
    xm_setw(WR_INCR, DRAW_WORDS);        // set write inc
    xm_setw(WR_ADDR, addr);              // set write address
    if (x & 1)
    {
        xm_setbl(SYS_CTRL, 0x3);
    }
    else
    {
        xm_setbl(SYS_CTRL, 0xc);
    }
    while (len--)
    {
        xm_setw(DATA, word);        // set full word
    }
    xm_setbl(SYS_CTRL, 0xf);
}

static inline void wait_blit_done()
{
    xwait_blit_busy();
}

static inline void wait_blit_ready()
{
    xwait_blit_full();
}
#define NUM_BOBS 10        // number of sprites (ideally no "red" border)
struct bob
{
    int8_t   x_delta, y_delta;
    int16_t  x_pos, y_pos;
    uint16_t w_offset;
};

struct bob      bobs[NUM_BOBS];
static uint16_t blit_shift[4]  = {0xF000, 0x7801, 0x3C02, 0x1E03};
static uint16_t blit_rshift[4] = {0x8700, 0xC301, 0xE102, 0xF003};

void test_blit()
{
    static const int W_4BPP = 320 / 4;
    static const int H_4BPP = 240;

    static const int W_LOGO = 32 / 4;
    static const int H_LOGO = 16;

    dprintf("test_blit\n");

    // crop left and right 2 pixels
    xr_textmode_pb();
    xreg_setw(VID_RIGHT, xreg_getw_wait(VID_HSIZE) - 4);
    xreg_setw(VID_CTRL, 0xFF00);

    do
    {
        xreg_setw(PA_GFX_CTRL, 0x0040);        // bitmap + 8-bpp + Hx1 + Vx1
        xreg_setw(PA_DISP_ADDR, 0x0000);
        xreg_setw(PA_LINE_LEN, 136);                        // ~65536/480 words per line
        xr_printfxy(0, 0, "Blit VRAM 128KB fill\n");        // set write address

        // fill VRAM
        wait_vsync();
        xmem_setw(XR_COLOR_B_ADDR + 250, 0x8000);        // set write address
        xmem_setw(XR_COLOR_A_ADDR + 255, 0xf000);        // set write address

        for (int i = 0x100; i >= 0; i -= 0x4)
        {
            xmem_setw(XR_COLOR_A_ADDR + 255, 0xf000);        // set write address
            wait_blit_ready();
            wait_vsync();
            wait_not_vsync();
            while (xreg_getw_wait(SCANLINE) != 20)
                ;
            xmem_setw(XR_COLOR_A_ADDR + 255, 0xf0f0);        // set write address

            xreg_setw(BLIT_CTRL, 0x0013);              // constA, constB, decrement
            xreg_setw(BLIT_MOD_A, 0x0000);             // no modulo A
            xreg_setw(BLIT_SRC_A, i << 8 | i);         // A = fill pattern
            xreg_setw(BLIT_MOD_B, 0x0000);             // no modulo B
            xreg_setw(BLIT_SRC_B, 0xFFFF);             // AND with B (and disable transparency)
            xreg_setw(BLIT_MOD_C, 0x0000);             // no modulo C
            xreg_setw(BLIT_VAL_C, 0x0000);             // XOR with C
            xreg_setw(BLIT_MOD_D, 0x0000);             // no modulo D
            xreg_setw(BLIT_DST_D, 0xFFFF);             // VRAM display end address
            xreg_setw(BLIT_SHIFT, 0xFF00);             // no edge masking or shifting
            xreg_setw(BLIT_LINES, 0x0000);             // 1D
            xreg_setw(BLIT_WORDS, 0x10000 - 1);        // 64KW VRAM
            wait_blit_done();
            xmem_setw(XR_COLOR_A_ADDR + 255, 0xff00);        // set write address
            wait_vsync();
        }

        uint16_t daddr = 0x1000;

        uint16_t paddr = 0x9b00;
        show_test_pic(0, paddr);
        xreg_setw(VID_RIGHT, xreg_getw_wait(VID_HSIZE) - 4);
        xreg_setw(VID_CTRL, 0xFF00);
        xmem_setw(XR_COLOR_A_ADDR + 255, 0x0000);        // set write address

        xr_printfxy(0, 0, "Blit 320x240 16 color\n");        // set write address

        // 2D screen screen copy 0x0000 -> 0x4B00 320x240 4-bpp
        wait_blit_ready();
        xreg_setw(BLIT_CTRL, 0x0002);             // constB
        xreg_setw(BLIT_MOD_A, 0x0000);            // no modulo A
        xreg_setw(BLIT_SRC_A, paddr);             // A = source
        xreg_setw(BLIT_MOD_B, 0x0000);            // no modulo B
        xreg_setw(BLIT_SRC_B, 0xFFFF);            // AND with B (and disable transparency)
        xreg_setw(BLIT_MOD_C, 0x0000);            // no modulo C
        xreg_setw(BLIT_VAL_C, 0x0000);            // XOR with C
        xreg_setw(BLIT_MOD_D, 0x0000);            // no modulo D
        xreg_setw(BLIT_DST_D, daddr);             // VRAM display end address
        xreg_setw(BLIT_SHIFT, 0xFF00);            // no edge masking or shifting
        xreg_setw(BLIT_LINES, H_4BPP - 1);        // lines (0 for 1-D blit)
        xreg_setw(BLIT_WORDS, W_4BPP - 1);        // words to write -1
        wait_blit_done();
        xreg_setw(PA_DISP_ADDR, daddr);

        xr_printfxy(0, 0, "Blit 320x240 16 color\nShift right\n");        // set write address
        wait_vsync_start();
        for (int i = 0; i < 128; i++)
        {
            wait_blit_ready();                              // make sure blit ready (previous blit started)
            xreg_setw(BLIT_CTRL, 0x0002);                   // constB
            xreg_setw(BLIT_MOD_A, -1);                      // A modulo
            xreg_setw(BLIT_SRC_A, paddr);                   // A source VRAM addr (pacman)
            xreg_setw(BLIT_MOD_B, 0x0000);                  // B modulo
            xreg_setw(BLIT_SRC_B, 0xFFFF);                  // B const (non-zero to disable transparency)
            xreg_setw(BLIT_MOD_C, 0x0000);                  // C trans term
            xreg_setw(BLIT_VAL_C, 0x0000);                  // C const (XOR'd with value stored)
            xreg_setw(BLIT_MOD_D, -1);                      // D modulo
            xreg_setw(BLIT_DST_D, daddr + (i >> 2));        // D destination VRAM addr
            xreg_setw(BLIT_SHIFT,
                      blit_shift[i & 0x3]);           // first, last word nibble masks, and 0-3 shift (low two bits)
            xreg_setw(BLIT_LINES, H_4BPP - 1);        // lines (0 for 1-D blit)
            xreg_setw(BLIT_WORDS, W_4BPP);            // words to write -1
            xmem_setw(XR_COLOR_A_ADDR + 255, 0xfff0);        // set write address

            wait_blit_done();
            xmem_setw(XR_COLOR_A_ADDR + 255, 0xf0f0);        // set write address
            wait_vsync_start();
            xmem_setw(XR_COLOR_A_ADDR + 255, 0xff00);        // set write address
        }
        checkbail();
        xmem_setw(XR_COLOR_A_ADDR + 255, 0xFF0F);        // set write address
        delay_check(DELAY_TIME);
        xr_printfxy(0, 0, "Blit 320x240 16 color\nShift left (decrement)\n");        // set write address
        wait_vsync_start();
        for (int i = 127; i >= 3; i--)
        {
            wait_blit_ready();                                       // make sure blit ready (previous blit started)
            xreg_setw(BLIT_CTRL, 0x0012);                            // constB
            xreg_setw(BLIT_MOD_A, 1);                                // A modulo
            xreg_setw(BLIT_SRC_A, paddr + (H_4BPP * W_4BPP));        // A source VRAM addr (pacman)
            xreg_setw(BLIT_MOD_B, 0x0000);                           // B modulo
            xreg_setw(BLIT_SRC_B, 0xFFFF);                           // B const (non-zero to disable transparency)
            xreg_setw(BLIT_MOD_C, 0x0000);                           // C trans term
            xreg_setw(BLIT_VAL_C, 0x0000);                           // C const (XOR'd with value stored)
            xreg_setw(BLIT_MOD_D, 1);                                // D modulo
            xreg_setw(BLIT_DST_D,
                      (daddr + (H_4BPP * W_4BPP)) + (i >> 2));        // D destination VRAM addr
            xreg_setw(BLIT_SHIFT,
                      blit_rshift[i & 0x3]);          // first, last word nibble masks, and 0-3 shift (low two bits)
            xreg_setw(BLIT_LINES, H_4BPP - 1);        // lines (0 for 1-D blit)
            xreg_setw(BLIT_WORDS, W_4BPP);            // words to write -1
            xmem_setw(XR_COLOR_A_ADDR + 255, 0xfff0);        // set write address
            wait_blit_done();
            xmem_setw(XR_COLOR_A_ADDR + 255, 0xf0f0);        // set write address
            wait_vsync_start();
            xmem_setw(XR_COLOR_A_ADDR + 255, 0xff00);        // set write address
        }
        checkbail();

        xmem_setw(XR_COLOR_A_ADDR + 255, 0xFF0F);        // set write address
        delay_check(DELAY_TIME);

        // upload moto sprite
        uint16_t maddr = 0xf000;
        xm_setw(WR_INCR, 1);
        xm_setw(WR_ADDR, maddr);
        for (size_t i = 0; i < sizeof(moto_m); i += 2)
        {
            xm_setw(DATA, moto_m[i] << 8 | moto_m[i + 1]);
        }

        for (int b = 0; b < NUM_BOBS; b++)
        {
            bobs[b].x_pos = b * 22;
            bobs[b].y_pos = b * 18;
            uint16_t r;
            r               = xm_getw(LFSR);
            bobs[b].x_delta = r & 0x8 ? -((r & 3) - 1) : ((r & 3) + 1);
            r               = xm_getw(LFSR);
            bobs[b].y_delta = r & 0x8 ? -((r & 3) - 1) : ((r & 3) + 1);
        }

        wait_blit_ready();
        xreg_setw(BLIT_CTRL, 0xEE02);             // constB, 4bpp transp=E
        xreg_setw(BLIT_MOD_A, 0x0000);            // A mod (XOR)
        xreg_setw(BLIT_SRC_A, paddr);             // A source const word
        xreg_setw(BLIT_MOD_B, 0x0000);            // B mod (XOR)
        xreg_setw(BLIT_SRC_B, 0xFFFF);            // B AND const (non-zero to disable transparency)
        xreg_setw(BLIT_MOD_C, 0x0000);            // C mod (XOR)
        xreg_setw(BLIT_VAL_C, 0x0000);            // C XOR const
        xreg_setw(BLIT_MOD_D, 0x0000);            // D mod (ADD)
        xreg_setw(BLIT_DST_D, daddr);             // D destination VRAM addr
        xreg_setw(BLIT_SHIFT, 0xFF00);            // first, last word nibble masks, and 0-3 shift (low two bits)
        xreg_setw(BLIT_LINES, H_4BPP - 1);        // lines (0 for 1-D blit)
        xreg_setw(BLIT_WORDS, W_4BPP - 1);        // words to write -1

        xr_printfxy(0, 0, "Blit 320x240 16 color\nBOB test (single buffered)\n");        // set write address
        int nb = NUM_BOBS;
        dprintf("Num bobs = %d\n", nb);
        for (int i = 0; i < 256; i++)
        {
            for (int b = 0; b < nb; b++)
            {
                struct bob * bp = &bobs[b];
                wait_blit_ready();                             // make sure blit ready (previous blit started)
                xreg_setw(BLIT_CTRL, 0xEE02);                  // constB, 4bpp transp=E
                xm_setw(XR_DATA, W_4BPP - W_LOGO - 1);         // A modulo
                xm_setw(XR_DATA, paddr + bp->w_offset);        // A initial term (not used)
                xm_setw(XR_DATA, 0x0000);                      // B modulo
                xm_setw(XR_DATA, 0xFFFF);                      // B source+transp VRAM addr (moto_m)
                xm_setw(XR_DATA, 0x0000);                      // C modulo
                xm_setw(XR_DATA, 0x0000);                      // C XOR const
                xm_setw(XR_DATA, W_4BPP - W_LOGO - 1);         // D modulo
                xm_setw(XR_DATA, daddr + bp->w_offset);        // D destination VRAM addr
                xm_setw(XR_DATA, 0xFF00);                // first, last word nibble masks, and 0-3 shift (low two bits)
                xm_setw(XR_DATA, H_LOGO - 1);            // lines (0 for 1-D blit)
                xm_setw(XR_DATA, W_LOGO - 1 + 1);        // words to write -1

                bp->x_pos += bp->x_delta;
                if (bp->x_pos < -16)
                    bp->x_pos += 320 + 16;
                else if (bp->x_pos > 320)
                    bp->x_pos -= 320;

                bp->y_pos += bp->y_delta;
                if (bp->y_pos < -16)
                    bp->y_pos += 240 + 16;
                else if (bp->y_pos > 240)
                    bp->y_pos -= 240;
            }
            for (int b = 0; b < nb; b++)
            {
                struct bob * bp  = &bobs[b];
                uint16_t     off = (uint16_t)(bp->x_pos >> 2) + (uint16_t)((uint16_t)W_4BPP * bp->y_pos);
                bp->w_offset     = off;
                uint8_t shift    = bp->x_pos & 3;

                wait_blit_ready();                            // make sure blit ready (previous blit started)
                xreg_setw(BLIT_CTRL, 0x0001);                 // constA
                xm_setw(XR_DATA, 0x0000);                     // A modulo
                xm_setw(XR_DATA, 0xFFFF);                     // A initial term (not used)
                xm_setw(XR_DATA, -1);                         // B modulo
                xm_setw(XR_DATA, maddr);                      // B source+transp VRAM addr (moto_m)
                xm_setw(XR_DATA, 0x0000);                     // C modulo
                xm_setw(XR_DATA, 0x0000);                     // C XOR const
                xm_setw(XR_DATA, W_4BPP - W_LOGO - 1);        // D modulo
                xm_setw(XR_DATA, daddr + off);                // D destination VRAM addr
                xm_setw(XR_DATA,
                        blit_shift[shift]);              // first, last word nibble masks, and 0-3 shift (low two bits)
                xm_setw(XR_DATA, H_LOGO - 1);            // lines (0 for 1-D blit)
                xm_setw(XR_DATA, W_LOGO - 1 + 1);        // words to write -1
            }
            xmem_setw(XR_COLOR_A_ADDR + 255, 0xfff0);        // set write address
            checkbail();
            wait_blit_done();
            xmem_setw(XR_COLOR_A_ADDR + 255, 0xf0f0);        // set write address
            wait_vsync();
            xmem_setw(XR_COLOR_A_ADDR + 255, 0xff00);        // set write address
        }

        xmem_setw(XR_COLOR_A_ADDR + 255, 0xf000);        // set write address

        delay_check(DELAY_TIME);

    } while (false);
    xreg_setw(PA_GFX_CTRL, 0x0055);        // bitmap + 4-bpp + Hx2 + Vx2
    xreg_setw(PA_LINE_LEN, 320 / 4);
    xreg_setw(PA_DISP_ADDR, 0x0000);

    xreg_setw(VID_RIGHT, xreg_getw_wait(VID_HSIZE));
}

void test_true_color()
{

    uint16_t saddr = 0x0000;

    show_test_pic(TRUECOLOR_TEST_PIC, saddr);

    delay_check(DELAY_TIME * 2);

    //    load_sd_bitmap("/fractal_320x240_RG8B4.raw", saddr);

    //    delay_check(DELAY_TIME * 2);
}

void test_dual_8bpp()
{
    const uint16_t width  = DRAW_WIDTH;
    const uint16_t height = 200;
    // /    uint16_t       old_copp = xreg_getw_wait(COPP_CTRL);

    do
    {

        dprintf("test_dual_8pp\n");
        xr_textmode_pb();
        xr_printf("Dual 8-BPP blending\n");
        xreg_setw(PA_GFX_CTRL, 0x0080);
        restore_colors();            // colormem A normal colors
        restore_colors2(0x8);        // colormem B normal colors (alpha 50%)

        uint16_t addrA = 0;             // start of VRAM
        uint16_t addrB = 0x8000;        // 2nd half of VRAM
        xm_setbl(SYS_CTRL, 0xf);

        // clear all VRAM

        uint16_t vaddr = 0;
        xm_setw(WR_INCR, 1);
        xm_setw(WR_ADDR, vaddr);
        do
        {
            xm_setw(DATA, 0);
        } while (++vaddr != 0);

        wait_vsync();
        xreg_setw(VID_CTRL, 0x0000);           // border color = black
        xreg_setw(PA_GFX_CTRL, 0x00FF);        // blank screen
        xreg_setw(PB_GFX_CTRL, 0x00FF);
        // install 320x200 "crop" copper list
        xm_setw(XR_ADDR, XR_COPPER_ADDR);
        for (uint16_t i = 0; i < NUM_ELEMENTS(copper_320x200); i++)
        {
            xm_setw(XR_DATA, copper_320x200[i] >> 16);
            xm_setw(XR_DATA, copper_320x200[i] & 0xffff);
        }
        xreg_setw(COPP_CTRL, 0x8000);
        // set pf A 320x240 8bpp (cropped to 320x200)
        xreg_setw(PA_GFX_CTRL, 0x0065);
        xreg_setw(PA_TILE_CTRL, 0x000F);
        xreg_setw(PA_DISP_ADDR, addrA);
        xreg_setw(PA_LINE_LEN, DRAW_WORDS);
        xreg_setw(PA_HV_SCROLL, 0x0000);

        // set pf B 320x240 8bpp (cropped to 320x200)
        xreg_setw(PB_GFX_CTRL, 0x0065);
        xreg_setw(PB_TILE_CTRL, 0x000F);
        xreg_setw(PB_DISP_ADDR, addrB);
        xreg_setw(PB_LINE_LEN, DRAW_WORDS);
        xreg_setw(PB_HV_SCROLL, 0x0000);

        // enable copper
        wait_vsync();
        xmem_setw(XR_COPPER_ADDR + (1 * 2) + 1, 0x0065);
        xmem_setw(XR_COPPER_ADDR + (2 * 2) + 1, 0x00E5);

        uint16_t w = width;
        uint16_t x, y;
        x = 0;
        for (y = 0; y < height; y++)
        {
            int16_t len = w - x;
            if (x + len >= width)
            {
                len = width - x;
            }

            draw8bpp_h_line(addrA, ((y >> 2) + 1) & 0xff, x, y, len);

            w--;
            x++;
        }

        dprintf("Playfield A: 320x200 8bpp - horizontal-striped triangle + blanked B\n");
        delay_check(DELAY_TIME);


        wait_vsync();
        xmem_setw(XR_COPPER_ADDR + (1 * 2) + 1, 0x0065);
        xmem_setw(XR_COPPER_ADDR + (2 * 2) + 1, 0x0065);
        dprintf("Playfield A: 320x200 8bpp - horizontal-striped triangle + B enabled, but zeroed\n");
        delay_check(DELAY_TIME);


        w = height;
        y = 0;
        for (x = 0; x < width; x++)
        {
            int16_t len = w;
            if (len >= height)
            {
                len = height;
            }

            draw8bpp_v_line(addrB, ((x >> 2) + 1) & 0xff, x, y, len);
            w--;
        }

        wait_vsync();
        xmem_setw(XR_COPPER_ADDR + (1 * 2) + 1, 0x00E5);
        xmem_setw(XR_COPPER_ADDR + (2 * 2) + 1, 0x0065);
        dprintf("Playfield B: 320x200 8bpp - vertical-striped triangle, A blanked\n");
        delay_check(DELAY_TIME);


        wait_vsync();
        xmem_setw(XR_COPPER_ADDR + (1 * 2) + 1, 0x0065);
        xmem_setw(XR_COPPER_ADDR + (2 * 2) + 1, 0x0065);
        dprintf("Playfield A&B: mixed (alpha 0x8)\n");
        delay_check(DELAY_TIME);


        wait_vsync();
        restore_colors2(0x0);        // colormem B normal colors (alpha 0%)

        dprintf("Playfield A&B: colormap B alpha 0x0\n");
        delay_check(DELAY_TIME);


        wait_vsync();
        restore_colors2(0x4);        // colormem B normal colors (alpha 25%)

        dprintf("Playfield A&B: colormap B alpha 0x4\n");
        delay_check(DELAY_TIME);


        wait_vsync();
        restore_colors2(0x8);        // colormem B normal colors (alpha 50%)

        dprintf("Playfield A&B: colormap B alpha 0x8\n");
        delay_check(DELAY_TIME);


        wait_vsync();
        restore_colors2(0xF);        // colormem B normal colors (alpha 100%)

        dprintf("Playfield A&B: colormap B alpha 0xC\n");
        delay_check(DELAY_TIME);

    } while (false);

    dprintf("restore screen\n");
    restore_colors3();        // colormem B normal colors (alpha 0%)
    wait_vsync();
    xreg_setw(COPP_CTRL, 0x0000);
#if COPPER_TEST
    install_copper();
#endif

    xr_textmode_pb();
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

#if 1
static int8_t sinData[256] = {
    0,           // 0
    3,           // 1
    6,           // 2
    9,           // 3
    12,          // 4
    15,          // 5
    18,          // 6
    21,          // 7
    24,          // 8
    27,          // 9
    30,          // 10
    33,          // 11
    36,          // 12
    39,          // 13
    42,          // 14
    45,          // 15
    48,          // 16
    51,          // 17
    54,          // 18
    57,          // 19
    59,          // 20
    62,          // 21
    65,          // 22
    67,          // 23
    70,          // 24
    73,          // 25
    75,          // 26
    78,          // 27
    80,          // 28
    82,          // 29
    85,          // 30
    87,          // 31
    89,          // 32
    91,          // 33
    94,          // 34
    96,          // 35
    98,          // 36
    100,         // 37
    102,         // 38
    103,         // 39
    105,         // 40
    107,         // 41
    108,         // 42
    110,         // 43
    112,         // 44
    113,         // 45
    114,         // 46
    116,         // 47
    117,         // 48
    118,         // 49
    119,         // 50
    120,         // 51
    121,         // 52
    122,         // 53
    123,         // 54
    123,         // 55
    124,         // 56
    125,         // 57
    125,         // 58
    126,         // 59
    126,         // 60
    126,         // 61
    126,         // 62
    126,         // 63
    127,         // 64
    126,         // 65
    126,         // 66
    126,         // 67
    126,         // 68
    126,         // 69
    125,         // 70
    125,         // 71
    124,         // 72
    123,         // 73
    123,         // 74
    122,         // 75
    121,         // 76
    120,         // 77
    119,         // 78
    118,         // 79
    117,         // 80
    116,         // 81
    114,         // 82
    113,         // 83
    112,         // 84
    110,         // 85
    108,         // 86
    107,         // 87
    105,         // 88
    103,         // 89
    102,         // 90
    100,         // 91
    98,          // 92
    96,          // 93
    94,          // 94
    91,          // 95
    89,          // 96
    87,          // 97
    85,          // 98
    82,          // 99
    80,          // 100
    78,          // 101
    75,          // 102
    73,          // 103
    70,          // 104
    67,          // 105
    65,          // 106
    62,          // 107
    59,          // 108
    57,          // 109
    54,          // 110
    51,          // 111
    48,          // 112
    45,          // 113
    42,          // 114
    39,          // 115
    36,          // 116
    33,          // 117
    30,          // 118
    27,          // 119
    24,          // 120
    21,          // 121
    18,          // 122
    15,          // 123
    12,          // 124
    9,           // 125
    6,           // 126
    3,           // 127
    0,           // 128
    -3,          // 129
    -6,          // 130
    -9,          // 131
    -12,         // 132
    -15,         // 133
    -18,         // 134
    -21,         // 135
    -24,         // 136
    -27,         // 137
    -30,         // 138
    -33,         // 139
    -36,         // 140
    -39,         // 141
    -42,         // 142
    -45,         // 143
    -48,         // 144
    -51,         // 145
    -54,         // 146
    -57,         // 147
    -59,         // 148
    -62,         // 149
    -65,         // 150
    -67,         // 151
    -70,         // 152
    -73,         // 153
    -75,         // 154
    -78,         // 155
    -80,         // 156
    -82,         // 157
    -85,         // 158
    -87,         // 159
    -89,         // 160
    -91,         // 161
    -94,         // 162
    -96,         // 163
    -98,         // 164
    -100,        // 165
    -102,        // 166
    -103,        // 167
    -105,        // 168
    -107,        // 169
    -108,        // 170
    -110,        // 171
    -112,        // 172
    -113,        // 173
    -114,        // 174
    -116,        // 175
    -117,        // 176
    -118,        // 177
    -119,        // 178
    -120,        // 179
    -121,        // 180
    -122,        // 181
    -123,        // 182
    -123,        // 183
    -124,        // 184
    -125,        // 185
    -125,        // 186
    -126,        // 187
    -126,        // 188
    -126,        // 189
    -126,        // 190
    -126,        // 191
    -127,        // 192
    -126,        // 193
    -126,        // 194
    -126,        // 195
    -126,        // 196
    -126,        // 197
    -125,        // 198
    -125,        // 199
    -124,        // 200
    -123,        // 201
    -123,        // 202
    -122,        // 203
    -121,        // 204
    -120,        // 205
    -119,        // 206
    -118,        // 207
    -117,        // 208
    -116,        // 209
    -114,        // 210
    -113,        // 211
    -112,        // 212
    -110,        // 213
    -108,        // 214
    -107,        // 215
    -105,        // 216
    -103,        // 217
    -102,        // 218
    -100,        // 219
    -98,         // 220
    -96,         // 221
    -94,         // 222
    -91,         // 223
    -89,         // 224
    -87,         // 225
    -85,         // 226
    -82,         // 227
    -80,         // 228
    -78,         // 229
    -75,         // 230
    -73,         // 231
    -70,         // 232
    -67,         // 233
    -65,         // 234
    -62,         // 235
    -59,         // 236
    -57,         // 237
    -54,         // 238
    -51,         // 239
    -48,         // 240
    -45,         // 241
    -42,         // 242
    -39,         // 243
    -36,         // 244
    -33,         // 245
    -30,         // 246
    -27,         // 247
    -24,         // 248
    -21,         // 249
    -18,         // 250
    -15,         // 251
    -12,         // 252
    -9,          // 253
    -6,          // 254
    -4,          // 255
};
#endif

uint8_t * testsamp;
int       testsampsize;

static void test_audio(uint8_t * samp, int sampsize, int speed)
{
    __asm__ __volatile__("or.w    #0x0700,%sr");

    uint8_t * sp = samp;
    int       sc = sampsize;
    xm_setw(WR_INCR, 0x0000);
    xm_setw(WR_ADDR, 0x0000);

    while (1)
    {
        uint8_t val = *sp++;
        xm_setbh(DATA, val);
        xm_setbl(DATA, val);

        for (int d = speed; d != 0; d--)
        {
            __asm__ __volatile__("nop");
        }
        sc--;
        if (sc <= 0)
        {
            sp = samp;
            sc = sampsize;
        }
    }
}

static void test_audio_sin(uint8_t * samp, int speed)
{
    __asm__ __volatile__("or.w    #0x0700,%sr");

    uint8_t spl = 0;
    uint8_t spr = 128;
    xm_setw(WR_INCR, 0x0000);
    xm_setw(WR_ADDR, 0x0000);

    while (1)
    {
        uint8_t vall = samp[spl++];
        xm_setbh(DATA, vall);
        uint8_t valr = samp[spr++];
        xm_setbl(DATA, valr);

        for (int d = speed; d != 0; d--)
        {
            __asm__ __volatile__("nop");
        }
    }
}

static void test_audio_ramp(int speed)
{
    __asm__ __volatile__("or.w    #0x0700,%sr");

    uint8_t sp = 0;
    xm_setw(WR_INCR, 0x0000);
    xm_setw(WR_ADDR, 0x0000);

    while (1)
    {
        xm_setbh(DATA, sp);
        xm_setbl(DATA, sp);
        sp++;
        for (int d = speed; d != 0; d--)
        {
            __asm__ __volatile__("nop");
        }
    }
}


const char blurb[] =
    "\n"
    "\n"
    "Xosera is an FPGA based video adapter designed with the rosco_m68k retro\n"
    "computer in mind. Inspired in concept by it's \"namesake\" the Commander X16's\n"
    "VERA, Xosera is an original open-source video adapter design, built with open-\n"
    "source tools and is tailored with features generally appropriate for a Motorola\n"
    "68K era retro computer like the rosco_m68k (or even an 8-bit CPU).\n"
    "\n"
    "  \xf9  Uses low-cost FPGA instead of expensive semiconductor fabrication :)\n"
    "  \xf9  128KB of embedded video VRAM (16-bit words at 33 or 25 MHz)\n"
    "  \xf9  VGA output at 848x480 or 640x480 (16:9 or 4:3 @ 60Hz)\n"
    "  \xf9  Register based interface using 16 main 16-bit registers\n"
    "  \xf9  Read/write VRAM with programmable read/write address increment\n"
    "  \xf9  Fast 8-bit bus interface (using MOVEP) for rosco_m68k (by Ross Bamford)\n"
    "  \xf9  Dual video planes (playfields) with color blending and priority\n"
    "  \xf9  Dual 256 color palettes with 12-bit RGB (4096 colors) and 4-bit \"alpha\"\n"
    "  \xf9  Read/write tile memory for an additional 10KB of tiles or tilemap\n"
    "  \xf9  Text mode with up to 8x16 glyphs and 16 forground & background colors\n"
    "  \xf9  Graphic tile modes with 1024 8x8 glyphs, 16/256 colors and H/V tile mirror\n"
    "  \xf9  Bitmap modes with 1 (plus attribute colors), 4 or 8 bits per pixel\n"
    "  \xf9  Fast 2-D \"blitter\" unit with transparency, masking, shifting and logic ops\n"
    "  \xf9  Screen synchronized \"copper\" to change colors and registers mid-screen\n"
    "  \xf9  Pixel H/V repeat of 1x, 2x, 3x or 4x (e.g. for 424x240 or 320x240)\n"
    "  \xf9  Fractional H/V repeat scaling (e.g. for 320x200 or 512x384 retro modes)\n"
    "  \xf9  TODO: Wavetable stereo audio (similar to Amiga)\n"
    "  \xf9  TODO: Hardware sprites for mouse cursor etc. (similar to Amiga)\n"
    "  \xf9  TODO: High-speed USB UART (using FPGA FTDI interface)?\n"
    "  \xf9  TODO: Perhaps PS/2 keyboard or fast SPI SD card I/O?\n"
    "  \xf9  TODO: Whatever else fits into the FPGA while it still makes timing! :)\n"
    "\n"
    "\n";

static void test_xr_read()
{
    dprintf("test_xr\n");

    xcls();

    xreg_setw(PB_GFX_CTRL, 0x0000);
    xreg_setw(PB_TILE_CTRL, 0x000F);
    xreg_setw(PB_DISP_ADDR, 0xF000);
    uint16_t vaddr = 0;
    xm_setw(WR_INCR, 1);
    for (vaddr = 0xF000; vaddr != 0x0000; vaddr++)
    {
        xm_setw(WR_ADDR, vaddr);
        xm_setw(DATA, vaddr - 0xF000);
    }
    xm_setw(WR_ADDR, 0xF000);
    xm_setw(DATA, 0x1f00 | 'P');
    xm_setw(DATA, 0x1f00 | 'L');
    xm_setw(DATA, 0x1f00 | 'A');
    xm_setw(DATA, 0x1f00 | 'Y');
    xm_setw(DATA, 0x1f00 | 'F');
    xm_setw(DATA, 0x1f00 | 'I');
    xm_setw(DATA, 0x1f00 | 'E');
    xm_setw(DATA, 0x1f00 | 'L');
    xm_setw(DATA, 0x1f00 | 'D');
    xm_setw(DATA, 0x1f00 | '-');
    xm_setw(DATA, 0x1f00 | 'B');


    xm_setw(WR_INCR, 1);
    for (vaddr = 0; vaddr < 0x2000; vaddr++)
    {
        xm_setw(WR_ADDR, vaddr);
        xm_setw(DATA, vaddr + 0x0100);
    }
    xm_setw(WR_ADDR, 0x000);
    xm_setw(DATA, 0x1f00 | 'V');
    xm_setw(DATA, 0x1f00 | 'R');
    xm_setw(DATA, 0x1f00 | 'A');
    xm_setw(DATA, 0x1f00 | 'M');


    delay_check(DELAY_TIME * 2);

    for (int r = 0; r < 8; r++)
    {
        for (int w = XR_TILE_ADDR; w < XR_TILE_ADDR + 0x1400; w++)
        {
            uint16_t v = xmem_getw_wait(w);
            xm_setw(XR_DATA, ~v);        // toggle to prove read and set in VRAM
        }

        wait_vsync_start();
    }

    xreg_setw(PA_DISP_ADDR, 0x0000);
    xreg_setw(PA_GFX_CTRL, 0x0000);
    xreg_setw(PA_TILE_CTRL, 0x000F);
    delay_check(DELAY_TIME * 2);
}

void set_alpha_slow(int alpha)
{
    uint16_t a = (alpha & 0xf) << 12;
    for (int i = XR_COLOR_ADDR; i < XR_COLOR_ADDR + 256; i++)
    {
        uint16_t v = (xmem_getw_wait(i) & 0xfff) | a;
        xm_setw(XR_DATA, v);
    }
}

static void set_alpha(int alpha)
{
    uint16_t a = (alpha & 0xf) << 12;
    for (int i = XR_COLOR_ADDR; i < XR_COLOR_ADDR + 256; i++)
    {
        uint16_t v = (xmem_getw_wait(i) & 0xfff) | a;
        xm_setw(XR_DATA, v);
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

    printf("\033c\033[?12l");        // ANSI reset, cursor blink off

    dprintf("Xosera_test_m68k\n");
    cpu_delay(1000);
    dprintf("\nxosera_init(0)...");
    bool success = xosera_init(0);
    dprintf("%s (%dx%d)\n", success ? "succeeded" : "FAILED", xreg_getw_wait(VID_HSIZE), xreg_getw_wait(VID_VSIZE));

    uint8_t * init_ptr = xosera_initdata;
    for (int i = XR_COPPER_ADDR + XR_COPPER_SIZE - 16; i < XR_COPPER_ADDR + XR_COPPER_SIZE; i++)
    {
        uint16_t v = xmem_getw_wait(i);
        //        dprintf("0x%04x = 0x%04x\n", i, v);
        *init_ptr++ = (uint8_t)(v >> 8);
        *init_ptr++ = (uint8_t)(v & 0xff);
    }
    dprintf("ID: %s Githash:0x%02x%02x%02x%02x\n",
            xosera_initdata,
            xosera_initdata[28],
            xosera_initdata[29],
            xosera_initdata[30],
            xosera_initdata[31]);

    wait_vsync();
    xreg_setw(PA_GFX_CTRL, 0x0080);            // PA blanked
    xreg_setw(VID_CTRL, 0x0000);               // border color #0
    xmem_setw(XR_COLOR_A_ADDR, 0x0000);        // color # = black
    xr_textmode_pb();
    xr_printfxy(5, 0, "xosera_test_m68k\n");

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

    (void)sinData;

#if 1        // audio sin test
    {
        xreg_setw(PA_GFX_CTRL, 0x0000);
        xreg_setw(PA_TILE_CTRL, 0x000F);
        xreg_setw(PA_LINE_LEN, xreg_getw(VID_HSIZE) >> 3);
        xreg_setw(PA_DISP_ADDR, 0x0000);
        xreg_setw(PA_HV_SCROLL, 0x0000);
        xreg_setw(PA_HV_FSCALE, 0x0000);
        xcls();
        // fix signed -> unsigned
        uint8_t * sd = (uint8_t *)sinData;
        for (int i = 0; i < 256; i++)
        {
            sd[i] = sd[i] + 128;
        }

        test_audio_sin(sd, 4);
        test_audio_ramp(10);
    }
#elif 0        // audio waveform test
    if (load_test_audio("/Slide_8u.raw", &testsamp, &testsampsize))
    {
        test_audio(testsamp, testsampsize, 26);
    }
#endif


    if (use_sd)
    {
        xr_printf("\nLoading test images:\n");
        xr_printf("  pacbox-320x240\n");
        load_test_image(BM_4_BIT, "/pacbox-320x240.raw", "/pacbox-320x240_pal.raw");
        xr_printf("  ST_KingTut_Dpaint_16\n");
        load_test_image(BM_4_BIT_RETRO, "/ST_KingTut_Dpaint_16.raw", "/ST_KingTut_Dpaint_16_pal.raw");
        xr_printf("  space_shuttle_color_small\n");
        load_test_image(BM_MONO_ATTR, "/space_shuttle_color_small.raw", NULL);
        xr_printf("  parrot_320x240_RG8B4\n");
        load_test_image(BM_12_BIT, "/parrot_320x240_RG8B4.raw", "/true_color_pal.raw");
        xr_printf("  xosera_r1\n");
        load_test_image(BM_8_BIT, "/xosera_r1.raw", "/xosera_r1_pal.raw");
    }

    // D'oh! Uses timer    rosco_m68k_CPUMHz();

#if 1
    dprintf("Installing interrupt handler...");
    install_intr();
    dprintf("okay.\n");
#else
    dprintf("NOT Installing interrupt handler\n");
#endif

#if COPPER_TEST
    install_copper();
#endif

    while (true)
    {
        uint32_t t = XFrameCount;
        uint32_t h = t / (60 * 60 * 60);
        uint32_t m = t / (60 * 60) % 60;
        uint32_t s = (t / 60) % 60;
        dprintf("*** xosera_test_m68k iteration: %u, running %u:%02u:%02u\n", test_count++, h, m, s);

        uint16_t features = xreg_getw_wait(VERSION);
        //        uint32_t githash   = ((uint32_t)xreg_getw_wait(GITHASH_H) << 16) |
        //        (uint32_t)xreg_getw_wait(GITHASH_L);
        uint16_t monwidth  = xreg_getw_wait(VID_HSIZE);
        uint16_t monheight = xreg_getw_wait(VID_VSIZE);
        //        uint16_t monfreq   = xreg_getw_wait(VID_VFREQ);

        uint16_t gfxctrl  = xreg_getw_wait(PA_GFX_CTRL);
        uint16_t tilectrl = xreg_getw_wait(PA_TILE_CTRL);
        uint16_t dispaddr = xreg_getw_wait(PA_DISP_ADDR);
        uint16_t linelen  = xreg_getw_wait(PA_LINE_LEN);
        uint16_t hvscroll = xreg_getw_wait(PA_HV_SCROLL);
        uint16_t sysctrl  = xm_getw(SYS_CTRL);


        dprintf("%s #%02x%02x%02x%02x ",
                xosera_initdata,
                xosera_initdata[28],
                xosera_initdata[29],
                xosera_initdata[30],
                xosera_initdata[31]);
        dprintf("Features:0x%04x\n", features);
        dprintf("Monitor Native Res: %dx%d\n", monwidth, monheight);
        dprintf("\nPlayfield A:\n");
        dprintf("PA_GFX_CTRL : 0x%04x PA_TILE_CTRL: 0x%04x\n", gfxctrl, tilectrl);
        dprintf("PA_DISP_ADDR: 0x%04x PA_LINE_LEN : 0x%04x\n", dispaddr, linelen);
        dprintf("PA_HV_SCROLL: 0x%04x\n", hvscroll);
        dprintf("\n");

        dprintf("SYS_CTRL: 0x%04x\n", sysctrl);
        xm_setw(SYS_CTRL, sysctrl);
        dprintf("SYS_CTRL: 0x%04x\n", sysctrl);


#if COPPER_TEST
        if (test_count & 1)
        {
            dprintf("Copper test disabled for this iteration.\n");
            xreg_setw(COPP_CTRL, 0x0000);
        }
        else
        {
            dprintf("Copper test enabled for this interation.\n");
            xreg_setw(COPP_CTRL, 0x8000);
        }
#endif

        wait_vsync_start();
        restore_colors();
        dupe_colors(0xf);
        xmem_setw(XR_COLOR_B_ADDR, 0x0000);        // make sure we can see plane A under B

#if LR_MARGIN_TEST
        // crop left and right 2 pixels
        xreg_setw(VID_LEFT, 4);
        xreg_setw(VID_RIGHT, monwidth - 4);
#endif

        xr_textmode_pb();
        xr_msg_color(0x0f);
        xr_printfxy(5, 0, "xosera_test_m68k\n");

        xreg_setw(PA_GFX_CTRL, 0x0000);
        xreg_setw(PA_TILE_CTRL, 0x000F);
        xreg_setw(PA_LINE_LEN, xreg_getw_wait(VID_HSIZE) >> 3);
        xreg_setw(PA_DISP_ADDR, 0x0000);
        xreg_setw(PA_HV_SCROLL, 0x0000);
        xreg_setw(PA_HV_FSCALE, 0x0000);

        xcls();

        const char * bp    = blurb;
        int          color = 6;

        for (int y = 0; y < 30; y++)
        {
            bp = xmsg(0, y, color, bp);

            if (*bp != '\n')
            {
                color = (color + 1) & 0xf;
                if (color == 0)
                {
                    color = 1;
                }
            }
        }

        delay_check(DELAY_TIME * 10);

        if (use_sd)
        {
            test_blit();
        }

        if (use_sd)
        {
            xm_setbh(SYS_CTRL, 0x07);        // disable Xosera vsync interrupt

            show_test_pic(TRUECOLOR_TEST_PIC, 0x0000);
            delay_check(DELAY_TIME);
            show_test_pic(SELF_PIC, 0x0000);
            delay_check(DELAY_TIME);
            show_test_pic(TUT_PIC, 0x0000);
            delay_check(DELAY_TIME);
            show_test_pic(SHUTTLE_PIC, 0x0000);
            delay_check(DELAY_TIME);

            xm_setbl(TIMER, 0x08);           // clear any pending interrupt
            xm_setbh(SYS_CTRL, 0x08);        // enable Xosera vsync interrupt
        }

        // ugly: test_dual_8bpp();
        // delay_check(DELAY_TIME);

        // ugly: test_xr_read();
        // delay_check(DELAY_TIME);

        // ugly: test_hello();
        // delay_check(DELAY_TIME);

#if 0        // bored with this test. :)
        test_vram_speed();
        delay_check(DELAY_TIME);

#endif
    }

    // exit test
    reset_vid();
}
