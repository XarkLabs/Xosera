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

#include <basicio.h>
#include <machine.h>
#include <sdfat.h>

// #define DELAY_TIME 15000        // slow human speed
// #define DELAY_TIME 5000        // human speed
#define DELAY_TIME 1000        // impatient human speed
// #define DELAY_TIME 500        // machine speed

#define COPPER_TEST            1
#define AUDIO_CHAINING_TEST    0
#define INTERACTIVE_AUDIO_TEST 0
#define BLURB_AUDIO            1

#define BLIT_TEST_PIC      0
#define TUT_PIC            1
#define SHUTTLE_PIC        2
#define TRUECOLOR_TEST_PIC 3
#define SELF_PIC           4

#include "xosera_m68k_api.h"

extern void install_intr(void);
extern void remove_intr(void);

extern volatile uint32_t XFrameCount;
extern volatile uint16_t NukeColor;

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


#if COPPER_TEST
// Copper list

uint16_t       cop_none_bin[] = {COP_VPOS(COP_V_EOF)};
const uint16_t cop_none_size  = NUM_ELEMENTS(cop_none_bin);

uint16_t       cop_gray_bin[] = {COP_VPOS(30 * 0),  COP_MOVER(0x000, COLOR_A_ADDR + 0),
                                 COP_VPOS(30 * 1),  COP_MOVER(0x111, COLOR_A_ADDR + 0),
                                 COP_VPOS(30 * 2),  COP_MOVER(0x222, COLOR_A_ADDR + 0),
                                 COP_VPOS(30 * 3),  COP_MOVER(0x333, COLOR_A_ADDR + 0),
                                 COP_VPOS(30 * 4),  COP_MOVER(0x444, COLOR_A_ADDR + 0),
                                 COP_VPOS(30 * 5),  COP_MOVER(0x555, COLOR_A_ADDR + 0),
                                 COP_VPOS(30 * 6),  COP_MOVER(0x666, COLOR_A_ADDR + 0),
                                 COP_VPOS(30 * 7),  COP_MOVER(0x777, COLOR_A_ADDR + 0),
                                 COP_VPOS(30 * 8),  COP_MOVER(0x888, COLOR_A_ADDR + 0),
                                 COP_VPOS(30 * 9),  COP_MOVER(0x999, COLOR_A_ADDR + 0),
                                 COP_VPOS(30 * 10), COP_MOVER(0xaaa, COLOR_A_ADDR + 0),
                                 COP_VPOS(30 * 11), COP_MOVER(0xbbb, COLOR_A_ADDR + 0),
                                 COP_VPOS(30 * 12), COP_MOVER(0xccc, COLOR_A_ADDR + 0),
                                 COP_VPOS(30 * 13), COP_MOVER(0xddd, COLOR_A_ADDR + 0),
                                 COP_VPOS(30 * 14), COP_MOVER(0xeee, COLOR_A_ADDR + 0),
                                 COP_VPOS(30 * 15), COP_MOVER(0xfff, COLOR_A_ADDR + 0),
                                 COP_VPOS(30 * 16), COP_END()};
const uint16_t cop_gray_size  = NUM_ELEMENTS(cop_gray_bin);

// 320x200 copper
// Copper list
uint16_t cop_320x200_bin[] = {
    COP_VPOS(40),                                          // Wait for line 40
    COP_MOVER(0x0065, PA_GFX_CTRL),                        // Set to 8-bpp + Hx2 + Vx2
    COP_MOVER(0x0065, PB_GFX_CTRL),                        // Set to 8-bpp + Hx2 + Vx2
    COP_VPOS(40 + 400),                                    // Wait for line 440
    COP_MOVER(0x00E5, PA_GFX_CTRL),                        // Set to Blank + 8-bpp + Hx2 + Vx2
    COP_MOVER(XR_TILE_ADDR + 0x1000, PB_LINE_ADDR),        // Set PB line address to tilemem address
    COP_MOVER(0xF009, PB_GFX_CTRL),                        // Set to Blank + 8-bpp + Hx2 + Vx2
    COP_MOVER(0x0E07, PB_TILE_CTRL),                       // Set to Blank + 8-bpp + Hx2 + Vx2
    COP_MOVER(28, PB_LINE_LEN),                            // Set PB line length
    COP_VPOS(480),                                         // Wait for offscreen
    COP_MOVER(320 / 2, PB_LINE_LEN),                       // Set PB line length
    COP_MOVER(0x000F, PB_TILE_CTRL),                       // set back to 8x16 tiles
    COP_MOVER(0x00E5, PA_GFX_CTRL),                        // Set to Blank + 8-bpp + Hx2 + Vx2
    COP_MOVER(0x00E5, PB_GFX_CTRL),                        // Set to Blank + 8-bpp + Hx2 + Vx2
    COP_END()                                              // wait until next frame
};
const uint16_t cop_320x200_size = NUM_ELEMENTS(cop_320x200_bin);

#include "cop_diagonal.h"
_Static_assert(NUM_ELEMENTS(cop_diagonal_bin) < (XR_COPPER_SIZE << 1), "copper list too long");

#include "cop_wavey.h"
_Static_assert(NUM_ELEMENTS(cop_wavey_bin) < XR_COPPER_SIZE, "copper list too long");

#include "cop_blend_test.h"
_Static_assert(NUM_ELEMENTS(cop_blend_test_bin) < XR_COPPER_SIZE, "copper list too long");

#define COP_FLAG_HPOS (1 << 0)
#define COP_FLAG_SINE (1 << 1)

struct copper_fx
{
    const char * name;
    uint16_t *   cop_data;
    uint16_t     cop_length;
    uint16_t     flags;
};
struct copper_fx cop_fx[] = {{"Wavey", cop_wavey_bin, cop_wavey_size, COP_FLAG_SINE},
                             {"None", cop_none_bin, cop_none_size, 0},
                             {"gray", cop_gray_bin, cop_gray_size, 0},
                             {"Diagonal", cop_diagonal_bin, cop_diagonal_size, COP_FLAG_HPOS},
                             {NULL, NULL, 0, 0}};

uint16_t           cur_cop_fx;
struct copper_fx * cop_fx_ptr;
#endif

// dummy global variable
uint32_t global;        // this is used to prevent the compiler from optimizing out tests

uint16_t cop_buffer[XR_COPPER_SIZE];

union
{
    uint8_t  u8[128 * 1024];
    uint16_t u16[64 * 1024];
    uint32_t u32[32 * 1024];
} buffer;

xosera_info_t initinfo;

// timer helpers
uint32_t elapsed_tenthms;        // Xosera elapsed timer
uint16_t last_timer_val;
uint32_t start_time;

static void update_elapsed()
{
    xv_prep();
    uint16_t new_timer_val = xm_getw(TIMER);
    uint16_t delta         = (uint16_t)(new_timer_val - last_timer_val);
    last_timer_val         = new_timer_val;
    elapsed_tenthms += delta;
}

void timer_start()
{
    update_elapsed();
    uint32_t check_time = elapsed_tenthms;
    do
    {
        update_elapsed();
        start_time = elapsed_tenthms;
    } while (start_time == check_time);
}

uint32_t timer_stop()
{
    update_elapsed();
    uint32_t elapsed_time = elapsed_tenthms - start_time;
    return elapsed_time;
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

static inline void wait_vblank_start()
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

_NOINLINE void restore_colors()
{
    xv_prep();

    wait_vblank_start();
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

_NOINLINE void restore_colors2(uint8_t alpha)
{
    xv_prep();

    wait_vblank_start();
    xmem_setw_next_addr(XR_COLOR_B_ADDR);
    uint16_t sa = (alpha & 0xf) << 12;
    for (uint16_t i = 0; i < 256; i++)
    {
        uint16_t w = i ? (sa | (def_colors[i] & 0xfff)) : 0;
        xmem_setw_next(w);
    };
}

// sets test blend palette
_NOINLINE void restore_colors3()
{
    xv_prep();

    wait_vblank_start();
    xmem_setw_next_addr(XR_COLOR_B_ADDR);
    for (uint16_t i = 0; i < 256; i++)
    {
        uint16_t w = i ? ((i & 0x3) << 14) | (def_colors[i] & 0xfff) : 0x0000;
        xmem_setw_next(w);
    };
}

_NOINLINE void dupe_colors(int alpha)
{
    xv_prep();

    wait_vblank_start();
    uint16_t sa = (alpha & 0xf) << 12;
    for (uint16_t i = 0; i < 256; i++)
    {
        uint16_t v = sa | (xmem_getw_wait(XR_COLOR_A_ADDR + i) & 0xfff);
        xmem_setw(XR_COLOR_B_ADDR + i, v);
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
static void dprintf(const char * fmt, ...) __attribute__((__format__(__printf__, 1, 2)));
static void dprintf(const char * fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(dprint_buff, sizeof(dprint_buff), fmt, args);
    dprint(dprint_buff);
    va_end(args);
}

static void hexdump(void * ptr, size_t bytes)
{
    uint8_t * p = (uint8_t *)ptr;
    for (size_t i = 0; i < bytes; i++)
    {
        if ((i & 0xf) == 0)
        {
            if (i)
            {
                dprintf("    ");
                for (size_t j = i - 16; j < i; j++)
                {
                    int c = p[j];
                    dprintf("%c", c >= ' ' && c <= '~' ? c : '_');
                    // dprintf("%c", isprint(c) ? c : '_');
                }
                dprintf("\n");
            }
            dprintf("%04lx: ", i);
        }
        else
        {
            dprintf(", ");
        }
        dprintf("%02x", p[i]);
    }
    dprintf("\n");
}

void dump_xosera_regs(void)
{
    xv_prep();
    xmem_getw_next_addr(XR_COPPER_ADDR);
    uint16_t * wp = &cop_buffer[0];
    for (uint16_t i = 0; i < (sizeof(cop_buffer) / 2); i++)
    {
        *wp++ = xmem_getw_next_wait();
    }

    uint16_t feature   = xm_getw(FEATURE);
    uint16_t monwidth  = xosera_vid_width();
    uint16_t monheight = xosera_vid_height();

    uint16_t sysctrl = xm_getw(SYS_CTRL);
    uint16_t intctrl = xm_getw(INT_CTRL);

    uint16_t vidctrl  = xreg_getw(VID_CTRL);
    uint16_t coppctrl = xreg_getw(COPP_CTRL);
    uint16_t audctrl  = xreg_getw(AUD_CTRL);
    uint16_t vidleft  = xreg_getw(VID_LEFT);
    uint16_t vidright = xreg_getw(VID_RIGHT);

    uint16_t pa_gfxctrl  = xreg_getw(PA_GFX_CTRL);
    uint16_t pa_tilectrl = xreg_getw(PA_TILE_CTRL);
    uint16_t pa_dispaddr = xreg_getw(PA_DISP_ADDR);
    uint16_t pa_linelen  = xreg_getw(PA_LINE_LEN);
    uint16_t pa_hscroll  = xreg_getw(PA_H_SCROLL);
    uint16_t pa_vscroll  = xreg_getw(PA_V_SCROLL);
    uint16_t pa_hvfscale = xreg_getw(PA_HV_FSCALE);

    uint16_t pb_gfxctrl  = xreg_getw(PB_GFX_CTRL);
    uint16_t pb_tilectrl = xreg_getw(PB_TILE_CTRL);
    uint16_t pb_dispaddr = xreg_getw(PB_DISP_ADDR);
    uint16_t pb_linelen  = xreg_getw(PB_LINE_LEN);
    uint16_t pb_hscroll  = xreg_getw(PB_H_SCROLL);
    uint16_t pb_vscroll  = xreg_getw(PB_V_SCROLL);
    uint16_t pb_hvfscale = xreg_getw(PB_HV_FSCALE);

    dprintf("Initial Xosera state after init:\n");
    dprintf("DESCRIPTION : \"%s\"\n", initinfo.description_str);
    dprintf("VERSION BCD : %x.%02x\n", initinfo.version_bcd >> 8, initinfo.version_bcd & 0xff);
    dprintf("GIT HASH    : #%08lx %s\n", initinfo.githash, initinfo.git_modified ? "[modified]" : "[clean]");
    dprintf("FEATURE     : 0x%04x\n", feature);
    dprintf("MONITOR RES : %dx%d\n", monwidth, monheight);
    dprintf("\nConfig:\n");
    dprintf("SYS_CTRL    : 0x%04x  INT_CTRL    : 0x%04x\n", sysctrl, intctrl);
    dprintf("VID_CTRL    : 0x%04x  COPP_CTRL   : 0x%04x\n", vidctrl, coppctrl);
    dprintf("AUD_CTRL    : 0x%04x\n", audctrl);
    dprintf("VID_LEFT    : 0x%04x  VID_RIGHT   : 0x%04x\n", vidleft, vidright);
    dprintf("\nPlayfield A:                                Playfield B:\n");
    dprintf("PA_GFX_CTRL : 0x%04x  PA_TILE_CTRL: 0x%04x  PB_GFX_CTRL : 0x%04x  PB_TILE_CTRL: 0x%04x\n",
            pa_gfxctrl,
            pa_tilectrl,
            pb_gfxctrl,
            pb_tilectrl);
    dprintf("PA_DISP_ADDR: 0x%04x  PA_LINE_LEN : 0x%04x  PB_DISP_ADDR: 0x%04x  PB_LINE_LEN : 0x%04x\n",
            pa_dispaddr,
            pa_linelen,
            pb_dispaddr,
            pb_linelen);

    dprintf("PA_H_SCROLL : 0x%04x  PA_V_SCROLL : 0x%04x  PB_H_SCROLL : 0x%04x  PB_V_SCROLL : 0x%04x\n",
            pa_hscroll,
            pa_vscroll,
            pb_hscroll,
            pb_vscroll);

    dprintf("PA_HV_FSCALE: 0x%04x                        PB_HV_FSCALE: 0x%04x\n", pa_hvfscale, pb_hvfscale);
    dprintf("\n\n");

    // spammy...
    // dprintf("Initial copper program\n");
    // hexdump(&cop_buffer[0], 0x100);
    // dprintf("\n");
}

static uint16_t screen_addr;
static uint8_t  text_columns;
static uint8_t  text_rows;
static uint8_t  text_color = 0x02;        // dark green on black

static void get_textmode_settings()
{
    xv_prep();

    uint16_t vx          = (xreg_getw(PA_GFX_CTRL) & 3) + 1;
    uint16_t tile_height = (xreg_getw(PA_TILE_CTRL) & 0xf) + 1;
    screen_addr          = xreg_getw(PA_DISP_ADDR);
    text_columns         = (uint8_t)xreg_getw(PA_LINE_LEN);
    text_rows            = (uint8_t)(((xosera_vid_width() / vx) + (tile_height - 1)) / tile_height);
}

static void xcls()
{
    xv_prep();

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
    xv_prep();

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
    xv_prep();

    remove_intr();

    wait_vblank_start();

    xreg_setw(VID_CTRL, 0x0008);
    xreg_setw(COPP_CTRL, 0x0000);
    xreg_setw(AUD_CTRL, 0x0000);
    xreg_setw(VID_LEFT, 0);
    xreg_setw(VID_RIGHT, xosera_vid_width());
    xreg_setw(POINTER_H, 0x0000);
    xreg_setw(POINTER_V, 0x0000);

    xreg_setw(PA_GFX_CTRL, MAKE_GFX_CTRL(0x00, 0, GFX_1_BPP, 0, 0, 0));
    xreg_setw(PA_TILE_CTRL, MAKE_TILE_CTRL(XR_TILE_ADDR, 0, 0, 16));
    xreg_setw(PA_DISP_ADDR, 0x0000);
    xreg_setw(PA_LINE_LEN, xosera_vid_width() / 8);
    xreg_setw(PA_HV_FSCALE, MAKE_HV_FSCALE(0, 0));
    xreg_setw(PA_H_SCROLL, MAKE_H_SCROLL(0));
    xreg_setw(PA_V_SCROLL, MAKE_V_SCROLL(0, 0));

    xreg_setw(PB_GFX_CTRL, MAKE_GFX_CTRL(0x00, 1, GFX_1_BPP, 0, 0, 0));
    xreg_setw(PB_TILE_CTRL, MAKE_TILE_CTRL(XR_TILE_ADDR, 0, 0, 16));
    xreg_setw(PB_DISP_ADDR, 0x0000);
    xreg_setw(PB_LINE_LEN, xosera_vid_width() / 8);
    xreg_setw(PB_HV_FSCALE, MAKE_HV_FSCALE(0, 0));
    xreg_setw(PB_H_SCROLL, MAKE_H_SCROLL(0));
    xreg_setw(PB_V_SCROLL, MAKE_V_SCROLL(0, 0));

    restore_colors();

    printf("\033c");        // reset XANSI

    while (checkchar())
    {
        readchar();
    }
}

static void reset_vid_nosd(void)
{
    xv_prep();

    reset_vid();
#if 1        // handy for development to force Kermit upload
    dprintf("Disabling SD on next boot...\n");
    disable_sd_boot();
    xreg_setw(AUD_CTRL, 0x0);        // disable audio
#endif
}


static inline void checkbail()
{
    if (checkchar())
    {
        reset_vid_nosd();
        _WARM_BOOT();
    }
}

_NOINLINE void delay_check(int ms)
{
    xv_prep();

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
    wait_vblank_start();
    xmem_setw_next_addr(xr_screen_addr);
    for (int i = 0; i < xr_text_columns * xr_text_rows; i++)
    {
        xmem_setw_next(' ');
    }
    xr_x = 0;
    xr_y = 0;
}

static void xr_textmode_pb()
{

    xr_text_columns = 28;
    xr_text_rows    = 20;

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
#if 1
    xmem_setw(XR_COLOR_B_ADDR + 0xf0, 0x0000);        // set write address
    for (int i = 1; i < 16; i++)
    {
        xmem_setw(XR_COLOR_B_ADDR + 0xf0 + i, 0xf202 | (i << 4));
    }
    xmem_setw(XR_COLOR_B_ADDR, 0x0000);        // set write address
#endif

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

#if COPPER_TEST
static void setup_copper_fx()
{
    cop_fx_ptr = &cop_fx[cur_cop_fx];
    if (cop_fx[++cur_cop_fx].name == NULL)
    {
        cur_cop_fx = 0;
    }
}

static void setup_margins(void)
{
    xv_prep();

    uint16_t w = xosera_vid_width();
    xreg_setw(VID_LEFT, ((w - 640) / 2));
    xreg_setw(VID_RIGHT, ((w - 640) / 2) + 640);
}

static void install_copper()
{
    xv_prep();

    wait_vblank_start();
    xreg_setw(PA_H_SCROLL, 0);
    xreg_setw(PB_H_SCROLL, 0);
    xreg_setw(PA_V_SCROLL, 0);
    xreg_setw(PB_V_SCROLL, 0);

    if (cop_fx_ptr->flags & COP_FLAG_HPOS)
    {
        // modify HPOS wait SOL to be left edge horizontal position in 640x480 or 848x480 modes (including overscan)
        cop_diagonal_bin[cop_diagonal__hpos_sol] =
            COP_HPOS((xosera_vid_width() > 640 ? 1088 - 848 - 8 : 800 - 640 - 8));
        // modify HPOS wait EOL to be right edge horizontal position in 640x480 or 848x480 modes (including overscan)
        cop_diagonal_bin[cop_diagonal__hpos_eol] = COP_HPOS((xosera_vid_width() > 640 ? 1088 - 1 : 800 - 1));
    }
    if (cop_fx_ptr->flags & COP_FLAG_SINE)
    {
        uint8_t  ti  = 0;
        uint16_t eol = xosera_vid_width() > 640 ? ((xosera_vid_width() - 640) / 2) : 0;
        for (uint16_t i = 0; i < 256; i += 1)
        {
            uint16_t v                              = eol + (((sinData[ti++] >> 3) - 16) & 0x1f);
            cop_wavey_bin[cop_wavey__wavetable + i] = v;
        }
        xreg_setw(PA_H_SCROLL, 16);
        xreg_setw(PB_H_SCROLL, 16);
    }

    xmem_setw_next_addr(XR_COPPER_ADDR);
    for (uint16_t i = 0; i < cop_fx_ptr->cop_length; i++)
    {
        xmem_setw_next(cop_fx_ptr->cop_data[i]);
    }
}
#endif

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
        dprintf("%s(%d): NULL fileptr\n", __FILE__, __LINE__);
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

static bool load_test_audio(const char * filename, void ** out, int * size)
{
    void * file  = fl_fopen(filename, "r");
    int    fsize = (int)filesize(file);

    if (fsize <= 0)
    {
        dprintf("Can't get size for \"%s\" (not found?)\n", filename);
        return false;
    }

    if (fsize > (64 * 1024))
    {
        dprintf("Sample size reduced from %d to %d for \"%s\"\n", fsize, 65536, filename);
        fsize = 65536;
    }

    uint8_t * data = malloc(fsize);
    if (data == NULL)
    {
        dprintf("Allocating %d for \"%s\" failed\n", fsize, filename);
        return false;
    }
    *out = (int8_t *)data;

    int cnt   = 0;
    int rsize = 0;
    while ((cnt = fl_fread(data, 1, 512, file)) > 0)
    {
        if ((rsize & 0xFFF) == 0)
        {
            dprintf("\rReading \"%s\": %d KB ", filename, rsize >> 10);
            if (rsize)
            {
                uint8_t ox = xr_x;
                xr_printf("%3dK", rsize >> 10);
                xr_x = ox;
            }
        }

        data += cnt;
        rsize += cnt;
        checkbail();
        if (rsize >= fsize)
        {
            break;
        }
    }
    dprintf("\rLoaded \"%s\": %dKB (%d bytes).  \n", filename, rsize >> 10, rsize);
    xr_printf("%3dK\n", rsize >> 10);

    if (rsize != fsize)
    {
        dprintf("\nSize mismatch: ftell %d vs read %d\n", fsize, rsize);
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
        dprintf("Bad size %d for \"%s\"\n", fsize, filename);
        return false;
    }

    uint8_t * data = malloc(fsize);
    if (data == NULL)
    {
        dprintf("Allocating %d for \"%s\" failed\n", fsize, filename);
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
            if (rsize)
            {
                uint8_t ox = xr_x;
                xr_printf("%3dK", rsize >> 10);
                xr_x = ox;
            }
        }

        data += cnt;
        rsize += cnt;
        checkbail();
    }
    dprintf("\rLoaded \"%s\": %dKB (%d bytes).  \n", filename, rsize >> 10, rsize);
    xr_printf("%3dK\n", rsize >> 10);

    if (rsize != fsize)
    {
        dprintf("\nSize mismatch: ftell %d vs read %d\n", fsize, rsize);
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
            dprintf("Bad size %d for \"%s\"\n", csize, colorname);
            break;
        }

        uint16_t * cdata = malloc(csize);
        if (cdata == NULL)
        {
            dprintf("Allocating %d for \"%s\" failed\n", csize, colorname);
            break;
        }


        int        cnt   = 0;
        int        rsize = 0;
        uint16_t * rdata = cdata;
        while ((cnt = fl_fread(rdata, 1, 512, file)) > 0)
        {
            rdata += (cnt >> 1);
            rsize += cnt;
        }
        if (rsize != csize)
        {
            dprintf("Color read failed.\n");
            free(cdata);
            break;
        }
        dprintf("Loaded colors %d colors from \"%s\".  \n", rsize >> 1, colorname);
        ti->color      = cdata;
        ti->num_colors = rsize >> 1;

    } while (false);

    ti->mode = mode;

    return true;
}

void show_test_pic(int pic_num, uint16_t addr)
{
    xv_prep();

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

    wait_vblank_start();
    xreg_setw(PA_GFX_CTRL, 0x0080);        // blank screen
    xreg_setw(PB_GFX_CTRL, 0x0080);
    //    xreg_setw(PA_H_SCROLL, 0x0000);        // blank screen
    //    xreg_setw(PA_V_SCROLL, 0x0000);
    //    xreg_setw(PB_H_SCROLL, 0x0000);        // blank screen
    //    xreg_setw(PB_V_SCROLL, 0x0000);
    xreg_setw(VID_CTRL, 0x0000);               // set border to color #0
    xmem_setw(XR_COLOR_A_ADDR, 0x0000);        // set color #0 to black
    setup_margins();
    xm_setw(WR_INCR, 0x0001);
    xm_setw(WR_ADDR, addr);
    uint16_t * wp = (uint16_t *)ti->data;
    for (int w = 0; w < ti->size; w++)
    {
        xm_setw(DATA, *wp++);
    }

    if (ti->color)
    {
        wp = ti->color;
        xmem_setw_next_addr(XR_COLOR_A_ADDR);
        for (int w = 0; w < ti->num_colors; w++)
        {
            xmem_setw_next(*wp++);
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

    xwait_vblank();
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
    xv_prep();

    dprintf("Loading bitmap: \"%s\"", filename);
    void * file = fl_fopen(filename, "r");

    if (file != NULL)
    {
        int cnt = 0;

        while ((cnt = fl_fread(buffer.u8, 1, 512, file)) > 0)
        {
            if ((vaddr & 0xFFF) == 0)
            {
                dprintf(".");
            }

            uint16_t * maddr = (uint16_t *)buffer.u16;
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
    xv_prep();

    dprintf("Loading colormap: \"%s\"", filename);
    void * file = fl_fopen(filename, "r");

    if (file != NULL)
    {
        int cnt   = 0;
        int vaddr = 0;

        while ((cnt = fl_fread(buffer.u8, 1, 256 * 2 * 2, file)) > 0)
        {
            if ((vaddr & 0x7) == 0)
            {
                dprintf(".");
            }

            uint16_t * maddr = (uint16_t *)buffer.u16;
            xwait_vblank();
            xmem_setw_next_addr(XR_COLOR_ADDR);
            for (int i = 0; i < (cnt >> 1); i++)
            {
                uint16_t v = *maddr++;
                xmem_setw_next(v);
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
    xv_prep();

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
    xv_prep();

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

#define NUM_BOBS 10        // number of sprites (ideally no "red" border)
struct bob
{
    int8_t   x_delta, y_delta;
    int16_t  x_pos, y_pos;
    uint16_t w_offset;
};

struct bob      bobs[NUM_BOBS];
static uint16_t blit_shift[4] = {0xF000, 0x7801, 0x3C02, 0x1E03};

static uint16_t get_lfsr()
{
    xv_prep();

    static uint16_t lfsr = 42;
    uint32_t        msb  = (int16_t)lfsr < 0; /* Get MSB (i.e., the output bit). */
    lfsr <<= 1;                               /* Shift register */
    if (msb)                                  /* If the output bit is 1, */
        lfsr ^= 0x002Du;                      /*  apply toggle mask. */

    uint32_t r = lfsr + xreg_getw(SCANLINE) + xm_getw(TIMER) + _TIMER_100HZ;
    if (r >= 0x10000)
    {
        r++;
    }

    return r;
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

void print_digit(uint16_t off, uint16_t ll, uint16_t dig, uint16_t color)
{
    xv_prep();

    union lw
    {
        uint32_t l;
        uint16_t w[2];
    };

    union lw * lwp = (union lw *)&font[dig * 7];

    xm_setw(WR_INCR, 0x0001);        // set write inc
    for (uint16_t h = 0; h < 7; h++)
    {
        xm_setw(WR_ADDR, off + (h * ll));        // set write address
        xm_setbl(SYS_CTRL, (lwp->w[0] & 0x8000 ? 0xc : 0) | (lwp->w[0] & 0x0080 ? 0x3 : 0));
        xm_setw(DATA, lwp->w[0] & color);
        xm_setbl(SYS_CTRL, (lwp->w[1] & 0x8000 ? 0xc : 0) | (lwp->w[1] & 0x0080 ? 0x3 : 0));
        xm_setw(DATA, lwp->w[1] & color);
        lwp++;
    }
    xm_setbl(SYS_CTRL, 0xf);
}

void test_colormap()
{
    xv_prep();

    xwait_not_vblank();
    xwait_vblank();

    xreg_setw(VID_CTRL, 0x0005);
    xreg_setw(PA_GFX_CTRL, 0x0080);
    xreg_setw(PB_GFX_CTRL, 0x0080);

    xm_setw(WR_INCR, 0x0001);        // set write inc
    xm_setw(WR_ADDR, 0x0000);        // set write address

    for (int i = 0; i < 65536; i++)
    {
        xm_setw(DATA, 0x0000);
    }

    xwait_not_vblank();
    xwait_vblank();

    uint16_t linelen = 160;
    uint16_t w       = 10;
    uint16_t h       = 14;

    xreg_setw(VID_CTRL, 0x0000);
    setup_margins();
    xreg_setw(PA_GFX_CTRL, 0x0065);
    xreg_setw(PA_TILE_CTRL, 0x0C07);
    xreg_setw(PA_DISP_ADDR, 0x0000);
    xreg_setw(PA_LINE_LEN, linelen);        // line len
                                            //    xreg_setw(PA_H_SCROLL, 0x0000);
                                            //    xreg_setw(PA_V_SCROLL, 0x0000);
    xreg_setw(PA_HV_FSCALE, 0x0000);
    xreg_setw(PB_GFX_CTRL, 0x0080);

    xm_setw(WR_INCR, 0x0001);        // set write inc
    xm_setw(WR_ADDR, 0x0000);        // set write address

    uint16_t c = 0;

    for (uint16_t y = 0; y < 16; y++)
    {
        for (uint16_t yp = y * h; yp < ((y + 1) * h) - 2; yp++)
        {
            xm_setw(WR_ADDR, (linelen * (yp + 15)));
            c = y * 16;
            for (uint16_t x = 0; x < 16; x++)
            {
                for (uint16_t xp = x * w; xp < ((x + 1) * w) - 1; xp++)
                {
                    xm_setw(DATA, c << 8 | c);
                }
                xm_setw(DATA, 0x0000);
                c++;
            }
        }
    }

    c = 0;
    for (uint16_t y = 0; y < 16; y++)
    {
        for (uint16_t x = 0; x < 16; x++)
        {
            uint16_t col = xmem_getw_wait(XR_COLOR_A_ADDR + c);
            uint16_t off = (linelen * (h * y + 18)) + (x * w) + 2;
            print_digit(off, linelen, c / 100, ((col & 0x0880) == 0x880) ? 0x0000 : 0xffff);
            off += 2;
            print_digit(off, linelen, (c / 10) % 10, ((col & 0x0880) == 0x880) ? 0x0000 : 0xffff);
            off += 2;
            print_digit(off, linelen, c % 10, ((col & 0x0880) == 0x880) ? 0x0000 : 0xffff);
            c++;
        }
    }

    delay_check(DELAY_TIME * 3);

    c = 0;
    for (uint16_t y = 0; y < 16; y++)
    {
        for (uint16_t yp = y * h; yp < ((y + 1) * h) - 2; yp++)
        {
            xm_setw(WR_ADDR, (linelen * (yp + 15)));
            c = y * 16;
            for (uint16_t x = 0; x < 16; x++)
            {
                for (uint16_t xp = x * w; xp < ((x + 1) * w) - 1; xp++)
                {
                    xm_setw(DATA, c << 8 | c);
                }
                xm_setw(DATA, 0x0000);
                c++;
            }
        }
    }

    c = 0;
    for (uint16_t y = 0; y < 16; y++)
    {
        for (uint16_t x = 0; x < 16; x++)
        {
            uint16_t col = xmem_getw_wait(XR_COLOR_A_ADDR + c);
            uint16_t off = (linelen * (h * y + 18)) + (x * w) + 3;
            print_digit(off, linelen, c / 16, ((col & 0x0880) == 0x880) ? 0x0000 : 0xffff);
            off += 2;
            print_digit(off, linelen, c & 0xf, ((col & 0x0880) == 0x880) ? 0x0000 : 0xffff);
            c++;
        }
    }

    delay_check(DELAY_TIME * 3);
}

void test_blend()
{
    xv_prep();

    uint16_t copsave = xreg_getw(COPP_CTRL);
    xreg_setw(COPP_CTRL, 0x0000);

    xreg_setw(PA_GFX_CTRL, 0x0080);        // bitmap + 8-bpp + Hx1 + Vx1
    xreg_setw(PB_GFX_CTRL, 0x0080);        // bitmap + 8-bpp + Hx1 + Vx1
    xreg_setw(VID_CTRL, 0x0000);           // border color #0

    // modify HPOS wait SOL to be left edge horizontal position in 640x480 or 848x480 modes (including overscan)
    cop_blend_test_bin[cop_blend_test__hpos_sol] = 0x2000 | (xosera_vid_width() > 640 ? 1088 - 848 - 8 : 800 - 640 - 8);
    // modify HPOS wait EOL to be right edge horizontal position in 640x480 or 848x480 modes (including overscan)
    cop_blend_test_bin[cop_blend_test__hpos_eol] = 0x2000 | (xosera_vid_width() > 640 ? 1088 - 1 : 800 - 1);
    xmem_setw_next_addr(XR_COPPER_ADDR);
    for (uint16_t i = 0; i < cop_blend_test_size; i++)
    {
        uint16_t op = cop_blend_test_bin[i];
        xmem_setw_next(op);
    }
    xreg_setw(COPP_CTRL, 0x8000);

    delay_check(DELAY_TIME);

#if COPPER_TEST
    xreg_setw(COPP_CTRL, 0x0000);
    install_copper();
    xreg_setw(COPP_CTRL, copsave);
#endif
}

void test_blit()
{
    static const int W_4BPP = 320 / 4;
    static const int H_4BPP = 240;

    static const int W_LOGO = 32 / 4;
    static const int H_LOGO = 16;

    xv_prep();

    dprintf("test_blit\n");

    // clear ram with CPU in case no blitter

    xm_setw(WR_INCR, 0x0001);        // set write inc
    xm_setw(WR_ADDR, 0x0000);        // set write address

    for (int i = 0; i < 65536; i++)
    {
        xm_setw(DATA, 0x0000);
    }

    // crop left and right 2 pixels
    xr_textmode_pb();
    xreg_setw(VID_RIGHT, xreg_getw(VID_RIGHT) - 4);
    xreg_setw(VID_CTRL, 0x00FF);

    do
    {
        xreg_setw(PA_GFX_CTRL, 0x0040);        // bitmap + 8-bpp + Hx1 + Vx1
        xreg_setw(PA_DISP_ADDR, 0x0000);
        xreg_setw(PA_LINE_LEN, 136);                        // ~65536/480 words per line
        xr_printfxy(0, 0, "Blit VRAM 128KB fill\n");        // set write address

        // fill VRAM
        xwait_vblank();
        xmem_setw(XR_COLOR_B_ADDR + 250, 0x8000);        // set write address
        xmem_setw(XR_COLOR_A_ADDR + 255, 0xf000);        // set write address

        for (int i = 0x100; i >= 0; i -= 0x4)
        {
            xmem_setw(XR_COLOR_A_ADDR + 255, 0xf000);        // set write address
            xwait_blit_ready();
            wait_vblank_start();
            while (xreg_getw(SCANLINE) != 20)
                ;
            xmem_setw(XR_COLOR_A_ADDR + 255, 0xf0f0);        // set write address

            xreg_setw(BLIT_CTRL, 0x0001);              // no transp, constS
            xreg_setw(BLIT_ANDC, 0x0000);              // ANDC constant
            xreg_setw(BLIT_XOR, 0x0000);               // XOR constant
            xreg_setw(BLIT_MOD_S, 0x0000);             // no modulo S
            xreg_setw(BLIT_SRC_S, i << 8 | i);         // A = fill pattern
            xreg_setw(BLIT_MOD_D, 0x0000);             // no modulo D
            xreg_setw(BLIT_DST_D, 0x0000);             // VRAM display end address
            xreg_setw(BLIT_SHIFT, 0xFF00);             // no edge masking or shifting
            xreg_setw(BLIT_LINES, 0x0000);             // 1D
            xreg_setw(BLIT_WORDS, 0x10000 - 1);        // 64KW VRAM
            xwait_blit_done();
            xmem_setw(XR_COLOR_A_ADDR + 255, 0xff00);        // set write address
            xwait_vblank();
        }

        uint16_t daddr = 0x1000;

        uint16_t paddr = 0x9b00;
        show_test_pic(0, paddr);
        xreg_setw(VID_RIGHT, xreg_getw(VID_RIGHT) - 4);
        xreg_setw(VID_CTRL, 0x00FF);
        xmem_setw(XR_COLOR_A_ADDR + 255, 0x0000);        // set write address

        xr_printfxy(0, 0, "Blit 320x240 16 color\n");        // set write address

        // 2D screen screen copy 0x0000 -> 0x4B00 320x240 4-bpp
        xwait_blit_ready();
        xreg_setw(BLIT_CTRL, 0x0000);             // no transp
        xreg_setw(BLIT_ANDC, 0x0000);             // ANDC constant
        xreg_setw(BLIT_XOR, 0x0000);              // XOR constant
        xreg_setw(BLIT_MOD_S, 0x0000);            // no modulo A
        xreg_setw(BLIT_SRC_S, paddr);             // A = source
        xreg_setw(BLIT_MOD_D, 0x0000);            // no modulo D
        xreg_setw(BLIT_DST_D, daddr);             // VRAM display end address
        xreg_setw(BLIT_SHIFT, 0xFF00);            // no edge masking or shifting
        xreg_setw(BLIT_LINES, H_4BPP - 1);        // lines (0 for 1-D blit)
        xreg_setw(BLIT_WORDS, W_4BPP - 1);        // words to write -1
        xwait_blit_done();
        xreg_setw(PA_DISP_ADDR, daddr);

        xr_printfxy(0, 0, "Blit 320x240 16 color\nShift right\n");        // set write address
        wait_vblank_start();
        for (int i = 0; i < 128; i++)
        {
            xwait_blit_ready();                             // make sure blit ready (previous blit started)
            xreg_setw(BLIT_CTRL, 0x0000);                   // no transp
            xreg_setw(BLIT_ANDC, 0x0000);                   // ANDC constant
            xreg_setw(BLIT_XOR, 0x0000);                    // XOR constant
            xreg_setw(BLIT_MOD_S, -1);                      // A modulo
            xreg_setw(BLIT_SRC_S, paddr);                   // A source VRAM addr (pacman)
            xreg_setw(BLIT_MOD_D, -1);                      // D modulo
            xreg_setw(BLIT_DST_D, daddr + (i >> 2));        // D destination VRAM addr
            xreg_setw(BLIT_SHIFT,
                      blit_shift[i & 0x3]);           // first, last word nibble masks, and 0-3 shift (low two bits)
            xreg_setw(BLIT_LINES, H_4BPP - 1);        // lines (0 for 1-D blit)
            xreg_setw(BLIT_WORDS, W_4BPP);            // words to write -1
            xmem_setw(XR_COLOR_A_ADDR + 255, 0xfff0);        // set write address

            xwait_blit_done();
            xmem_setw(XR_COLOR_A_ADDR + 255, 0xf0f0);        // set write address
            wait_vblank_start();
            xmem_setw(XR_COLOR_A_ADDR + 255, 0xff00);        // set write address
        }
        checkbail();
        xmem_setw(XR_COLOR_A_ADDR + 255, 0xFF0F);        // set write address
        delay_check(DELAY_TIME);
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
            r               = get_lfsr();
            bobs[b].x_delta = r & 0x8 ? -((r & 3) - 1) : ((r & 3) + 1);
            r               = get_lfsr();
            bobs[b].y_delta = r & 0x8 ? -((r & 3) - 1) : ((r & 3) + 1);
        }

        xwait_blit_ready();
        xreg_setw(BLIT_CTRL, 0x0000);             // no transp
        xreg_setw(BLIT_MOD_S, 0x0000);            // A mod (XOR)
        xreg_setw(BLIT_SRC_S, paddr);             // A source const word
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
                xwait_blit_ready();                          // make sure blit ready (previous blit started)
                xreg_setw(BLIT_CTRL, 0xEE10);                // E=4bpp transp
                xmem_setw_next(0x0000);                      // ANCC const
                xmem_setw_next(0x0000);                      // XOR const
                xmem_setw_next(W_4BPP - W_LOGO - 1);         // S modulo
                xmem_setw_next(paddr + bp->w_offset);        // S addr
                xmem_setw_next(W_4BPP - W_LOGO - 1);         // D modulo
                xmem_setw_next(daddr + bp->w_offset);        // D destination VRAM addr
                xmem_setw_next(0xFF00);                // first, last word nibble masks, and 0-3 shift (low two bits)
                xmem_setw_next(H_LOGO - 1);            // lines (0 for 1-D blit)
                xmem_setw_next(W_LOGO - 1 + 1);        // words to write -1

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

                xwait_blit_ready();                         // make sure blit ready (previous blit started)
                xreg_setw(BLIT_CTRL, 0x0000);               // no transp
                xmem_setw_next(0x0000);                     // ANDC const
                xmem_setw_next(0x0000);                     // XOR const
                xmem_setw_next(-1);                         // S modulo
                xmem_setw_next(maddr);                      // S address
                xmem_setw_next(W_4BPP - W_LOGO - 1);        // D modulo
                xmem_setw_next(daddr + off);                // D destination VRAM addr
                xmem_setw_next(blit_shift[shift]);        // first, last word nibble masks, and 0-3 shift (low two bits)
                xmem_setw_next(H_LOGO - 1);               // lines (0 for 1-D blit)
                xmem_setw_next(W_LOGO - 1 + 1);           // words to write -1
            }
            xmem_setw(XR_COLOR_A_ADDR + 255, 0xfff0);        // set write address
            checkbail();
            xwait_blit_done();
            xmem_setw(XR_COLOR_A_ADDR + 255, 0xf0f0);        // set write address
            xwait_vblank();
            xmem_setw(XR_COLOR_A_ADDR + 255, 0xff00);        // set write address
        }

        xmem_setw(XR_COLOR_A_ADDR + 255, 0xf000);        // set write address

        delay_check(DELAY_TIME);

    } while (false);
    xreg_setw(PA_GFX_CTRL, 0x0055);        // bitmap + 4-bpp + Hx2 + Vx2
    xreg_setw(PA_LINE_LEN, 320 / 4);
    xreg_setw(PA_DISP_ADDR, 0x0000);

    setup_margins();
}

void test_true_color()
{

    uint16_t saddr = 0x0000;

    show_test_pic(TRUECOLOR_TEST_PIC, saddr);

    delay_check(DELAY_TIME * 2);

    //    load_sd_bitmap("/fractal_320x240_RG8B4.raw", saddr);

    //    delay_check(DELAY_TIME * 2);
}

// this tests a problem switching modes
void test_mode_glitch()
{
    xv_prep();

    int width = xosera_vid_width();
    xm_setw(WR_ADDR, 0);
    xm_setw(WR_INCR, 1);
    xm_setbl(SYS_CTRL, 0xF);
    for (int i = 0; i < (width / 2) * 240; ++i)
    {
        xm_setw(DATA, 0x0101);
    }

    for (int i = 0; i < 10; i++)
    {

        // set to tiled 1-bpp
        xreg_setw(PA_LINE_LEN, width / 8);
        xreg_setw(PA_GFX_CTRL, 0x0000);

        delay(1000000);
        wait_vblank_start();

        // set to bitmap 8-bpp, Hx2, Vx2
        xreg_setw(PA_LINE_LEN, (width / 2) / 2);
        xreg_setw(PA_GFX_CTRL, 0x0065);

        delay(1000000);

        delay_check(DELAY_TIME);
    }
}

void test_dual_8bpp()
{
    xv_prep();

    const uint16_t width  = DRAW_WIDTH;
    const uint16_t height = 200;
    // /    uint16_t       old_copp = xreg_getw(COPP_CTRL);

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

        xwait_vblank();
        xreg_setw(VID_CTRL, 0x0000);           // border color = black
        xreg_setw(PA_GFX_CTRL, 0x0080);        // blank screen
        xreg_setw(PB_GFX_CTRL, 0x0080);
        // install 320x200 "crop" copper list
        xmem_setw_next_addr(XR_COPPER_ADDR);
        for (uint16_t i = 0; i < cop_320x200_size; i++)
        {
            xmem_setw_next(cop_320x200_bin[i]);
        }
        xreg_setw(COPP_CTRL, 0x8000);
        // set pf A 320x240 8bpp (cropped to 320x200)
        xreg_setw(PA_GFX_CTRL, 0x0065);
        xreg_setw(PA_TILE_CTRL, 0x000F);
        xreg_setw(PA_DISP_ADDR, addrA);
        xreg_setw(PA_LINE_LEN, DRAW_WORDS);
        //        xreg_setw(PA_H_SCROLL, 0x0000);
        //        xreg_setw(PA_V_SCROLL, 0x0000);

        // set pf B 320x240 8bpp (cropped to 320x200)
        xreg_setw(PB_GFX_CTRL, 0x0065);
        xreg_setw(PB_TILE_CTRL, 0x000F);
        xreg_setw(PB_DISP_ADDR, addrB);
        xreg_setw(PB_LINE_LEN, DRAW_WORDS);
        //        xreg_setw(PB_H_SCROLL, 0x0000);
        //        xreg_setw(PB_V_SCROLL, 0x0000);

        // enable copper
        xwait_vblank();
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


        xwait_vblank();
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

        xwait_vblank();
        xmem_setw(XR_COPPER_ADDR + (1 * 2) + 1, 0x00E5);
        xmem_setw(XR_COPPER_ADDR + (2 * 2) + 1, 0x0065);
        dprintf("Playfield B: 320x200 8bpp - vertical-striped triangle, A blanked\n");
        delay_check(DELAY_TIME);


        xwait_vblank();
        xmem_setw(XR_COPPER_ADDR + (1 * 2) + 1, 0x0065);
        xmem_setw(XR_COPPER_ADDR + (2 * 2) + 1, 0x0065);
        dprintf("Playfield A&B: mixed (alpha 0x8)\n");
        delay_check(DELAY_TIME);


        xwait_vblank();
        restore_colors2(0x0);        // colormem B normal colors (alpha 0%)

        dprintf("Playfield A&B: colormap B alpha 0x0\n");
        delay_check(DELAY_TIME);


        xwait_vblank();
        restore_colors2(0x4);        // colormem B normal colors (alpha 25%)

        dprintf("Playfield A&B: colormap B alpha 0x4\n");
        delay_check(DELAY_TIME);


        xwait_vblank();
        restore_colors2(0x8);        // colormem B normal colors (alpha 50%)

        dprintf("Playfield A&B: colormap B alpha 0x8\n");
        delay_check(DELAY_TIME);


        xwait_vblank();
        restore_colors2(0xF);        // colormem B normal colors (alpha 100%)

        dprintf("Playfield A&B: colormap B alpha 0xC\n");
        delay_check(DELAY_TIME);

    } while (false);

    dprintf("restore screen\n");
    restore_colors3();        // colormem B normal colors (alpha 0%)
    xwait_vblank();
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

    xv_prep();

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

    uint16_t reps = 2;        // just a few flashes for write test
    xmsg(0, 0, 0x02, "VRAM write     ");
    dprintf("VRAM write x %d\n", reps);
    timer_start();
    uint32_t v = ((0x0f00 | 'G') << 16) | (0xf000 | 'o');
    for (int loop = 0; loop < reps; loop++)
    {
        uint16_t count = 0x800;        // VRAM long count
        __asm__ __volatile__(
                    "0:     movep.l  %[tmp]," XM_STR(XM_DATA) "(%[xptr])\n"
                    "       movep.l  %[tmp]," XM_STR(XM_DATA) "(%[xptr])\n"
                    "       movep.l  %[tmp]," XM_STR(XM_DATA) "(%[xptr])\n"
                    "       movep.l  %[tmp]," XM_STR(XM_DATA) "(%[xptr])\n"
                    "       movep.l  %[tmp]," XM_STR(XM_DATA) "(%[xptr])\n"
                    "       movep.l  %[tmp]," XM_STR(XM_DATA) "(%[xptr])\n"
                    "       movep.l  %[tmp]," XM_STR(XM_DATA) "(%[xptr])\n"
                    "       movep.l  %[tmp]," XM_STR(XM_DATA) "(%[xptr])\n"
                    "       movep.l  %[tmp]," XM_STR(XM_DATA) "(%[xptr])\n"
                    "       movep.l  %[tmp]," XM_STR(XM_DATA) "(%[xptr])\n"
                    "       movep.l  %[tmp]," XM_STR(XM_DATA) "(%[xptr])\n"
                    "       movep.l  %[tmp]," XM_STR(XM_DATA) "(%[xptr])\n"
                    "       movep.l  %[tmp]," XM_STR(XM_DATA) "(%[xptr])\n"
                    "       movep.l  %[tmp]," XM_STR(XM_DATA) "(%[xptr])\n"
                    "       movep.l  %[tmp]," XM_STR(XM_DATA) "(%[xptr])\n"
                    "       movep.l  %[tmp]," XM_STR(XM_DATA) "(%[xptr])\n"
                    "       dbf     %[cnt],0b"
                    : [cnt] "=&d"(count)
                    : [xptr] "a"(xosera_ptr), [tmp] "d"(v)
                    : );
        v ^= 0xff00ff00;
    }
    vram_write = timer_stop();
    global     = v;        // save v so GCC doesn't optimize away test
    if (checkchar())
    {
        return;
    }
    // register" write)
    xmsg(0, 0, 0x02, "main RAM write ");
    dprintf("main RAM write x %d\n", reps);
    timer_start();
    for (int loop = 0; loop < reps; loop++)
    {
        uint16_t   count = 0x800;        // VRAM long count
        uint32_t * ptr   = buffer.u32;
        __asm__ __volatile__(
            "0:     move.l  %[tmp],(%[dptr])\n"
            "       move.l  %[tmp],(%[dptr])\n"
            "       move.l  %[tmp],(%[dptr])\n"
            "       move.l  %[tmp],(%[dptr])\n"
            "       move.l  %[tmp],(%[dptr])\n"
            "       move.l  %[tmp],(%[dptr])\n"
            "       move.l  %[tmp],(%[dptr])\n"
            "       move.l  %[tmp],(%[dptr])\n"
            "       move.l  %[tmp],(%[dptr])\n"
            "       move.l  %[tmp],(%[dptr])\n"
            "       move.l  %[tmp],(%[dptr])\n"
            "       move.l  %[tmp],(%[dptr])\n"
            "       move.l  %[tmp],(%[dptr])\n"
            "       move.l  %[tmp],(%[dptr])\n"
            "       move.l  %[tmp],(%[dptr])\n"
            "       move.l  %[tmp],(%[dptr])\n"
            "       dbf     %[cnt],0b"
            : [cnt] "=&d"(count)
            : [dptr] "a"(ptr), [tmp] "d"(v)
            :);
        v ^= 0xff00ff00;
    }
    main_write = timer_stop();
    global     = v;        // save v so GCC doesn't optimize away test
    if (checkchar())
    {
        return;
    }
    xmsg(0, 0, 0x02, "VRAM read      ");
    dprintf("VRAM read x %d\n", reps);
    timer_start();
    for (int loop = 0; loop < reps; loop++)
    {
        uint16_t count = 0x800;        // VRAM long count
        __asm__ __volatile__(
                    "0:    movep.l  " XM_STR(XM_DATA) "(%[xptr]),%[tmp]\n"
                    "      movep.l  " XM_STR(XM_DATA) "(%[xptr]),%[tmp]\n"
                    "      movep.l  " XM_STR(XM_DATA) "(%[xptr]),%[tmp]\n"
                    "      movep.l  " XM_STR(XM_DATA) "(%[xptr]),%[tmp]\n"
                    "      movep.l  " XM_STR(XM_DATA) "(%[xptr]),%[tmp]\n"
                    "      movep.l  " XM_STR(XM_DATA) "(%[xptr]),%[tmp]\n"
                    "      movep.l  " XM_STR(XM_DATA) "(%[xptr]),%[tmp]\n"
                    "      movep.l  " XM_STR(XM_DATA) "(%[xptr]),%[tmp]\n"
                    "      movep.l  " XM_STR(XM_DATA) "(%[xptr]),%[tmp]\n"
                    "      movep.l  " XM_STR(XM_DATA) "(%[xptr]),%[tmp]\n"
                    "      movep.l  " XM_STR(XM_DATA) "(%[xptr]),%[tmp]\n"
                    "      movep.l  " XM_STR(XM_DATA) "(%[xptr]),%[tmp]\n"
                    "      movep.l  " XM_STR(XM_DATA) "(%[xptr]),%[tmp]\n"
                    "      movep.l  " XM_STR(XM_DATA) "(%[xptr]),%[tmp]\n"
                    "      movep.l  " XM_STR(XM_DATA) "(%[xptr]),%[tmp]\n"
                    "      movep.l  " XM_STR(XM_DATA) "(%[xptr]),%[tmp]\n"
                    "      dbf     %[cnt],0b"
                    : [tmp] "=&d"(v), [cnt] "=&d"(count)
                    : [xptr] "a"(xosera_ptr)
                    :);
    }
    vram_read = timer_stop();
    global    = v;        // save v so GCC doesn't optimize away test
    if (checkchar())
    {
        return;
    }
    xmsg(0, 0, 0x02, "main RAM read  ");
    dprintf("main RAM read x %d\n", reps);
    timer_start();
    for (int loop = 0; loop < reps; loop++)
    {
        uint16_t   count = 0x800;        // VRAM long count
        uint32_t * ptr   = buffer.u32;
        __asm__ __volatile__(
            "0:    move.l  (%[sptr]),%[tmp]\n"
            "      move.l  (%[sptr]),%[tmp]\n"
            "      move.l  (%[sptr]),%[tmp]\n"
            "      move.l  (%[sptr]),%[tmp]\n"
            "      move.l  (%[sptr]),%[tmp]\n"
            "      move.l  (%[sptr]),%[tmp]\n"
            "      move.l  (%[sptr]),%[tmp]\n"
            "      move.l  (%[sptr]),%[tmp]\n"
            "      move.l  (%[sptr]),%[tmp]\n"
            "      move.l  (%[sptr]),%[tmp]\n"
            "      move.l  (%[sptr]),%[tmp]\n"
            "      move.l  (%[sptr]),%[tmp]\n"
            "      move.l  (%[sptr]),%[tmp]\n"
            "      move.l  (%[sptr]),%[tmp]\n"
            "      move.l  (%[sptr]),%[tmp]\n"
            "      move.l  (%[sptr]),%[tmp]\n"
            "      dbf     %[cnt],0b"
            : [tmp] "=&d"(v), [cnt] "=&d"(count)
            : [sptr] "a"(ptr)
            :);
        v ^= 0xff00ff00;
    }
    main_read = timer_stop();
    if (checkchar())
    {
        return;
    }
#if 1
    dprintf("done\n");

    dprintf("MOVEP.L VRAM write      128KB x %d (%d KB)    %lu.%04lu sec (%lu KB/sec)\n",
            reps,
            128 * reps,
            vram_write / 10000U,
            vram_write % 10000U,
            (10000U * 128 * reps) / vram_write);
    dprintf("MOVEP.L VRAM read       128KB x %d (%d KB)    %lu.%04lu sec (%lu KB/sec)\n",
            reps,
            128 * reps,
            vram_read / 10000U,
            vram_read % 10000U,
            (10000U * 128 * reps) / vram_read);
    dprintf("MOVE.L  main RAM write  128KB x %d (%d KB)    %lu.%04lu sec (%lu KB/sec)\n",
            reps,
            128 * reps,
            main_write / 10000U,
            main_write % 10000U,
            (10000U * 128 * reps) / main_write);
    dprintf("MOVE.L  main RAM read   128KB x %d (%d KB)    %lu.%04lu sec (%lu KB/sec)\n",
            reps,
            128 * reps,
            main_read / 10000U,
            main_read % 10000U,
            (10000U * 128 * reps) / main_read);
#endif
}

void test_8bpp_tiled()
{
    xv_prep();

    // setup 4-bpp 16 x 16 tiled screen showing 4 sample buffers
    xreg_setw(PA_GFX_CTRL, 0x0020);             // colorbase = 0x00, tiled, 8-bpp, Hx2 Vx2
                                                //    xreg_setw(PA_HV_FSCALE, 0x0044);            // set 512x384 scaling
    xreg_setw(PA_HV_FSCALE, 0x0000);            // set 512x384 scaling
    xreg_setw(PA_TILE_CTRL, 0x0000 | 7);        // tiledata @ 0x800, 8 high
    xreg_setw(PA_DISP_ADDR, 0x0000);            // display VRAM @ 0x0000
    xreg_setw(PA_LINE_LEN, 80);                 // 16 chars per line

    // set colormap
    for (uint16_t i = i; i < 16; i++)
    {
        xmem_setw(XR_COLOR_A_ADDR + 0 + i, (i << 8) | (i << 4) | (i << 0));
        xmem_setw(XR_COLOR_A_ADDR + 16 + i, (i << 8) | (0 << 4) | (0 << 0));
        xmem_setw(XR_COLOR_A_ADDR + 32 + i, (0 << 8) | (i << 4) | (0 << 0));
        xmem_setw(XR_COLOR_A_ADDR + 48 + i, (i << 8) | (0 << 4) | (i << 0));
    }

    xm_setw(WR_INCR, 0x0001);
    int c = 0;

    xm_setw(WR_ADDR, 0x0000);
    for (int x = 0; x < 0x1000; x += 1)
    {
        xm_setw(DATA, c);        // use colorbase for channel tint
        c += 1;
    }


    xmem_setw_next_addr(XR_TILE_ADDR + 0x0000);
    for (int i = 0x0000; i < 0x1000; i++)
    {
        xmem_setw_next(i & 0x08 ? ~(uint16_t)i : (uint16_t)i);
    }
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


#define SILENCE_ADDR (XR_TILE_ADDR + XR_TILE_SIZE - 1)        // last word of TILE memory
#define SILENCE_LEN  (AUD_LENGTH_TILEMEM_F | (1 - 1))         // tilemem flag, | length -1
#define SILENCE_PER  0x7FFF                                   // arbitrary, but slow (to save VRAM DMA)

static void play_silence()
{
    xv_prep();

    // upload word of silence (in TILE memory or VRAM)
    if (SILENCE_LEN & AUD_LENGTH_TILEMEM_F)
    {
        xmem_setw(SILENCE_ADDR, 0);
    }
    else
    {
        vram_setw(SILENCE_ADDR, 0);
    }

    // play slow silence
    for (int v = 0; v < 4; v++)
    {
        uint16_t vo = v << 2;
        xreg_setw(AUD0_VOL + vo, 0x8080);
        xreg_setw_next(/* AUD0_PERIOD + vo, */ SILENCE_PER);
        xreg_setw_next(/* AUD0_LENGTH + vo, */ SILENCE_LEN);
        xreg_setw_next(/* AUD0_START + vo,  */ SILENCE_ADDR);
        xreg_setw(AUD0_PERIOD + vo, AUD_PERIOD_RESTART_F | SILENCE_PER);
    }
}

uint8_t num_audio_channels;
uint8_t audio_channel_mask;

static int init_audio()
{
    xv_prep();

    xreg_setw(AUD_CTRL, 0x0000);        // disable audio

    play_silence();

    xreg_setw(AUD_CTRL, 0x0001);        // enable audio

    num_audio_channels = 0;
    audio_channel_mask = 0;

    uint8_t aud_ena = xreg_getw(AUD_CTRL) & 1;
    if (!aud_ena)
    {
        dprintf("Xosera audio support disabled.\n");
        return 0;
    }

    // check if audio fully disbled

    audio_channel_mask = xm_getbl(INT_CTRL) & INT_CTRL_AUD_ALL_F;
    while (audio_channel_mask & (1 << num_audio_channels))
    {
        num_audio_channels++;
    }

    if (num_audio_channels == 0)
    {
        dprintf("FIXME: Xosera has audio support, but no channels?\n");
    }

    uint8_t feature_chans = XV_(xm_getw(FEATURE), FEATURE_AUDCHAN_B, FEATURE_AUDCHAN_W);
    if (num_audio_channels != feature_chans)
    {
        dprintf("FIXME: Mismatch between detected channels and FEATURE!\n");
    }

    play_silence();

    return num_audio_channels;
}

void * testsamp;
int    testsampsize;

static void test_audio_sample(const char * name, int8_t * samp, int bytesize, int speed)
{
    uint16_t test_vaddr = 0x8000;
    uint16_t chan       = 0;
    uint16_t coff       = chan << 2;

    xv_prep();

    xm_setw(SYS_CTRL, 0x000F);        // make sure no nibbles masked
    xm_setw(WR_INCR, 0x0001);         // set write increment
    xm_setw(WR_ADDR, 0x0000);         // set write address
    xm_setw(DATA, 0);

    xm_setw(WR_INCR, 0x0001);            // set write increment
    xm_setw(WR_ADDR, test_vaddr);        // set write address

    for (int i = 0; i < bytesize; i += 2)
    {
        xm_setbh(DATA, *samp++);
        xm_setbl(DATA, *samp++);
    }

    uint16_t p  = speed;
    uint8_t  lv = 0x40;
    uint8_t  rv = 0x40;

    xr_printfxy(0, 0, "Xosera audio test\n%s: %d B\n", name, bytesize);

    dprintf("\nTesting audio sample: \"%s\" (%d bytes)...\n\n", name, bytesize);
    dprintf("Press: 'Z' and 'X' to change sample volume (hold shift for faster)\n");
    dprintf("       'Q' and 'W' to change left volume (hold shift for faster)\n");
    dprintf("       'E' and 'R' to change right volume (hold shift for faster)\n");
    dprintf("       ',' and '.' to change sample period (hold shift for faster)\n");
    dprintf("       '0' to  '3' to change channel\n");
    dprintf("       ESC to reboot rosco\n");
    dprintf("       SPACE to continue to next test\n\n");

    dprintf("%d: Volume (128=1.0): L:%3d/R:%3d    Period (1/pclk): %5d", chan, lv, rv, p);

    xreg_setw(AUD0_VOL + coff, lv << 8 | rv);                          // set left and right volume
    xreg_setw_next(/* AUD0_PERIOD+coff, */ p);                         // set period
    xreg_setw_next(/* AUD0_LENGTH+coff, */ (bytesize / 2) - 1);        // sample length in words -1 (and VRAM/TILE flag)
    xreg_setw_next(/* AUD0_START+coff,  */ test_vaddr);                // sample address in VRAM
    xreg_setw(AUD0_PERIOD + coff, p | AUD_PERIOD_RESTART_F);           // set period and restart sample

    bool done = false;

    while (1)
    {
        char c = readchar();

        switch (c)
        {
            case 'z':
                lv = lv - 1;
                rv = lv;
                break;
            case 'x':
                lv = lv + 1;
                rv = lv;
                break;
            case 'Z':
                lv = lv - 16;
                rv = lv;
                break;
            case 'X':
                lv = lv + 16;
                rv = lv;
                break;
            case 'q':
                lv = lv - 1;
                break;
            case 'w':
                lv = lv + 1;
                break;
            case 'Q':
                lv = lv - 16;
                break;
            case 'W':
                lv = lv + 16;
                break;
            case 'e':
                rv = rv - 1;
                break;
            case 'r':
                rv = rv + 1;
                break;
            case 'E':
                rv = rv - 16;
                break;
            case 'R':
                rv = rv + 16;
                break;
            case ',':
                p = p - 1;
                break;
            case '.':
                p = p + 1;
                break;
            case '<':
                p = p - 16;
                break;
            case '>':
                p = p + 16;
                break;
            case '0':
                chan = 0;
                xreg_setw(AUD0_VOL, lv << 8 | rv);                            // set left 100% volume, right 50% volume
                xreg_setw_next(/* AUD0_PERIOD, */ p);                         // 1000 clocks per each sample byte
                xreg_setw_next(/* AUD0_LENGTH, */ (bytesize / 2) - 1);        // length in words (256 8-bit samples)
                xreg_setw_next(/* AUD0_START,  */ test_vaddr);                // address in VRAM
                xreg_setw(AUD0_PERIOD, AUD_PERIOD_RESTART_F | p);             // 1000 clocks per each sample byte
                break;
            case '1':
                chan = 1;
                xreg_setw(AUD1_VOL, lv << 8 | rv);                           // set left 100% volume, right 50% volume
                xreg_setw_next(/*AUD1_PERIOD, */ p);                         // 1000 clocks per each sample byte
                xreg_setw_next(/*AUD1_LENGTH, */ (bytesize / 2) - 1);        // length in words (256 8-bit samples)
                xreg_setw_next(/*AUD1_START,  */ test_vaddr);                // address in VRAM
                xreg_setw(AUD1_PERIOD, AUD_PERIOD_RESTART_F | p);            // 1000 clocks per each sample byte
                break;
            case '2':
                chan = 2;
                xreg_setw(AUD2_VOL, lv << 8 | rv);                            // set left 100% volume, right 50% volume
                xreg_setw_next(/* AUD2_PERIOD, */ p);                         // 1000 clocks per each sample byte
                xreg_setw_next(/* AUD2_LENGTH, */ (bytesize / 2) - 1);        // length in words (256 8-bit samples)
                xreg_setw_next(/* AUD2_START,  */ test_vaddr);                // address in VRAM
                xreg_setw(AUD2_PERIOD, AUD_PERIOD_RESTART_F | p);             // 1000 clocks per each sample byte
                break;
            case '3':
                chan = 3;
                xreg_setw(AUD3_VOL, lv << 8 | rv);                            // set left 100% volume, right 50% volume
                xreg_setw_next(/* AUD3_PERIOD, */ p);                         // 1000 clocks per each sample byte
                xreg_setw_next(/* AUD3_LENGTH, */ (bytesize / 2) - 1);        // length in words (256 8-bit samples)
                xreg_setw_next(/* AUD3_START,  */ test_vaddr);                // address in VRAM
                xreg_setw(AUD3_PERIOD, AUD_PERIOD_RESTART_F | p);             // 1000 clocks per each sample byte
                break;
            case ' ':
                done = true;
                break;
            case '\x1b':
                dprintf("\nExit!\n");
                reset_vid();
                _WARM_BOOT();
                break;
        }
        if (done)
        {
            break;
        }
        dprintf("\r%d: Volume (128 = 1.0): L:%3d R:%3d  Period (1/pclk): %5d", chan, lv, rv, p);
        coff = chan << 2;
        xreg_setw(AUD0_VOL + coff, lv << 8 | rv);           // set left 100% volume, right 50% volume
        xreg_setw_next(/* AUD0_PERIOD + coff, */ p);        // 1000 clocks per each sample byte
    }

    play_silence();

    dprintf("\rSample playback done.                                       \n");
    xr_printfxy(0, 0, "Xosera audio test\n\n");
}

// wait at least one scanline
static void wait_scanline()
{
    uint16_t l;
    xv_prep();

    do
    {
        l = xreg_getw(SCANLINE);
    } while (l == xreg_getw(SCANLINE));
    do
    {
        l = xreg_getw(SCANLINE);
    } while (l == xreg_getw(SCANLINE));
}

static void upload_audio(void * memdata, uint16_t vaddr, int len)
{
    xv_prep();

    xm_setbl(SYS_CTRL, 0x0F);        // vram mask
    xm_setw(WR_INCR, 0x0001);
    xm_setw(WR_ADDR, vaddr);
    uint16_t * wp = memdata;
    for (int i = len; i > 0; i -= 2)
    {
        xm_setw(DATA, *wp++);
    }
}

static void play_blurb_sample(uint16_t vaddr, uint16_t len, uint16_t rate)
{
    xv_prep();

    if (num_audio_channels != 0)
    {
        uint32_t clk_hz = xosera_sample_hz();
        uint16_t period = (clk_hz + rate - 1) / rate;


        play_silence();
        xreg_setw(AUD_CTRL, 0x0001);
        uint16_t ic = xm_getw(INT_CTRL);
        xreg_setw(AUD0_START, SILENCE_ADDR);
        xreg_setw(AUD1_START, SILENCE_ADDR);
        xreg_setw(AUD2_START, SILENCE_ADDR);
        xreg_setw(AUD3_START, SILENCE_ADDR);
        xm_setbl(INT_CTRL, INT_CTRL_CLEAR_ALL_F);
        uint16_t ic2 = xm_getw(INT_CTRL);
        dprintf("INT_CTRL:0x%04x -> 0x%04x (silence queued)\n", ic, ic2);

        for (int v = 0; v < num_audio_channels; v++)
        {
            uint16_t vo = v << 2;
            uint16_t audvol;
            switch (v)
            {
                case 0:
                    audvol = 0x8080;
                    break;
                case 1:
                    audvol = 0x8000;
                    break;
                case 2:
                    audvol = 0x0080;
                    break;
                default:
                    audvol = 0x4040;
                    break;
            }
            xreg_setw(AUD0_VOL + vo, audvol);
            xreg_setw_next(/* AUD0_PERIOD + vo, */ period);        // force instant sample start
            xreg_setw_next(/* AUD0_LENGTH + vo, */ (len / 2) - 1);
            xreg_setw_next(/* AUD0_START + vo,  */ vaddr);
            xreg_setw(AUD0_PERIOD + vo, period | AUD_PERIOD_RESTART_F);        // force instant sample start
            ic = xm_getw(INT_CTRL);
            xreg_setw(AUD0_LENGTH + vo, SILENCE_LEN);                   // length-1 and TILE flag
            xreg_setw_next(/* AUD0_START + vo, */ SILENCE_ADDR);        // queue silence
            xm_setbl(INT_CTRL, (INT_CTRL_AUD0_INTR_F << v));            // clear voice interrupt status

            ic2 = xm_getw(INT_CTRL);
            dprintf("Started channel %d... INT_CTRL = 0x%04x -> 0x%04x\n", v, ic, ic2);

            delay_check(250);
            period += 350;
        }

        // wait for each channels to be ready (after they have started SILENCE)
        for (int v = 0; v < num_audio_channels; v++)
        {
            ic = xm_getw(INT_CTRL);
            dprintf("Waiting channel %d... INT_CTRL = 0x%04x", v, ic);
            do
            {
                delay_check(1);
                ic = xm_getw(INT_CTRL);
            } while (!(ic & (INT_CTRL_AUD0_INTR_F << v)));
            dprintf(" -> 0x%04x\n", ic);
        }

        dprintf("Audio completed\n");
        play_silence();
        xreg_setw(AUD_CTRL, 0x0000);
    }
    else
    {
        dprintf("Audio disabled\n");
    }
}

const char blurb[] =
    "\n"
    "\n"
    "Xosera is an FPGA based video/audio adapter designed with the rosco_m68k retro\n"
    "computer in mind. Inspired in concept by it's \"namesake\" the Commander X16's\n"
    "VERA, Xosera is an original open-source video adapter design, built with open-\n"
    "source tools and is tailored with features generally appropriate for a\n"
    "Motorola 68K era retro computer like the rosco_m68k (or even an 8-bit CPU).\n"
    "\n"
    "\n"
    "  \xf9  Uses low-cost FPGA instead of expensive semiconductor fabrication :)\n"
    "  \xf9  128KB of embedded video VRAM (16-bit words at 25/33 MHz)\n"
    "  \xf9  VGA output at 640x480 or 848x480 16:9 wide-screen (both @ 60Hz)\n"
    "  \xf9  Register based interface using 16 direct 16-bit registers\n"
    "  \xf9  Additional indirect read/write registers for easy use and programming\n"
    "  \xf9  Fast 8-bit bus interface (using MOVEP) for rosco_m68k (by Ross Bamford)\n"
    "  \xf9  Read/write VRAM with programmable read/write address increment\n"
    "  \xf9  Optional easy pixel X,Y bitmap address and write-mask calculation\n"
    "  \xf9  Dual video planes (playfields) with alpha color blending and priority\n"
    "  \xf9  Dual 256 color palettes with 12-bit RGB (4096 colors) and 4-bit \"alpha\"\n"
    "  \xf9  Read/write tile memory for an additional 10KB of tiles or tilemap\n"
    "  \xf9  Text mode with up to 8x16 glyphs and 16 foreground & background colors\n"
    "  \xf9  Graphic tiled modes with 1024 glyphs, 16/256 colors and H/V tile mirror\n"
    "  \xf9  Bitmap modes with 1 (plus attribute colors), 4 or 8 bits per pixel\n"
    "  \xf9  32x32 16 color native resolution pointer \"sprite\" overlay\n"
    "  \xf9  Fast 2-D \"blitter\" with transparency, masking, shifting and logic ops\n"
    "  \xf9  Screen synchronized \"copper\" to change colors and registers mid-screen\n"
    "  \xf9  Wavetable DMA 8-bit audio with 4 independent stereo channels\n"
    "  \xf9  Pixel H/V repeat of 1x, 2x, 3x or 4x (e.g. for 424x240 or 320x240)\n"
    "  \xf9  Fractional H/V repeat scaling (for 320x200 or 512x384 retro modes)\n"
    "\n"
    "\n";

#if AUDIO_CHAINING_TEST
static void test_audio_ping_pong()
{
    void *   pingpong_sample[2];
    int      pingpong_length[2];
    uint16_t pingpong_addr[2];

    uint8_t chan_ping = rand() & 0xF;

    xv_prep();

    xr_cls();
    xr_printf(" Audio chaining test\n\n");
    xr_printf("\xAF Loading ping sample ");
    load_test_audio("/ping_8000.raw", &pingpong_sample[0], &pingpong_length[0]);
    xr_printf("\xAF Loading pong sample ");
    load_test_audio("/pong_8000.raw", &pingpong_sample[1], &pingpong_length[1]);

    pingpong_addr[0] = 0x1000;
    pingpong_addr[1] = 0x9000;

    upload_audio(pingpong_sample[0], pingpong_addr[0], pingpong_length[0]);
    upload_audio(pingpong_sample[1], pingpong_addr[1], pingpong_length[1]);

    pingpong_length[0] = (pingpong_length[0] >> 1) - 1;
    pingpong_length[1] = (pingpong_length[1] >> 1) - 1;

    xm_setbl(INT_CTRL, 0xf);
    uint16_t plays = 0;
    uint16_t ic;
    while (plays < 200)
    {
        for (int v = 0; v < num_audio_channels; v++)
        {
            uint16_t vb = 1 << v;
            ic          = xm_getw(INT_CTRL);
            if (ic & vb)
            {
                uint16_t pp = (chan_ping & vb) ? 1 : 0;
                chan_ping ^= vb;
                uint16_t vo = v << 2;
                uint16_t p  = 2000 + ((rand() & 0x7ff) - 0x3ff);

                xreg_setw(AUD0_VOL + vo, pp ? 0x8010 : 0x1080);
                xreg_setw_next(/* AUD0_PERIOD + vo, */ p);
                xreg_setw_next(/* AUD0_LENGTH + vo, */ (pingpong_length[pp]));
                xreg_setw_next(/* AUD0_START + vo,  */ (pingpong_addr[pp]));

                xm_setbl(INT_CTRL, vb);

                xr_pos(0, 8 + v);
                xr_printf("%d #%3d Play %s %4d", v, plays, pp ? "pong" : "ping", p);

                plays++;
            }
            rand();
        }
        delay_check(1);
    }
    xm_setbl(INT_CTRL, 0xF);
    do
    {
        ic = xm_getw(INT_CTRL);
    } while ((ic & 0xf) != 0xf);

    play_silence();
    delay_check(DELAY_TIME * 10);

    free(pingpong_sample[1]);
    free(pingpong_sample[0]);
}
#endif

static void test_xr_read()
{
    xv_prep();

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
            xmem_setw_next(~v);        // toggle to prove read and set in VRAM
        }

        wait_vblank_start();
    }

    xreg_setw(PA_DISP_ADDR, 0x0000);
    xreg_setw(PA_GFX_CTRL, 0x0000);
    xreg_setw(PA_TILE_CTRL, 0x000F);
    delay_check(DELAY_TIME * 2);
}

void set_alpha_slow(int alpha)
{
    xv_prep();

    uint16_t a = (alpha & 0xf) << 12;
    for (int i = XR_COLOR_ADDR; i < XR_COLOR_ADDR + 256; i++)
    {
        uint16_t v = (xmem_getw_wait(i) & 0xfff) | a;
        xmem_setw_next(v);
    }
}

static void set_alpha(int alpha)
{
    xv_prep();

    uint16_t a = (alpha & 0xf) << 12;
    for (int i = XR_COLOR_ADDR; i < XR_COLOR_ADDR + 256; i++)
    {
        uint16_t v = (xmem_getw_wait(i) & 0xfff) | a;
        xmem_setw_next(v);
    }
}
#if BLURB_AUDIO
void * xosera_audio;
int    xosera_audio_len;
#endif

uint32_t test_count;
void     xosera_test()
{
    xv_prep();

    dprintf("Xosera_test_m68k\n");

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

    dprintf("Calling xosera_init(XINIT_CONFIG_640x480)...");
    bool success = xosera_init(XINIT_CONFIG_640x480);
    dprintf("%s (%dx%d)\n\n", success ? "succeeded" : "FAILED", xosera_vid_width(), xosera_vid_height());

    if (!success)
    {
        dprintf("Exiting without Xosera init.\n");
        exit(1);
    }
    last_timer_val = xm_getw(TIMER);

    xosera_get_info(&initinfo);
    dump_xosera_regs();
    init_audio();

    while (checkchar())        // clear any queued input
    {
        readchar();
    }
    cpu_delay(3000);

    // dprintf("\nPress key to begin...\n");
    // readchar();

    wait_vblank_start();
    xreg_setw(PA_GFX_CTRL, 0x0080);            // PA blanked
    xreg_setw(VID_CTRL, 0x0001);               // border color #1 (blue)
    xmem_setw(XR_COLOR_A_ADDR, 0x0000);        // color # = black
    xr_textmode_pb();
    xreg_setw(VID_CTRL, 0x0001);                      // border color #0
    xmem_setw(XR_COLOR_B_ADDR + 0xFF, 0xFfff);        // color # = black
    xr_msg_color(0x0f);
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

#if AUDIO_CHAINING_TEST
    test_audio_ping_pong();
#endif

#if INTERACTIVE_AUDIO_TEST        // audio waveform test
    if (load_test_audio("/ping_8000.raw", &testsamp, &testsampsize))
    {
        test_audio_sample("ping_8000.raw", testsamp, testsampsize, 3150);

        memset(testsamp, 0, testsampsize);

        free(testsamp);
    }
    if (load_test_audio("/xosera_8000.raw", &testsamp, &testsampsize))
    {
        test_audio_sample("xosera_8000.raw", testsamp, testsampsize, 3150);

        memset(testsamp, 0, testsampsize);

        free(testsamp);
    }
    if (load_test_audio("/Boing.raw", &testsamp, &testsampsize))
    {
        test_audio_sample("Boing.raw", testsamp, testsampsize, 3150);

        memset(testsamp, 0, testsampsize);

        free(testsamp);
    }
    {
        test_audio_sample("sine wave", sinData, sizeof(sinData), 1000);
    }
#endif


    // test_8bpp_tiled();

    // play_blurb_sample(0, 0x7fff, 800);

    // readchar();

    // uint16_t v = 7;
    // while (1)
    // {
    //     xmem_setw(XR_TILE_ADDR, v++);
    // }

    xr_textmode_pb();
    xreg_setw(VID_CTRL, 0x0001);                      // border color #1
    xmem_setw(XR_COLOR_B_ADDR + 0xFF, 0xFfff);        // color # = black
    xr_msg_color(0x0f);
    xr_printfxy(5, 0, "xosera_test_m68k\n");

    if (use_sd)
    {
        xr_printf("\nLoading test assets:\n");
        xr_printf(" \xAF 320x240 pac-mock ");
        load_test_image(BM_4_BIT, "/pacbox-320x240.raw", "/pacbox-320x240_pal.raw");
        xr_printf(" \xAF 320x200 King Tut ");
        load_test_image(BM_4_BIT_RETRO, "/ST_KingTut_Dpaint_16.raw", "/ST_KingTut_Dpaint_16_pal.raw");
        xr_printf(" \xAF 640x480 Shuttle  ");
        load_test_image(BM_MONO_ATTR, "/space_shuttle_color_small.raw", NULL);
        xr_printf(" \xAF RGB-12 Parrot    ");
        load_test_image(BM_12_BIT, "/parrot_320x240_RG8B4.raw", "/true_color_pal.raw");
        xr_printf(" \xAF Xosera 8-bpp     ");
        load_test_image(BM_8_BIT, "/xosera_r1.raw", "/xosera_r1_pal.raw");
#if BLURB_AUDIO
        if (num_audio_channels)
        {
            xr_printf(" \xAF Xark audio clip  ");
            load_test_audio("/xosera_8000.raw", &xosera_audio, &xosera_audio_len);
        }
#endif
    }

    // D'oh! Uses timer    rosco_m68k_CPUMHz();
#if 1
    uint16_t ic = xm_getw(INT_CTRL);
    dprintf("Installing interrupt handler.  INT_CTRL=0x%04x\n", ic);
    install_intr();
    xm_setw(TIMER, 10 - 1);        // color cycle twice a second as TIMER_INTR test
    ic = xm_getw(INT_CTRL);
    dprintf("Done.                          INT_CTRL=0x%04x\n", ic);

#else
    dprintf("NOT Installing interrupt handler\n");
#endif

    uint8_t config_num = 0;

    while (true)
    {
        uint32_t t = XFrameCount;
        uint16_t h = t / (60 * 60 * 60);
        uint16_t m = t / (60 * 60) % 60;
        uint16_t s = (t / 60) % 60;

        if (test_count && (test_count & 3) == 0)
        {
            config_num++;
            dprintf("\n [ xosera_init(%u)...", config_num % 3);
            bool success = xosera_init(config_num % 3);
            dprintf("%s (%dx%d) ]\n", success ? "succeeded" : "FAILED", xosera_vid_width(), xosera_vid_height());
            last_timer_val = xm_getw(TIMER);
            init_audio();
#if 1
            uint16_t ic = xm_getw(INT_CTRL);
            dprintf("Installing interrupt handler.  INT_CTRL=0x%04x\n", ic);
            install_intr();
            ic = xm_getw(INT_CTRL);
            dprintf("Done.                          INT_CTRL=0x%04x\n", ic);

#else
            dprintf("NOT Installing interrupt handler\n");
#endif
            cpu_delay(1000);        // give monitor time to adjust with grey screen (vs black)
        }
        dprintf("\n*** xosera_test_m68k iteration: %lu, running %u:%02u:%02u\n", test_count++, h, m, s);

        setup_margins();

#if COPPER_TEST
        if (test_count & 1)
        {
            setup_copper_fx();
            dprintf("Copper effect \"%s\" enabled for this interation.\n", cop_fx_ptr->name);
            install_copper();
            xreg_setw(COPP_CTRL, 0x8000);
        }
        else
        {
            dprintf("Copper disabled for this iteration.\n");
            xreg_setw(COPP_CTRL, 0x0000);
            xreg_setw(PA_H_SCROLL, 0);
            xreg_setw(PB_V_SCROLL, 0);
        }
#endif
        if (test_count & 2)
        {
            dprintf("Color cycling enabled for this iteration.\n");
            NukeColor = 0;
        }
        else
        {
            dprintf("Color cycling disabled for this iteration.\n");
            NukeColor = 0xffff;
        }

        wait_vblank_start();
        restore_colors();
        dupe_colors(0xf);
        xmem_setw(XR_COLOR_B_ADDR, 0x0000);        // make sure we can see plane A under B

        xr_textmode_pb();
        xr_msg_color(0x0f);
        xr_printfxy(5, 0, "xosera_test_m68k\n");

        xreg_setw(PA_GFX_CTRL, 0x0000);
        xreg_setw(PA_TILE_CTRL, 0x000F);
        xreg_setw(PA_LINE_LEN, xosera_vid_width() >> 3);
        xreg_setw(PA_DISP_ADDR, 0x0000);
        //        xreg_setw(PA_H_SCROLL, 0x0000);
        //        xreg_setw(PA_V_SCROLL, 0x0000);
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

        // code to test mode switching problem
        // delay_check(DELAY_TIME);
        // test_mode_glitch();

#if BLURB_AUDIO
        if (num_audio_channels)
        {
            upload_audio(xosera_audio, 0x2000, xosera_audio_len);
            play_blurb_sample(0x2000, xosera_audio_len, 8000);
        }
#endif

        //        xreg_setw(PA_GFX_CTRL, 0x0080);
        xreg_setw(VID_CTRL, 0x0000);
        delay_check(DELAY_TIME * 3);

        restore_colors();

        test_vram_speed();

        test_colormap();

        test_blend();

        if (use_sd)
        {
            test_blit();
        }

        if (use_sd)
        {
            xm_setbh(SYS_CTRL, 0x07);        // disable Xosera vsync interrupt

            show_test_pic(TRUECOLOR_TEST_PIC, 0x0000);
#if 0 && BLURB_AUDIO        // play audio while 8bpp+4bpp showing (but corrupts parrot)
            if (num_audio_channels)
            {
                upload_audio(xosera_audio, 0x2000, xosera_audio_len);
                play_blurb_sample(0x2000, xosera_audio_len, 8000);
            }
#endif
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
