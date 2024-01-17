#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <basicio.h>

#include "xosera_m68k_api.h"

#include "xosera_boing_defs.h"


#define PI  3.1415926f
#define PAU (1.5f * PI)
#define TAU (2.0f * PI)

#define BALL_THETA_START (+PI)
#define BALL_THETA_STOP  (0.0f)
#define BALL_THETA_STEP  ((BALL_THETA_STOP - BALL_THETA_START) / 8.0f)

#define BALL_PHI_START (-PI)
#define BALL_PHI_STOP  (0.0f)
#define BALL_PHI_STEP  ((BALL_PHI_STOP - BALL_PHI_START) / 8.0f)

#define WALL_DIST   32
#define WALL_LEFT   (WALL_DIST)
#define WALL_RIGHT  (320 - WALL_DIST)
#define WALL_BOTTOM (WALL_DIST)
#define WALL_TOP    (240 - WALL_DIST)

#define PAINT_BALL true
#define USE_AUDIO  true

// blit methods
#define USE_COPASM    0
#define USE_COPMACROS 1
#define USE_CPU_BLIT  2

#define USE_BLIT_METHOD USE_COPASM        // which blit method to use

#if USE_AUDIO
extern char _binary_Boing_raw_start[];
extern char _binary_Boing_raw_end[];
#endif

uint16_t vid_hsize;        // this is cached xosera_vid_width()
uint32_t clk_hz;           // pixel clock Hz (25125000 at 640x480 or 33750000 at 848x480)

uint8_t  bg_bitmap[HEIGHT_A][WIDTH_A]                                                                     = {0};
uint8_t  ball_bitmap[BALL_BITMAP_HEIGHT][BALL_BITMAP_WIDTH]                                               = {0};
uint16_t ball_tiles[BALL_TILES_HEIGHT][BALL_TILES_WIDTH][TILE_HEIGHT_B][TILE_WIDTH_B / PIXELS_PER_WORD_B] = {0};


#if USE_BLIT_METHOD == USE_COPASM
#include "boing_copper.h"

#elif USE_BLIT_METHOD == USE_COPMACROS

// parameters words in copper memory
enum
{
    cop_draw_ball = 0x030,

    cop_frame_count = 0x060,
    cop_ball_dst,
    cop_ball_prev,
    cop_ball_gfx_ctrl,
    cop_ball_h_scroll,
    cop_ball_v_scroll,
};

uint16_t copper_list[] = {
    // 0x000 = entry
    [0x000] = COP_VPOS(479),                                          // wait for offscreen
    COP_VPOS(COP_V_WAITBLIT),                                         // wait until blitter not in use
    COP_CMPM(cop_ball_prev),                                          // check for zero ball_prev
    COP_BRGE(cop_draw_ball),                                          // skip erase before 1st draw
    COP_MOVI(MAKE_BLIT_CTRL(0x00, 0, 0, 1), XR_BLIT_CTRL),            // no transp, constS
    COP_MOVI(0x0000, XR_BLIT_ANDC),                                   // ANDC constant
    COP_MOVI(0x0000, XR_BLIT_XOR),                                    // XOR constant
    COP_MOVI(0x0000, XR_BLIT_MOD_S),                                  // no modulo S
    COP_MOVI(0x0000, XR_BLIT_SRC_S),                                  // blank ball
    COP_MOVI(WIDTH_WORDS_B - BALL_TILES_WIDTH, XR_BLIT_MOD_D),        // modulo D
    COP_MOVM(cop_ball_prev, XR_BLIT_DST_D),                           // previous ball position
    COP_MOVI(MAKE_BLIT_SHIFT(0xF, 0xF, 0), XR_BLIT_SHIFT),            // no edge masking or shifting
    COP_MOVI(BALL_TILES_HEIGHT - 1, XR_BLIT_LINES),                   // 2-D blit
    COP_MOVI(BALL_TILES_WIDTH - 1, XR_BLIT_WORDS),                    // go!
    COP_BRLT(cop_draw_ball),                                          // branch to draw ball
    COP_BRGE(cop_draw_ball),                                          // branch to draw ball
    [cop_draw_ball] = COP_MOVM(cop_ball_dst, cop_ball_prev),          // update previous ball position
    COP_MOVI(MAKE_BLIT_CTRL(0x00, 0, 0, 0), XR_BLIT_CTRL),            // no transp, no constS
    COP_MOVI(0x0000, XR_BLIT_ANDC),                                   // ANDC constant
    COP_MOVI(0x0000, XR_BLIT_XOR),                                    // XOR constant
    COP_MOVI(0x0000, XR_BLIT_MOD_S),                                  // no modulo S
    COP_MOVI(VRAM_BASE_BALL, XR_BLIT_SRC_S),                          // ball
    COP_MOVI(WIDTH_WORDS_B - BALL_TILES_WIDTH, XR_BLIT_MOD_D),        // modulo D
    COP_MOVM(cop_ball_dst, XR_BLIT_DST_D),                            // ball position
    COP_MOVI(MAKE_BLIT_SHIFT(0xF, 0xF, 0), XR_BLIT_SHIFT),            // no edge masking or shifting
    COP_MOVI(BALL_TILES_HEIGHT - 1, XR_BLIT_LINES),                   // 2-D blit
    COP_MOVI(BALL_TILES_WIDTH - 1, XR_BLIT_WORDS),                    // go!
    COP_MOVM(cop_ball_gfx_ctrl, XR_PB_GFX_CTRL),                      // colorbase to rotate colors
    COP_MOVM(cop_ball_h_scroll, XR_PB_H_SCROLL),                      // h scroll for fine scroll
    COP_MOVM(cop_ball_v_scroll, XR_PB_V_SCROLL),                      // v scroll for fine scroll
    COP_LDM(cop_frame_count),                                         // get frame_count
    COP_ADDI(1),                                                      // increment
    COP_STM(cop_frame_count),                                         // store frame_count
    COP_VPOS(COP_V_EOF),                                              // wait for next frame
    // data words
    [cop_frame_count]   = 0,
    [cop_ball_dst]      = 0,
    [cop_ball_prev]     = 0,
    [cop_ball_gfx_ctrl] = 0,
    [cop_ball_h_scroll] = 0,
    [cop_ball_v_scroll] = 0,
};
#endif

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

int abs(int x)
{
    if (x < 0)
    {
        return -x;
    }
    else
    {
        return x;
    }
}

static inline int interpolate(int x0, int x1, int y0, int y1, int x)
{
    int dx = x1 - x0;
    int dy = y1 - y0;
    if (dx == 0)
    {
        return y0 + dy / 2;
    }
    else
    {
        return y0 + dy * (x - x0) / dx;
    }
}

static inline uint8_t interpolate_colour(int x0, int x1, uint8_t c0, uint8_t c1, int x)
{
    c0 -= 2;
    c1 -= 2;

    if (c1 < c0)
    {
        c1 += 14;
    }

    return interpolate(x0, x1, c0, c1, x) % 14 + 2;
}

void draw_line(int width, int height, uint8_t bitmap[height][width], int x0, int y0, int x1, int y1, uint8_t colour)
{
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? +1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? +1 : -1;

    int err = dx + dy;

    while (true)
    {
        bitmap[height - 1 - y0][x0] = colour;
        if (x0 == x1 && y0 == y1)
        {
            break;
        }

        int e2 = 2 * err;
        if (e2 >= dy)
        {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}

// used to draw shadow
void draw_line_chunky(int     width,
                      int     height,
                      uint8_t bitmap[height][width],
                      int     x0,
                      int     y0,
                      int     x1,
                      int     y1,
                      uint8_t colour)
{
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? +1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? +1 : -1;

    int err = dx + dy;

    while (true)
    {
        bitmap[height - 1 - y0][x0] = colour;
        if (x0 == x1 && y0 == y1)
        {
            break;
        }

        int e2 = 2 * err;
        if (e2 >= dy)
        {
            err += dy;
            x0 += sx;
        }
        else if (e2 <= dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}

static inline long scale_coord(long v, long scale, long v_center, long scale_center)
{
    return (v - v_center) * scale / scale_center + v_center;
}

static inline void wait_vblank_start()
{
    xv_prep();
    xwait_not_vblank();
    xwait_vblank();
}

void draw_line_scale(int     width,
                     int     height,
                     uint8_t bitmap[height][width],
                     int     x0,
                     int     y0,
                     int     scale0,
                     int     x1,
                     int     y1,
                     int     scale1,
                     int     scale_base,
                     uint8_t colour)
{
    x0 = scale_coord(x0, scale0, width / 2, scale_base);
    y0 = scale_coord(y0, scale0, height / 2, scale_base);
    x1 = scale_coord(x1, scale1, width / 2, scale_base);
    y1 = scale_coord(y1, scale1, height / 2, scale_base);
    draw_line(width, height, bitmap, x0, y0, x1, y1, colour);
}

void draw_bg(void)
{
    const int scale_base  = 16;
    const int scale_front = 18;
    const int scale_back  = 14;
    const int dx          = 16;
    const int dy          = 16;
    const int dscale      = 1;

    // Draw back
    for (int x = WALL_LEFT; x <= WALL_RIGHT; x += dx)
    {
        draw_line_scale(
            WIDTH_A, HEIGHT_A, bg_bitmap, x, WALL_BOTTOM, scale_back, x, WALL_TOP, scale_back, scale_base, 1);
    }
    for (int y = WALL_BOTTOM; y <= WALL_TOP; y += dy)
    {
        draw_line_scale(
            WIDTH_A, HEIGHT_A, bg_bitmap, WALL_LEFT, y, scale_back, WALL_RIGHT, y, scale_back, scale_base, 1);
    }

    // Draw floor
    for (int x = WALL_LEFT; x <= WALL_RIGHT; x += dx)
    {
        draw_line_scale(
            WIDTH_A, HEIGHT_A, bg_bitmap, x, WALL_BOTTOM, scale_back, x, WALL_BOTTOM, scale_front, scale_base, 1);
    }
    for (int scale = scale_back; scale <= scale_front; scale += dscale)
    {
        draw_line_scale(
            WIDTH_A, HEIGHT_A, bg_bitmap, WALL_LEFT, WALL_BOTTOM, scale, WALL_RIGHT, WALL_BOTTOM, scale, scale_base, 1);
    }
}

void do_tiles(void)
{        // Replace with generic bitmap to tilemap
    for (uint16_t tile_row = 0; tile_row < BALL_TILES_HEIGHT; ++tile_row)
    {
        for (uint16_t tile_col = 0; tile_col < BALL_TILES_WIDTH; ++tile_col)
        {
            for (uint16_t row_in_tile = 0; row_in_tile < TILE_HEIGHT_B; ++row_in_tile)
            {
                for (uint16_t word_in_tile_row = 0; word_in_tile_row < TILE_WIDTH_B / PIXELS_PER_WORD_B;
                     ++word_in_tile_row)
                {
                    int pixel_row = tile_row * TILE_HEIGHT_B + row_in_tile;
                    int pixel_col = tile_col * TILE_WIDTH_B + word_in_tile_row * PIXELS_PER_WORD_B;

                    uint16_t word = 0;
                    for (int nibble = 0; nibble < PIXELS_PER_WORD_B; ++nibble)
                    {
                        word = word << PIXELS_PER_WORD_B | ball_bitmap[pixel_row][pixel_col + nibble];
                    }

                    ball_tiles[tile_row][tile_col][row_in_tile][word_in_tile_row] = word;
                }
            }
        }
    }
}

void draw_face_line(int     x_bl,
                    int     y_bl,
                    int     x_br,
                    int     y_br,
                    int     x_tl,
                    int     y_tl,
                    int     x_tr,
                    int     y_tr,
                    int     x_b,
                    int     x_t,
                    uint8_t colour)
{
    if (x_b < x_bl || x_b > x_br || x_t < x_tl || x_t > x_tr)
    {
        return;
    }

    int y_b = interpolate(x_bl, x_br, y_bl, y_br, x_b);
    int y_t = interpolate(x_tl, x_tr, y_tl, y_tr, x_t);

    draw_line_chunky(BALL_BITMAP_WIDTH, BALL_BITMAP_HEIGHT, ball_bitmap, x_b, y_b, x_t, y_t, colour);
}

void draw_face(int     x_bl,
               int     y_bl,
               int     x_br,
               int     y_br,
               int     x_tl,
               int     y_tl,
               int     x_tr,
               int     y_tr,
               uint8_t colour_start,
               uint8_t colour_end)
{
    if (x_tr - x_tl < x_br - x_bl)
    {
        for (int x_b = x_bl; x_b <= x_br; ++x_b)
        {
            int x_t    = interpolate(x_bl, x_br, x_tl, x_tr, x_b);
            int colour = interpolate_colour(x_bl, x_br, colour_start, colour_end, x_b);
            draw_face_line(x_bl, y_bl, x_br, y_br, x_tl, y_tl, x_tr, y_tr, x_b, x_t, colour);
        }
    }
    else
    {
        for (int x_t = x_tl; x_t <= x_tr; ++x_t)
        {
            int x_b    = interpolate(x_tl, x_tr, x_bl, x_br, x_t);
            int colour = interpolate_colour(x_tl, x_tr, colour_start, colour_end, x_t);
            draw_face_line(x_bl, y_bl, x_br, y_br, x_tl, y_tl, x_tr, y_tr, x_b, x_t, colour);
        }
    }
}

void fill_ball(void)
{
    memset(ball_bitmap, 0, sizeof(ball_bitmap));

    uint8_t colour = 3;

    for (float theta = BALL_THETA_START; theta > BALL_THETA_STOP - BALL_THETA_STEP / 2; theta += BALL_THETA_STEP)
    {
        for (float phi = BALL_PHI_START; phi < BALL_PHI_STOP + BALL_PHI_STEP / 2; phi += BALL_PHI_STEP)
        {
            float theta_b = theta;
            float theta_t = theta + BALL_THETA_STEP;
            float phi_l   = phi;
            float phi_r   = phi + BALL_PHI_STEP;

            float s_theta_b = sinf(theta_b);
            float c_theta_b = cosf(theta_b);
            float s_theta_t = sinf(theta_t);
            float c_theta_t = cosf(theta_t);
            float c_phi_l   = cosf(phi_l);
            float c_phi_r   = cosf(phi_r);

            float x_bl = BALL_RADIUS * c_phi_l * s_theta_b;
            float y_bl = BALL_RADIUS * c_theta_b;
            float x_br = BALL_RADIUS * c_phi_r * s_theta_b;
            float y_br = BALL_RADIUS * c_theta_b;
            float x_tl = BALL_RADIUS * c_phi_l * s_theta_t;
            float y_tl = BALL_RADIUS * c_theta_t;
            float x_tr = BALL_RADIUS * c_phi_r * s_theta_t;
            float y_tr = BALL_RADIUS * c_theta_t;

            float rot = -0.28;

            float s_rot = sinf(rot);
            float c_rot = cosf(rot);

            float x_bl_r = x_bl * c_rot - y_bl * s_rot;
            float y_bl_r = y_bl * c_rot + x_bl * s_rot;
            float x_br_r = x_br * c_rot - y_br * s_rot;
            float y_br_r = y_br * c_rot + x_br * s_rot;
            float x_tl_r = x_tl * c_rot - y_tl * s_rot;
            float y_tl_r = y_tl * c_rot + x_tl * s_rot;
            float x_tr_r = x_tr * c_rot - y_tr * s_rot;
            float y_tr_r = y_tr * c_rot + x_tr * s_rot;

            uint8_t next_colour = (colour - 2 + 7) % 14 + 2;

            draw_face(BALL_CENTER_X + roundf(x_bl_r),
                      BALL_CENTER_Y + roundf(y_bl_r),
                      BALL_CENTER_X + roundf(x_br_r),
                      BALL_CENTER_Y + roundf(y_br_r),
                      BALL_CENTER_X + roundf(x_tl_r),
                      BALL_CENTER_Y + roundf(y_tl_r),
                      BALL_CENTER_X + roundf(x_tr_r),
                      BALL_CENTER_Y + roundf(y_tr_r),
                      colour,
                      next_colour);

            colour = next_colour;
        }
    }
}

void shadow_ball(void)
{
    for (int row = 0; row < BALL_BITMAP_HEIGHT; ++row)
    {
        if (row < SHADOW_OFFSET_Y || row >= BALL_BITMAP_HEIGHT - SHADOW_OFFSET_Y)
        {
            continue;
        }
        for (int col = 0; col < BALL_BITMAP_WIDTH; ++col)
        {
            if (col < -SHADOW_OFFSET_X || col >= BALL_BITMAP_WIDTH + SHADOW_OFFSET_X)
            {
                continue;
            }
            if (ball_bitmap[row][col] == 0 && ball_bitmap[row + SHADOW_OFFSET_Y][col - SHADOW_OFFSET_X] > 1)
            {
                ball_bitmap[row][col] = 1;
            }
        }
    }
}

void draw_ball_at(int width_words, int height_words, int x, int y)
{
    // Convert world coordinates to screen coordinates
    y = height_words * ROWS_PER_WORD_B - 1 - y;

    int top_left_x = x - BALL_CENTER_X;
    int top_left_y = y - BALL_CENTER_Y;

    int      top_left_row = (top_left_y + TILE_HEIGHT_B - 1) / TILE_WIDTH_B;
    int      top_left_col = (top_left_x + TILE_WIDTH_B - 1) / TILE_WIDTH_B;
    uint16_t dst          = VRAM_BASE_B + top_left_row * width_words + top_left_col;

    int scroll_x = -(top_left_x - top_left_col * TILE_WIDTH_B);
    int scroll_y = -(top_left_y - top_left_row * TILE_HEIGHT_B);

    xv_prep();
#if USE_BLIT_METHOD == USE_COPASM
    xmem_setw(XR_COPPER_ADDR + boing_copper__ball_dst, dst);
    xmem_setw(XR_COPPER_ADDR + boing_copper__ball_h_scroll, MAKE_H_SCROLL(scroll_x));
    xmem_setw(XR_COPPER_ADDR + boing_copper__ball_v_scroll, MAKE_V_SCROLL(0, scroll_y));
#elif USE_BLIT_METHOD == USE_COPMACROS
    xmem_setw(XR_COPPER_ADDR + cop_ball_dst, dst);
    xmem_setw(XR_COPPER_ADDR + cop_ball_h_scroll, MAKE_H_SCROLL(scroll_x));
    xmem_setw(XR_COPPER_ADDR + cop_ball_v_scroll, MAKE_V_SCROLL(0, scroll_y));
#elif USE_BLIT_METHOD == USE_CPU_BLIT
    static uint16_t prev_dst;

    xreg_setw(PB_H_SCROLL, MAKE_H_SCROLL(scroll_x));
    xreg_setw(PB_V_SCROLL, MAKE_V_SCROLL(0, scroll_y));

    if (prev_dst)
    {
        xwait_blit_ready();
        xreg_setw(BLIT_CTRL, 0x0001);
        xreg_setw(BLIT_ANDC, 0x0000);
        xreg_setw(BLIT_XOR, 0x0000);
        xreg_setw(BLIT_MOD_S, 0x0000);
        xreg_setw(BLIT_SRC_S, 0x0000);
        xreg_setw(BLIT_MOD_D, WIDTH_WORDS_B - BALL_TILES_WIDTH);
        xreg_setw(BLIT_DST_D, prev_dst);
        xreg_setw(BLIT_SHIFT, 0xFF00);
        xreg_setw(BLIT_LINES, BALL_TILES_HEIGHT - 1);
        xreg_setw(BLIT_WORDS, BALL_TILES_WIDTH - 1);        // Starts operation
    }
    prev_dst = dst;

    xwait_blit_ready();
    xreg_setw(BLIT_CTRL, 0x0000);
    xreg_setw(BLIT_ANDC, 0x0000);
    xreg_setw(BLIT_XOR, 0x0000);
    xreg_setw(BLIT_MOD_S, 0x0000);
    xreg_setw(BLIT_SRC_S, VRAM_BASE_BALL);
    xreg_setw(BLIT_MOD_D, WIDTH_WORDS_B - BALL_TILES_WIDTH);
    xreg_setw(BLIT_DST_D, dst);
    xreg_setw(BLIT_SHIFT, 0xFF00);
    xreg_setw(BLIT_LINES, BALL_TILES_HEIGHT - 1);
    xreg_setw(BLIT_WORDS, BALL_TILES_WIDTH - 1);        // Starts operation
#endif
}

void set_ball_colour(uint8_t colour_base)
{
    xv_prep();

    uint16_t gfx_ctrl = MAKE_GFX_CTRL(colour_base, 0, GFX_4_BPP, 0, 0, 0);
#if USE_BLIT_METHOD == USE_COPASM
    xmem_setw(XR_COPPER_ADDR + boing_copper__ball_gfx_ctrl, gfx_ctrl);
#elif USE_BLIT_METHOD == USE_COPMACROS
    xmem_setw(XR_COPPER_ADDR + cop_ball_gfx_ctrl, gfx_ctrl);
#elif USE_BLIT_METHOD == USE_CPU_BLIT
    xreg_setw(PB_GFX_CTRL, gfx_ctrl);
#endif
}

#if USE_AUDIO
void upload_audio()
{
    xv_prep();

    xreg_setw(AUD_CTRL, MAKE_AUD_CTRL(0));               // disable audio
    vram_setw_addr_incr(VRAM_AUDIO_BASE, 0x0001);        // set VRAM address, write increment of 1
    for (uint16_t i = 0; i < VRAM_SILENCE_LEN; i++)
    {
        vram_setw_next(0x0000);        // zero = silence sample
    }
    // upload boing audio sample (from embedded binary data)
    for (uint16_t * wp = (uint16_t *)_binary_Boing_raw_start; wp < (uint16_t *)_binary_Boing_raw_end; wp++)
    {
        vram_setw_next(*wp);
    }

    uint16_t rate   = 8000;
    uint16_t period = (clk_hz + (rate / 2)) / rate;
    // start silence sample
    xreg_setw(AUD0_PERIOD, period);
    xreg_setw(AUD0_VOL, 0x8080);                         // full volume l+r
    xreg_setw(AUD0_LENGTH, VRAM_SILENCE_LEN - 1);        // 1 word length (-1)
    xreg_setw(AUD0_START, VRAM_AUDIO_BASE);              // sample start

    xreg_setw(AUD1_PERIOD, period);
    xreg_setw(AUD1_VOL, 0x8080);                         // full volume l+r
    xreg_setw(AUD1_LENGTH, VRAM_SILENCE_LEN - 1);        // 1 word length (-1)
    xreg_setw(AUD1_START, VRAM_AUDIO_BASE);              // sample start

    xreg_setw(AUD2_PERIOD, period);
    xreg_setw(AUD2_VOL, 0x8080);                         // full volume l+r
    xreg_setw(AUD2_LENGTH, VRAM_SILENCE_LEN - 1);        // 1 word length (-1)
    xreg_setw(AUD2_START, VRAM_AUDIO_BASE);              // sample start

    xreg_setw(AUD3_PERIOD, period);
    xreg_setw(AUD3_VOL, 0x8080);                         // full volume l+r
    xreg_setw(AUD3_LENGTH, VRAM_SILENCE_LEN - 1);        // 1 word length (-1)
    xreg_setw(AUD3_START, VRAM_AUDIO_BASE);              // sample start

    xreg_setw(AUD_CTRL, MAKE_AUD_CTRL(1));        // enable audio (and leave on)
}

void play_audio(uint16_t pos_x)
{
    xv_prep();

    uint16_t wordsize = ((_binary_Boing_raw_end - _binary_Boing_raw_start) / 2) - 1;
    uint16_t rate     = 8000 + ((xm_getw(TIMER) & 0xfff) - 0x800);        // randomize rate a bit
    uint16_t period   = (clk_hz + (rate / 2)) / rate;
    int16_t  rv       = (pos_x - 320) / 2;
    int16_t  lv       = (320 - pos_x) / 2;
    if (rv < 0)
        rv = 0;
    else if (rv > 128)
        rv = 128;
    if (lv < 0)
        lv = 0;
    else if (lv > 128)
        lv = 128;
    uint16_t wvol = ((64 + lv) << 8) | (64 + rv);

    static uint16_t chan;
    uint16_t        vo = (chan << 2);
    chan               = (chan + 1) & 0x3;
    xreg_setw(AUD0_VOL + vo, wvol);
    xreg_setw_next(/*AUD0_PERIOD + vo , */ period);
    xreg_setw_next(/*(AUD0_LENGTH + vo, */ wordsize);
    xreg_setw_next(/*(AUD0_START + vo,  */ VRAM_AUDIO_BASE);
    xreg_setw(AUD0_PERIOD + vo, period | 0x8000);        // force new sound start immediately
    xreg_setw(AUD0_START + vo, VRAM_AUDIO_BASE + VRAM_SILENCE_LEN);
    xreg_setw(AUD0_LENGTH + vo, VRAM_SILENCE_LEN - 1);        // queue silence sample to play next
}
#endif

#define BITS_PER_PIXEL_1BPP 1
#define BITS_PER_PIXEL_4BPP 4
#define BITS_PER_PIXEL_8BPP 8

#define PIXELS_PER_WORD_1BPP 8
#define PIXELS_PER_WORD_4BPP 4
#define PIXELS_PER_WORD_8BPP 2

void vram_write_bitmap_1bpp(int      width,
                            int      height,
                            uint8_t  bitmap[height][width],
                            uint16_t line_len,
                            uint16_t base,
                            uint8_t  colours)
{
    xv_prep();

    int width_words = width / PIXELS_PER_WORD_1BPP;

    for (uint16_t row = 0; row < height; ++row)
    {
        const uint16_t row_base = base + row * line_len;
        vram_setw_addr_incr(row_base, 0x0001);

        for (uint16_t word = 0; word < width_words; ++word)
        {
            uint16_t val = 0;
            for (int pixel = 0; pixel < PIXELS_PER_WORD_1BPP; ++pixel)
            {
                val <<= BITS_PER_PIXEL_1BPP;
                val |= bitmap[row][word * PIXELS_PER_WORD_1BPP + pixel] & 0x01;
            }

            vram_setw_next((uint16_t)colours << 8 | val);
        }
    }
}

void vram_write_bitmap_4bpp(int width, int height, uint8_t bitmap[height][width], uint16_t line_len, uint16_t base)
{
    xv_prep();

    int width_words = width / PIXELS_PER_WORD_4BPP;

    for (uint16_t row = 0; row < height; ++row)
    {
        const uint16_t row_base = base + row * line_len;
        vram_setw_addr_incr(row_base, 0x0001);

        for (uint16_t word = 0; word < width_words; ++word)
        {
            uint16_t val = 0;
            for (int pixel = 0; pixel < PIXELS_PER_WORD_4BPP; ++pixel)
            {
                val <<= BITS_PER_PIXEL_4BPP;
                val |= bitmap[row][word * PIXELS_PER_WORD_4BPP + pixel] & 0x0F;
            }

            vram_setw_next(val);
        }
    }
}

void vram_write_bitmap_8bpp(int width, int height, uint8_t bitmap[height][width], uint16_t line_len, uint16_t base)
{
    xv_prep();

    int width_words = width / PIXELS_PER_WORD_8BPP;

    for (uint16_t row = 0; row < height; ++row)
    {
        const uint16_t row_base = base + row * line_len;
        vram_setw_addr_incr(row_base, 0x0001);

        for (uint16_t word = 0; word < width_words; ++word)
        {
            uint16_t val = 0;
            for (int pixel = 0; pixel < PIXELS_PER_WORD_8BPP; ++pixel)
            {
                val <<= BITS_PER_PIXEL_8BPP;
                val |= bitmap[row][word * PIXELS_PER_WORD_8BPP + pixel] & 0xFF;
            }

            vram_setw_next(val);
        }
    }
}

void vram_write_tiled(int      width_tiles,
                      int      height_tiles,
                      uint16_t tilemap[height_tiles][width_tiles],
                      uint16_t line_len,
                      uint16_t base)
{
    xv_prep();

    for (uint16_t row = 0; row < height_tiles; ++row)
    {
        const uint16_t row_base = base + row * line_len;
        vram_setw_addr_incr(row_base, 0x0001);

        for (uint16_t col = 0; col < width_tiles; ++col)
        {
            vram_setw_next(tilemap[row][col]);
        }
    }
}

void vram_fill_bitmap_1bpp(int width, int height, uint8_t colour, uint16_t line_len, uint16_t base, uint8_t colours)
{
    xv_prep();

    int width_words = width / PIXELS_PER_WORD_1BPP;

    for (uint16_t row = 0; row < height; ++row)
    {
        const uint16_t row_base = base + row * line_len;
        vram_setw_addr_incr(row_base, 0x0001);

        for (uint16_t word = 0; word < width_words; ++word)
        {
            uint16_t val = 0;
            for (int pixel = 0; pixel < PIXELS_PER_WORD_1BPP; ++pixel)
            {
                val <<= BITS_PER_PIXEL_1BPP;
                val |= colour & 0x01;
            }

            vram_setw_next((uint16_t)colours << 8 | val);
        }
    }
}

void vram_fill_bitmap_4bpp(int width, int height, uint8_t colour, uint16_t line_len, uint16_t base)
{
    xv_prep();

    int width_words = width / PIXELS_PER_WORD_4BPP;

    for (uint16_t row = 0; row < height; ++row)
    {
        const uint16_t row_base = base + row * line_len;
        vram_setw_addr_incr(row_base, 0x0001);

        for (uint16_t word = 0; word < width_words; ++word)
        {
            uint16_t val = 0;
            for (int pixel = 0; pixel < PIXELS_PER_WORD_4BPP; ++pixel)
            {
                val <<= BITS_PER_PIXEL_4BPP;
                val |= colour & 0x0F;
            }

            vram_setw_next(val);
        }
    }
}

void vram_fill_bitmap_8bpp(int width, int height, uint8_t colour, uint16_t line_len, uint16_t base)
{
    xv_prep();

    int width_words = width / PIXELS_PER_WORD_8BPP;

    for (uint16_t row = 0; row < height; ++row)
    {
        const uint16_t row_base = base + row * line_len;
        vram_setw_addr_incr(row_base, 0x0001);

        for (uint16_t word = 0; word < width_words; ++word)
        {
            uint16_t val = 0;
            for (int pixel = 0; pixel < PIXELS_PER_WORD_8BPP; ++pixel)
            {
                val <<= BITS_PER_PIXEL_8BPP;
                val |= colour & 0xFF;
            }

            vram_setw_next(val);
        }
    }
}

void vram_fill_tiled(int width_tiles, int height_tiles, uint16_t tile, uint16_t line_len, uint16_t base)
{
    xv_prep();

    for (uint16_t row = 0; row < height_tiles; ++row)
    {
        const uint16_t row_base = base + row * line_len;
        vram_setw_addr_incr(row_base, 0x0001);

        for (uint16_t col = 0; col < width_tiles; ++col)
        {
            vram_setw_next(tile);
        }
    }
}

void vram_sequence_tiled(int      width_tiles,
                         int      height_tiles,
                         uint16_t tile_start,
                         uint16_t tile_incr_row,
                         uint16_t tile_incr_col,
                         uint16_t line_len,
                         uint16_t base)
{
    xv_prep();

    for (uint16_t row = 0; row < height_tiles; ++row)
    {
        const uint16_t row_base = base + row * line_len;
        vram_setw_addr_incr(row_base, 0x0001);

        for (uint16_t col = 0; col < width_tiles; ++col)
        {
            vram_setw_next(tile_start);

            tile_start += tile_incr_col;
        }

        tile_start += tile_incr_row;
    }
}

void copper_load_list(uint16_t length, uint16_t list[length], uint16_t base)
{
    xv_prep();

    xmem_setw_next_addr(base);
    for (uint16_t i = 0; i < length; ++i)
    {
        xmem_setw_next_wait(list[i]);
    }
}

void xosera_boing()
{
    xv_prep();

    dprintf("Xosera boing\n");

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

    printf("\rXoboing: Copyright (c) 2022 Thomas Jager - Preparing assets, one moment...");
    dprintf("Xoboing: Copyright (c) 2022 Thomas Jager - Preparing assets, one moment...\n");

    draw_bg();
    fill_ball();

    // initialize Xosera
    xosera_init(xosera_cur_config());
    vid_hsize = xosera_vid_width();
    clk_hz    = xosera_sample_hz();
    // set border color to black
    xreg_setw(VID_CTRL, MAKE_VID_CTRL(0, 0x00));
    // set screen width to 640 (adjusting LEFT and RIGHT margins if in 848 mode)
    if (vid_hsize > 640)
    {
        xreg_setw(VID_LEFT, (vid_hsize - 640) / 2);
        xreg_setw(VID_RIGHT, vid_hsize - (vid_hsize - 640) / 2);
    }

    shadow_ball();
    do_tiles();

    // set playfield A display address to VRAM 0x0000
    xreg_setw(PA_DISP_ADDR, 0);

    xreg_setw(PA_GFX_CTRL, MAKE_GFX_CTRL(0x00, 1, GFX_1_BPP, 1, 1, 1));
    xreg_setw(PB_GFX_CTRL, MAKE_GFX_CTRL(0x00, 1, GFX_1_BPP, 1, 1, 1));

#if USE_AUDIO
    upload_audio();
#endif

#if USE_BLIT_METHOD == USE_COPASM
    dprintf("Using CopAsm COPPER blit version\n");
    copper_load_list(boing_copper_size, boing_copper_bin, boing_copper_start);
#elif USE_BLIT_METHOD == USE_COPMACROS
    dprintf("Using macro COPPER blit version\n");
    copper_load_list(NUM_ELEMENTS(copper_list), copper_list, XR_COPPER_ADDR);
#elif USE_BLIT_METHOD == USE_CPU_BLIT
    dprintf("Using CPU based blit version\n");
#endif

    // PA Colours:
    xmem_setw_wait(XR_COLOR_A_ADDR + 0x0, 0x0BBB);        // Grey
    xmem_setw_wait(XR_COLOR_A_ADDR + 0x1, 0x0B0B);        // Purple

    // PB Colours:
    if (PAINT_BALL)
    {
        for (uint16_t palette_index = 0; palette_index < 14; ++palette_index)
        {
            uint8_t  colour_base  = palette_index * 16;
            uint16_t palette_base = XR_COLOR_B_ADDR + colour_base;
            xmem_setw_wait(palette_base + 0, 0x0000);        // Transparent Black
            xmem_setw_wait(palette_base + 1, 0x6000);        // Translucent Black
            for (uint16_t colour = 0; colour < 7; ++colour)
            {
                xmem_setw_wait(palette_base + 2 + (palette_index + colour) % 14, 0xFFFF);        // White
            }
            for (uint16_t colour = 7; colour < 14; ++colour)
            {
                xmem_setw_wait(palette_base + 2 + (palette_index + colour) % 14, 0xFF00);        // Red
            }
        }
    }
    else
    {
        xmem_setw_wait(XR_COLOR_B_ADDR + 0x2, 0xF000);        // Black
        xmem_setw_wait(XR_COLOR_B_ADDR + 0x3, 0xFFFF);        // White
        xmem_setw_wait(XR_COLOR_B_ADDR + 0x4, 0xFF00);        // Red
        xmem_setw_wait(XR_COLOR_B_ADDR + 0x5, 0xFF70);        // Orange
        xmem_setw_wait(XR_COLOR_B_ADDR + 0x6, 0xFFF0);        // Yellow
        xmem_setw_wait(XR_COLOR_B_ADDR + 0x7, 0xF7F0);        // Spring Green
        xmem_setw_wait(XR_COLOR_B_ADDR + 0x8, 0xF0F0);        // Green
        xmem_setw_wait(XR_COLOR_B_ADDR + 0x9, 0xF0F7);        // Turquoise
        xmem_setw_wait(XR_COLOR_B_ADDR + 0xA, 0xF0FF);        // Cyan
        xmem_setw_wait(XR_COLOR_B_ADDR + 0xB, 0xF07F);        // Ocean
        xmem_setw_wait(XR_COLOR_B_ADDR + 0xC, 0xF00F);        // Blue
        xmem_setw_wait(XR_COLOR_B_ADDR + 0xD, 0xF70F);        // Violet
        xmem_setw_wait(XR_COLOR_B_ADDR + 0xE, 0xFF0F);        // Magenta
        xmem_setw_wait(XR_COLOR_B_ADDR + 0xF, 0xFF07);        // Raspberry
    }

    xreg_setw(PA_LINE_LEN, WIDTH_WORDS_A);
    xreg_setw(PB_LINE_LEN, WIDTH_WORDS_B);

    // Load PA bitmap
    vram_write_bitmap_1bpp(WIDTH_A, HEIGHT_A, bg_bitmap, WIDTH_WORDS_A, VRAM_BASE_A, 0x01);

    // Load PB tiles
    vram_setw_addr_incr(TILE_BASE_B, 0x0001);
    for (uint16_t i = 0; i < 0x4000; ++i)
    {
        vram_setw_next(((uint16_t *)ball_tiles)[i]);
    }

    // Load PB tilemap
    // Two extra rows needed for fine scrolling, one for the bottom row, and one for the bottom right tile
    vram_fill_tiled(WIDTH_WORDS_B, HEIGHT_WORDS_B + 2, 0, WIDTH_WORDS_B, VRAM_BASE_B);

    // Load blank tilemap
    vram_fill_tiled(BALL_TILES_WIDTH, BALL_TILES_HEIGHT, 0, BALL_TILES_WIDTH, VRAM_BASE_BLANK);

    // Load ball tilemap
    vram_sequence_tiled(BALL_TILES_WIDTH, BALL_TILES_HEIGHT, 0, 0, 1, BALL_TILES_WIDTH, VRAM_BASE_BALL);

    float pos_x   = 320.0f;
    float pos_y   = 320.0f;
    float vel_x   = 128.0f;
    float vel_y   = 0.0f;
    float acc_x   = 0.0f;
    float acc_y   = -512.0f;
    float pos_phi = 0.0f;
    float vel_phi = 2.0f;

    xwait_vblank();
    xreg_setw(PA_GFX_CTRL, MAKE_GFX_CTRL(0x00, 0, GFX_1_BPP, 1, 1, 1));
    xreg_setw(PA_DISP_ADDR, VRAM_BASE_A);

    xreg_setw(PB_GFX_CTRL, MAKE_GFX_CTRL(0x00, 0, GFX_4_BPP, 0, 0, 0));
    xreg_setw(PB_TILE_CTRL, MAKE_TILE_CTRL(TILE_BASE_B, 0, 1, TILE_HEIGHT_B));
    xreg_setw(PB_DISP_ADDR, VRAM_BASE_B);

#if USE_BLIT_METHOD == USE_COPASM
    xreg_setw(COPP_CTRL, MAKE_COPP_CTRL(1));        // enable
#elif USE_BLIT_METHOD == USE_COPMACROS
    xreg_setw(COPP_CTRL, MAKE_COPP_CTRL(1));        // enable
#elif USE_BLIT_METHOD == USE_CPU_BLIT
    xreg_setw(COPP_CTRL, MAKE_COPP_CTRL(0));        // disable
#endif

    static uint16_t prev_timer;
    prev_timer = xm_getw(TIMER);
#if (USE_BLIT_METHOD == USE_COPASM) || (USE_BLIT_METHOD == USE_COPMACROS)
    uint16_t last_frame = ~0;
#endif
    while (!checkchar())
    {
        uint16_t timer = xm_getw(TIMER);
        float    dt    = (uint16_t)(timer - prev_timer) / 10000.0f;
        prev_timer     = timer;

        // Half-step position
        pos_x += vel_x / 2.0f * dt;
        pos_y += vel_y / 2.0f * dt;
        // Half-step velocity
        vel_x += acc_x / 2.0f * dt;
        vel_y += acc_y / 2.0f * dt;
        // Bounce on walls
        if (pos_x < WALL_LEFT * 2 + BALL_RADIUS || pos_x >= WALL_RIGHT * 2 - BALL_RADIUS)
        {
            vel_x = -vel_x;
#if USE_AUDIO
            play_audio(roundf(pos_x));
#endif
        }
        if (pos_y < WALL_BOTTOM * 2 + BALL_RADIUS || pos_y >= WALL_TOP * 2 - BALL_RADIUS)
        {
            vel_y = -vel_y;
#if USE_AUDIO
            play_audio(roundf(pos_x));
#endif
        }
        // Half-step velocity
        vel_x += acc_x / 2.0f * dt;
        vel_y += acc_y / 2.0f * dt;
        // Half-step position
        pos_x += vel_x / 2.0f * dt;
        pos_y += vel_y / 2.0f * dt;

        int pos_x_int = roundf(pos_x);
        int pos_y_int = roundf(pos_y);

        // Half-step angular position
        pos_phi += vel_phi / 2.0f * dt;
        // Update angular velocity
        vel_phi = copysignf(vel_phi, vel_x);
        // Half-step angular position
        pos_phi += vel_phi / 2.0f * dt;
        // Wrap at full rotations
        pos_phi = fmodf(pos_phi, TAU);
        if (pos_phi < 0.0f)
        {
            pos_phi += TAU;
        }

        const float colour_cycle_angle = BALL_PHI_STEP * 2;
        float       angle_in_cycle     = fmodf(pos_phi / colour_cycle_angle, 1.0f);
        uint8_t     palatte_index      = angle_in_cycle * 14;
        uint8_t     colour_base        = palatte_index * 16;

#if USE_BLIT_METHOD == USE_COPASM
        uint16_t this_frame;
        do
        {
            this_frame = xmem_getw_wait(XR_COPPER_ADDR + boing_copper__frame_count);
        } while (this_frame == last_frame);
        last_frame = this_frame;
#elif USE_BLIT_METHOD == USE_COPMACROS
        uint16_t this_frame;
        do
        {
            this_frame = xmem_getw_wait(XR_COPPER_ADDR + cop_frame_count);
        } while (this_frame == last_frame);
        last_frame = this_frame;
#elif USE_BLIT_METHOD == USE_CPU_BLIT
        while (xreg_getw(SCANLINE) < 479)
            ;
#endif
        if (PAINT_BALL)
        {
            set_ball_colour(colour_base);
        }
        draw_ball_at(WIDTH_WORDS_B, HEIGHT_WORDS_B, pos_x_int, pos_y_int);
    }
    readchar();
    xwait_not_vblank();
    xwait_vblank();
    xreg_setw(VID_CTRL, MAKE_VID_CTRL(0, 0x08));
    xreg_setw(AUD_CTRL, MAKE_AUD_CTRL(0));
    xreg_setw(COPP_CTRL, MAKE_COPP_CTRL(0));

    xosera_xansi_restore();
    dprintf("Exit\n");
}
