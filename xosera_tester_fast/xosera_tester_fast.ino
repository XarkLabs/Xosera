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
    XVID_RD_INC,         // reg 0 0000: read addr increment value (write-only)
    XVID_WR_INC,         // reg 1 0001: write addr increment value (write-only)
    XVID_RD_MOD,         // reg 2 0010: TODO read modulo width (write-only)
    XVID_WR_MOD,         // reg 3 0011: TODO write modulo width (write-only)
    XVID_WIDTH,          // reg 4 0100: TODO width for 2D blit (write-only)
    XVID_RD_ADDR,        // reg 5 0110: address to read from VRAM (write-only)
    XVID_WR_ADDR,        // reg 6 0111: address to write from VRAM (write-only)
    XVID_DATA,           // reg 7 1000: read/write word from/to VRAM RD/WR
    XVID_DATA_2,         // reg 8 1001: read/write word from/to VRAM RD/WR (for 32-bit)
    XVID_COUNT,          // reg 9 0101: TODO blitter "repeat" count (write-only)
    XVID_VID_CTRL,       // reg A 1010: TODO read status/write control settings (read/write)
    XVID_VID_DATA,       // reg B 1011: TODO video data (as set by VID_CTRL) (write-only)
    XVID_AUX_RD_ADDR,    // reg C 1100: TODO aux read address (font audio etc.?) (write-only)
    XVID_AUX_WR_ADDR,    // reg D 1101: TODO aux write address (font audio etc.?) (write-only)
    XVID_AUX_CTRL,       // reg E 1110: TODO audio and other control? (read/write)
    XVID_AUX_DATA        // reg F 1111: TODO aux memory/register data read/write value
};

static inline void xvid_setw(uint8_t r, uint16_t word)
{
    uint8_t msb = word >> 8;
    uint8_t lsb = word;
    PORTC       = r;                                           // set reg num
    PORTB       = BUS_OFF | BUS_WR | BUS_MSB;                  // de-select Xosera, set write, MSB select
    PORTD       = msb;                                         // set MSB data d7-d2
    PORTB       = BUS_ON | BUS_WR | BUS_MSB | (msb & 0x03);    // select Xosera, set write, MSB select, MSB data d0-d1
    PORTB       = BUS_OFF | BUS_WR | BUS_LSB;                  // de-select Xosera, set write LSB select
    PORTD       = lsb;                                         // set LSB data d7-d2
    PORTB       = BUS_ON | BUS_WR | BUS_LSB | (lsb & 0x03);    // select Xosera set write, LSB select, LSB data d0-d1
    PORTB       = BUS_OFF | BUS_WR | BUS_MSB;                  // de-select Xosera, set write, LSB select
}

static inline void xvid_setlb(uint8_t r, uint8_t lsb)
{
    PORTC = r;                                           // set reg num
    PORTB = BUS_OFF | BUS_WR | BUS_LSB;                  // de-select Xosera, set write LSB select
    PORTD = lsb;                                         // set LSB data d7-d2
    PORTB = BUS_ON | BUS_WR | BUS_LSB | (lsb & 0x03);    // select Xosera set write, LSB select, LSB data d0-d1
    PORTB = BUS_OFF | BUS_WR | BUS_MSB;                  // de-select Xosera, set write, LSB select
}

static inline void xvid_sethb(uint8_t r, uint8_t msb)
{
    PORTC = r;                                           // set reg num
    PORTB = BUS_OFF | BUS_WR | BUS_MSB;                  // de-select Xosera, set write MSB select
    PORTD = msb;                                         // set MSB data d7-d2
    PORTB = BUS_ON | BUS_WR | BUS_MSB | (msb & 0x03);    // select Xosera set write, MSB select, MSB data d0-d1
    PORTB = BUS_OFF | BUS_WR | BUS_MSB;                  // de-select Xosera, set write, MSB select
}

static inline uint16_t xvid_getw(uint8_t r)
{
    PORTC = r;                            // set reg num
    DDRD  = PD_BUS_RD;                    // set data d7-d2 as input
    DDRB  = PB_BUS_RD;                    // set control signals as output and data d1-d0 as input
    PORTB = BUS_ON | BUS_RD | BUS_MSB;    // select Xosera, set read, MSB select
    NOP();                                // 1 cycle delay needed for AVR >= 8MHz (> ~100ns CS pulse)
#if (F_CPU >= 16000000)                   // if 16MHz add an additional
    NOP();                                //  1 cycle delay needed for AVR @ 16MHz (> ~100ns CS pulse)
#endif                                    // end 16MHz
    uint8_t msb =
        (PIND & 0xFC) | (PINB & 0x03);     // read data bus 8-bit MSB value (NOTE: AVR input has a cycle latency)
    PORTB = BUS_OFF | BUS_RD | BUS_LSB;    // de-select Xosera, set read, LSB select
    PORTB = BUS_ON | BUS_RD | BUS_LSB;     // select Xosera, set read, LSB select
    NOP();                                 // 1 cycle delay needed for AVR >= 8MHz (> ~100ns CS pulse)
#if (F_CPU >= 16000000)                    // if 16MHz add an additional
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
#if (F_CPU >= 16000000)                    // if 16MHz add an additional
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

#define TIME_TEST 0                      // normal or timer
static bool     silent_check = false;    // print messages about verify errors
static bool     verify_bad   = false;
static uint8_t  leds;    // diagnostic LEDs
static uint8_t  width;
static uint8_t  status_color = 0x02;    // color for status line (green or red after error)
static uint32_t count;                  // test iteration count
static uint16_t data = 0x0100;          // test "data" value
static uint16_t addr;                   // test starting address (to leave status line)
static uint32_t err;                    // read verify error count
static uint16_t rdata;                  // used to hold data read from VRAM for verification
static uint16_t vdata;


#if (F_CPU == 16000000)
#define MHZSTR "16MHz"
#elif (F_CPU == 8000000)
#define MHZSTR "8MHz"
#else
#define MHZSTR "??MHz"
#endif

static uint8_t shuf_l[256];
static uint8_t shuf_h[256];

static void xcolor(uint8_t color)
{
    xvid_sethb(XVID_DATA, color);
}

static void xprint(const char * s)
{
    while (*s != '\0')
    {
        xvid_setlb(XVID_DATA, *s++);
    }
}

static void xprint(uint16_t v)
{
    static const char hex[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
    xvid_setlb(XVID_DATA, hex[(v >> 12) & 0xf]);
    xvid_setlb(XVID_DATA, hex[(v >> 8) & 0xf]);
    xvid_setlb(XVID_DATA, hex[(v >> 4) & 0xf]);
    xvid_setlb(XVID_DATA, hex[(v >> 0) & 0xf]);
}

static void xprint(uint32_t n, bool decimal)
{
    uint32_t poten = 100000000;
    uint32_t v = n;
    if (v > 999999999)
        v = 999999999;
    while (poten)
    {
        uint8_t d = v / poten;
        if (d || n > poten)
          xvid_setlb(XVID_DATA, '0' + d);
        v -= d * poten;
        poten = poten / 10;
    }
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

    delay(100);    // FPGA meeds a bit to configure and initialize VRAM

    xvid_setw(XVID_VID_CTRL, 0);    // select width
    uint16_t x = xvid_getw(XVID_VID_DATA);
    Serial.print("width = ");
    Serial.println(x);

    xvid_setw(XVID_VID_CTRL, 1);    // select width
    uint16_t y = xvid_getw(XVID_VID_DATA);
    Serial.print("height = ");
    Serial.println(y);

    width = x / 8;
    addr  = width;

    // clear screen
    xvid_setw(XVID_WR_ADDR, 0);
    xvid_setw(XVID_WR_INC, 1);
    xvid_setw(XVID_DATA, 0x0A00 | ' ');
    for (uint16_t i = 1; i < width * 30; i++)
    {
        xvid_setlb(XVID_DATA, ' ');
    }

    xvid_setw(XVID_WR_ADDR, 0 * width);
    xcolor(0x02);    // green-on-black
    xprint("Xosera Retro Graphics Adapter: Mode ");
    xprint(x, true);
    xprint("x");
    xprint(y, true);
    xprint(" AVR " MHZSTR " test.");
    delay(1000);

    xvid_setw(XVID_WR_ADDR, 2 * width);
    xcolor(0x02);    // green-on-black
    xprint("Reg 0-6 read/write test...");

    bool fail = false;

    for (uint8_t r = 0; r < 7; r++)
    {
        uint16_t v = 1;
        do
        {
            xvid_setw(r, v);
            uint16_t rv = xvid_getw(r);
            if (v != rv)
            {
                Serial.print("reg[");
                Serial.print(r, HEX);
                Serial.print("] w=");
                Serial.print(v, HEX);
                Serial.print("!= r=");
                Serial.println(rv, HEX);
                fail = true;
                break;
            }
        } while (v++);
    }
    xvid_setw(XVID_VID_CTRL, 0x0000);   // set text start addr
    xvid_setw(XVID_VID_DATA, 106);      // to one line down

    xvid_setw(XVID_VID_CTRL, 0x0001);   // set line length
    xvid_setw(XVID_VID_DATA, 128);      // to 128 chars long

    xvid_setw(XVID_VID_CTRL, 0x0002);   // set palette[0]
    xvid_setw(XVID_VID_DATA, 0xfff);    // to white

    xvid_setw(XVID_WR_INC, 1);
    xvid_setw(XVID_WR_ADDR, 3 * width);
    xcolor(fail ? 0x40 : 0x02);    // green-on-black
    xprint(fail ? "FAILED!" : "Passed.");

    delay(fail ? 10000 : 3000);
    if (fail)
    {
        status_color = 0x40;
        leds |= TEST_RED;
        DDRC |= leds;
    }

    xvid_setw(XVID_WR_ADDR, 0 * width);
    xvid_setw(XVID_DATA, 0x0A00 | ' ');
    for (uint16_t i = 1; i < width; i++)
    {
        xvid_setlb(XVID_DATA, ' ');
    }

    randomSeed(0xc0ffee42);    // deterministic seed TODO
}

void loop()
{
#if TIME_TEST
    xvid_setw(XVID_WR_ADDR, addr);
    xvid_setw(XVID_WR_INC, 1);

    // write test
    {
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
        } while (++i);
        uint16_t elapsed_time = millis() - start_time;
        Serial.print("64KB x 16-bit write time = ");
        Serial.print(elapsed_time);
        Serial.println(" ms");
    }

    // write test
    {
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
        } while (++i);
        uint16_t elapsed_time = millis() - start_time;
        Serial.print("64KB x 8-bit write time = ");
        Serial.print(elapsed_time);
        Serial.println(" ms");
    }


    xvid_setw(XVID_RD_ADDR, addr);
    xvid_setw(XVID_RD_INC, 1);

    // read test
    {
        uint16_t test_time  = millis();
        uint16_t start_time = millis();
        while (start_time == test_time)    // start on "fresh" millisecond to reduce jitter
        {
            start_time = millis();
        }
        uint16_t i = 0;
        do
        {
            rdata = xvid_getw(XVID_DATA);
            if (rdata != data)
            {
                leds |= TEST_RED;
                DDRC |= leds;
                status_color = 0x40;
                break;
            }
        } while (++i);
        uint16_t elapsed_time = millis() - start_time;
        Serial.print("64KB x 16-bit read time = ");
        Serial.print(elapsed_time);
        Serial.println(" ms");
    }

    // read test
    {
        uint16_t test_time  = millis();
        uint16_t start_time = millis();
        while (start_time == test_time)    // start on "fresh" millisecond to reduce jitter
        {
            start_time = millis();
        }
        uint16_t i = 0;
        do
        {
            rdata = xvid_getlb(XVID_DATA);
            if (rdata != (data & 0xff))
            {
                leds |= TEST_RED;
                DDRC |= leds;
                status_color = 0x40;
                break;
            }
        } while (++i);
        uint16_t elapsed_time = millis() - start_time;
        Serial.print("64KB x 8-bit read time = ");
        Serial.print(elapsed_time);
        Serial.println(" ms");
    }

#else

    uint16_t rdata;

    xvid_setw(XVID_WR_ADDR, addr);
    xvid_setw(XVID_WR_INC, 1);

    {
        uint16_t i = width;
        do
        {
            xvid_setw(XVID_DATA, data);
        } while (++i);
    }

    xvid_setw(XVID_RD_ADDR, addr);
    xvid_setw(XVID_RD_INC, 1);
    {
        uint16_t i = width;
        do
        {
            rdata = xvid_getw(XVID_DATA);
            if (rdata != data)
            {
                err++;
                if (!(leds & TEST_RED))
                {
                    Serial.println(" *** ERR");
                    // light red error LED
                    status_color = 0x40;
                    leds |= TEST_RED;
                    DDRC |= leds;
                }
                if (!silent_check)
                {
                    Serial.print(addr + i, HEX);
                    Serial.print(": WR=");
                    Serial.print(data, HEX);
                    Serial.print(" != RD=");
                    Serial.print(rdata, HEX);
                    Serial.print("    \n");

                    if (err >= 10)
                    {
                        Serial.println("(Silent check enabled > 10 errors)");
                        silent_check = true;
                    }
                }
                verify_bad = true;
                vdata      = data;
                break;
            }
        } while (++i);
    }
    Serial.print(".");
    if ((++count & 0x3f) == 0)
    {
        Serial.print(" : ");
        Serial.print(count);
        if (err)
        {
            Serial.print(" [Err=");
            Serial.print(err);
            Serial.print("]");
        }
        Serial.println("");
    }
#endif

    data++;
#if 0
    addr += width;

    if (addr >= (width * 30))
    {
        addr = 0;
    }
#endif

    // blink green activity LED
    leds ^= TEST_GREEN;
    DDRC = leds | PC_OUTPUTS;

    xvid_setw(XVID_WR_INC, 1);
    xvid_setw(XVID_WR_ADDR, 0);
    xcolor(status_color);
    xprint("Xosera VRAM read/write test: d=");
    xprint(data);
    xprint(" c=");
    xprint(count, true);
    if (err)
    {
        xprint(" Err: ");
        xprint(err >> 16);
        xprint(err);
        if (verify_bad)
        {
            xprint(" ");
            xprint(vdata);
            xprint(" vs ");
            xprint(rdata);
        }
        verify_bad = false;
    }
}
