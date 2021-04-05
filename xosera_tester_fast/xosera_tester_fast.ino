#pragma GCC optimize("O3")
#include <Arduino.h>
#include <stdio.h>
#include <stdlib.h>
// Xosera Test Jig (using Arduino Pro Mini AVR @ 8MHz/3.3v with direct port access)
// A FPGA based video card for rosco_m68k retro computers (and others)
// See https://github.com/rosco-m68k/hardware-projects/tree/feature/xosera/xosera
// See
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
    BUS_D6 = 1 << PD6,    // (ordered so bits align with AVR port and no shifting needed)
    BUS_D5 = 1 << PD5,
    BUS_D4 = 1 << PD4,
    BUS_D3 = 1 << PD3,
    BUS_D2 = 1 << PD2,
    BUS_D1 = 1 << PB1,
    BUS_D0 = 1 << PB0,

    // diagnostic Arduino LEDs (on extra A4 and A5 on Pro Mini)
    TEST_GREEN = 1 << PC5,    // green=writing, off=reading
    TEST_RED   = 1 << PC4,    // off=no read errors, on=one or more read errors

    // "logical" defines for signal meanings
    BUS_ACTIVE = 0,              // LOW to select Xosera
    BUS_OFF    = BUS_CS_N,       // HIGH to de-select Xosera
    BUS_WRITE  = 0,              // LOW write to Xosera
    BUS_READ   = BUS_RNW,        // HIGH read from Xosera (will outut on data bus when selected)
    BUS_MSB    = 0,              // LOW even byte (MSB, bits [15:8] for Xosera)
    BUS_LSB    = BUS_BYTESEL,    // HIGH odd byte (LSB, bits  [7:0] for Xosera)

    PB_OUTPUTS = LED | BUS_CS_N | BUS_RNW | BUS_BYTESEL,
    PC_OUTPUTS = TEST_GREEN | TEST_RED | BUS_REG_NUM0 | BUS_REG_NUM1 | BUS_REG_NUM2 | BUS_REG_NUM3,
};

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
    XVID_RD_ADDR,        // reg 0 0000: address to read from VRAM
    XVID_WR_ADDR,        // reg 1 0001: address to write from VRAM
    XVID_DATA,           // reg 2 0010: read/write word from/to VRAM RD/WR
    XVID_DATA_2,         // reg 3 0011: read/write word from/to VRAM RD/WR (for 32-bit)
    XVID_VID_CTRL,       // reg 4 0100: TODO video display mode
    XVID_VID_DATA,       // reg 5 0101: TODO blitter mode/control/status
    XVID_RD_INC,         // reg 6 0110: RD_ADDR increment value
    XVID_WR_INC,         // reg 7 0111: WR_ADDR increment value
    XVID_RD_MOD,         // reg 8 1000: TODO read modulo width
    XVID_WR_MOD,         // reg A 1001: TODO write modulo width
    XVID_WIDTH,          // reg 9 1010: TODO width for 2D blit
    XVID_COUNT,          // reg B 1011: TODO blitter "repeat" count
    XVID_AUX_RD_ADDR,    // reg C 1100: TODO aux read address (font audio etc.?)
    XVID_AUX_WR_ADDR,    // reg D 1101: TODO aux write address (font audio etc.?)
    XVID_AUX_CTRL,       // reg E 1110: TODO audio and other control, controls AUX_DATA operation
    XVID_AUX_DATA        // reg F 1111: TODO read/write data word to AUX_WR_ADDR (depending on AUX_CTRL)
};

// static uint8_t leds = 0;
#define leds GPIOR0    // use obscure GPIOR0 "spare" IO register to shave a cycle off current LED status global load

static inline void xvid_set_reg(uint8_t r, uint16_t word)
{
    uint8_t msb = word >> 8;
    uint8_t lsb = word & 0xff;
    PORTC       = leds | r;    // set reg num (and test LEDs)
    PORTB       = LED | BUS_OFF | BUS_WRITE | BUS_MSB |
            (msb & 0x03);    // de-select Xosera, set write, MSB select, MSB data d0-d1
    PORTD = msb;             // set MSB data d7-d2
    PORTB = LED | BUS_ACTIVE | BUS_WRITE | BUS_MSB |
            (msb & 0x03);    // select Xosera, set write, MSB select, MSB data d0-d1
    PORTB =
        LED | BUS_OFF | BUS_WRITE | BUS_LSB | (lsb & 0x03);    // de-select Xosera, set write LSB select, LSB data d0-d1
    PORTD = lsb;                                               // set LSB data d7-d2
    PORTB =
        LED | BUS_ACTIVE | BUS_WRITE | BUS_LSB | (lsb & 0x03);    // select Xosera set write, LSB select, LSB data d0-d1
    PORTB = LED | BUS_OFF | BUS_WRITE | BUS_LSB;                  // de-select Xosera, set write, LSB select
}

static inline void xvid_set_regb(uint8_t r, uint8_t lsb)
{
    PORTC = leds | r;    // set reg num (and test LEDs)
    PORTB =
        LED | BUS_OFF | BUS_WRITE | BUS_LSB | (lsb & 0x03);    // de-select Xosera, set write LSB select, LSB data d0-d1
    PORTD = lsb;                                               // set LSB data d7-d2
    PORTB =
        LED | BUS_ACTIVE | BUS_WRITE | BUS_LSB | (lsb & 0x03);    // select Xosera set write, LSB select, LSB data d0-d1
    PORTB = LED | BUS_OFF | BUS_WRITE | BUS_LSB;                  // de-select Xosera, set write, LSB select
}

static inline uint16_t xvid_get_reg(uint8_t r)
{
    DDRD        = 0;                                        // set data d7-d2 as input
    DDRB        = PB_OUTPUTS;                               // set control signals as output and data d1-d0 as input
    PORTC       = leds | r;                                 // set reg num (and test LEDs)
    PORTB       = LED | BUS_OFF | BUS_READ | BUS_MSB;       // de-select Xosera, set read, MSB select
    PORTB       = LED | BUS_ACTIVE | BUS_READ | BUS_MSB;    // select Xosera, set read, MSB select
    PORTB       = LED | BUS_ACTIVE | BUS_READ | BUS_MSB;    // again (NOTE: delay needed with 8MHz AVR)
    uint8_t msb = (PIND & 0xFC) | (PINB & 0x03);            // read data bus 8-bit MSB value
    PORTB       = LED | BUS_OFF | BUS_READ | BUS_LSB;       // de-select Xosera, set read, LSB select
    PORTB       = LED | BUS_ACTIVE | BUS_READ | BUS_LSB;    // select Xosera, set read, LSB select
    PORTB       = LED | BUS_ACTIVE | BUS_READ | BUS_LSB;    // again (NOTE: delay needed with 8MHz AVR)
    uint8_t lsb = (PIND & 0xFC) | (PINB & 0x03);            // grab data bus for msg
    PORTB       = LED | BUS_OFF | BUS_WRITE | BUS_LSB;      // de-select Xosera, set write, LSB select
    DDRD        = BUS_D7 | BUS_D6 | BUS_D5 | BUS_D4 | BUS_D3 | BUS_D2;    // set data d7-d2 as outputs
    DDRB        = PB_OUTPUTS | BUS_D1 | BUS_D0;    // set control signals and data d1-d0 as outputs
    return (msb << 8) | lsb;
}

static bool       read_check   = true;
static bool       silent_check = false;
static uint32_t   count        = 0;
static uint16_t   data         = 0x0100;
static uint16_t   addr         = 106;
static uint16_t   inc          = 0x0000;
static uint32_t   err          = 0;
static const char msg[]        = "Xosera AVR 8MHz register read/write test ready...";

void setup()
{
    Serial.begin(115200);
    Serial.println("Xosera Tester");

    leds  = TEST_GREEN;                       // set default test LEDs
    PORTB = BUS_OFF | BUS_WRITE | BUS_MSB;    // reset Xosera (LED off), deselect Xosera, set write, set MSB
    DDRB  = PB_OUTPUTS;                       // set control signals as outputs
    DDRC  = PC_OUTPUTS;
    DDRD  = 0;
    PORTC = leds | 0;                               // all register number to 0 and set test LEDs
    Serial.println(msg);                            // serial msg
    delay(10);                                      // hold reset a moment
    PORTD = 0;                                      // clear output data d7-d2
    PORTB = LED | BUS_OFF | BUS_WRITE | BUS_MSB;    // deselect Xosera, set write, set MSB byte, clear data d1-d0
    DDRD  = BUS_D7 | BUS_D6 | BUS_D5 | BUS_D4 | BUS_D3 | BUS_D2;    // set data d7-d2 as outputs
    DDRB  = PB_OUTPUTS | BUS_D1 | BUS_D0;                           // set control signals and data d1-d0 as outputs

    delay(100);    // FPGA meeds a bit to configure and initialize VRAM

    xvid_set_reg(XVID_WR_INC, 1);
    xvid_set_reg(XVID_WR_ADDR, 0);
    xvid_set_reg(XVID_DATA, 0x0A00 | ' ');
    for (uint16_t i = 1; i < 106 * 30; i++)
    {
        xvid_set_regb(XVID_DATA, ' ');
    }

    xvid_set_reg(XVID_WR_ADDR, 0);
    xvid_set_reg(XVID_DATA, 0x0A00 | msg[0]);
    for (uint8_t i = 1; msg[i] != 0; i++)
    {
        xvid_set_regb(XVID_DATA, msg[i]);
    }
    delay(3000);    // allow screen to fully come up

    for (uint8_t i = '3'; i > '0'; i--)
    {
        xvid_set_regb(XVID_DATA, i);
        xvid_set_regb(XVID_DATA, '.');
        xvid_set_regb(XVID_DATA, '.');
        xvid_set_regb(XVID_DATA, '.');
        delay(1000);
    }
    xvid_set_regb(XVID_DATA, 'G');
    xvid_set_regb(XVID_DATA, 'o');
    xvid_set_regb(XVID_DATA, '!');
}

void loop()
{
    // play with increment for fun
    if ((data & 0xff) == 0x00)
    {
        inc++;
        if (!inc)
        {
            inc++;
        }
    }
    xvid_set_reg(XVID_WR_ADDR, addr);
    xvid_set_reg(XVID_WR_INC, inc);

#if 0    // timer
    uint16_t test_time = millis();
    uint16_t start_time = millis();
    while (start_time == test_time)
    {
      start_time = millis();
    }
#endif
    {
        uint8_t i = 0;
        uint8_t j = 0;
        do
        {
            do
            {
                xvid_set_reg(XVID_DATA, data);
            } while (++i);
        } while (++j);
    }
#if 0
    uint16_t end_time = millis();
    Serial.print("T=");
    Serial.println(end_time-start_time);
#endif

    xvid_set_reg(XVID_RD_ADDR, addr);
    xvid_set_reg(XVID_RD_INC, inc);

    if (read_check)
    {
        uint8_t i = 0;
        uint8_t j = 0;
        do
        {
            do
            {
                uint16_t rdata = xvid_get_reg(XVID_DATA);
                if (rdata != data)
                {
                    if (!(leds & TEST_RED))
                    {
                        Serial.println(" *** ERR");
                        leds |= TEST_RED;
                        PORTC |= leds;
                    }
                    if (!silent_check)
                    {
                        Serial.print((j << 8) | i, HEX);
                        Serial.print(": WR=");
                        Serial.print(data, HEX);
                        Serial.print(" != RD=");
                        Serial.print(rdata, HEX);
                        Serial.print("    \n");
                    }
                    if (++err >= 10 && !silent_check)
                    {
                        Serial.println("(Silent check disabled, > 10 errors)");
                        silent_check = true;
                    }
                }
            } while (++i);
        } while (++j);
    }

    data++;
    addr += 106;

    if (addr >= (106 * 30))
    {
        addr = 0;
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
}