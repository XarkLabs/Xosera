#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <basicio.h>

#include "xosera_m68k_api.h"

#define USE_AUDIO 1

#if USE_AUDIO
extern char _binary_Boing_raw_start[];
extern char _binary_Boing_raw_end[];
#endif

const uint16_t vram_audio_base = 0x0080;
const uint16_t vram_base_a     = 0x4000;
const uint16_t vram_base_b     = 0xA000;
const uint16_t vram_base_blank = 0xB800;
const uint16_t vram_base_ball  = 0xBC00;
const uint16_t tile_base_b     = 0xC000;

#define PI  3.1415926f
#define PAU (1.5f * PI)
#define TAU (2.0f * PI)

#define WIDTH  640
#define HEIGHT 480

#define WIDTH_A  (WIDTH / 2)
#define HEIGHT_A (HEIGHT / 2)
#define WIDTH_B  (WIDTH / 1)
#define HEIGHT_B (HEIGHT / 1)

#define TILE_WIDTH_B  8
#define TILE_HEIGHT_B 8

#define PIXELS_PER_WORD_A 8
#define PIXELS_PER_WORD_B 4

#define COLS_PER_WORD_A 8
#define ROWS_PER_WORD_A 1
#define COLS_PER_WORD_B TILE_WIDTH_B
#define ROWS_PER_WORD_B TILE_HEIGHT_B

#define WIDTH_WORDS_A  (WIDTH_A / COLS_PER_WORD_A)
#define HEIGHT_WORDS_A (HEIGHT_A / ROWS_PER_WORD_A)
#define WIDTH_WORDS_B  (WIDTH_B / COLS_PER_WORD_B)
#define HEIGHT_WORDS_B (HEIGHT_B / ROWS_PER_WORD_B)

#define BALL_BITMAP_WIDTH  256
#define BALL_BITMAP_HEIGHT 256

#define BALL_TILES_WIDTH  (BALL_BITMAP_WIDTH / TILE_WIDTH_B)
#define BALL_TILES_HEIGHT (BALL_BITMAP_WIDTH / TILE_HEIGHT_B)

#define SHADOW_OFFSET_X 36
#define SHADOW_OFFSET_Y 0

#define BALL_SHIFT_X (-SHADOW_OFFSET_X / 2)
#define BALL_SHIFT_Y (-SHADOW_OFFSET_Y / 2)

#define BALL_CENTER_X (BALL_BITMAP_WIDTH / 2 + BALL_SHIFT_X)
#define BALL_CENTER_Y (BALL_BITMAP_HEIGHT / 2 + BALL_SHIFT_Y)

#define BALL_RADIUS 80

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

#define WIDESCREEN true
#define PAINT_BALL true
#define USE_COPPER true

uint8_t  bg_bitmap[HEIGHT_A][WIDTH_A]                                                                     = {0};
uint8_t  ball_bitmap[BALL_BITMAP_HEIGHT][BALL_BITMAP_WIDTH]                                               = {0};
uint16_t ball_tiles[BALL_TILES_HEIGHT][BALL_TILES_WIDTH][TILE_HEIGHT_B][TILE_WIDTH_B / PIXELS_PER_WORD_B] = {0};

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

static inline void wait_vsync()
{
    while (xreg_getw(SCANLINE) < 0x8000)
        ;
}

static inline void wait_vsync_start()
{
    while (xreg_getw(SCANLINE) >= 0x8000)
        ;
    wait_vsync();
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

    int            top_left_row = (top_left_y + TILE_HEIGHT_B - 1) / TILE_WIDTH_B;
    int            top_left_col = (top_left_x + TILE_WIDTH_B - 1) / TILE_WIDTH_B;
    const uint16_t dst          = vram_base_b + top_left_row * width_words + top_left_col;

    int scroll_x = -(top_left_x - top_left_col * TILE_WIDTH_B);
    int scroll_y = -(top_left_y - top_left_row * TILE_HEIGHT_B);

#if USE_COPPER
    xmem_setw(XR_COPPER_ADDR + (0x0000 << 1 | 0x1), dst);
    xmem_setw(XR_COPPER_ADDR + (0x0001 << 1 | 0x1), MAKE_HV_SCROLL(scroll_x, scroll_y << 2));
#else
    static int     prev_top_left_row;
    static int     prev_top_left_col;
    const uint16_t prev_dst = vram_base_b + prev_top_left_row * width_words + prev_top_left_col;

    xreg_setw(BLIT_CTRL,
              XB_(0x00, 8, 8) | XB_(0, 5, 1) | XB_(0, 4, 1) | XB_(0, 3, 1) | XB_(0, 2, 1) | XB_(1, 1, 1) |
                  XB_(0, 0, 1));
    xreg_setw(BLIT_MOD_A, 0x0000);
    xreg_setw(BLIT_MOD_B, 0x0000);
    xreg_setw(BLIT_SRC_B, 0xFFFF);
    xreg_setw(BLIT_MOD_C, 0x0000);
    xreg_setw(BLIT_VAL_C, 0x0000);
    xreg_setw(BLIT_MOD_D, width_words - BALL_TILES_WIDTH);
    xreg_setw(BLIT_SHIFT, XB_(0xF, 12, 4) | XB_(0xF, 8, 4) | XB_(0, 0, 2));
    xreg_setw(BLIT_LINES, BALL_TILES_HEIGHT - 1);

    while (xm_getw(SYS_CTRL) & XB_(1, 5, 1))
    {
        // Wait while blitter queue is full
    }
    xreg_setw(BLIT_SRC_A, vram_base_blank);
    xreg_setw(BLIT_DST_D, prev_dst);
    xreg_setw(BLIT_WORDS, BALL_TILES_WIDTH - 1);        // Starts operation

    while (xm_getw(SYS_CTRL) & XB_(1, 5, 1))
    {
        // Wait while blitter queue is full
    }
    xreg_setw(BLIT_SRC_A, vram_base_ball);
    xreg_setw(BLIT_DST_D, dst);
    xreg_setw(BLIT_WORDS, BALL_TILES_WIDTH - 1);        // Starts operation

    xreg_setw(PB_HV_SCROLL, MAKE_HV_SCROLL(scroll_x, scroll_y << 2));

    top_left_col = prev_top_left_col;
    top_left_row = prev_top_left_row;
#endif
}

void set_ball_colour(uint8_t colour_base)
{
    uint16_t gfx_ctrl = MAKE_GFX_CTRL(colour_base, 0, XR_GFX_BPP_4, 0, 0, 0);
#if USE_COPPER
    xmem_setw(XR_COPPER_ADDR + (0x0002 << 1 | 0x1), gfx_ctrl);
#else
    xreg_setw(PB_GFX_CTRL, gfx_ctrl);
#endif
}

#if USE_COPPER
uint32_t copper_list[] = {
    [0x0000] = COP_MOVEC(vram_base_b, 0x0041 << 1 | 0x1),        // Fill dst
    [0x0001] = COP_MOVEC(0, 0x0106 << 1 | 0x1),                  // Fill scroll
    [0x0002] = COP_MOVEC(MAKE_GFX_CTRL(0x00, 0, XR_GFX_BPP_4, 0, 0, 0),
                         0x0107 << 1 | 0x1),        // Fill gfx_ctrl with colorbase
    COP_JUMP(0x0040 << 1),

    [0x0040] =
        COP_MOVEC(COP_MOVEC(0, 0x0104 << 1 | 0x1) >> 16, 0x0041 << 1 | 0x0),        // Make following move point to dst
    [0x0041] = COP_MOVEC(0, 0),
    COP_MOVEC(COP_MOVEC(0, 0x0101 << 1 | 0x1) >> 16, 0x0041 << 1 | 0x0),        // Make preceding move point to prev_dst

    [0x0043] = COP_JUMP(0x0080 << 1),        // Jumps either to blitter load or to wait_f
    [0x0044] =
        COP_MOVEC(COP_JUMP(0x0080 << 1) >> 16, 0x0043 << 1 | 0x0),        // Make branching jump go to 0x0080 << 1
    COP_WAIT_F(),

    [0x0080] =
        COP_MOVEC(COP_JUMP(0x0044 << 1) >> 16, 0x0043 << 1 | 0x0),        // Make branching jump go to 0x0044 << 1
    COP_JUMP(0x00C0 << 1),

    // Load fixed blitter settings
    [0x00C0] = COP_MOVER(XB_(0x00, 8, 8) | XB_(0, 5, 1) | XB_(0, 4, 1) | XB_(0, 3, 1) | XB_(0, 2, 1) | XB_(1, 1, 1) |
                             XB_(0, 0, 1),
                         BLIT_CTRL),
    COP_MOVER(0x0000, BLIT_MOD_A),
    COP_MOVER(0x0000, BLIT_MOD_B),
    COP_MOVER(0xFFFF, BLIT_SRC_B),
    COP_MOVER(0x0000, BLIT_MOD_C),
    COP_MOVER(0x0000, BLIT_VAL_C),
    COP_MOVER(WIDTH_WORDS_B - BALL_TILES_WIDTH, BLIT_MOD_D),
    COP_MOVER(XB_(0xF, 12, 4) | XB_(0xF, 8, 4) | XB_(0, 0, 2), BLIT_SHIFT),
    COP_MOVER(BALL_TILES_HEIGHT - 1, BLIT_LINES),

    COP_WAIT_V(480),
    COP_JUMP(0x0100 << 1),

    // Blank existing ball
    [0x0100] = COP_MOVER(vram_base_blank, BLIT_SRC_A),
    [0x0101] = COP_MOVER(vram_base_b, BLIT_DST_D),        // Fill in prev_dst
    [0x0102] = COP_MOVER(BALL_TILES_WIDTH - 1, BLIT_WORDS),

    // Draw ball
    [0x0103] = COP_MOVER(vram_base_ball, BLIT_SRC_A),
    [0x0104] = COP_MOVER(0, BLIT_DST_D),        // Fill in dst
    [0x0105] = COP_MOVER(BALL_TILES_WIDTH - 1, BLIT_WORDS),

    [0x0106] = COP_MOVER(0, PB_HV_SCROLL),        // Fill scroll

    [0x0107] = COP_MOVER(0, PB_GFX_CTRL),        // Fill gfx_ctrl with colorbase

    COP_JUMP(0x0003 << 1),
};
#endif

#if USE_AUDIO
static inline void wait_scanline()
{
    uint16_t l = xreg_getw(SCANLINE) & 0x7fff;
    while (l == (xreg_getw(SCANLINE) & 0x7fff))
        ;
    l = xreg_getw(SCANLINE) & 0x7fff;
    while (l == (xreg_getw(SCANLINE) & 0x7fff))
        ;
    l = xreg_getw(SCANLINE) & 0x7fff;
    while (l == (xreg_getw(SCANLINE) & 0x7fff))
        ;
    l = xreg_getw(SCANLINE) & 0x7fff;
    while (l == (xreg_getw(SCANLINE) & 0x7fff))
        ;
}

void play_audio()
{
    xreg_setw(AUD0_START, vram_audio_base);
    xreg_setw(AUD0_LENGTH, 0);
    wait_scanline();
    xreg_setw(VID_CTRL, 0x0000);
    wait_scanline();
    xreg_setw(AUD0_START, vram_audio_base + 1);
    uint16_t bytesize = (_binary_Boing_raw_end - _binary_Boing_raw_start) / 2;
    xreg_setw(AUD0_LENGTH, bytesize);

    uint32_t clk_hz = xreg_getw(VID_HSIZE) > 640 ? 33750000 : 25125000;
    uint16_t rate   = 8000 - 256 + (xm_getw(TIMER) & 0x1ff);
    uint16_t period = (clk_hz + rate - 1) / rate;
    xreg_setw(AUD0_PERIOD, period);

    wait_scanline();
    xreg_setw(VID_CTRL, 0x0010);
    wait_scanline();
    xreg_setw(AUD0_START, vram_audio_base);
    xreg_setw(AUD0_LENGTH, 0);
}

void upload_audio()
{
    xm_setw(WR_ADDR, vram_audio_base);
    xm_setw(WR_INCR, 0x0001);
    xm_setw(DATA, 0x0000);        // zero = silence sample
    for (uint16_t * wp = (uint16_t *)_binary_Boing_raw_start; wp < (uint16_t *)_binary_Boing_raw_end; wp++)
    {
        xm_setw(DATA, *wp);
    }

    uint32_t clk_hz = xreg_getw(VID_HSIZE) > 640 ? 33750000 : 25125000;
    uint16_t rate   = 8000;
    uint16_t period = (clk_hz + rate - 1) / rate;
    xreg_setw(AUD0_PERIOD, period);
    xreg_setw(AUD0_START, vram_audio_base);
    xreg_setw(AUD0_LENGTH, 0);
    xreg_setw(VID_CTRL, 0x0010);
    xreg_setw(AUD0_VOL, 0x8080);
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
    int width_words = width / PIXELS_PER_WORD_1BPP;

    for (uint16_t row = 0; row < height; ++row)
    {
        const uint16_t row_base = base + row * line_len;
        xm_setw(WR_ADDR, row_base);

        for (uint16_t word = 0; word < width_words; ++word)
        {
            uint16_t val = 0;
            for (int pixel = 0; pixel < PIXELS_PER_WORD_1BPP; ++pixel)
            {
                val <<= BITS_PER_PIXEL_1BPP;
                val |= bitmap[row][word * PIXELS_PER_WORD_1BPP + pixel] & 0x01;
            }

            xm_setw(DATA, (uint16_t)colours << 8 | val);
        }
    }
}

void vram_write_bitmap_4bpp(int width, int height, uint8_t bitmap[height][width], uint16_t line_len, uint16_t base)
{
    int width_words = width / PIXELS_PER_WORD_4BPP;

    for (uint16_t row = 0; row < height; ++row)
    {
        const uint16_t row_base = base + row * line_len;
        xm_setw(WR_ADDR, row_base);

        for (uint16_t word = 0; word < width_words; ++word)
        {
            uint16_t val = 0;
            for (int pixel = 0; pixel < PIXELS_PER_WORD_4BPP; ++pixel)
            {
                val <<= BITS_PER_PIXEL_4BPP;
                val |= bitmap[row][word * PIXELS_PER_WORD_4BPP + pixel] & 0x0F;
            }

            xm_setw(DATA, val);
        }
    }
}

void vram_write_bitmap_8bpp(int width, int height, uint8_t bitmap[height][width], uint16_t line_len, uint16_t base)
{
    int width_words = width / PIXELS_PER_WORD_8BPP;

    for (uint16_t row = 0; row < height; ++row)
    {
        const uint16_t row_base = base + row * line_len;
        xm_setw(WR_ADDR, row_base);

        for (uint16_t word = 0; word < width_words; ++word)
        {
            uint16_t val = 0;
            for (int pixel = 0; pixel < PIXELS_PER_WORD_8BPP; ++pixel)
            {
                val <<= BITS_PER_PIXEL_8BPP;
                val |= bitmap[row][word * PIXELS_PER_WORD_8BPP + pixel] & 0xFF;
            }

            xm_setw(DATA, val);
        }
    }
}

void vram_write_tiled(int      width_tiles,
                      int      height_tiles,
                      uint16_t tilemap[height_tiles][width_tiles],
                      uint16_t line_len,
                      uint16_t base)
{
    for (uint16_t row = 0; row < height_tiles; ++row)
    {
        const uint16_t row_base = base + row * line_len;
        xm_setw(WR_ADDR, row_base);

        for (uint16_t col = 0; col < width_tiles; ++col)
        {
            xm_setw(DATA, tilemap[row][col]);
        }
    }
}

void vram_fill_bitmap_1bpp(int width, int height, uint8_t colour, uint16_t line_len, uint16_t base, uint8_t colours)
{
    int width_words = width / PIXELS_PER_WORD_1BPP;

    for (uint16_t row = 0; row < height; ++row)
    {
        const uint16_t row_base = base + row * line_len;
        xm_setw(WR_ADDR, row_base);

        for (uint16_t word = 0; word < width_words; ++word)
        {
            uint16_t val = 0;
            for (int pixel = 0; pixel < PIXELS_PER_WORD_1BPP; ++pixel)
            {
                val <<= BITS_PER_PIXEL_1BPP;
                val |= colour & 0x01;
            }

            xm_setw(DATA, (uint16_t)colours << 8 | val);
        }
    }
}

void vram_fill_bitmap_4bpp(int width, int height, uint8_t colour, uint16_t line_len, uint16_t base)
{
    int width_words = width / PIXELS_PER_WORD_4BPP;

    for (uint16_t row = 0; row < height; ++row)
    {
        const uint16_t row_base = base + row * line_len;
        xm_setw(WR_ADDR, row_base);

        for (uint16_t word = 0; word < width_words; ++word)
        {
            uint16_t val = 0;
            for (int pixel = 0; pixel < PIXELS_PER_WORD_4BPP; ++pixel)
            {
                val <<= BITS_PER_PIXEL_4BPP;
                val |= colour & 0x0F;
            }

            xm_setw(DATA, val);
        }
    }
}

void vram_fill_bitmap_8bpp(int width, int height, uint8_t colour, uint16_t line_len, uint16_t base)
{
    int width_words = width / PIXELS_PER_WORD_8BPP;

    for (uint16_t row = 0; row < height; ++row)
    {
        const uint16_t row_base = base + row * line_len;
        xm_setw(WR_ADDR, row_base);

        for (uint16_t word = 0; word < width_words; ++word)
        {
            uint16_t val = 0;
            for (int pixel = 0; pixel < PIXELS_PER_WORD_8BPP; ++pixel)
            {
                val <<= BITS_PER_PIXEL_8BPP;
                val |= colour & 0xFF;
            }

            xm_setw(DATA, val);
        }
    }
}

void vram_fill_tiled(int width_tiles, int height_tiles, uint16_t tile, uint16_t line_len, uint16_t base)
{
    for (uint16_t row = 0; row < height_tiles; ++row)
    {
        const uint16_t row_base = base + row * line_len;
        xm_setw(WR_ADDR, row_base);

        for (uint16_t col = 0; col < width_tiles; ++col)
        {
            xm_setw(DATA, tile);
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
    for (uint16_t row = 0; row < height_tiles; ++row)
    {
        const uint16_t row_base = base + row * line_len;
        xm_setw(WR_ADDR, row_base);

        for (uint16_t col = 0; col < width_tiles; ++col)
        {
            xm_setw(DATA, tile_start);

            tile_start += tile_incr_col;
        }

        tile_start += tile_incr_row;
    }
}

void copper_load_list(uint16_t length, uint32_t list[length], uint16_t base)
{
    for (uint16_t i = 0; i < length; ++i)
    {
        xmem_setw(base + i * 2 + 0, (list[i] >> 16) & 0xFFFF);
        xmem_setw(base + i * 2 + 1, (list[i] >> 0) & 0xFFFF);
    }
}

void xosera_boing()
{
    xosera_init(xreg_getw(VID_HSIZE) > 640 ? 1 : 0);
    printf("\033c\033[?25l");
    xreg_setw(PA_DISP_ADDR, 0);
    xreg_setw(VID_LEFT, (xreg_getw(VID_HSIZE) - 640) / 2);
    xreg_setw(VID_RIGHT, xreg_getw(VID_HSIZE) - (xreg_getw(VID_HSIZE) - 640) / 2);
    printf("Xoboing: Copyright (c) 2022 Thomas Jager - Preparing assets, one moment...");        // ANSI reset, disable
                                                                                                 // input cursor
    xreg_setw(VID_CTRL, MAKE_VID_CTRL(0x00, 0));

    draw_bg();
    fill_ball();
    shadow_ball();
    do_tiles();
#if USE_AUDIO
    upload_audio();
#endif

#if USE_COPPER
    copper_load_list(sizeof copper_list / sizeof copper_list[0], copper_list, XR_COPPER_ADDR);
#endif

    xreg_setw(PA_GFX_CTRL, MAKE_GFX_CTRL(0x00, 0, XR_GFX_BPP_1, 1, 1, 1));
    xreg_setw(PA_DISP_ADDR, vram_base_a);

    xreg_setw(PB_GFX_CTRL, MAKE_GFX_CTRL(0x00, 0, XR_GFX_BPP_4, 0, 0, 0));
    xreg_setw(PB_TILE_CTRL, MAKE_TILE_CTRL(tile_base_b, 0, 1, TILE_HEIGHT_B));
    xreg_setw(PB_DISP_ADDR, vram_base_b);

    // PA Colours:
    wait_vsync_start();
    xmem_setw(XR_COLOR_A_ADDR + 0x0, 0x0BBB);        // Grey
    xmem_setw(XR_COLOR_A_ADDR + 0x1, 0x0B0B);        // Purple

    // PB Colours:
    if (PAINT_BALL)
    {
        for (uint16_t palette_index = 0; palette_index < 14; ++palette_index)
        {
            uint8_t  colour_base  = palette_index * 16;
            uint16_t palette_base = XR_COLOR_B_ADDR + colour_base;
            wait_vsync();
            xmem_setw(palette_base + 0, 0x0000);        // Transparent Black
            wait_vsync();
            xmem_setw(palette_base + 1, 0x6000);        // Translucent Black
            for (uint16_t colour = 0; colour < 7; ++colour)
            {
                wait_vsync();
                xmem_setw(palette_base + 2 + (palette_index + colour) % 14, 0xFFFF);        // White
            }
            for (uint16_t colour = 7; colour < 14; ++colour)
            {
                wait_vsync();
                xmem_setw(palette_base + 2 + (palette_index + colour) % 14, 0xFF00);        // Red
            }
        }
    }
    else
    {
        wait_vsync();
        xmem_setw(XR_COLOR_B_ADDR + 0x2, 0xF000);        // Black
        wait_vsync();
        xmem_setw(XR_COLOR_B_ADDR + 0x3, 0xFFFF);        // White
        wait_vsync();
        xmem_setw(XR_COLOR_B_ADDR + 0x4, 0xFF00);        // Red
        wait_vsync();
        xmem_setw(XR_COLOR_B_ADDR + 0x5, 0xFF70);        // Orange
        wait_vsync();
        xmem_setw(XR_COLOR_B_ADDR + 0x6, 0xFFF0);        // Yellow
        wait_vsync();
        xmem_setw(XR_COLOR_B_ADDR + 0x7, 0xF7F0);        // Spring Green
        wait_vsync();
        xmem_setw(XR_COLOR_B_ADDR + 0x8, 0xF0F0);        // Green
        wait_vsync();
        xmem_setw(XR_COLOR_B_ADDR + 0x9, 0xF0F7);        // Turquoise
        wait_vsync();
        xmem_setw(XR_COLOR_B_ADDR + 0xA, 0xF0FF);        // Cyan
        wait_vsync();
        xmem_setw(XR_COLOR_B_ADDR + 0xB, 0xF07F);        // Ocean
        wait_vsync();
        xmem_setw(XR_COLOR_B_ADDR + 0xC, 0xF00F);        // Blue
        wait_vsync();
        xmem_setw(XR_COLOR_B_ADDR + 0xD, 0xF70F);        // Violet
        wait_vsync();
        xmem_setw(XR_COLOR_B_ADDR + 0xE, 0xFF0F);        // Magenta
        wait_vsync();
        xmem_setw(XR_COLOR_B_ADDR + 0xF, 0xFF07);        // Raspberry
    }

    xreg_setw(PA_LINE_LEN, WIDTH_WORDS_A);
    xreg_setw(PB_LINE_LEN, WIDTH_WORDS_B);

    xm_setw(WR_INCR, 1);

    // Load PA bitmap
    vram_write_bitmap_1bpp(WIDTH_A, HEIGHT_A, bg_bitmap, WIDTH_WORDS_A, vram_base_a, 0x01);

    // Load PB tiles
    xm_setw(WR_ADDR, tile_base_b);
    for (uint16_t i = 0; i < 0x4000; ++i)
    {
        xm_setw(DATA, ((uint16_t *)ball_tiles)[i]);
    }

    // Load PB tilemap
    // Two extra rows needed for fine scrolling, one for the bottom row, and one for the bottom right tile
    vram_fill_tiled(WIDTH_WORDS_B, HEIGHT_WORDS_B + 2, 0, WIDTH_WORDS_B, vram_base_b);

    // Load blank tilemap
    vram_fill_tiled(BALL_TILES_WIDTH, BALL_TILES_HEIGHT, 0, BALL_TILES_WIDTH, vram_base_blank);

    // Load ball tilemap
    vram_sequence_tiled(BALL_TILES_WIDTH, BALL_TILES_HEIGHT, 0, 0, 1, BALL_TILES_WIDTH, vram_base_ball);

    float pos_x   = 320.0f;
    float pos_y   = 320.0f;
    float vel_x   = 128.0f;
    float vel_y   = 0.0f;
    float acc_x   = 0.0f;
    float acc_y   = -512.0f;
    float pos_phi = 0.0f;
    float vel_phi = 2.0f;

    xreg_setw(COPP_CTRL, XB_(1, 15, 1) | XB_(0, 0, 11));

#if USE_AUDIO
    bool bounced = false;
#endif
    while (!checkchar())
    {
        static uint16_t prev_timer;
        uint16_t        timer = xm_getw(TIMER);
        float           dt    = (uint16_t)(timer - prev_timer) / 10000.0f;
        prev_timer            = timer;

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
            bounced = true;
#endif
        }
        if (pos_y < WALL_BOTTOM * 2 + BALL_RADIUS || pos_y >= WALL_TOP * 2 - BALL_RADIUS)
        {
            vel_y = -vel_y;
#if USE_AUDIO
            bounced = true;
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

#if !USE_COPPER
        while (!(xreg_getw(SCANLINE) & XB_(1, 15, 1)))
        {
            // Wait while not VBlank
        }
#endif

        draw_ball_at(WIDTH_WORDS_B, HEIGHT_WORDS_B, pos_x_int, pos_y_int);
        if (PAINT_BALL)
        {
            set_ball_colour(colour_base);
        }

#if USE_AUDIO
        if (bounced && (xreg_getw(SCANLINE) >= 0x8000))
        {
            play_audio();
            bounced = false;
        }
#endif

#if !USE_COPPER
        while (xreg_getw(SCANLINE) & XB_(1, 15, 1))
        {
            // Wait while VBlank
        }
#endif
    }
    readchar();
    wait_vsync_start();

    xreg_setw(VID_CTRL, 0x0800);
    xreg_setw(COPP_CTRL, 0x0000);        // disable copper
    xreg_setw(VID_LEFT, 0);
    xreg_setw(VID_RIGHT, xreg_getw(VID_HSIZE));
    xreg_setw(PA_GFX_CTRL, 0x0000);
    xreg_setw(PA_TILE_CTRL, 0x000F);
    xreg_setw(PA_DISP_ADDR, 0x0000);
    xreg_setw(PA_LINE_LEN, xreg_getw(VID_HSIZE) / 8);        // line len
    xreg_setw(PA_HV_SCROLL, 0x0000);
    xreg_setw(PA_HV_FSCALE, 0x0000);
    xreg_setw(PB_GFX_CTRL, 0x0080);

    printf("\033c");
}
