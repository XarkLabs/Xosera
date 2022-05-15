#if !defined(ROSCO_M68K_SUPPORT_H)
#define ROSCO_M68K_SUPPORT_H
//
// rosco_m68k support routines
//
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <basicio.h>
#include <machine.h>

#if !defined(NUM_ELEMENTS)
#define NUM_ELEMENTS(a) (sizeof(a) / sizeof(a[0]))
#endif

#if DEBUG
#define _ASSERT_STR(s) #s
#define ASSERT(test, fmt, ...)                                                                                         \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(test))                                                                                                   \
        {                                                                                                              \
            dprintf("\n%s:%d ASSERT(%s) failed: " fmt, __FILE__, __LINE__, _ASSERT_STR(test), __VA_ARGS__);            \
        }                                                                                                              \
    } while (false)
#else
#define ASSERT(test) (void)0
#endif

#if !defined(_NOINLINE)
#define _NOINLINE __attribute__((noinline))
#endif

unsigned int crc32b(unsigned int crc, const void * buf, size_t size);
void         disable_sd_boot();

#if !defined(checkchar)        // newer rosco_m68k library addition, this is in case not present
bool checkchar();
#endif

void   dprintf(const char * fmt, ...) __attribute__((format(__printf__, 1, 2)));
void   dputc(char c);
void   dputs(const char * str);
int    dreadline(char * buf, int buf_size);
char * next_token(char ** next_token);

#endif        // ROSCO_M68K_SUPPORT_H
