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

#define DELAY_TIME 5000        // human speed
//#define DELAY_TIME 1000        // impatient human speed
//#define DELAY_TIME 100        // machine speed

#include "xosera_api.h"


const uint16_t defpal[16] = {
    0x0000,        // black
    0x000A,        // blue
    0x00A0,        // green
    0x00AA,        // cyan
    0x0A00,        // red
    0x0A0A,        // magenta
    0x0AA0,        // brown
    0x0AAA,        // light gray
    0x0555,        // dark gray
    0x055F,        // light blue
    0x05F5,        // light green
    0x05FF,        // light cyan
    0x0F55,        // light red
    0x0F5F,        // light magenta
    0x0FF5,        // yellow
    0x0FFF         // white
};


// Define rosco_m68k Xosera board base address pointer (See
// https://github.com/rosco-m68k/hardware-projects/blob/feature/xosera/xosera/code/pld/decoder/ic3_decoder.pld#L25)
volatile xreg_t * const xosera_ptr = (volatile xreg_t * const)0xf80060;        // rosco_m68k Xosera base

// dummy global variable
uint32_t global;        // this is used to prevent the compiler from optimizing out tests

uint16_t screen_addr;
uint8_t  text_color = 0x02;        // dark green on black
uint8_t  text_columns;
uint8_t  text_rows;
int8_t   text_h;
int8_t   text_v;

static void get_textmode_settings()
{
    int  mode      = xv_reg_getw(gfxctrl);
    bool v_dbl     = mode & 2;
    int  tile_size = ((xv_reg_getw(fontctrl) & 0xf) + 1) << (v_dbl ? 1 : 0);
    screen_addr    = xv_reg_getw(dispstart);
    text_columns   = xv_reg_getw(dispwidth);
    text_rows      = (xv_reg_getw(vidheight) + (tile_size - 1)) / tile_size;
}

static void xpos(uint8_t h, uint8_t v)
{
    text_h = h;
    text_v = v;
}

static void xcolor(uint8_t color)
{
    text_color = color;
}

static void xhome()
{
    get_textmode_settings();
    xpos(0, 0);
}

static void xcls()
{
    // clear screen
    xhome();
    xv_setw(wr_addr, screen_addr);
    xv_setw(wr_inc, 1);
    xv_setbh(data, text_color);
    for (uint16_t i = 0; i < (text_columns * text_rows); i++)
    {
        xv_setbl(data, ' ');
    }
    xv_setw(wr_addr, screen_addr);
}

static void xprint(const char * str)
{
    xv_setw(wr_inc, 1);
    xv_setw(wr_addr, screen_addr + (text_v * text_columns) + text_h);
    xv_setbh(data, text_color);

    char c;
    while ((c = *str++) != '\0')
    {
        if (c >= ' ')
        {
            xv_setbl(data, c);
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
                xv_setw(wr_addr, screen_addr + (text_v * text_columns) + text_h);
                break;
            case '\n':
                text_h = 0;
                if (++text_v >= text_rows)
                {
                    text_v = text_rows - 1;
                }
                xv_setw(wr_addr, screen_addr + (text_v * text_columns) + text_h);
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
                xv_setw(wr_addr, screen_addr + (text_v * text_columns) + text_h);
                break;
            case '\f':
                xcls();
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

typedef struct
{
    float x1, y1, x2, y2;
} Coord;

static void draw_line(Coord coord, int color)
{
    uint8_t busy;
    do {
        busy = xv_getbh(wr_pr_cmd);
    } while(busy & 0x80);

    xv_setw(wr_pr_cmd, PR_COORDX0 | ((int)(coord.x1) & 0x0FFF));
    xv_setw(wr_pr_cmd, PR_COORDY0 | ((int)(coord.y1) & 0x0FFF));
    xv_setw(wr_pr_cmd, PR_COORDX1 | ((int)(coord.x2) & 0x0FFF));
    xv_setw(wr_pr_cmd, PR_COORDY1 | ((int)(coord.y2) & 0x0FFF));
    xv_setw(wr_pr_cmd, PR_COLOR | color);
    xv_setw(wr_pr_cmd, PR_EXECUTE | PR_LINE);
}

static void draw_filled_rectangle(Coord coord, int color)
{
    uint8_t busy;
    do {
        busy = xv_getbh(wr_pr_cmd);
    } while(busy & 0x80);

    xv_setw(wr_pr_cmd, PR_COORDX0 | ((int)(coord.x1) & 0x0FFF));
    xv_setw(wr_pr_cmd, PR_COORDY0 | ((int)(coord.y1) & 0x0FFF));
    xv_setw(wr_pr_cmd, PR_COORDX1 | ((int)(coord.x2) & 0x0FFF));
    xv_setw(wr_pr_cmd, PR_COORDY1 | ((int)(coord.y2) & 0x0FFF));
    xv_setw(wr_pr_cmd, PR_COLOR | color);
    xv_setw(wr_pr_cmd, PR_EXECUTE | PR_FILLED_RECTANGLE);
}

// Color conversion
// Ref.:
// https://stackoverflow.com/questions/3018313/algorithm-to-convert-rgb-to-hsv-and-hsv-to-rgb-in-range-0-255-for-both

typedef struct
{
    double r;        // a fraction between 0 and 1
    double g;        // a fraction between 0 and 1
    double b;        // a fraction between 0 and 1
} RGB;

typedef struct
{
    double h;        // angle in degrees
    double s;        // a fraction between 0 and 1
    double v;        // a fraction between 0 and 1
} HSV;

RGB hsv2rgb(HSV in)
{
    double hh, p, q, t, ff;
    long   i;
    RGB    out;

    if (in.s <= 0.0)
    {        // < is bogus, just shuts up warnings
        out.r = in.v;
        out.g = in.v;
        out.b = in.v;
        return out;
    }
    hh = in.h;
    if (hh >= 360.0)
        hh = 0.0;
    hh /= 60.0;
    i  = (long)hh;
    ff = hh - i;
    p  = in.v * (1.0 - in.s);
    q  = in.v * (1.0 - (in.s * ff));
    t  = in.v * (1.0 - (in.s * (1.0 - ff)));

    switch (i)
    {
        case 0:
            out.r = in.v;
            out.g = t;
            out.b = p;
            break;
        case 1:
            out.r = q;
            out.g = in.v;
            out.b = p;
            break;
        case 2:
            out.r = p;
            out.g = in.v;
            out.b = t;
            break;

        case 3:
            out.r = p;
            out.g = q;
            out.b = in.v;
            break;
        case 4:
            out.r = t;
            out.g = p;
            out.b = in.v;
            break;
        case 5:
        default:
            out.r = in.v;
            out.g = p;
            out.b = q;
            break;
    }
    return out;
}


void set_palette(double hue_offset)
{
    double hue = hue_offset;
    for (uint16_t i = 0; i < 256; i++)
    {
        xv_setw(aux_addr, XV_AUX_COLORMEM | i);

        if (i < 16)
        {
            xv_setw(aux_data, defpal[i]);        // set palette data
        }
        else
        {
            HSV      hsv = {hue, 1.0, 1.0};
            RGB      rgb = hsv2rgb(hsv);
            uint8_t  r   = 15.0 * rgb.r;
            uint8_t  g   = 15.0 * rgb.g;
            uint8_t  b   = 15.0 * rgb.b;
            uint16_t c   = (r << 8) | (g << 4) | b;
            xv_setw(aux_data, c);        // set palette data
        }

        hue += 360.0 / 256.0;
    }
}

void clear()
{
    // Blue background
    xv_setw(wr_inc, 0x0001);
    xv_setw(wr_addr, 0x0000);
    for (int i = 0; i < 320 * 240 / 2; ++i)
        xv_setw(data, 0x0101);
}

void demo_lines()
{
    const Coord coords[] = {{0, 0, 2, 4},   {0, 4, 2, 0},   {3, 4, 3, 0},      {3, 0, 5, 0},   {5, 0, 5, 4},
                                    {5, 4, 3, 4},   {8, 0, 6, 0},   {6, 0, 6, 2},      {6, 2, 8, 2},   {8, 2, 8, 4},
                                    {8, 4, 6, 4},   {9, 0, 11, 0},  {9, 0, 9, 4},      {9, 2, 11, 2},  {9, 4, 11, 4},
                                    {12, 0, 14, 0}, {14, 0, 14, 2}, {14, 2, 12, 2},    {12, 2, 14, 4}, {12, 4, 12, 0},
                                    {15, 4, 16, 0}, {16, 0, 17, 4}, {15.5, 2, 16.5, 2}};


    clear();

    double angle = 0.0;
    for (int i = 0; i < 2048; i++)
    {
        float x = 1024.0f * cos(angle);
        float y = 1024.0f * sin(angle);
        Coord c = {240, 120, 240 + x, 120 + y};
        draw_line(c, i % (256 - 16) + 16);
        angle += 2.0f * M_PI / 2048.0f;
    }

    float scale_x  = 4;
    float scale_y  = 5;
    float offset_x = 0;
    float offset_y = 0;

    for (int i = 0; i < 10; ++i)
    {
        for (size_t j = 0; j < sizeof(coords) / sizeof(Coord); ++j)
        {
            Coord coord = coords[j];

            coord.x1 = coord.x1 * scale_x + offset_x;
            coord.y1 = coord.y1 * scale_y + offset_y;
            coord.x2 = coord.x2 * scale_x + offset_x;
            coord.y2 = coord.y2 * scale_y + offset_y;
            draw_line(coord, i + 2);
        }

        offset_y += 5 * scale_y;
        scale_x += 1;
        scale_y += 1;
    }
}

typedef struct {
    int x, y;
    int radius;
    int color;
    int speed_x;
    int speed_y;
} Particle;



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

void demo_filled_rectangles()
{
    Particle particles[100];

    for(size_t i = 0; i < 100; ++i) {
        Particle* p = &particles[i];
        p->x = rand2() % 320;
        p->y = rand2() % 240;
        p->radius = rand2() % 10 + 5;
        p->color = rand2() % 256;
        p->speed_x = rand2() % 10 - 5;
        p->speed_y = rand2() % 10 - 5;
    }

    //for (int i = 0; i < 1000; ++i) {
    for (;;) {

        Coord c = {0, 0, 320, 240};
        draw_filled_rectangle(c, 1);

        for(size_t j = 0; j < 100; ++j) {
            Particle* p = &particles[j];
            Coord c = {p->x - p->radius, p->y - p->radius, p->x + p->radius, p->y + p->radius};
            draw_filled_rectangle(c, p->color);
        }

        for(size_t j = 0; j < 100; ++j) {
            Particle* p = &particles[j];
            p->x += p->speed_x;
            p->y += p->speed_y;
            if (p->x <= 0 || p->x >= 320)
                p->speed_x = -p->speed_x;
            if (p->y <= 0 || p->y >= 240)
                p->speed_y = -p->speed_y;
        }

        //delay(10);
    }
}

void xosera_demo()
{

    while(!xosera_sync());

    xv_reg_setw(gfxctrl, 0x0005);

/*
    xcolor(0x02);
    xcls();
    xprintf("Xosera\nPrimitive\nRenderer\nDemo\n");
    delay(2000);
*/
    xv_reg_setw(gfxctrl, 0x0075);
    xv_reg_setw(dispstart, 0x0000);
    xv_reg_setw(dispwidth, 160);

    set_palette(0);

    //demo_lines();
    //delay(2000);

    demo_filled_rectangles();
    delay(2000);

    while(1);
}
