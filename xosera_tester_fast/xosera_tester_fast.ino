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
    LED         = 1 << PB5,    // LED, also Upduino RST pin via ~470 ohm resistor (LED on, else FPGA reset)
    BUS_CS_N    = 1 << PB2,    // active LOW select signal for Xosera
    BUS_RNW     = 1 << PB3,    // write/read signal for Xosera (0=write to Xosera, 1=read from Xosera)
    BUS_BYTESEL = 1 << PB4,    // even/oodd byte select (address line a0 or A1 for 68K with MOVEP)

    BUS_REG_NUM0 = 1 << PC0,    // 4-bit register number (see enum below)
    BUS_REG_NUM1 = 1 << PC1,
    BUS_REG_NUM2 = 1 << PC2,
    BUS_REG_NUM3 = 1 << PC3,

    BUS_D7 = 1 << PD7,    // 8-bit bi-directional data bus (Xosera outputs when RNW=1 and CS=0)
    BUS_D6 = 1 << PD6,    // (ordered so bits align with AVR ports and no shifting needed)
    BUS_D5 = 1 << PD5,
    BUS_D4 = 1 << PD4,
    BUS_D3 = 1 << PD3,
    BUS_D2 = 1 << PD2,
    BUS_D1 = 1 << PB1,
    BUS_D0 = 1 << PB0,

    // diagnostic Arduino LEDs (on extra A4 and A5 on Pro Mini)
    // NOTE: These are hooked up active LOW (so LOW value lights LED)
    // (Bbecause GPIO is always 0, but only set to an output to turn on LED)
    TEST_GREEN = 1 << PC5,    // green=blinks while testing
    TEST_RED   = 1 << PC4,    // off=no read errors, on=one or more read verify errors

    // "logical" defines for signal meanings (makes code easier to read)
    // NOTE: We always want LED on, unless except to reset FPGA at startup
    BUS_ON  = LED | 0,              // LOW to select Xosera
    BUS_OFF = LED | BUS_CS_N,       // HIGH to de-select Xosera
    BUS_WR  = LED | 0,              // LOW write to Xosera
    BUS_RD  = LED | BUS_RNW,        // HIGH read from Xosera (will outut on data bus when selected)
    BUS_MSB = LED | 0,              // LOW even byte (MSB, bits [15:8] for Xosera)
    BUS_LSB = LED | BUS_BYTESEL,    // HIGH odd byte (LSB, bits  [7:0] for Xosera)

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
    XVID_AUX_ADDR,    // reg 0: TODO video data (as set by VID_CTRL)
    XVID_CONST,       // reg 1: TODO CPU data (instead of read from VRAM)
    XVID_RD_ADDR,     // reg 2: address to read from VRAM
    XVID_WR_ADDR,     // reg 3: address to write from VRAM

    // special, odd byte write triggers
    XVID_DATA,        // reg 4: read/write word from/to VRAM RD/WR
    XVID_DATA_2,      // reg 5: read/write word from/to VRAM RD/WR (for 32-bit)
    XVID_AUX_DATA,    // reg 6: aux data (font/audio)
    XVID_COUNT,       // reg 7: TODO blitter "repeat" count/trigger

    // write only, 16-bit
    XVID_RD_INC,       // reg 9: read addr increment value
    XVID_WR_INC,       // reg A: write addr increment value
    XVID_WR_MOD,       // reg C: TODO write modulo width for 2D blit
    XVID_RD_MOD,       // reg B: TODO read modulo width for 2D blit
    XVID_WIDTH,        // reg 8: TODO width for 2D blit
    XVID_BLIT_CTRL,    // reg D: TODO
    XVID_UNUSED_1,     // reg E: TODO
    XVID_UNUSED_2,     // reg F: TODO

    AUX_VID  = 1 << 15,
    AUX_FONT = 1 << 14,
    AUX_PAL  = 1 << 13,
    AUX_AUD  = 1 << 12
};

static inline void xvid_setw(uint8_t r, uint16_t word)
{
    uint8_t msb = word >> 8;
    uint8_t lsb = word;
    PORTC       = r;                                           // set reg num
    PORTB       = BUS_OFF | BUS_WR | BUS_MSB;                  // de-select Xosera, set write, MSB select
    PORTD       = msb;                                         // set MSB data d7-d2
    PORTB       = BUS_ON | BUS_WR | BUS_MSB | (msb & 0x03);    // select Xosera, set write, MSB select, MSB data d0-d1
    NOP();                                               //  1 cycle delay needed for AVR @ 16MHz (> ~100ns CS pulse)
    PORTB = BUS_OFF | BUS_WR | BUS_LSB;                  // de-select Xosera, set write LSB select
    PORTD = lsb;                                         // set LSB data d7-d2
    PORTB = BUS_ON | BUS_WR | BUS_LSB | (lsb & 0x03);    // select Xosera set write, LSB select, LSB data d0-d1
    NOP();                                               //  1 cycle delay needed for AVR @ 16MHz (> ~100ns CS pulse)
    PORTB = BUS_OFF | BUS_WR | BUS_MSB;                  // de-select Xosera, set write, LSB select
}

static inline void xvid_setlb(uint8_t r, uint8_t lsb)
{
    PORTC = r;                                           // set reg num
    PORTB = BUS_OFF | BUS_WR | BUS_LSB;                  // de-select Xosera, set write LSB select
    PORTD = lsb;                                         // set LSB data d7-d2
    PORTB = BUS_ON | BUS_WR | BUS_LSB | (lsb & 0x03);    // select Xosera set write, LSB select, LSB data d0-d1
    NOP();                                               //  1 cycle delay needed for AVR @ 16MHz (> ~100ns CS pulse)
    PORTB = BUS_OFF | BUS_WR | BUS_MSB;                  // de-select Xosera, set write, LSB select
}

static inline void xvid_sethb(uint8_t r, uint8_t msb)
{
    PORTC = r;                                           // set reg num
    PORTB = BUS_OFF | BUS_WR | BUS_MSB;                  // de-select Xosera, set write MSB select
    PORTD = msb;                                         // set MSB data d7-d2
    PORTB = BUS_ON | BUS_WR | BUS_MSB | (msb & 0x03);    // select Xosera set write, MSB select, MSB data d0-d1
    NOP();                                               //  1 cycle delay needed for AVR @ 16MHz (> ~100ns CS pulse)
    PORTB = BUS_OFF | BUS_WR | BUS_MSB;                  // de-select Xosera, set write, MSB select
}

static inline uint16_t xvid_getw(uint8_t r)
{
    PORTC = r;                            // set reg num
    DDRD  = PD_BUS_RD;                    // set data d7-d2 as input
    DDRB  = PB_BUS_RD;                    // set control signals as output and data d1-d0 as input
    PORTB = BUS_ON | BUS_RD | BUS_MSB;    // select Xosera, set read, MSB select
    NOP();                                // 1 cycle delay needed for AVR >= 8MHz (> ~100ns CS pulse)
    NOP();                                // 1 cycle delay needed for AVR >= 8MHz (> ~100ns CS pulse)
#if (F_CPU >= 16000000)                   // if 16MHz add an additional
    NOP();                                //  1 cycle delay needed for AVR @ 16MHz (> ~100ns CS pulse)
    NOP();                                //  1 cycle delay needed for AVR @ 16MHz (> ~100ns CS pulse)
#endif                                    // end 16MHz
    uint8_t msb =
        (PIND & 0xFC) | (PINB & 0x03);     // read data bus 8-bit MSB value (NOTE: AVR input has a cycle latency)
    PORTB = BUS_OFF | BUS_RD | BUS_LSB;    // de-select Xosera, set read, LSB select
    PORTB = BUS_ON | BUS_RD | BUS_LSB;     // select Xosera, set read, LSB select
    NOP();                                 // 1 cycle delay needed for AVR >= 8MHz (> ~100ns CS pulse)
    NOP();                                 // 1 cycle delay needed for AVR >= 8MHz (> ~100ns CS pulse)
#if (F_CPU >= 16000000)                    // if 16MHz add an additional
    NOP();                                 //  1 cycle delay needed for AVR @ 16MHz (> ~100ns CS pulse)
    NOP();                                 //  1 cycle delay needed for AVR @ 16MHz (> ~100ns CS pulse)
#endif                                     // end 16MHz
    uint8_t lsb =
        (PIND & 0xFC) | (PINB & 0x03);     // read data bus 8-bit lsb value (NOTE: AVR input has a cycle latency)
    PORTB = BUS_OFF | BUS_WR | BUS_MSB;    // de-select Xosera, set write, LSB select
    DDRD  = PD_BUS_WR;                     // set data d7-d2 as outputs
    DDRB  = PB_BUS_WR;                     // set control signals and data d1-d0 as outputs
    return (msb << 8) | lsb;
}

// bytesel = LSB (default) or 0 for MSB
static inline uint8_t xvid_getb(uint8_t r, uint8_t bytesel = BUS_LSB)
{
    PORTC = r;                             // set reg num
    PORTB = BUS_OFF | BUS_RD | bytesel;    // de-select Xosera, set read, MSB select
    DDRD  = PD_BUS_RD;                     // set data d7-d2 as input
    DDRB  = PB_BUS_RD;                     // set control signals as output and data d1-d0 as input
    PORTB = BUS_ON | BUS_RD | bytesel;     // select Xosera, set read, MSB select
    NOP();                                 // 1 cycle delay needed for AVR >= 8MHz (> ~100ns CS pulse)
    NOP();                                 // 1 cycle delay needed for AVR >= 8MHz (> ~100ns CS pulse)
#if (F_CPU >= 16000000)                    // if 16MHz add an additional
    NOP();                                 //  1 cycle delay needed for AVR @ 16MHz (> ~100ns CS pulse)
    NOP();                                 //  1 cycle delay needed for AVR @ 16MHz (> ~100ns CS pulse)
#endif                                     // end 16MHz
    uint8_t data =
        (PIND & 0xFC) | (PINB & 0x03);     // read data bus 8-bit MSB value (NOTE: AVR input has a cycle latency)
    PORTB = BUS_OFF | BUS_WR | BUS_MSB;    // de-select Xosera, set write, LSB select
    DDRD  = PD_BUS_WR;                     // set data d7-d2 as outputs
    DDRB  = PB_BUS_WR;                     // set control signals and data d1-d0 as outputs
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
static uint8_t  leds;                // diagnostic LEDs
static uint8_t  columns;             // in texts chars (words)
static uint8_t  rows;                // in texts chars (words)
static uint8_t  cur_color = 0x02;    // color for status line (green or red after error)
static uint16_t width;               // in pixels
static uint16_t height;              // in pixels
static uint32_t count;               // test iteration count
static uint16_t data = 0x0100;       // test "data" value
static uint16_t addr;                // test starting address (to leave status line)
static uint32_t errors;              // read verify error count
static uint16_t rdata;

const uint16_t defpal[16] = {
    0x0000,    // black
    0x000A,    // blue
    0x00A0,    // green
    0x00AA,    // cyan
    0x0A00,    // red
    0x0A0A,    // magenta
    0x0AA0,    // brown
    0x0AAA,    // light gray
    0x0555,    // dark gray
    0x055F,    // light blue
    0x05F5,    // light green
    0x05FF,    // light cyan
    0x0F55,    // light red
    0x0F5F,    // light magenta
    0x0FF5,    // yellow
    0x0FFF     // white
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
    xvid_sethb(XVID_DATA, color);
}

uint8_t ln = 0;

static void xhome()
{
    // home wr addr
    xvid_setw(XVID_WR_ADDR, 0);
    xvid_setw(XVID_WR_INC, 1);
    xcolor(cur_color);    // green-on-black
    ln = 0;
}

static void xcls(uint8_t v = ' ')
{
    // clear screen
    xhome();
    for (uint16_t i = 0; i < columns * rows; i++)
    {
        xvid_setw(XVID_DATA, cur_color << 8 | v);
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
    xvid_setlb(XVID_DATA, pgm_read_byte(hex + ((v >> 12) & 0xf)));
    xvid_setlb(XVID_DATA, pgm_read_byte(hex + ((v >> 8) & 0xf)));
    xvid_setlb(XVID_DATA, pgm_read_byte(hex + ((v >> 4) & 0xf)));
    xvid_setlb(XVID_DATA, pgm_read_byte(hex + ((v >> 0) & 0xf)));
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

static void error(const char * msg, uint16_t rdata, uint16_t vdata)
{
    errors++;
    Serial.println("");
    Serial.print(msg);
    Serial.print(" (rd=");
    Serial.print(rdata, HEX);
    Serial.print(" vs ");
    Serial.print(vdata, HEX);
    Serial.print(") Errors: ");
    Serial.println(errors);
    error_flag = true;
}

void setup()
{
    PORTB = BUS_CS_N;      // reset Xosera (LED off) and de-select (for safety)
    DDRB  = PB_OUTPUTS;    // set control signals as outputs
    leds  = TEST_GREEN;    // set default test LEDs
    DDRC  = leds | PC_OUTPUTS;
    PORTC = 0;    // set register number bits to 0 and set green "blink" LED
    Serial.begin(115200);
    Serial.println("Xosera AVR Tester (direct port access AVR @ " MHZSTR ")");
    delay(1);                              // hold reset a moment
    PORTD = 0;                             // clear output data d7-d2
    PORTB = BUS_OFF | BUS_WR | BUS_MSB;    // deselect Xosera, set write, set MSB byte, clear data d1-d0
    DDRD  = PD_BUS_WR;                     // set data d7-d2 as outputs
    DDRB  = PB_BUS_WR;                     // set control signals and data d1-d0 as outputs

    Serial.print("Rebooting FPGA");
    xvid_setw(XVID_BLIT_CTRL, 0x8080);    // reboot FPGA to config 0
    do
    {
        delay(10);
        Serial.print(".");
        xvid_setw(XVID_RD_ADDR, 0x1234);
    } while (xvid_getw(XVID_RD_ADDR) != 0x1234);
    Serial.println("ready.");
    delay(4000);    // let the stunning boot logo display. :)

    xvid_setw(XVID_AUX_ADDR, AUX_VID | 0);    // select width
    uint16_t width = xvid_getw(XVID_AUX_DATA);
    Serial.print("width = ");
    Serial.println(width);

    xvid_setw(XVID_AUX_ADDR, AUX_VID | 1);    // select height
    uint16_t height = xvid_getw(XVID_AUX_DATA);
    Serial.print("height = ");
    Serial.println(height);

    columns = width / 8;
    rows    = width / 8;
    addr    = columns;

    randomSeed(0xc0ffee42);    // deterministic seed TODO
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
    "        \xf9   Full speed 8-bit (using MOVEP opcode) rosco_m68k bus interface (by Ross Bamford)\n"
    "        \xf9   8KB of font RAM with multiple fonts (2KB per 8x8 fonts, 4K per 8x16 font)\n"
    "        \xf9   \"Blitter\" to accelerate VRAM copy and fill operations (TODO, but used at init)\n"
    "        \xf9   2-D operations \"blitter\" with modulo and shifting/masking (TODO)\n"
    "        \xf9   Dual overlayed \"planes\" of video (TODO)\n"
    "        \xf9   Wavetable stereo audio (TODO, spare debug IO for now)\n"
    "        \xf9   Bit-mapped 16 and 256 color graphics modes (256 color TODO)\n"
    "        \xf9   16-color tile mode with \"game\" attributes (e.g., mirroring) (TODO)\n"
    "        \xf9   At least one \"cursor\" sprite (and likely more, TODO)\n";

void show_blurb()
{
    xcls();
    xcolor(cur_color);
    xprint_P(blurb);
    delay(3000);

    xvid_setw(XVID_AUX_ADDR, AUX_VID | 3);    // A_font_ctrl
    xvid_setw(XVID_AUX_DATA, 0x0207);         // 2nd font, 8 high
    delay(3000);

    xvid_setw(XVID_AUX_ADDR, AUX_VID | 3);    // A_font_ctrl
    xvid_setw(XVID_AUX_DATA, 0x000F);         // 1nd font, 16 high
    delay(3000);

    int16_t r = 0;
    for (uint16_t i = 0; i < (rows * 4); i++)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_VID | 0);    // set text start addr
        xvid_setw(XVID_AUX_DATA, r * columns);    // to one line down
        delay(10);
        if (++r > rows)
        {
            r = -rows;
        }
    }
    xvid_setw(XVID_AUX_ADDR, AUX_VID | 0);    // set text start addr
    xvid_setw(XVID_AUX_DATA, 0x0000);
    delay(2000);
}

void test_palette()
{
    xcls();
    for (uint8_t c = 0; c < 16; c++)
    {
        xvid_setw(XVID_WR_ADDR, ((c + 10) * columns) + 10);
        xvid_setw(XVID_DATA, (c << 12) | (c << 8) | ' ');
        for (int l = 1; l < 80; l++)
        {
            xvid_setlb(XVID_DATA, ' ');
        }
    }

    delay(1000);

    for (uint8_t i = 1; i < 16; i++)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_PAL | i);                  // use WR address for palette index
        xvid_setw(XVID_AUX_DATA, ((i << 8) | (i << 4) | i));    // set palette data
    }

    delay(1000);

    for (uint8_t i = 1; i < 16; i++)
    {
        xvid_setw(XVID_AUX_ADDR, AUX_PAL | i);    // use WR address for palette index
        xvid_setw(XVID_AUX_DATA, defpal[i]);      // set palette data
    }
}

void test_reg_access()
{
    xcls();
    xprint("Register read/write self-test...");

    for (int8_t r = XVID_AUX_ADDR; r <= XVID_WR_ADDR; r++)
    {
        uint16_t v = 0;
        do
        {
            xvid_setw(r, v);
            rdata = xvid_getw(r);
            if (rdata != v)
            {
                error("reg verify", rdata, v);
                break;
            }
        } while (--v);
    }

    if (error_flag)
    {
        xcolor(cur_color);
        xprint("Register read/write self-test...FAILED! [FATAL]");
        while (true)
            ;
    }
    else
    {
        xprint("Register read/write self-test...Passed.");
    }
    delay(2000);
}

void vram_speed()
{
    xcls();
    xcolor(cur_color);    // green on black
    xprint("VRAM 16-bit write test, 128KB word:");
    xprint_hex(data);
    xprint("\n");

    // write test
    {
        xvid_setw(XVID_WR_ADDR, addr);
        uint16_t test_time  = millis();
        uint16_t start_time = millis();
        while (start_time == test_time)    // start on "fresh" millisecond to reduce jitter
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
        xcolor(cur_color);
        xprint("VRAM 16-bit write test, 128KB word:");
        xprint_hex(data);
        xprint(" (Time:");
        xprint_dec(elapsed_time);
        xprint(" ms");
        xprint(")\n");
        delay(500);
    }

    xhome();
    xcolor(cur_color);    // green on black
    xprint("VRAM  8-bit write test, 128KB word:");
    xprint_hex(data);
    xprint("\n");

    // write test
    {
        xvid_setw(XVID_WR_ADDR, addr);
        uint16_t test_time  = millis();
        uint16_t start_time = millis();
        while (start_time == test_time)    // start on "fresh" millisecond to reduce jitter
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
    xcolor(cur_color);    // green on black
    xprint("VRAM 16-bit read test, 128KB word:");
    xprint_hex(data);
    xprint("\n");

    // read test
    {
        xvid_setw(XVID_WR_ADDR, addr);
        uint16_t test_time  = millis();
        uint16_t start_time = millis();
        while (start_time == test_time)    // start on "fresh" millisecond to reduce jitter
        {
            start_time = millis();
        }
        uint16_t i = columns;
        do
        {
            rdata = xvid_getw(XVID_DATA);
            if (rdata != data)
            {
                error("16-bit read", rdata, data);
                break;
            }
        } while (i++);
        i = columns;
        do
        {
            rdata = xvid_getw(XVID_DATA);
            if (rdata != data)
            {
                error("16-bit read", rdata, data);
                break;
            }
        } while (--i);
        uint16_t elapsed_time = millis() - start_time;
        xhome();
        xcolor(cur_color);
        xprint("VRAM 16-bit read test, 128KB word:");
        xprint_hex(data);
        xprint(" (Time:");
        xprint_dec(elapsed_time);
        xprint(" ms");
        xprint(")\n");
        delay(500);
    }

    xhome();
    xcolor(cur_color);    // green on black
    xprint("VRAM  8-bit read test, 128KB word:");
    xprint_hex(data);
    xprint("\n");

    // read test
    {
        xvid_setw(XVID_RD_ADDR, addr);
        uint16_t test_time  = millis();
        uint16_t start_time = millis();
        while (start_time == test_time)    // start on "fresh" millisecond to reduce jitter
        {
            start_time = millis();
        }
        uint16_t i = 0;
        do
        {
            rdata = (xvid_gethb(XVID_DATA) << 8) | xvid_getlb(XVID_DATA);
            if (rdata != data)
            {
                error("8-bit read", rdata, data);
                break;
            }
        } while (i++ < 0x8000);
        uint16_t elapsed_time = millis() - start_time;
        xhome();
        xcolor(cur_color);
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
                error("VRAM read", rdata, data);
                break;
            }
        } while (++i);
    }
}

void loop()
{
    xcls();
    xprint("Xosera Retro Graphics Adapter: Mode ");
    xprint_int(width);
    xprint("x");
    xprint_int(height);
    xprint(" (AVR " MHZSTR " test rig)\n\n");

    for (int i = 0; i < 500; i++)
    {
        xprint("Hello! ");
    }
    delay(1000);

    test_reg_access();

    show_blurb();

    vram_speed();

    vram_verify();

    Serial.print(error_flag ? "X" : ".");
    error_flag = false;
    if ((++count & 0x3f) == 0)
    {
        Serial.print(" : ");
        Serial.print(count);
        if (errors)
        {
            Serial.print(" [Err=");
            Serial.print(errors);
            Serial.print("]");
        }
        Serial.println("");
    }
    data++;

    // blink green activity LED
    leds ^= TEST_GREEN;
    DDRC = leds | PC_OUTPUTS;

    xvid_setw(XVID_WR_INC, 1);
    xvid_setw(XVID_WR_ADDR, 0);
    xcolor(cur_color);
    xprint("Xosera VRAM read/write test: d:");
    xprint_hex(data);
    xprint(" c:");
    xprint_int(count);
    if (errors)
    {
        xprint(" Err: ");
        xprint_int(errors);
    }
    error_flag = false;
}
