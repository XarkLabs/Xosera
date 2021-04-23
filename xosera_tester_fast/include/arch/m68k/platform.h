#ifndef __XOSERA_TESTER_FAST_PLAF_68K_H
#define __XOSERA_TESTER_FAST_PLAF_68K_H

#include <stdint.h>

#define PLATFORM "rosco_m68k"
#define PROGMEM

#ifdef __cplusplus
extern "C" {
#endif

#define pgm_read_byte(v)  (*((uint8_t*)v))
#define pgm_read_word(v)  (*((uint16_t*)v))

#define MHZSTR            "10MHz"

typedef struct {
  void (*begin)(uint32_t);
  void (*println)(const char*);
  void (*print)(const char*);
} _Serial;

extern _Serial Serial;

void xvid_setw(uint8_t r, uint16_t word);
void xvid_setlb(uint8_t r, uint8_t lsb);
void xvid_sethb(uint8_t r, uint8_t msb);
uint16_t xvid_getw(uint8_t r);
// bytesel = LSB (default) or 0 for MSB
uint8_t xvid_getb(uint8_t r, uint8_t bytesel);
uint8_t xvid_getlb(uint8_t r);
uint8_t xvid_gethb(uint8_t r);

void interrupts();
void noInterrupts();
void randomSeed(uint32_t v);
uint32_t random(uint32_t max);
void delay(uint32_t v);
uint16_t millis();

void platform_print_bin(uint32_t dw);
void platform_print_dec(uint32_t dw);
void platform_print_hex(uint32_t dw);

void platform_setup();
void platform_activity();
void platform_on_error();

#ifdef __cplusplus
}
#endif

#endif//__XOSERA_TESTER_FAST_PLAF_68K_H
