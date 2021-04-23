#ifndef XOSERA_ARDUINO
#include <stdio.h>
#include <stdlib.h>
#include "platform.h"
#include "xosera_tester_fast.h"
#endif

#define delay_ms delay

static bool     error_flag = false;
static uint8_t  cur_color = 0x02;        // color for status line (green or red after error)
static uint16_t width;                   // in pixels
static uint16_t height;                  // in pixels
static uint16_t features;                // feature bits
static uint8_t  columns;                 // in texts chars (words)
static uint8_t  rows;                    // in texts chars (words)
static uint16_t data = 0x0100;           // test "data" value
static uint16_t addr;                    // test starting address (to leave status line)
static uint16_t rdata;
static uint32_t errors;        // read verify error count
static uint32_t count;         // read verify error count

const PROGMEM uint16_t defpal[16] = {
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

const PROGMEM uint16_t greypal[16] = {
    0x0000,        // black
    0x0111,        // blue
    0x0222,        // green
    0x0333,        // cyan
    0x0444,        // red
    0x0555,        // magenta
    0x0666,        // brown
    0x0777,        // light gray
    0x0888,        // dark gray
    0x0999,        // light blue
    0x0aaa,        // light green
    0x0bbb,        // light cyan
    0x0ccc,        // light red
    0x0ddd,        // light magenta
    0x0eee,        // yellow
    0x0FFF         // white
};

static void xcolor(uint8_t color)
{
    uint16_t wa = xvid_getw(XVID_WR_ADDR);
    xvid_sethb(XVID_DATA, color);
    xvid_setw(XVID_WR_ADDR, wa);
}

static uint8_t ln;

static void xhome()
{
    // home wr addr
    read_Settings();
    xvid_setw(XVID_WR_INC, 1);
    xvid_setw(XVID_WR_ADDR, 0);
    ln = 0;
    xcolor(cur_color);        // green-on-black
}

static void xpos(uint8_t h, uint8_t v)
{
    xvid_setw(XVID_WR_INC, 1);
    xvid_setw(XVID_WR_ADDR, (v * columns) + h);
    xcolor(cur_color);        // green-on-black
    ln = v;
}

static void xcls(uint8_t v = ' ')
{
    // clear screen
    xhome();
    for (uint16_t i = 0; i < (columns * rows); i++)
    {
        xvid_setlb(XVID_DATA, v);
    }
    xvid_setw(XVID_WR_ADDR, 0);
}

static void xprint_P(const char * s)
{
    uint8_t c;
    while ((c = pgm_read_byte(s++)) != '\0')
    {
        if (c == '\n')
        {
            xvid_setw(XVID_WR_ADDR, ++ln * columns);
            continue;
        }
        xvid_setlb(XVID_DATA, c);
    }
}

static void xprint_P_rainbow(uint8_t color, const char * s)
{
    uint8_t c;
    xcolor(color);
    while ((c = pgm_read_byte(s++)) != '\0')
    {
        if (c == '\n')
        {
            xvid_setw(XVID_WR_ADDR, ++ln * columns);
            color = (color + 1) & 0xf;
            if (color == 0)        // skip black
                color++;
            xcolor(color);
            continue;
        }
        xvid_setlb(XVID_DATA, c);
    }
}

#define xprint(str)                                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        static const PROGMEM char _str[] = str;                                                                        \
        xprint_P(_str);                                                                                                \
    } while (0)

static void xprint_hex(uint16_t v)
{
    static const PROGMEM char hex[16] = {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
    xvid_setw(XVID_DATA, (cur_color << 8) | pgm_read_byte(hex + ((v >> 12) & 0xf)));
    xvid_setw(XVID_DATA, (cur_color << 8) | pgm_read_byte(hex + ((v >> 8) & 0xf)));
    xvid_setw(XVID_DATA, (cur_color << 8) | pgm_read_byte(hex + ((v >> 4) & 0xf)));
    xvid_setw(XVID_DATA, (cur_color << 8) | pgm_read_byte(hex + ((v >> 0) & 0xf)));
}

static void xprint_int(uint32_t n)
{
    uint32_t poten = 100000000;
    uint32_t v     = n;
    if (v > 999999999) v = 999999999;
    while (poten)
    {
        uint8_t d = v / poten;
        if (d || n > poten)
        {
            xvid_setlb(XVID_DATA, '0' + d);
        }
        v -= d * poten;
        poten = poten / 10;
    }
}

static void xprint_dec(uint16_t n)
{
    uint16_t poten = 10000;
    uint16_t v     = n;
    while (poten)
    {
        uint8_t d = v / poten;
        if (d || n > poten)
        {
            xvid_setlb(XVID_DATA, '0' + d);
        }
        else
        {
            xvid_setlb(XVID_DATA, ' ');
        }
        v -= d * poten;
        poten = poten / 10;
    }
}

// read scanline register and wait for non-visible line (bit[15])
static void wait_vsync(uint16_t num = 1)
{
    uint8_t v_flag;
    while (num--)
    {
        do
        {
            xvid_setlb(XVID_AUX_ADDR, AUX_VID_R_SCANLINE);        // set scanline reg
            v_flag = xvid_gethb(XVID_AUX_DATA);                   // read scanline upper byte
        } while ((v_flag & 0x80));                                // loop if on blanked line
        do
        {
            xvid_setlb(XVID_AUX_ADDR, AUX_VID_R_SCANLINE);        // set scanline reg
            v_flag = xvid_gethb(XVID_AUX_DATA);                   // read scanline upper byte
        } while (!(v_flag & 0x80));                               // loop if on visible line
    }
}


static void error(const char * msg, uint16_t addr, uint16_t rdata, uint16_t vdata)
{
    errors++;
    Serial.println("");
    Serial.print(msg);
    Serial.print(" (at ");
    platform_print_hex(addr);
    Serial.print(" rd=");
    platform_print_hex(rdata);
    Serial.print(" vs ");
    platform_print_hex(vdata);
    Serial.print(") Errors: ");
    platform_print_dec(errors);

    platform_on_error();
    error_flag = true;
}

static void read_Settings()
{

    xvid_setw(XVID_AUX_ADDR, AUX_VID_R_WIDTH);        // select width
    width = xvid_getw(XVID_AUX_DATA);

    xvid_setw(XVID_AUX_ADDR, AUX_VID_R_HEIGHT);        // select height
    height = xvid_getw(XVID_AUX_DATA);

    xvid_setw(XVID_AUX_ADDR, AUX_VID_R_FEATURES);        // select features
    features = xvid_getw(XVID_AUX_DATA);
}

static void reboot_Xosera(uint8_t config)
{
#if 1
    Serial.print("Xosera resetting, switching to config #");
    platform_print_dec(config & 0x3);
    xvid_setw(XVID_BLIT_CTRL, 0x8080 | ((config & 0x3) << 8));        // reboot FPGA to config
#endif
    do
    {
        delay(20);
        Serial.print(".");
        xvid_setw(XVID_RD_ADDR, 0x1234);
        xvid_setw(XVID_CONST, 0xABCD);
    } while (xvid_getw(XVID_RD_ADDR) != 0x1234 || xvid_getw(XVID_CONST) != 0xABCD);

    read_Settings();
    Serial.print("(");
    platform_print_dec(width);
    Serial.print("x");
    platform_print_dec(height);
    Serial.print(" Feature bits:");
    platform_print_bin(features);
    Serial.println(").  Xosera ready.");

    columns = width / 8;
    rows    = height / 16;
    addr    = columns;
}

void setup()
{
    platform_setup();

    reboot_Xosera(0);

    delay(2000);        // let the stunning boot logo display. :)

    randomSeed(0xc0ffee42);        // deterministic seed TODO
}

const PROGMEM char blurb[] =
    "01234567890123456789012345678901234567890123456789012345678901234567890123456789\n"
    "\n"
    "Xosera is an FPGA based video adapter designed with the rosco_m68k retro\n"
    "computer in mind. Inspired in concept by it's \"namesake\" the Commander X16's\n"
    "VERA, Xosera is an original open-source video adapter design, built with open-\n"
    "source tools, that is being tailored with features appropriate for a Motorola\n"
    "68K era retro computer.\n"
    "\n"
    "  \xf9  VGA or HDMI/DVI output at 848x480 or 640x480 (16:9 or 4:3 @ 60Hz)\n"
    "  \xf9  16 or 256 color palette out of 4096 colors (12-bit RGB)\n"
    "  \xf9  128KB of embedded video RAM (16-bit words @33/25 MHz)\n"
    "  \xf9  Character tile based modes with color attribute byte\n"
    "  \xf9  Pixel doubled bitmap modes (e.g. 424x240 or 320x240)\n"
    "  \xf9  Smooth horizontal and vertical tile scrolling\n"
    "  \xf9  8x8 or 8x16 character tile size (or truncated e.g., 8x10)\n"
    "  \xf9  Register based interface with 16 16-bit registers\n"
    "  \xf9  Read/write VRAM with programmable read/write address increment\n"
    "  \xf9  Full speed bus interface (with MOVEP) for rosco_m68k (by Ross Bamford)\n"
    "  \xf9  Multiple fonts (2KB per 8x8 fonts, 4K per 8x16 font)\n"
    "  \xf9  \"Blitter\" for fast VRAM copy & fill operations (TODO, but used at init)\n"
    "  \xf9  2-D operations \"blitter\" with modulo and shifting/masking (TODO)\n"
    "  \xf9  Dual overlayed \"planes\" of video (TODO)\n"
    "  \xf9  Wavetable stereo audio (TODO, spare debug IO for now)\n"
    "  \xf9  Bit-mapped 16 and 256 color graphics modes (256 color TODO)\n"
    "  \xf9  16-color tile mode with \"game\" attributes (e.g., mirroring) (TODO)\n"
    "  \xf9  At least one \"cursor\" sprite (and likely more, TODO)\n";

void show_blurb()
{
    // Show some text
    Serial.println("Blurb text");
    xcls();
    xprint_P(blurb);
    delay(500);

    // 2nd font (ST 8x8)
    Serial.println("ST 8x8 font");
    xvid_setw(XVID_AUX_ADDR, AUX_VID_W_FONTCTRL);        // A_font_ctrl
    xvid_setw(XVID_AUX_DATA, 0x0207);                    // 2nd font in bank 2, 8 high
    delay(500);

    // 3rd font (hex 8x8 debug)
    Serial.println("hex 8x8 font");
    xvid_setw(XVID_AUX_ADDR, AUX_VID_W_FONTCTRL);        // A_font_ctrl
    xvid_setw(XVID_AUX_DATA, 0x0307);                    // 3st font in bank 3, 8 high
    delay(500);

    // restore 1st font (ST 8x16)
    Serial.println("ST 8x16 font");
    xvid_setw(XVID_AUX_ADDR, AUX_VID_W_FONTCTRL);        // A_font_ctrl
    xvid_setw(XVID_AUX_DATA, 0x000F);                    // back to 1st font in bank 0, 16 high
    delay(500);

    // shrink font height
    Serial.println("Shrink font height");
    for (int v = 15; v >= 0; v--)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_VID_W_FONTCTRL);        // A_font_ctrl
        // set font height and switch to 8x8 font when < 8
        xvid_setw(XVID_AUX_DATA, (v < 8 ? 0x0200 : 0) | v);

        wait_vsync(5);
    }

    Serial.println("Grow font height");
    for (int v = 0; v < 16; v++)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_VID_W_FONTCTRL);        // A_font_ctrl
        // set font height and switch to 8x8 font when < 8
        xvid_setw(XVID_AUX_DATA, (v < 8 ? 0x0200 : 0) | v);
        wait_vsync(5);
    }

    // restore 1st font (ST 8x16)
    Serial.println("ST 8x16 font");
    xvid_setw(XVID_AUX_ADDR, AUX_VID_W_FONTCTRL);        // A_font_ctrl
    xvid_setw(XVID_AUX_DATA, 0x000F);                    // back to 1st font in bank 0, 16 high
    delay(500);

    Serial.println("Scroll via video VRAM display address");
    int16_t r = 0;
    for (uint16_t i = 0; i < static_cast<uint16_t>(rows * 3); i++)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_VID_W_DISPSTART);        // set text start addr
        xvid_setw(XVID_AUX_DATA, r * columns);                // to one line down
        for (int8_t f = 0; f < 16; f++)
        {
            xvid_setw(XVID_AUX_ADDR, AUX_VID_W_SCROLLXY);        // v fine scroll
            xvid_setw(XVID_AUX_DATA, f);

            wait_vsync(1);
        }
        if (++r > (rows * 2))
        {
            r = -rows;
        }
    }
    xvid_setw(XVID_AUX_ADDR, AUX_VID_W_DISPSTART);        // reset text start addr
    xvid_setw(XVID_AUX_DATA, 0x0000);
    xvid_setw(XVID_AUX_ADDR, AUX_VID_W_SCROLLXY);        // reset text start addr
    xvid_setw(XVID_AUX_DATA, 0x0000);
    delay(500);
#if 1
    Serial.println("Horizontal fine scroll");
    for (int x = 0; x < 8; x++)
    {
        wait_vsync();
        xvid_setw(XVID_AUX_ADDR, AUX_VID_W_SCROLLXY);        // scroll
        // set font height and switch to 8x8 font when < 8
        xvid_setw(XVID_AUX_DATA, x << 8);
        delay(500);
    }
    for (int x = 7; x > 0; x--)
    {
        wait_vsync();
        xvid_setw(XVID_AUX_ADDR, AUX_VID_W_SCROLLXY);        // scroll
        // set font height and switch to 8x8 font when < 8
        xvid_setw(XVID_AUX_DATA, x << 8);
        delay(500);
    }
    delay(1000);
#endif
#if 0
    Serial.println("Vertical fine scroll");
    for (int x = 0; x < 16; x++)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_VID_W_SCROLLXY);        // scroll
        // set font height and switch to 8x8 font when < 8
        xvid_setw(XVID_AUX_DATA, x << 8);
        delay(100);
    }
    for (int x = 15; x > 0; x--)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_VID_W_SCROLLXY);        // scroll
        // set font height and switch to 8x8 font when < 8
        xvid_setw(XVID_AUX_DATA, x << 8);
        delay(100);
    }
    delay(1000);
#endif
}

void test_palette()
{
    Serial.println("palette test");
    xcls();
    xcolor(0xf);
    xprint_P(blurb);

    // restore default palette
    Serial.println("restore palette test");
    for (uint8_t i = 0; i < 16; i++)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | i);                // use WR address for palette index
        xvid_setw(XVID_AUX_DATA, pgm_read_word(greypal + i));        // set palette data
    }
    delay(500);

    Serial.println("Rosco rainbow cycle");
    for (uint16_t k = 0; k < 500; k++)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_VID_R_SCANLINE);        // set scanline reg
        uint16_t l = xvid_getw(XVID_AUX_DATA);               // read scanline
        l          = l | ((0xf - (l & 0xf)) << 8);           // invert blue for some red
        xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | 0);        // set palette entry #0
        xvid_setw(XVID_AUX_DATA, l);                         // set palette data

        wait_vsync();
    }
    xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | 0);        // set palette entry #0
    xvid_setw(XVID_AUX_DATA, 0x0104);                    // set palette data

    xhome();
    xprint_P_rainbow(1, blurb);
    delay(500);
    // color cycle palette
    Serial.println("color cycle nuclear glow");
    for (uint16_t k = 0; k < 500; k++)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | 2);
        uint16_t n = random(0x0fff) & 0x777;
        xvid_setw(XVID_AUX_DATA, n);        // set palette data
        wait_vsync();
    }
    delay(5000);
    // restore default palette
    Serial.println("restore palette test");
    for (uint8_t i = 0; i < 16; i++)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | i);               // use WR address for palette index
        xvid_setw(XVID_AUX_DATA, pgm_read_word(defpal + i));        // set palette data
    }

    Serial.println("double wide");
    xvid_setw(XVID_AUX_ADDR, AUX_VID_W_GFXCTRL);        // use WR address for palette index
    xvid_setw(XVID_AUX_DATA, 0x0001);                   // set palette data
    delay(2000);
    Serial.println("double wide");
    xvid_setw(XVID_AUX_ADDR, AUX_VID_W_GFXCTRL);        // use WR address for palette index
    xvid_setw(XVID_AUX_DATA, 0x0000);                   // set palette data
    delay(1000);

#if 1        // TODO: something funny here (blanks 9" monitor)?
    // gray default palette
    Serial.println("grey palette test");
    for (uint8_t i = 0; i < 16; i++)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | i);        // use WR address for 595e index
        xvid_setw(XVID_AUX_DATA, i);                         // set palette data
    }
    wait_vsync(60);
    for (uint8_t i = 0; i < 16; i++)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | i);        // use WR address for 595e index
        xvid_setw(XVID_AUX_DATA, i << 4);                    // set palette data
    }
    wait_vsync(60);
    for (uint8_t i = 0; i < 16; i++)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | i);        // use WR address for 595e index
        xvid_setw(XVID_AUX_DATA, i << 8);                    // set palette data
    }
    wait_vsync(60);
    for (uint8_t i = 0; i < 16; i++)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | i);        // use WR address for 595e index
        uint16_t v = i ? 0x000 : 0xfff;
        xvid_setw(XVID_AUX_DATA, v);        // set palette data
    }
    wait_vsync(60);
    for (uint8_t i = 0; i < 16; i++)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | i);        // use WR address for 595e index
        uint16_t v = i ? 0xfff : 0x000;
        xvid_setw(XVID_AUX_DATA, v);        // set palette data
    }
    wait_vsync(60);
    for (uint8_t i = 0; i < 16; i++)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | i);        // use WR address for 595e index
        uint16_t v = i ? 0xfff : 0xfff;
        xvid_setw(XVID_AUX_DATA, v);        // set palette data
    }
    wait_vsync(60);
    for (uint8_t i = 0; i < 16; i++)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | i);               // use WR address for palette index
        xvid_setw(XVID_AUX_DATA, pgm_read_word(defpal + i));        // set palette data
    }
#endif

    // restore default palette
    Serial.println("restore palette test");
    for (uint8_t i = 0; i < 16; i++)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | i);               // use WR address for palette index
        xvid_setw(XVID_AUX_DATA, pgm_read_word(defpal + i));        // set palette data
    }

    // color cycle palette
    Serial.println("color cycle palette test");
    uint8_t n = 3, m = 7;
    for (uint16_t k = 0; k < 5; k++)
    {
        for (uint8_t j = 1; j < 16; j++)
        {
            for (uint8_t i = 1; i < 16; i++)
            {
                xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | j);        // use WR address for palette index
                n = (m + n) & 0xf;
                if (!n)
                {
                    n++;
                }
                xvid_setw(XVID_AUX_DATA, pgm_read_word(defpal + n));        // set palette data
            }
            wait_vsync();
        }
        n += 1;
        m += 3;
    }

    // restore default palette
    Serial.println("restore palette test");
    for (uint8_t i = 0; i < 16; i++)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | i);               // use WR address for palette index
        xvid_setw(XVID_AUX_DATA, pgm_read_word(defpal + i));        // set palette data
    }
    delay(1000);
}

static const PROGMEM uint16_t data_pat[8] = {0xA5A5, 0x5A5A, 0xFFFF, 0x0123, 0x4567, 0x89AB, 0xCDEF, 0x0220};

void test_reg_access()
{
    xvid_setw(XVID_AUX_ADDR, AUX_VID_W_DISPSTART);        // reset text start addr
    xvid_setw(XVID_AUX_DATA, columns);
    xvid_setw(XVID_AUX_ADDR, AUX_VID_W_TILEWIDTH);        // reset text start addr
    xvid_setw(XVID_AUX_DATA, columns);
    xvid_setw(XVID_AUX_ADDR, AUX_VID_W_SCROLLXY);        // reset text start addr
    xvid_setw(XVID_AUX_DATA, 0x0000);
    xcls();
    xprint("Xosera read/write register self-test...\n");

    for (uint8_t r = XVID_AUX_ADDR; r <= XVID_WR_ADDR; r++)
    {
        xhome();
        xpos(4, 4 + r);
        xprint("Register: ");
        switch (r)
        {
            case XVID_AUX_ADDR: {
                xprint("XVID_AUX_ADDR");
            }
            break;
            case XVID_CONST: {
                xprint("XVID_CONST   ");
            }
            break;
            case XVID_RD_ADDR: {
                xprint("XVID_RD_ADDR ");
            }
            break;
            case XVID_WR_ADDR: {
                xprint("XVID_WR_ADDR ");
            }
            break;
        }
        xprint(" <=> ");

        uint16_t cp = xvid_getw(XVID_WR_ADDR);
        uint16_t v  = 0;
        do
        {
            if ((v & 0xf) == 0xf)
            {
                xvid_setw(XVID_WR_ADDR, cp);
                xcolor(cur_color);
                xprint_hex(v);
            }
            xvid_setw(r, v);
            rdata = xvid_getw(r);
            if (rdata != v)
            {
                error("reg verify", r, rdata, v);
                break;
            }
        } while (--v);
        if (!error_flag)
        {
            xvid_setw(XVID_WR_ADDR, cp);
            xcolor(0x0a);
            xprint("PASSED");
            xcolor(cur_color);
        }
        else
        {
            xvid_setw(XVID_WR_ADDR, cp);
            xcolor(cur_color);
            xprint("FAILED");
        }
    }

    if (error_flag)
    {
        xpos(0, 8);
        xprint("Register self-test FAILED!");
        delay(2000);
    }
    else
    {
        xpos(0, 8);
        xprint("Register self-test passed.");
    }

    xpos(0, 12);
    xprint("VRAM read/write check...");

    delay(1000);

    xpos(4, 14);
    xprint("VRAM[");
    uint16_t ap = xvid_getw(XVID_WR_ADDR);
    xprint("    ] <=> ");
    uint16_t vp = xvid_getw(XVID_WR_ADDR);

    for (uint8_t i = 0; i < 8; i++)
    {
        uint16_t v = pgm_read_word(data_pat + i);
        xvid_setw(XVID_WR_ADDR, vp);
        xcolor(cur_color);
        xprint_hex(v);

        for (uint16_t a = (rows / 2) * columns; a != 0; a++)
        {
            if ((a & 0xff) == 0xfff)
            {
                xvid_setw(XVID_WR_ADDR, ap);
                xcolor(cur_color);
                xprint_hex(a);
            }
            xvid_setw(XVID_WR_ADDR, a);
            xvid_setw(XVID_DATA, v);
            xvid_setw(XVID_RD_ADDR, a);
            rdata = xvid_getw(XVID_DATA);
            if (rdata != v)
            {
                error("VRAM test", a, rdata, v);
                break;
            }
        }
        if (error_flag)
        {
            break;
        }
    }

    if (error_flag)
    {
        xpos(0, 16);
        xprint("VRAM check FAILED!");
    }
    else
    {
        xpos(0, 16);
        xprint("VRAM check passed.");
    }

    delay(2000);
}

void vram_speed()
{
    xcls();
    xprint("VRAM 16-bit write test, 128KB word:");
    xprint_hex(data);
    xprint("\n");

    // write test
    {
        xvid_setw(XVID_WR_ADDR, addr);
        uint16_t test_time  = millis();
        uint16_t start_time = millis();
        while (start_time == test_time)        // start on "fresh" millisecond to reduce jitter
        {
            start_time = millis();
        }
        uint16_t i = 0;
        do
        {
            xvid_setw(XVID_DATA, data);
        } while (i++);
        uint16_t elapsed_time = millis() - start_time;
        xhome();
        xprint("VRAM 16-bit write test, 128KB word:");
        xprint_hex(data);
        xprint(" (Time:");
        xprint_dec(elapsed_time);
        xprint(" ms");
        xprint(")\n");
        delay(500);
    }

    xhome();
    xprint("VRAM  8-bit write test, 128KB word:");
    xprint_hex(data);
    xprint("\n");

    // write test
    {
        xvid_setw(XVID_WR_ADDR, addr);
        uint16_t test_time  = millis();
        uint16_t start_time = millis();
        while (start_time == test_time)        // start on "fresh" millisecond to reduce jitter
        {
            start_time = millis();
        }
        uint16_t i = 0;
        do
        {
            xvid_setlb(XVID_DATA, data);
        } while (i++);
        uint16_t elapsed_time = millis() - start_time;
        xhome();
        xcolor(cur_color);
        xprint("VRAM  8-bit write test, 128KB word:");
        xprint_hex(data);
        xprint(" (Time:");
        xprint_dec(elapsed_time);
        xprint(" ms");
        xprint(")\n");
        delay(500);
    }

    xhome();
    xprint("VRAM 16-bit read test, 128KB word:");
    xprint_hex(data);
    xprint("\n");

    // read test
    {
        xvid_setw(XVID_WR_ADDR, addr);
        uint16_t test_time  = millis();
        uint16_t start_time = millis();
        while (start_time == test_time)        // start on "fresh" millisecond to reduce jitter
        {
            start_time = millis();
        }
        uint16_t i = columns;
        do
        {
            rdata = xvid_getw(XVID_DATA);
            if (rdata != data)
            {
                error("16-bit read", addr, rdata, data);
                break;
            }
        } while (i++);
        i = columns;
        do
        {
            rdata = xvid_getw(XVID_DATA);
            if (rdata != data)
            {
                error("16-bit read", addr, rdata, data);
                break;
            }
        } while (--i);
        uint16_t elapsed_time = millis() - start_time;
        xhome();
        xprint("VRAM 16-bit read test, 128KB word:");
        xprint_hex(data);
        xprint(" (Time:");
        xprint_dec(elapsed_time);
        xprint(" ms");
        xprint(")\n");
        delay(500);
    }

    xhome();
    xprint("VRAM  8-bit read test, 128KB word:");
    xprint_hex(data);
    xprint("\n");

    // read test
    {
        xvid_setw(XVID_RD_ADDR, addr);
        uint16_t test_time  = millis();
        uint16_t start_time = millis();
        while (start_time == test_time)        // start on "fresh" millisecond to reduce jitter
        {
            start_time = millis();
        }
        uint16_t i = 0;
        do
        {
            rdata = (xvid_gethb(XVID_DATA) << 8) | xvid_getlb(XVID_DATA);
            if (rdata != data)
            {
                error("8-bit read", addr + i, rdata, data);
                break;
            }
        } while (i++ < 0x8000);
        uint16_t elapsed_time = millis() - start_time;
        xhome();
        xprint("VRAM  8-bit read test, 128KB word:");
        xprint_hex(data);
        xprint(" (Time:");
        xprint_dec(elapsed_time);
        xprint(" ms");
        xprint(")\n");
        delay(100);
    }
}

void vram_verify()
{
    xvid_setw(XVID_WR_ADDR, addr);
    xvid_setw(XVID_WR_INC, 1);

    {
        uint16_t i = columns;
        do
        {
            xvid_setw(XVID_DATA, data);
        } while (++i);
    }

    xvid_setw(XVID_RD_ADDR, addr);
    xvid_setw(XVID_RD_INC, 1);
    {
        uint16_t i = columns;
        do
        {
            rdata = xvid_getw(XVID_DATA);
            if (rdata != data)
            {
                error("VRAM read", addr + i, rdata, data);
                break;
            }
        } while (++i);
    }
}

void font_write()
{
    Serial.println("Font memory write");
    xcls();
    xprint_P(blurb);
    for (uint16_t a = 0; a < 4096; a += 4)
    {
        for (uint16_t b = a; b < (a + 4); b++)
        {
            xvid_setw(XVID_AUX_ADDR, AUX_W_FONT | b);
            // set font height and switch to 8x8 font when < 8
            xvid_setw(XVID_AUX_DATA, (a & 1) ? 0x5555 : 0xaaaa);
        }
        wait_vsync(1);
    }

    delay(1000);        // let monitor sync
}

#if 0
static void reg_read_test()
{
    static uint16_t v[4];
    for (uint8_t r = 0; r < 4; r++)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_VID_R_SCANLINE);        // set scanline reg
        v[r] =
    }
    xvid_setw(XVID_AUX_ADDR, AUX_VID_R_SCANLINE);        // set scanline reg
    uint16_t l = xvid_getw(XVID_AUX_DATA);               // read scanline
}
#endif

void activity()
{
    platform_activity();
}

void test_4096_colors()
{
    xcls();
    for (int c = 0; c < columns; c++)
    {
        xvid_setw(XVID_WR_ADDR, c);
        xvid_setw(XVID_WR_INC, columns);
        uint8_t color = (c / (columns / 16)) ^ 0xf;
        for (int y = 0; y < rows; y++)
        {
            xvid_setw(XVID_DATA, (color << 12) | (color << 8) | ' ');
        }
    }
    delay(500);

    for (int count = 0; count < (60 * 5); count++)
    {
        uint8_t v_flag;
        xvid_setw(XVID_AUX_ADDR, AUX_VID_R_SCANLINE);        // set scanline reg
        do
        {
            xvid_setlb(XVID_AUX_ADDR, AUX_VID_R_SCANLINE);        // set scanline reg
            v_flag = xvid_gethb(XVID_AUX_DATA);                   // read scanline upper byte
        } while ((v_flag & 0x80));                                // loop if in blank
        do
        {
            xvid_setlb(XVID_AUX_ADDR, AUX_VID_R_SCANLINE);        // set scanline reg
            v_flag = xvid_gethb(XVID_AUX_DATA);                   // read scanline upper byte
        } while (!(v_flag & 0x80));                               // loop if on visible line
        noInterrupts();
        do
        {
            xvid_setlb(XVID_AUX_ADDR, AUX_VID_R_SCANLINE);        // set scanline reg
            v_flag = xvid_gethb(XVID_AUX_DATA);                   // read scanline upper byte
        } while ((v_flag & 0x80));                                // loop if in blank

        uint8_t  l  = 0;
        uint16_t ls = 0;
        do
        {
            xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | 0);        // use WR address for palette index
            xvid_setw(XVID_AUX_DATA, ls);                        // set palette data
            for (uint8_t i = 1; i != 16; i++)
            {
                xvid_setlb(XVID_AUX_ADDR, AUX_W_COLORTBL | i);        // use WR address for palette index
                xvid_setw(XVID_AUX_DATA, ls | i);                     // set palette data
            }
            ls = ++l << 4;
            xvid_setw(XVID_AUX_ADDR, AUX_VID_R_SCANLINE);        // set scanline reg
            do
            {
                xvid_setlb(XVID_AUX_ADDR, AUX_VID_R_SCANLINE);        // set scanline reg
                v_flag = xvid_gethb(XVID_AUX_DATA);                   // read scanline upper byte
            } while (!(v_flag & 0x40));                               // loop if on visible line
        } while (l);
        xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | 0);        // use WR address for palette index
        xvid_setw(XVID_AUX_DATA, 0);                         // set palette data
        for (uint8_t i = 1; i != 16; i++)
        {
            xvid_setlb(XVID_AUX_ADDR, AUX_W_COLORTBL | i);        // use WR address for palette index
            xvid_setw(XVID_AUX_DATA, 0);                          // set palette data
        }
        interrupts();
    }
    for (uint8_t i = 0; i < 16; i++)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | i);               // use WR address for palette index
        xvid_setw(XVID_AUX_DATA, pgm_read_word(defpal + i));        // set palette data
    }

    delay(1000);
}

void test_smoothscroll()
{
    xcls();
    xprint_P_rainbow(1, blurb);
    xvid_setw(XVID_AUX_ADDR, AUX_VID_W_GFXCTRL);        // use WR address for palette index
    xvid_setw(XVID_AUX_DATA, 0x0000);                   // set palette data
    delay(2000);
    xvid_setw(XVID_AUX_ADDR, AUX_VID_W_GFXCTRL);        // use WR address for palette index
    xvid_setw(XVID_AUX_DATA, 0x0001);                   // set palette data
    delay(2000);
    xvid_setw(XVID_AUX_ADDR, AUX_VID_W_GFXCTRL);        // use WR address for palette index
    xvid_setw(XVID_AUX_DATA, 0x0002);                   // set palette data
    delay(2000);
    xvid_setw(XVID_AUX_ADDR, AUX_VID_W_GFXCTRL);        // use WR address for palette index
    xvid_setw(XVID_AUX_DATA, 0x0003);                   // set palette data
    delay(2000);
    xvid_setw(XVID_AUX_ADDR, AUX_VID_W_GFXCTRL);        // use WR address for palette index
    xvid_setw(XVID_AUX_DATA, 0x0000);                   // set palette data
    delay(2000);

    for (int r = 0; r < 2; r++)
    {
        for (int x = 0; x < 8; x++)
        {
            wait_vsync();
            xvid_setw(XVID_AUX_ADDR, AUX_VID_W_SCROLLXY);        // fine scroll
            xvid_setw(XVID_AUX_DATA, x << 8);
            delay_ms(150);
        }
        for (int x = 7; x >= 0; x--)
        {
            wait_vsync();
            xvid_setw(XVID_AUX_ADDR, AUX_VID_W_SCROLLXY);        // fine scroll
            xvid_setw(XVID_AUX_DATA, x << 8);
            delay_ms(150);
        }
    }

    for (int r = 0; r < 2; r++)
    {
        for (int x = 0; x < 8; x++)
        {
            wait_vsync(2);
            xvid_setw(XVID_AUX_ADDR, AUX_VID_W_SCROLLXY);        // fine scroll
            xvid_setw(XVID_AUX_DATA, x << 8);
        }
        for (int x = 7; x >= 0; x--)
        {
            wait_vsync(2);
            xvid_setw(XVID_AUX_ADDR, AUX_VID_W_SCROLLXY);        // fine scroll
            xvid_setw(XVID_AUX_DATA, x << 8);
        }
    }

    xvid_setw(XVID_AUX_ADDR, AUX_VID_W_TILEWIDTH);        // set width
    xvid_setw(XVID_AUX_DATA, columns * 2);
    xvid_setw(XVID_AUX_ADDR, AUX_VID_W_SCROLLXY);        // zero fine scroll
    xvid_setw(XVID_AUX_DATA, 0);

    for (int r = 0; r < 2; r++)
    {
        for (int x = 0; x < 100; x++)
        {
            xvid_setw(XVID_AUX_ADDR, AUX_VID_W_DISPSTART);        // start addr
            xvid_setw(XVID_AUX_DATA, x >> 3);
            xvid_setw(XVID_AUX_ADDR, AUX_VID_W_SCROLLXY);        // fine scroll
            xvid_setw(XVID_AUX_DATA, (x & 0x7) << 8);
            wait_vsync(1);
        }
        for (int x = 100; x >= 0; x--)
        {
            xvid_setw(XVID_AUX_ADDR, AUX_VID_W_DISPSTART);        // start addr
            xvid_setw(XVID_AUX_DATA, x >> 3);
            xvid_setw(XVID_AUX_ADDR, AUX_VID_W_SCROLLXY);        // fine scroll
            xvid_setw(XVID_AUX_DATA, (x & 0x7) << 8);
            wait_vsync(1);
        }
    }

    for (int r = 0; r < 2; r++)
    {
        for (int x = 0; x < 100; x++)
        {
            xvid_setw(XVID_AUX_ADDR, AUX_VID_W_DISPSTART);        // start addr
            xvid_setw(XVID_AUX_DATA, ((x >> 4) * (columns * 2)) + (x >> 3));
            xvid_setw(XVID_AUX_ADDR, AUX_VID_W_SCROLLXY);        // fine scroll
            xvid_setw(XVID_AUX_DATA, (x & 0x7) << 8 | (x & 0xf));
            wait_vsync(1);
        }
        for (int x = 100; x >= 0; x--)
        {
            xvid_setw(XVID_AUX_ADDR, AUX_VID_W_DISPSTART);        // start addr
            xvid_setw(XVID_AUX_DATA, ((x >> 4) * (columns * 2)) + (x >> 3));
            xvid_setw(XVID_AUX_ADDR, AUX_VID_W_SCROLLXY);        // fine scroll
            xvid_setw(XVID_AUX_DATA, (x & 0x7) << 8 | (x & 0xf));
            wait_vsync(1);
        }
    }

    xvid_setw(XVID_AUX_ADDR, AUX_VID_W_GFXCTRL);        // use WR address for palette index
    xvid_setw(XVID_AUX_DATA, 0x0001);                   // set palette data

    for (int r = 0; r < 2; r++)
    {
        for (int x = 0; x < 100; x++)
        {
            xvid_setw(XVID_AUX_ADDR, AUX_VID_W_DISPSTART);        // start addr
            xvid_setw(XVID_AUX_DATA, ((x >> 4) * (columns * 2)) + (x >> 4));
            xvid_setw(XVID_AUX_ADDR, AUX_VID_W_SCROLLXY);        // fine scroll
            xvid_setw(XVID_AUX_DATA, (x & 0xf) << 8 | (x & 0xf));
            wait_vsync(1);
        }
        for (int x = 100; x >= 0; x--)
        {
            xvid_setw(XVID_AUX_ADDR, AUX_VID_W_DISPSTART);        // start addr
            xvid_setw(XVID_AUX_DATA, ((x >> 4) * (columns * 2)) + (x >> 4));
            xvid_setw(XVID_AUX_ADDR, AUX_VID_W_SCROLLXY);        // fine scroll
            xvid_setw(XVID_AUX_DATA, (x & 0xf) << 8 | (x & 0xf));
            wait_vsync(1);
        }
    }

    xvid_setw(XVID_AUX_ADDR, AUX_VID_W_GFXCTRL);        // use WR address for palette index
    xvid_setw(XVID_AUX_DATA, 0x0003);                   // set palette data

    for (int r = 0; r < 2; r++)
    {
        for (int x = 0; x < 100; x++)
        {
            xvid_setw(XVID_AUX_ADDR, AUX_VID_W_DISPSTART);        // start addr
            xvid_setw(XVID_AUX_DATA, ((x >> 5) * (columns * 2)) + (x >> 4));
            xvid_setw(XVID_AUX_ADDR, AUX_VID_W_SCROLLXY);        // fine scroll
            xvid_setw(XVID_AUX_DATA, (x & 0xf) << 8 | (x & 0x1f));
            wait_vsync(1);
        }
        for (int x = 100; x >= 0; x--)
        {
            xvid_setw(XVID_AUX_ADDR, AUX_VID_W_DISPSTART);        // start addr
            xvid_setw(XVID_AUX_DATA, ((x >> 5) * (columns * 2)) + (x >> 4));
            xvid_setw(XVID_AUX_ADDR, AUX_VID_W_SCROLLXY);        // fine scroll
            xvid_setw(XVID_AUX_DATA, (x & 0xf) << 8 | (x & 0x1f));
            wait_vsync(1);
        }
    }

    xvid_setw(XVID_AUX_ADDR, AUX_VID_W_DISPSTART);        // start addr
    xvid_setw(XVID_AUX_DATA, 0x0000);                     // set palette data
    xvid_setw(XVID_AUX_ADDR, AUX_VID_W_SCROLLXY);         // fine scroll
    xvid_setw(XVID_AUX_DATA, 0x0000);                     // set palette data
    xvid_setw(XVID_AUX_ADDR, AUX_VID_W_TILEWIDTH);        // set width
    xvid_setw(XVID_AUX_DATA, columns);

    delay(5000);

    xvid_setw(XVID_AUX_ADDR, AUX_VID_W_GFXCTRL);        // use WR address for palette index
    xvid_setw(XVID_AUX_DATA, 0x0000);                   // set palette data

    xvid_setw(XVID_AUX_ADDR, AUX_VID_W_TILEWIDTH);        // set width
    xvid_setw(XVID_AUX_DATA, columns);

    delay(2000);
}

void loop()
{
    activity();        // blink LED

    delay(3000);
#if 0
    // clear all VRAM
    uint16_t i = 0;
    do
    {
        xvid_setw(XVID_RD_ADDR, i);
        xvid_setw(XVID_DATA, 0x0220);        // green on black space
    } while (++i);
#endif
    xcls();
    xprint("Xosera Retro Graphics Adapter: Mode ");
    xprint_int(width);
    xprint("x");
    xprint_int(height);
    xprint(" (" PLATFORM " " MHZSTR " test rig)\n\n");

    for (int i = 0; i < 2048; i++)
    {
        uint8_t c = (i & 0xf) ? (i & 0xf) : 1;
        xcolor(c);
        xprint("Hello rosco_m68k! ");
    }
    delay(2000);

    activity();        // blink LED
    test_smoothscroll();

    activity();        // blink LED
    show_blurb();

    activity();        // blink LED
    test_palette();

    activity();        // blink LED
    test_reg_access();

    activity();        // blink LED
    test_4096_colors();

    activity();        // blink LED
    font_write();

    //    vram_speed();

    //    vram_verify();

    activity();        // blink LED
    count++;
    Serial.print("Completed run ");
    platform_print_dec(count);
    reboot_Xosera(count & 1);        // re-configure to reload fonts
    delay(1000);

    error_flag = false;
}
