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

#if (F_CPU == 16000000)
#define MHZSTR "16MHz"
#elif (F_CPU == 8000000)
#define MHZSTR "8MHz"
#else
#define MHZSTR "??MHz"
#endif

#define PLATFORM "AVR"

enum
{
    // AVR hardware pins
    LED         = 1 << PB5,        // Arduino LED
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
    BUS_ON  = 0,                  // LOW to select Xosera
    BUS_OFF = BUS_CS_N,           // HIGH to de-select Xosera
    BUS_WR  = 0,                  // LOW write to Xosera
    BUS_RD  = BUS_RNW,            // HIGH read from Xosera (will outut on data bus when selected)
    BUS_MSB = 0,                  // LOW even byte (MSB, bits [15:8] for Xosera)
    BUS_LSB = BUS_BYTESEL,        // HIGH odd byte (LSB, bits  [7:0] for Xosera)

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

    // AUX read-only setting AUX_ADDR, reading AUX_DATA
    AUX_VID             = 0x0000,        // 0-8191 8-bit address (bits 15:8 ignored writing)
    AUX_VID_W_DISPSTART = 0x0000,        // display start address
    AUX_VID_W_TILEWIDTH = 0x0001,        // tile line width (usually WIDTH/8)
    AUX_VID_W_SCROLLXY  = 0x0002,        // [10:8] H fine scroll, [3:0] V fine scroll
    AUX_VID_W_FONTCTRL  = 0x0003,        // [9:8] 2KB font bank, [3:0] font height
    AUX_VID_W_GFXCTRL   = 0x0004,        // [1] v double TODO, [0] h double

    // AUX write-only setting AUX_ADDR, writing AUX_DATA
    AUX_VID_R_WIDTH    = 0x0000,        // display resolution width
    AUX_VID_R_HEIGHT   = 0x0001,        // display resolution height
    AUX_VID_R_FEATURES = 0x0002,        // [15] = 1 (test)
    AUX_VID_R_SCANLINE = 0x0003,        // [15] V blank, [14:11] zero [10:0] V line
    AUX_W_FONT         = 0x4000,        // 0x4000-0x5FFF 8K byte font memory (even byte [15:8] ignored)
    AUX_W_COLORTBL     = 0x8000,        // 0x8000-0x80FF 256 word color lookup table (0xXRGB)
    AUX_W_AUD          = 0xc000         // 0xC000-0x??? TODO (audio registers)
};

// for slower testing
#if 0
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

static uint8_t  leds;                    // diagnostic LEDs

static inline uint8_t xvid_getlb(uint8_t r)
{
    return xvid_getb(r, BUS_LSB);
};
static inline uint8_t xvid_gethb(uint8_t r)
{
    return xvid_getb(r, BUS_MSB);
}

void platform_print_bin(uint32_t dw)
{
    Serial.print(dw, BIN);
}

void platform_print_dec(uint32_t dw)
{
    Serial.print(dw);
}

void platform_print_hex(uint32_t dw)
{
    Serial.print(dw, HEX);
}

void platform_setup()
{
    PORTB = BUS_CS_N;          // de-select Xosera (for safety)
    DDRB  = PB_OUTPUTS;        // set control signals as outputs
    leds  = TEST_GREEN;        // set default test LEDs
    DDRC  = leds | PC_OUTPUTS;
    PORTC = 0;        // set register number bits to 0 and set green "blink" LED
    Serial.begin(115200);
    Serial.println("\f\r\nXosera AVR Tester (direct port access AVR @ " MHZSTR ")");
    PORTD = 0;                                 // clear output data d7-d2
    PORTB = BUS_OFF | BUS_WR | BUS_MSB;        // deselect Xosera, set write, set MSB byte, clear data d1-d0
    DDRD  = PD_BUS_WR;                         // set data d7-d2 as outputs
    DDRB  = PB_BUS_WR;                         // set control signals and data d1-d0 as outputs
}

void platform_activity()
{
    // blink green activity LED
    leds ^= TEST_GREEN;
    DDRC = leds | PC_OUTPUTS;
}

void platform_on_error()
{
    leds |= TEST_RED;
}


#define XOSERA_ARDUINO

// Nasty hack - test file just gets appended here...

