/*
 * vim: set et ts=4 sw=4
 *------------------------------------------------------------
 *                                  ___ ___ _
 *  ___ ___ ___ ___ ___       _____|  _| . | |_
 * |  _| . |_ -|  _| . |     |     | . | . | '_|
 * |_| |___|___|___|___|_____|_|_|_|___|___|_,_|
 *                     |_____|
 * ------------------------------------------------------------
 * Copyright (c) 2021 Xark & Contributors
 * MIT License
 *
 * Test and tech-demo for Xosera FPGA "graphics card"
 * ------------------------------------------------------------
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <basicio.h>
#include <machine.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288
#endif


#include <xosera_m68k_api.h>
#include <pr_api.h>

#include "cube.h"

extern void install_intr(void);
extern void remove_intr(void);

// dummy global variable
uint32_t global;        // this is used to prevent the compiler from optimizing out tests

uint16_t g_margin_height = 1;

uint16_t screen_addr = 0x0000;
uint8_t  text_columns = 80;
uint8_t  text_rows = 2;
int8_t   text_h;
int8_t   text_v;
uint8_t  text_color = 0x02;        // dark green on black

const uint8_t  copper_list_len = 20;
const uint16_t copper_list[] = {
    0x0014, 0x0002, // wait  0, 20                   ; Wait for line 20, H position ignored
    0x9010, 0x0075, // mover 0x0075, PA_GFX_CTRL     ; Set to 8-bpp + Hx2 + Vx2
    0x01a2, 0x0002, // wait  0, 418                  ; Wait for line 418, H position ignored
    0x9010, 0x00f5, // mover 0x00F5, PA_GFX_CTRL     ; Blank
    0x01b8, 0x0002, // wait  0, 440                  ; Wait for line 440, H position ignored
    0x9010, 0x0000, // mover 0x0000, PA_GFX_CTRL     ; Set to text mode
    0x9015, 0x0000, // mover PA_LINE_ADDR, 0x0000
    0x01c8, 0x0002, // wait  0, 456                  ; Wait for line 456, H position ignored
    0x9010, 0x00f5, // mover 0x00F5, PA_GFX_CTRL     ; Blank
    0x0000, 0x0003  // nextf
};


/*
static void get_textmode_settings()
{
    uint16_t vx          = (xreg_getw(PA_GFX_CTRL) & 3) + 1;
    uint16_t tile_height = (xreg_getw(PA_TILE_CTRL) & 0xf) + 1;
    screen_addr          = xreg_getw(PA_DISP_ADDR);
    text_columns         = (uint8_t)xreg_getw(PA_LINE_LEN);
    text_rows            = (uint8_t)(((xreg_getw(VID_VSIZE) / vx) + (tile_height - 1)) / tile_height);
}
*/


static void xpos(uint8_t h, uint8_t v)
{
    text_h = h;
    text_v = v;
}

static void xcolor(uint8_t color)
{
    text_color = color;
}

/*
static void xhome()
{
    get_textmode_settings();
    xpos(0, 0);
}
*/

/*
static void xcls()
{
    // clear screen
    xhome();
    xm_setw(WR_ADDR, screen_addr);
    xm_setw(WR_INCR, 1);
    xm_setbh(DATA, text_color);
    for (uint16_t i = 0; i < (text_columns * text_rows); i++)
    {
        xm_setbl(DATA, ' ');
    }
    xm_setw(WR_ADDR, screen_addr);
}
*/
static void xcls()
{
    xpos(0, 0);


    xm_setw(WR_ADDR, screen_addr);
    xm_setw(WR_INCR, 1);
    xm_setbh(DATA, text_color);
    for (uint16_t i = 0; i < g_margin_height * 320 / 2; i++)
    {
        xm_setbl(DATA, ' ');
    }
    xm_setw(WR_ADDR, screen_addr);
}


static void xprint(const char * str)
{
    xm_setw(WR_INCR, 1);
    xm_setw(WR_ADDR, screen_addr + (text_v * text_columns) + text_h);
    xm_setbh(DATA, text_color);

    char c;
    while ((c = *str++) != '\0')
    {
        if (c >= ' ')
        {
            xm_setbl(DATA, c);
            if (++text_h >= text_columns)
            {
                text_h = 0;
                if (++text_v >= text_rows)
                {
                    text_v = 0;
                }
            }
            continue;
        }
        switch (c)
        {
            case '\r':
                text_h = 0;
                xm_setw(WR_ADDR, screen_addr + (text_v * text_columns) + text_h);
                break;
            case '\n':
                text_h = 0;
                if (++text_v >= text_rows)
                {
                    text_v = text_rows - 1;
                }
                xm_setw(WR_ADDR, screen_addr + (text_v * text_columns) + text_h);
                break;
            case '\b':
                if (--text_h < 0)
                {
                    text_h = text_columns - 1;
                    if (--text_v < 0)
                    {
                        text_v = 0;
                    }
                }
                xm_setw(WR_ADDR, screen_addr + (text_v * text_columns) + text_h);
                break;
            default:
                break;
        }
    }
}

static char xprint_buff[4096];
static void xprintf(const char * fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(xprint_buff, sizeof(xprint_buff), fmt, args);
    xprint(xprint_buff);
    va_end(args);
}

unsigned long int next = 1;

int rand2(void)
{
    next = next * 1103515243 + 12345;
    return (unsigned int)(next / 65536) % 32768;
}

void srand2(unsigned int seed)
{
    next = seed;
}

typedef enum {
    CLEAR,
    TRIANGLES,
    CUBE
} BenchType;

void bench(BenchType bench_type)
{
    pr_clear();
    pr_finish();

    uint16_t t1 = xm_getw(TIMER);

    switch (bench_type)
    {
    case CLEAR:
        pr_clear();
        break;

    case TRIANGLES:
        for (int i = 0; i < 1000; ++i)
            pr_draw_filled_triangle(0, 0, 0, 0, 0, 0, 0);
        break;

    case CUBE:
        {
            mat4x4 mat_proj, mat_rot_z, mat_rot_x;
            get_projection_matrix(&mat_proj);
            get_rotation_x_matrix(1.0f, &mat_rot_x);
            get_rotation_z_matrix(1.0f, &mat_rot_z);
            draw_cube(&mat_proj, &mat_rot_z, &mat_rot_x, true);
        }
        break;

    
    default:
        break;
    }

    pr_swap(false);
    uint16_t t2 = xm_getw(TIMER);

    uint16_t dt;
    if (t2 > t1) {
        dt = t2 - t1;
    } else {
        dt = 65535 - t1 + t2;
    }

    xpos(0, 0);
    xprintf("%s: Period: %d/10 ms               ", bench_type == CLEAR ? "Clear" : bench_type == TRIANGLES ? "1000 Triangles" : "Cube", dt);
}

void xosera_demo()
{
    xosera_init(0);

    // Set the Xosera interrupt mask
    uint16_t sc = xm_getw(SYS_CTRL);
    xm_setw(SYS_CTRL, sc | 0x8);

    install_intr();

    xm_setw(XR_ADDR, XR_COPPER_MEM);

    for (uint8_t i = 0; i < copper_list_len; i++) {
        xm_setw(XR_DATA, copper_list[i]);
    }    

    xreg_setw(PA_DISP_ADDR, 0x0000);
    xreg_setw(PA_LINE_ADDR, 0x0000);
    xreg_setw(PA_LINE_LEN, 160);

    pr_init(g_margin_height, 200 - g_margin_height);

    // initialize swap
    pr_init_swap();

    // enable Copper
    xreg_setw(COPP_CTRL, 0x8000);        

    xcolor(0x02);

    xcls();

    while(1)
    {
        for (int i = 0; i < 1000; ++i)
            bench(CLEAR);
        for (int i = 0; i < 100; ++i)
            bench(TRIANGLES);
        for (int i = 0; i < 100; ++i)
            bench(CUBE);
    }

    // disable Copper
    xreg_setw(COPP_CTRL, 0x0000);
}
