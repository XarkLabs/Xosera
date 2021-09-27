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

#define NB_RECTS        100
#define NB_TRIANGLES    50

#include <xosera_m68k_api.h>
#include <pr_api.h>
#include <cube.h>

extern void install_intr(void);
extern void remove_intr(void);

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

uint16_t pal[256][3];

// dummy global variable
uint32_t global;        // this is used to prevent the compiler from optimizing out tests

uint16_t screen_addr;
uint8_t  text_columns;
uint8_t  text_rows;
int8_t   text_h;
int8_t   text_v;
uint8_t  text_color = 0x02;        // dark green on black

uint8_t  disp_buffer = 0;

const uint8_t  copper_list_len = 10;
const uint16_t copper_list[] = {
    0x0028, 0x0002, // wait  0, 40                   ; Wait for line 40, H position ignored
    0x9010, 0x0075, // mover 0x0075, PA_GFX_CTRL     ; Set to 8-bpp + Hx2 + Vx2
    0x01b8, 0x0002, // wait  0, 440                  ; Wait for line 440, H position ignored
    0x9010, 0x00F5, // mover 0x00F5, PA_GFX_CTRL     ; Blank
    0x0000, 0x0003  // nextf
};



static void get_textmode_settings()
{
    uint16_t vx          = (xreg_getw(PA_GFX_CTRL) & 3) + 1;
    uint16_t tile_height = (xreg_getw(PA_TILE_CTRL) & 0xf) + 1;
    screen_addr          = xreg_getw(PA_DISP_ADDR);
    text_columns         = (uint8_t)xreg_getw(PA_LINE_LEN);
    text_rows            = (uint8_t)(((xreg_getw(VID_VSIZE) / vx) + (tile_height - 1)) / tile_height);
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
    xm_setw(WR_ADDR, screen_addr);
    xm_setw(WR_INCR, 1);
    xm_setbh(DATA, text_color);
    for (uint16_t i = 0; i < (text_columns * text_rows); i++)
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
} Coord2;

typedef struct
{
    float x1, y1, x2, y2, x3, y3;
} Coord3;

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


void calc_palette_color()
{
    double hue = 0.0;
    for (uint16_t i = 0; i < 256; i++)
    {
        if (i < 16)
        {
            uint16_t c = defpal[i];
            pal[i][0] = (uint8_t)(c >> 8 & 0xf);
            pal[i][1] = (uint8_t)(c >> 4 & 0xf);
            pal[i][2] = (uint8_t)(c & 0xf);
        }
        else
        {
            HSV hsv = {hue, 1.0, 1.0};
            RGB rgb = hsv2rgb(hsv);
            pal[i][0] = 15.0 * rgb.r;
            pal[i][1] = 15.0 * rgb.g;
            pal[i][2] = 15.0 * rgb.b;
        }

        hue += 360.0 / 256.0;
    }
}

void calc_palette_mono()
{
    for (uint16_t i = 0; i < 256; i++)
    {
        pal[i][0] = i >> 4;
        pal[i][1] = i >> 4;
        pal[i][2] = i >> 4;
    }
}

void set_palette(float value)
{
    for (uint16_t i = 0; i < 256; i++)
    {
        xm_setw(XR_ADDR, XR_COLOR_MEM | i);

        uint16_t r = pal[i][0] * value;
        uint16_t g = pal[i][1] * value;
        uint16_t b = pal[i][2] * value;

        uint16_t c   = (r << 8) | (g << 4) | b;
        xm_setw(XR_DATA, c);        // set palette data
    }
}

void fade_in()
{
    for (int i = 0; i <= 100; i += 20)
        set_palette((float)i / 100.0f);
}

void fade_out()
{
    for (int i = 0; i <= 100; i += 20)
        set_palette(1.0f - (float)i / 100.0f);
}

void demo_lines()
{
    const Coord2 coords[] = {{0, 0, 2, 4},   {0, 4, 2, 0},   {3, 4, 3, 0},      {3, 0, 5, 0},   {5, 0, 5, 4},
                                    {5, 4, 3, 4},   {8, 0, 6, 0},   {6, 0, 6, 2},      {6, 2, 8, 2},   {8, 2, 8, 4},
                                    {8, 4, 6, 4},   {9, 0, 11, 0},  {9, 0, 9, 4},      {9, 2, 11, 2},  {9, 4, 11, 4},
                                    {12, 0, 14, 0}, {14, 0, 14, 2}, {14, 2, 12, 2},    {12, 2, 14, 4}, {12, 4, 12, 0},
                                    {15, 4, 16, 0}, {16, 0, 17, 4}, {15.5, 2, 16.5, 2}};


    pr_clear();

    double angle = 0.0;
    for (int i = 0; i < 256; i++)
    {
        float x = 80.0f * cos(angle);
        float y = 80.0f * sin(angle);
        Coord2 c = {240, 120, 240 + x, 120 + y};
        pr_draw_line(c.x1, c.y1, c.x2, c.y2, i % (256 - 16) + 16);
        angle += 2.0f * M_PI / 256.0f;
    }

    float scale_x  = 4;
    float scale_y  = 5;
    float offset_x = 0;
    float offset_y = 0;

    for (int i = 0; i < 10; ++i)
    {
        for (size_t j = 0; j < sizeof(coords) / sizeof(Coord2); ++j)
        {
            Coord2 coord = coords[j];

            coord.x1 = coord.x1 * scale_x + offset_x;
            coord.y1 = coord.y1 * scale_y + offset_y;
            coord.x2 = coord.x2 * scale_x + offset_x;
            coord.y2 = coord.y2 * scale_y + offset_y;
            pr_draw_line(coord.x1, coord.y1, coord.x2, coord.y2, i + 2);
        }

        offset_y += 5 * scale_y;
        scale_x += 1;
        scale_y += 1;
    }

    pr_swap(true);
    calc_palette_color();
    fade_in();
    delay(2000);
    fade_out();
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

void demo_filled_rectangles(int nb_iterations)
{
    Particle particles[NB_RECTS];

    for(size_t i = 0; i < NB_RECTS; ++i) {
        Particle* p = &particles[i];
        p->x = rand2() % 320;
        p->y = rand2() % 200;
        p->radius = rand2() % 10 + 5;
        p->color = rand2() % 256;
        p->speed_x = rand2() % 10 - 5;
        p->speed_y = rand2() % 10 - 5;
    }

    pr_clear();
    pr_swap(true);

    calc_palette_color();
    fade_in();

    for (int i = 0; i < nb_iterations; ++i) {

        pr_clear();

        for(size_t j = 0; j < NB_RECTS; ++j) {
            Particle* p = &particles[j];
            pr_draw_filled_rectangle(p->x - p->radius, p->y - p->radius, p->x + p->radius, p->y + p->radius, p->color);
        }

        pr_swap(true);

        for(size_t j = 0; j < NB_RECTS; ++j) {
            Particle* p = &particles[j];
            p->x += p->speed_x;
            p->y += p->speed_y;
            if (p->x <= 0 || p->x >= 320)
                p->speed_x = -p->speed_x;
            if (p->y <= 0 || p->y >= 200)
                p->speed_y = -p->speed_y;
        }
    }

    fade_out();
}

void demo_filled_triangles(int nb_iterations)
{
    Particle particles[3*NB_TRIANGLES];

    for(size_t i = 0; i < 3*NB_TRIANGLES; ++i) {
        Particle* p = &particles[i];
        p->x = rand2() % 320;
        p->y = rand2() % 200;
        p->radius = 0;
        p->color = rand2() % 256;
        p->speed_x = rand2() % 10 - 5;
        p->speed_y = rand2() % 10 - 5;
    }

    pr_clear();
    pr_swap(true);

    calc_palette_color();
    fade_in();

    for (int i = 0; i < nb_iterations; ++i) {

        pr_clear();

        for(size_t j = 0; j < 3*NB_TRIANGLES; j+=3) {
            Particle* p1 = &particles[j];
            Particle* p2 = &particles[j+1];
            Particle* p3 = &particles[j+2];
            pr_draw_filled_triangle(p1->x, p1->y, p2->x, p2->y, p3->x, p3->y, p1->color);
        }

        for(size_t j = 0; j < 3*NB_TRIANGLES; ++j) {
            Particle* p = &particles[j];
            p->x += p->speed_x;
            p->y += p->speed_y;
            if (p->x <= 0 || p->x >= 320)
                p->speed_x = -p->speed_x;
            if (p->y <= 0 || p->y >= 200)
                p->speed_y = -p->speed_y;
        }

        pr_swap(true);
    }

    fade_out();
}

void demo_cube(int nb_iterations)
{
    pr_clear();
    pr_swap(true);

    calc_palette_mono();
    fade_in();

    float theta = 0.0f;
    mat4x4 mat_proj, mat_rot_z, mat_rot_x;

    get_projection_matrix(&mat_proj);
    
    for (int i = 0; i < nb_iterations; ++i) {

        pr_clear();

        get_rotation_z_matrix(theta, &mat_rot_z);
        get_rotation_x_matrix(theta, &mat_rot_x);
        draw_cube(&mat_proj, &mat_rot_z, &mat_rot_x, true);

        pr_swap(true);

        theta += 0.1f;
    }

    fade_out();
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

    pr_init(0, 200);

    calc_palette_color();
    set_palette(0.0f);

    while(1) {

        xreg_setw(PA_GFX_CTRL, 0x0005);

        xcolor(0x02);
        xcls();

        xprintf("Xosera\nPrimitive\nRenderer\nDemo\n");
        calc_palette_color();
        fade_in();
        delay(2000);
        fade_out();

        // initialize swap
        pr_init_swap();

        // enable Copper
        xreg_setw(COPP_CTRL, 0x8000);        

        demo_lines();
        demo_filled_rectangles(1000);
        demo_filled_triangles(500);
        demo_cube(100);

        // disable Copper
        xreg_setw(COPP_CTRL, 0x0000);
    }
}
