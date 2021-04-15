#pragma GCC optimize("O3")
#include <Arduino.h>
#include <stdio.h>
#include <stdlib.h>
// Xosera Test Jig (using Arduino Pro Mini AVR @ 8MHz/3.3v with direct port access)
// A FPGA based video card for rosco_m68k retro computers (and others)
// See https://github.com/rosco-m68k/hardware-projects/tree/feature/xosera/xosera
// See

// Times I observed (AVR 328P @ 16MHz):
// 64KB x 16-bit write time = 78 ms
// 64KB x 8-bit write time = 65 ms
// 64KB x 16-bit read time = 157 ms
// 64KB x 8-bit read time = 119 ms

enum
{
    // AVR hardware pins
    LED         = 1 << PB5,        // LED, also Upduino RST pin via ~470 ohm resistor (LED on, else FPGA reset)
    BUS_CS_N    = 1 << PB2,        // active LOW select signal for Xosera
    BUS_RNW     = 1 << PB3,        // write/read signal for Xosera (0=write to Xosera, 1=read from Xosera)
    BUS_BYTESEL = 1 << PB4,        // even/oodd byte select (address line a0 or A1 for 68K with MOVEP)

    BUS_REG_NUM0 = 1 << PC0,        // 4-bit register number (see enum below)
    BUS_REG_NUM1 = 1 << PC1,
    BUS_REG_NUM2 = 1 << PC2,
    BUS_REG_NUM3 = 1 << PC3,

    BUS_D7 = 1 << PD7,        // 8-bit bi-directional data bus (Xosera outputs when RNW=1 and CS=0)
    BUS_D6 = 1 << PD6,        // (ordered so bits align with AVR ports and no shifting needed)
    BUS_D5 = 1 << PD5,
    BUS_D4 = 1 << PD4,
    BUS_D3 = 1 << PD3,
    BUS_D2 = 1 << PD2,
    BUS_D1 = 1 << PB1,
    BUS_D0 = 1 << PB0,

    // diagnostic Arduino LEDs (on extra A4 and A5 on Pro Mini)
    // NOTE: These are hooked up active LOW (so LOW value lights LED)
    // (Bbecause GPIO is always 0, but only set to an output to turn on LED)
    TEST_GREEN = 1 << PC5,        // green=blinks while testing
    TEST_RED   = 1 << PC4,        // off=no read errors, on=one or more read verify errors

    // "logical" defines for signal meanings (makes code easier to read)
    // NOTE: We always want LED on, unless except to reset FPGA at startup
    BUS_ON  = LED | 0,                  // LOW to select Xosera
    BUS_OFF = LED | BUS_CS_N,           // HIGH to de-select Xosera
    BUS_WR  = LED | 0,                  // LOW write to Xosera
    BUS_RD  = LED | BUS_RNW,            // HIGH read from Xosera (will outut on data bus when selected)
    BUS_MSB = LED | 0,                  // LOW even byte (MSB, bits [15:8] for Xosera)
    BUS_LSB = LED | BUS_BYTESEL,        // HIGH odd byte (LSB, bits  [7:0] for Xosera)

    // defines for GPIO output signals (BUS_Dx are bi-directional)
    PB_OUTPUTS = LED | BUS_CS_N | BUS_RNW | BUS_BYTESEL,
    PC_OUTPUTS = BUS_REG_NUM0 | BUS_REG_NUM1 | BUS_REG_NUM2 | BUS_REG_NUM3,
    PB_BUS_WR  = PB_OUTPUTS | BUS_D1 | BUS_D0,
    PD_BUS_WR  = BUS_D7 | BUS_D6 | BUS_D5 | BUS_D4 | BUS_D3 | BUS_D2,
    PB_BUS_RD  = PB_OUTPUTS,
    PD_BUS_RD  = 0
};
#define NOP()                                                                                                          \
    do                                                                                                                 \
    {                                                                                                                  \
        __asm__ __volatile__("nop");                                                                                   \
    } while (0)

// Xosera is operated via 16 16-bit registers the basics of which are outlined below.
//
// NOTE: TODO registers below are planned but not yet "wired up" in Xosera design
//
// Xosera uses 128 KB of embedded SPRAM (inside iCE40UP5K FPGA) for VRAM.
// This VRAM is arranged as 65536 x 16-bits so all Xosera addresses are 16-bit
// and all data transfers to/from VRAM are in 16-bit words.  Since Xosera uses an
// an 8-bit data bus, it uses big-endian (68K-style) byte transfers with MSB in even
// bytes and LSB in odd bytes (indicated via BUS_BYTESEL signal).
//
// When XVID_DATA or XVID_DATA2 is read, a 16-bit word is read from VRAM[XVID_RD_ADDR] and
// XVID_RD_ADDR += XVID_WR_INC (twos-complement, overflow ignored).
// Similarly, when the LSB of XVID_DATA or XVID_DATA2 is written to, a 16-bit value is
// written to VRAM[XVID_WR_ADDR] and XVID_WR_ADDR += XVID_WR_INC (twos-complement, overflow
// ignored).  The MSB of the word written will be the MSB previously written to XVID_DATA
// or XVID_DATA2 or zero if the last register write was to a different register.
// This allows faster output if only the LSB changes (e.g., text output with constant
// attribute byte).  Also both XVID_DATA or XVID_DATA2 exist to allow m68K to benefit
// from 32-bit data transfers using MOVEP.L instruction (using 4 8-bit transfers).

// Registers are currently write-only except XVID_DATA and XVID_DATA_2 (only upper two
// register number bits are used to decode register reads).


enum
{
    // register 16-bit read/write (no side effects)
    XVID_AUX_ADDR,        // reg 0: TODO video data (as set by VID_CTRL)
    XVID_CONST,           // reg 1: TODO CPU data (instead of read from VRAM)
    XVID_RD_ADDR,         // reg 2: address to read from VRAM
    XVID_WR_ADDR,         // reg 3: address to write from VRAM

    // special, odd byte write triggers
    XVID_DATA,            // reg 4: read/write word from/to VRAM RD/WR
    XVID_DATA_2,          // reg 5: read/write word from/to VRAM RD/WR (for 32-bit)
    XVID_AUX_DATA,        // reg 6: aux data (font/audio)
    XVID_COUNT,           // reg 7: TODO blitter "repeat" count/trigger

    // write only, 16-bit
    XVID_RD_INC,           // reg 9: read addr increment value
    XVID_WR_INC,           // reg A: write addr increment value
    XVID_WR_MOD,           // reg C: TODO write modulo width for 2D blit
    XVID_RD_MOD,           // reg B: TODO read modulo width for 2D blit
    XVID_WIDTH,            // reg 8: TODO width for 2D blit
    XVID_BLIT_CTRL,        // reg D: TODO
    XVID_UNUSED_E,         // reg E: TODO
    XVID_UNUSED_F,         // reg F: TODO

    // AUX access using AUX_ADDR/AUX_DATA
    AUX_VID             = 0x0000,        // 0-8191 8-bit address (bits 15:8 ignored writing)
    AUX_VID_W_DISPSTART = 0x0000,        // display start address
    AUX_VID_W_TILEWIDTH = 0x0001,        // tile line width (usually WIDTH/8)
    AUX_VID_W_SCROLLXY  = 0x0002,        // [10:8] H fine scroll, [3:0] V fine scroll
    AUX_VID_W_FONTCTRL  = 0x0003,        // [9:8] 2KB font bank, [3:0] font height
    AUX_VID_R_WIDTH     = 0x0000,        // display resolution width
    AUX_VID_R_HEIGHT    = 0x0001,        // display resolution height
    AUX_VID_R_UNUSED_2  = 0x0003,        // TODO    <--- 🤔
    AUX_VID_R_SCANLINE  = 0x0002,        // [15] V blank, [14:11] zero [10:0] V line
    AUX_W_FONT          = 0x4000,        // 0x4000-0x5FFF 8K byte font memory (even byte [15:8] ignored)
    AUX_W_COLORTBL      = 0x8000,        // 0x8000-0x80FF 256 word color lookup table (0xXRGB)
    AUX_W_AUD           = 0xc000         // 0xC000-0x??? TODO (audio registers)
};

// for slower testing
#if 1
#define SLOW()                                                                                                         \
    NOP();                                                                                                             \
    NOP();                                                                                                             \
    NOP();                                                                                                             \
    NOP();
#else
#define SLOW()
#endif

static inline void xvid_setw(uint8_t r, uint16_t word)
{
    uint8_t msb = word >> 8;
    uint8_t lsb = word;
    PORTC       = r;                                         // set reg num
    PORTB       = BUS_OFF | BUS_WR | BUS_MSB;                // de-select Xosera, set write, MSB select
    PORTD       = msb;                                       // set MSB data d7-d2
    PORTB = BUS_ON | BUS_WR | BUS_MSB | (msb & 0x03);        // select Xosera, set write, MSB select, MSB data d0-d1
    SLOW();
#if (F_CPU >= 16000000)        // if 16MHz add an additional
    NOP();                     //  1 cycle delay needed for AVR @ 16MHz (> ~100ns CS pulse)
#endif
    PORTB = BUS_OFF | BUS_WR | BUS_LSB;                      // de-select Xosera, set write LSB select
    PORTD = lsb;                                             // set LSB data d7-d2
    PORTB = BUS_ON | BUS_WR | BUS_LSB | (lsb & 0x03);        // select Xosera set write, LSB select, LSB data d0-d1
    SLOW();
#if (F_CPU >= 16000000)        // if 16MHz add an additional
    NOP();                     //  1 cycle delay needed for AVR @ 16MHz (> ~100ns CS pulse)
#endif
    PORTB = BUS_OFF | BUS_WR | BUS_MSB;        // de-select Xosera, set write, LSB select
}

static inline void xvid_setlb(uint8_t r, uint8_t lsb)
{
    PORTC = r;                                               // set reg num
    PORTB = BUS_OFF | BUS_WR | BUS_LSB;                      // de-select Xosera, set write LSB select
    PORTD = lsb;                                             // set LSB data d7-d2
    PORTB = BUS_ON | BUS_WR | BUS_LSB | (lsb & 0x03);        // select Xosera set write, LSB select, LSB data d0-d1
    SLOW();
#if (F_CPU >= 16000000)        // if 16MHz add an additional delay
    NOP();                     //  1 cycle delay needed for AVR @ 16MHz (> ~100ns CS pulse)
#endif
    PORTB = BUS_OFF | BUS_WR | BUS_MSB;        // de-select Xosera, set write, LSB select
}

static inline void xvid_sethb(uint8_t r, uint8_t msb)
{
    PORTC = r;                                               // set reg num
    PORTB = BUS_OFF | BUS_WR | BUS_MSB;                      // de-select Xosera, set write MSB select
    PORTD = msb;                                             // set MSB data d7-d2
    PORTB = BUS_ON | BUS_WR | BUS_MSB | (msb & 0x03);        // select Xosera set write, MSB select, MSB data d0-d1
    SLOW();
#if (F_CPU >= 16000000)        // if 16MHz add an additional delay
    NOP();                     //  1 cycle delay needed for AVR @ 16MHz (> ~100ns CS pulse)
#endif
    PORTB = BUS_OFF | BUS_WR | BUS_MSB;        // de-select Xosera, set write, MSB select
}

static inline uint16_t xvid_getw(uint8_t r)
{
    PORTC = r;                                // set reg num
    DDRD  = PD_BUS_RD;                        // set data d7-d2 as input
    DDRB  = PB_BUS_RD;                        // set control signals as output and data d1-d0 as input
    PORTB = BUS_ON | BUS_RD | BUS_MSB;        // select Xosera, set read, MSB select
    NOP();                                    // 1 cycle delay needed for AVR >= 8MHz
    NOP();                                    // 1 cycle delay needed for AVR >= 8MHz
    SLOW();
#if (F_CPU >= 16000000)        // if 16MHz add an additional delay
    NOP();                     //  1 cycle delay needed for AVR @ 16MHz
    NOP();                     //  1 cycle delay needed for AVR @ 16MHz
#endif                         // end 16MHz
    uint8_t msb =
        (PIND & 0xFC) | (PINB & 0x03);         // read data bus 8-bit MSB value (NOTE: AVR input has a cycle latency)
    PORTB = BUS_OFF | BUS_RD | BUS_LSB;        // de-select Xosera, set read, LSB select
    PORTB = BUS_ON | BUS_RD | BUS_LSB;         // select Xosera, set read, LSB select
    NOP();                                     // 1 cycle delay needed for AVR >= 8MHz
    NOP();                                     // 1 cycle delay needed for AVR >= 8MHz
    SLOW();
#if (F_CPU >= 16000000)        // if 16MHz add an additional delay
    NOP();                     //  1 cycle delay needed for AVR @ 16MHz
    NOP();                     //  1 cycle delay needed for AVR @ 16MHz
#endif                         // end 16MHz
    uint8_t lsb =
        (PIND & 0xFC) | (PINB & 0x03);         // read data bus 8-bit lsb value (NOTE: AVR input has a cycle latency)
    PORTB = BUS_OFF | BUS_WR | BUS_MSB;        // de-select Xosera, set write, LSB select
    DDRD  = PD_BUS_WR;                         // set data d7-d2 as outputs
    DDRB  = PB_BUS_WR;                         // set control signals and data d1-d0 as outputs
    return (msb << 8) | lsb;
}

// bytesel = LSB (default) or 0 for MSB
static inline uint8_t xvid_getb(uint8_t r, uint8_t bytesel = BUS_LSB)
{
    PORTC = r;                                 // set reg num
    PORTB = BUS_OFF | BUS_RD | bytesel;        // de-select Xosera, set read, MSB select
    DDRD  = PD_BUS_RD;                         // set data d7-d2 as input
    DDRB  = PB_BUS_RD;                         // set control signals as output and data d1-d0 as input
    PORTB = BUS_ON | BUS_RD | bytesel;         // select Xosera, set read, MSB select
    NOP();                                     // 1 cycle delay needed for AVR >= 8MHz (> ~100ns CS pulse)
    NOP();                                     // 1 cycle delay needed for AVR >= 8MHz (> ~100ns CS pulse)
    SLOW();
#if (F_CPU >= 16000000)        // if 16MHz add an additional delay
    NOP();                     //  1 cycle delay needed for AVR @ 16MHz (> ~100ns CS pulse)
    NOP();                     //  1 cycle delay needed for AVR @ 16MHz (> ~100ns CS pulse)
#endif                         // end 16MHz
    uint8_t data =
        (PIND & 0xFC) | (PINB & 0x03);         // read data bus 8-bit MSB value (NOTE: AVR input has a cycle latency)
    PORTB = BUS_OFF | BUS_WR | BUS_MSB;        // de-select Xosera, set write, LSB select
    DDRD  = PD_BUS_WR;                         // set data d7-d2 as outputs
    DDRB  = PB_BUS_WR;                         // set control signals and data d1-d0 as outputs
    return data;
}

static inline uint8_t xvid_getlb(uint8_t r)
{
    return xvid_getb(r, BUS_LSB);
};
static inline uint8_t xvid_gethb(uint8_t r)
{
    return xvid_getb(r, BUS_MSB);
}

static bool     error_flag = false;
static uint8_t  leds;                    // diagnostic LEDs
static uint8_t  columns;                 // in texts chars (words)
static uint8_t  rows;                    // in texts chars (words)
static uint8_t  cur_color = 0x02;        // color for status line (green or red after error)
static uint16_t width;                   // in pixels
static uint16_t height;                  // in pixels
static uint16_t data = 0x0100;           // test "data" value
static uint16_t addr;                    // test starting address (to leave status line)
static uint32_t errors;                  // read verify error count
static uint16_t rdata;

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


#if (F_CPU == 16000000)
#define MHZSTR "16MHz"
#elif (F_CPU == 8000000)
#define MHZSTR "8MHz"
#else
#define MHZSTR "??MHz"
#endif

static void xcolor(uint8_t color)
{
    uint16_t wa = xvid_getw(XVID_WR_ADDR);
    xvid_sethb(XVID_DATA, color);
    xvid_setw(XVID_WR_ADDR, wa);
}

uint8_t ln = 0;

static void xhome()
{
    // home wr addr
    xvid_setw(XVID_WR_INC, 1);
    xvid_setw(XVID_WR_ADDR, 0);
    xcolor(cur_color);        // green-on-black
    ln = 0;
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
    for (uint16_t i = 0; i < columns * rows; i++)
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

static void error(const char * msg, uint16_t addr, uint16_t rdata, uint16_t vdata)
{
    errors++;
    Serial.println("");
    Serial.print(msg);
    Serial.print(" (at ");
    Serial.print(addr, HEX);
    Serial.print(" rd=");
    Serial.print(rdata, HEX);
    Serial.print(" vs ");
    Serial.print(vdata, HEX);
    Serial.print(") Errors: ");
    Serial.println(errors);
    leds |= TEST_RED;
    error_flag = true;
}

static void reboot_Xosera(uint8_t config)
{
    Serial.print("Reconfig Xosera FPGA");
    xvid_setw(XVID_BLIT_CTRL, 0x8080 | ((config & 0x3) << 8));        // reboot FPGA to config
    do
    {
        delay(10);
        Serial.print(".");
        xvid_setw(XVID_RD_ADDR, 0x1234);
    } while (xvid_getw(XVID_RD_ADDR) != 0x1234);
    Serial.println("ready.");
}

void setup()
{
    PORTB = BUS_CS_N;          // reset Xosera (LED off) and de-select (for safety)
    DDRB  = PB_OUTPUTS;        // set control signals as outputs
    leds  = TEST_GREEN;        // set default test LEDs
    DDRC  = leds | PC_OUTPUTS;
    PORTC = 0;        // set register number bits to 0 and set green "blink" LED
    Serial.begin(115200);
    Serial.println("\f\r\nXosera AVR Tester (direct port access AVR @ " MHZSTR ")");
    delay(1);                                  // hold reset a moment
    PORTD = 0;                                 // clear output data d7-d2
    PORTB = BUS_OFF | BUS_WR | BUS_MSB;        // deselect Xosera, set write, set MSB byte, clear data d1-d0
    DDRD  = PD_BUS_WR;                         // set data d7-d2 as outputs
    DDRB  = PB_BUS_WR;                         // set control signals and data d1-d0 as outputs

    reboot_Xosera(0);

    xvid_setw(XVID_AUX_ADDR, AUX_VID_R_WIDTH);        // select width
    uint16_t width = xvid_getw(XVID_AUX_DATA);

    xvid_setw(XVID_AUX_ADDR, AUX_VID_R_HEIGHT);        // select height
    uint16_t height = xvid_getw(XVID_AUX_DATA);
    if (width != height && width > height)
    {
        Serial.print("Xosera detected at resolution ");
        Serial.print(width);
        Serial.print("x");
        Serial.println(height);
    }

    columns = width / 8;
    rows    = height / 16;
    addr    = columns;
    delay(4000);        // let the stunning boot logo display. :)

    randomSeed(0xc0ffee42);        // deterministic seed TODO
}

const PROGMEM char blurb[] =
    "\n\n"
    "    Xosera is an FPGA based video adapter designed with the rosco_m68k retro computer\n"
    "    in mind. Inspired in concept by it's \"namesake\" the Commander X16's VERA, Xosera\n"
    "    is an original open-source video adapter design, built with open-source tools, that\n"
    "    is being tailored with features appropriate for a Motorola 68K era retro computer.\n"
    "    \n"
    "        \xf9   VGA or HDMI (DVI) output at 848x480@60Hz or 640x480@60Hz (16:9 or 4:3)\n"
    "        \xf9   16 or 256 color palette out of 4096 colors (12-bit RGB)\n"
    "        \xf9   128KB of embedded video RAM (16-bit words @33/25 MHz)\n"
    "        \xf9   Character tile based modes with color attribute byte\n"
    "        \xf9   Pixel doubled bitmap modes (e.g. 424x240 or 320x240)\n"
    "        \xf9   Smooth horizontal and vertical tile scrolling\n"
    "        \xf9   8x8 or 8x16 character tile size (or truncated e.g., 8x10)\n"
    "        \xf9   Register based interface with 16 16-bit registers\n"
    "        \xf9   Read/write VRAM with programmable read/write address increment\n"
    "        \xf9   Full speed 8-bit bus interface (with MOVEP) for rosco_m68k (by Ross Bamford)\n"
    "        \xf9   8KB of font RAM with multiple fonts (2KB per 8x8 fonts, 4K per 8x16 font)\n"
    "        \xf9   \"Blitter\" for fast VRAM copy & fill operations (TODO, but used at init)\n"
    "        \xf9   2-D operations \"blitter\" with modulo and shifting/masking (TODO)\n"
    "        \xf9   Dual overlayed \"planes\" of video (TODO)\n"
    "        \xf9   Wavetable stereo audio (TODO, spare debug IO for now)\n"
    "        \xf9   Bit-mapped 16 and 256 color graphics modes (256 color TODO)\n"
    "        \xf9   16-color tile mode with \"game\" attributes (e.g., mirroring) (TODO)\n"
    "        \xf9   At least one \"cursor\" sprite (and likely more, TODO)\n";

void show_blurb()
{
    // Show some text
    Serial.println("Blurb text");
    xcls();
    xprint_P(blurb);
    delay(1000);

    // 2nd font (ST 8x8)
    Serial.println("ST 8x8 font");
    xvid_setw(XVID_AUX_ADDR, AUX_VID_W_FONTCTRL);        // A_font_ctrl
    xvid_setw(XVID_AUX_DATA, 0x0207);                    // 2nd font in bank 2, 8 high
    delay(1000);

    // 3rd font (hex 8x8 debug)
    Serial.println("hex 8x8 font");
    xvid_setw(XVID_AUX_ADDR, AUX_VID_W_FONTCTRL);        // A_font_ctrl
    xvid_setw(XVID_AUX_DATA, 0x0307);                    // 3st font in bank 3, 8 high
    delay(5000);

    // restore 1st font (ST 8x16)
    Serial.println("ST 8x16 font");
    xvid_setw(XVID_AUX_ADDR, AUX_VID_W_FONTCTRL);        // A_font_ctrl
    xvid_setw(XVID_AUX_DATA, 0x000F);                    // back to 1st font in bank 0, 16 high
    delay(1000);

    // shrink font height
    Serial.println("Shrink font height");
    for (int v = 15; v >= 0; v--)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_VID_W_FONTCTRL);        // A_font_ctrl
        // set font height and switch to 8x8 font when < 8
        xvid_setw(XVID_AUX_DATA, (v < 8 ? 0x0200 : 0) | v);
        delay(100);
    }

    Serial.println("Grow font height");
    for (int v = 0; v < 16; v++)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_VID_W_FONTCTRL);        // A_font_ctrl
        // set font height and switch to 8x8 font when < 8
        xvid_setw(XVID_AUX_DATA, (v < 8 ? 0x0200 : 0) | v);
        delay(100);
    }

    // restore 1st font (ST 8x16)
    Serial.println("ST 8x16 font");
    xvid_setw(XVID_AUX_ADDR, AUX_VID_W_FONTCTRL);        // A_font_ctrl
    xvid_setw(XVID_AUX_DATA, 0x000F);                    // back to 1st font in bank 0, 16 high
    delay(1000);

    Serial.println("Scroll via video VRAM display address");
    int16_t r = 0;
    for (uint16_t i = 0; i < (rows * 4); i++)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_VID_W_DISPSTART);        // set text start addr
        xvid_setw(XVID_AUX_DATA, r * columns);                // to one line down
        for (int8_t f = 0; f < 16; f++)
        {
            xvid_setw(XVID_AUX_ADDR, AUX_VID_W_SCROLLXY);        // v fine scroll
            xvid_setw(XVID_AUX_DATA, f);

            // vsync
            xvid_setw(XVID_AUX_ADDR, AUX_VID_R_SCANLINE);        // set scanline reg
            while (static_cast<int16_t>(xvid_getw(XVID_AUX_DATA)) >= 0)
            {}
            // vsync
            xvid_setw(XVID_AUX_ADDR, AUX_VID_R_SCANLINE);        // set scanline reg
            while (static_cast<int16_t>(xvid_getw(XVID_AUX_DATA)) >= 0)
            {}
            // vsync
            xvid_setw(XVID_AUX_ADDR, AUX_VID_R_SCANLINE);        // set scanline reg
            while (static_cast<int16_t>(xvid_getw(XVID_AUX_DATA)) >= 0)
            {}
        }
        if (++r > rows)
        {
            r = -rows;
        }
    }
    xvid_setw(XVID_AUX_ADDR, AUX_VID_W_DISPSTART);        // reset text start addr
    xvid_setw(XVID_AUX_DATA, 0x0000);
    xvid_setw(XVID_AUX_ADDR, AUX_VID_W_SCROLLXY);        // reset text start addr
    xvid_setw(XVID_AUX_DATA, 0x0000);
    delay(1000);

    Serial.println("Horizontal fine scroll");
    for (int x = 0; x < 8; x++)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_VID_W_SCROLLXY);        // scroll
        // set font height and switch to 8x8 font when < 8
        xvid_setw(XVID_AUX_DATA, x);
        delay(100);
    }
    for (int x = 7; x > 0; x--)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_VID_W_SCROLLXY);        // scroll
        // set font height and switch to 8x8 font when < 8
        xvid_setw(XVID_AUX_DATA, x);
        delay(100);
    }
    delay(1000);
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
}

void test_palette()
{
    Serial.println("palette test");
    xcls();
    xcolor(0xf);
    xprint_P(blurb);

    Serial.println("Rosco rainbow cycle");
    for (uint8_t i = 0; i < 16; i++)
    {
        for (uint16_t k = 0; k != 0x4000; k++)
        {
            xvid_setw(XVID_AUX_ADDR, AUX_VID_R_SCANLINE);        // set scanline reg
            uint16_t l = xvid_getw(XVID_AUX_DATA);               // read scanline
            l          = l | ((0xf - (l & 0xf)) << 8);           // invert blue for some red
            xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | 0);        // set palette entry #0
            xvid_setw(XVID_AUX_DATA, l);                         // set palette data
        }
    }
    xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | 0);        // set palette entry #0
    xvid_setw(XVID_AUX_DATA, 0x0104);                    // set palette data

    xhome();
    xprint_P_rainbow(1, blurb);
    delay(1000);
    // color cycle palette
    Serial.println("color cycle nuclear glow");
    for (uint16_t k = 0; k < 120; k++)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | 2);
        uint16_t n = random(0x0fff) & 0x777;
        xvid_setw(XVID_AUX_DATA, n);        // set palette data
        delay(16);
    }

#if 1        // TODO: something funny here (blanks 9" monitor)?
    // gray default palette
    Serial.println("grey palette test");
    for (uint8_t i = 0; i < 16; i++)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | i);        // use WR address for 595e index
        xvid_setw(XVID_AUX_DATA, i);                         // set palette data
    }
    delay(1000);
    for (uint8_t i = 0; i < 16; i++)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | i);        // use WR address for 595e index
        xvid_setw(XVID_AUX_DATA, i << 4);                    // set palette data
    }
    delay(1000);
    for (uint8_t i = 0; i < 16; i++)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | i);        // use WR address for 595e index
        xvid_setw(XVID_AUX_DATA, i << 8);                    // set palette data
    }
    delay(1000);
    for (uint8_t i = 0; i < 16; i++)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | i);        // use WR address for 595e index
        uint16_t v = i ? 0x000 : 0xfff;
        xvid_setw(XVID_AUX_DATA, v);        // set palette data
    }
    delay(2000);
    for (uint8_t i = 0; i < 16; i++)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | i);        // use WR address for 595e index
        uint16_t v = i ? 0xfff : 0x000;
        xvid_setw(XVID_AUX_DATA, v);        // set palette data
    }
    delay(2000);
    for (uint8_t i = 0; i < 16; i++)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | i);        // use WR address for 595e index
        uint16_t v = i ? 0xfff : 0xfff;
        xvid_setw(XVID_AUX_DATA, v);        // set palette data
    }
    delay(2000);
    for (uint8_t i = 0; i < 16; i++)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | i);               // use WR address for palette index
        xvid_setw(XVID_AUX_DATA, pgm_read_word(defpal + i));        // set palette data
    }
#if 0        // evil color?

    delay(1000);
    xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | 12);        // use WR address for 595e index
    xvid_setw(XVID_AUX_DATA, 0xCCC);                      // set palette data
    delay(1000);
    xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | 12);        // use WR address for 595e index
    xvid_setw(XVID_AUX_DATA, 0xC0C);                      // set palette data
    delay(1000);
    xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | 12);        // use WR address for 595e index
    xvid_setw(XVID_AUX_DATA, 0x0CC);                      // set palette data
    delay(1000);
    xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | 12);        // use WR address for 595e index
    xvid_setw(XVID_AUX_DATA, 0x00C);                      // set palette data
    delay(1000);
#endif
    xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | 0);        // use WR address for 595e index
    xvid_setw(XVID_AUX_DATA, 0x000);                     // set palette data
    delay(1000);
    xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | 1);        // use WR address for 595e index
    xvid_setw(XVID_AUX_DATA, 0x111);                     // set palette data
    delay(1000);
    xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | 2);        // use WR address for 595e index
    xvid_setw(XVID_AUX_DATA, 0x222);                     // set palette data
    delay(1000);
    xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | 3);        // use WR address for 595e index
    xvid_setw(XVID_AUX_DATA, 0x333);                     // set palette data
    delay(1000);
    xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | 4);        // use WR address for 595e index
    xvid_setw(XVID_AUX_DATA, 0x444);                     // set palette data
    delay(1000);
    xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | 5);        // use WR address for 595e index
    xvid_setw(XVID_AUX_DATA, 0x555);                     // set palette data
    delay(1000);
    xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | 6);        // use WR address for 595e index
    xvid_setw(XVID_AUX_DATA, 0x666);                     // set palette data
    delay(1000);
    xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | 7);        // use WR address for 595e index
    xvid_setw(XVID_AUX_DATA, 0x777);                     // set palette data
    delay(1000);
    xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | 8);        // use WR address for 595e index
    xvid_setw(XVID_AUX_DATA, 0x888);                     // set palette data
    delay(1000);
    xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | 9);        // use WR address for 595e index
    xvid_setw(XVID_AUX_DATA, 0x999);                     // set palette data
    delay(1000);
    xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | 10);        // use WR address for 595e index
    xvid_setw(XVID_AUX_DATA, 0xAAA);                      // set palette data
    delay(1000);
    xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | 11);        // use WR address for 595e index
    xvid_setw(XVID_AUX_DATA, 0xBBB);                      // set palette data
    delay(1000);
#if 0
    xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | 12);        // use WR address for 595e index
    xvid_setw(XVID_AUX_DATA, 0xCCC);                      // set palette data
    delay(1000);
#endif
    xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | 13);        // use WR address for 595e index
    xvid_setw(XVID_AUX_DATA, 0xDDD);                      // set palette data
    delay(1000);
    xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | 14);        // use WR address for 595e index
    xvid_setw(XVID_AUX_DATA, 0xEEE);                      // set palette data
    delay(1000);
    xvid_setw(XVID_AUX_ADDR, AUX_W_COLORTBL | 15);        // use WR address for 595e index
    xvid_setw(XVID_AUX_DATA, 0xFFF);                      // set palette data
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
    for (uint16_t k = 0; k < 10; k++)
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
            delay(17);
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

static const PROGMEM uint16_t data_pat[8] = {0xA5A5, 0x5A5A, 0xFFFF, 0x0123, 0x4567, 0x89AB, 0xCDEF, 0x0a20};

void test_reg_access()
{
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
        delay(500);
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
    for (uint16_t a = 0; a < 4096; a++)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_W_FONT | a);        // scroll
        // set font height and switch to 8x8 font when < 8
        xvid_setw(XVID_AUX_DATA, a & 1 ? 0x5555 : 0xaaaa);
        delay(5);
    }

    reboot_Xosera(0);        // re-configure to reload fonts
    delay(2000);             // let monitor sync
}

void activity()
{
    // blink green activity LED
    leds ^= TEST_GREEN;
    DDRC = leds | PC_OUTPUTS;
}

void loop()
{
    activity();        // blink LED

    // clear all VRAM
    uint16_t i = 0;
    do
    {
        xvid_setw(XVID_RD_ADDR, i);
        xvid_setw(XVID_DATA, 0x0220);        // green on black space
    } while (++i);

    xcls();
    xprint("Xosera Retro Graphics Adapter: Mode ");
    xprint_int(width);
    xprint("x");
    xprint_int(height);
    xprint(" (AVR " MHZSTR " test rig)\n\n");

    for (int i = 0; i < 409; i++)
    {
        uint8_t c = (i & 0xf) ? (i & 0xf) : 1;
        xcolor(c);
        xprint("Hello! ");
    }
    delay(1000);

    activity();        // blink LED
    show_blurb();

    activity();        // blink LED
    test_palette();

    activity();        // blink LED
    test_reg_access();

    activity();        // blink LED
    font_write();

    //    vram_speed();

    //    vram_verify();

    activity();        // blink LED

    error_flag = false;
}