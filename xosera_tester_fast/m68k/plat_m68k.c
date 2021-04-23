#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <machine.h>
#include "xosera_tester_fast.h"
#include "arch/m68k/platform.h"

static uint8_t *xvid = (uint8_t*)0xf80060;
static volatile uint32_t * const upticks = (volatile uint32_t * const)0x40C;

#define REGHI(r) ((r * 4))
#define REGLO(r) ((r * 4) + 2)

_Serial Serial;

void xvid_setw(uint8_t r, uint16_t word)
{
    xvid[REGHI(r)] = (word & 0xFF00) >> 8;
    xvid[REGLO(r)] = (word & 0x00FF);
}

void xvid_setlb(uint8_t r, uint8_t lsb)
{
    xvid[REGLO(r)] = lsb;
}

void xvid_sethb(uint8_t r, uint8_t msb)
{
    xvid[REGHI(r)] = msb;
}

uint16_t xvid_getw(uint8_t r)
{
    return (xvid[REGHI(r)] << 8) | xvid[REGLO(r)];
}

// bytesel = LSB (default) or 0 for MSB
uint8_t xvid_getb(uint8_t r, uint8_t bytesel)
{
    if (bytesel == 0) {
        return xvid[REGHI(r)];
    } else {
        return xvid[REGLO(r)];
    }
}

uint8_t xvid_getlb(uint8_t r)
{
    return xvid_getb(r, 1);
}

uint8_t xvid_gethb(uint8_t r)
{
    return xvid_getb(r, 0);
}

void interrupts()
{
    mcEnableInterrupts();
}

void noInterrupts()
{
    mcDisableInterrupts();
}

void randomSeed(uint32_t v)
{
    // TODO not currently supported
    (void)(v);
}

uint32_t random(uint32_t max)
{
    return rand() % max;
}

void delay(uint32_t v)
{
    mcDelaymsec10(v / 20);
}

uint16_t millis()
{
    return *upticks;
}

void platform_print_bin(uint32_t dw)
{
    printf("%032b", dw);
}

void platform_print_dec(uint32_t dw)
{
    printf("%d", dw);
}

void platform_print_hex(uint32_t dw)
{
    printf("%08x", dw);
}

static void plaf_ser_begin(uint32_t p)
{
    // This space intentionally left blank
    (void)(p);
}

static void plaf_ser_print(const char *s)
{
    printf(s);
}

static void plaf_ser_println(const char *s)
{
    printf("%s\n", s);
}

void platform_setup()
{
    // TODO stop firmware controlling LEDs
    
    Serial.begin = plaf_ser_begin;
    Serial.print = plaf_ser_print;
    Serial.println = plaf_ser_println;
}

void platform_activity()
{
    // TODO blink green LED
}

void platform_on_error()
{
    // TODO red LED
}

void kmain() {
    setup();

    do {
      loop();
    } while (1);
}

