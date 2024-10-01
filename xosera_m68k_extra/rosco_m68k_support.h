#if !defined(ROSCO_M68K_SUPPORT_H)
#define ROSCO_M68K_SUPPORT_H
//
// rosco_m68k support routines
//

#include <stdlib.h>

#if !defined(NUM_ELEMENTS)
#define NUM_ELEMENTS(a) (sizeof(a) / sizeof(a[0]))
#endif

#if !defined(_NOINLINE)
#define _NOINLINE __attribute__((noinline))
#endif

#if !defined(ASSERT)
#if defined(DEBUG)
#define _ASSERT_STR(s) #s
#define ASSERT(test, fmt, ...)                                                                                         \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(test))                                                                                                   \
        {                                                                                                              \
            debug_printf("\n%s:%d ASSERT(%s) failed: " fmt, __FILE__, __LINE__, _ASSERT_STR(test), __VA_ARGS__);       \
        }                                                                                                              \
    } while (false)
#else
#define ASSERT(test, fmt, ...) (void)0
#endif
#endif

#define DEBUG_MSG_SIZE 4096
extern char debug_msg_buffer[DEBUG_MSG_SIZE];

// debug printing on UART 0
void debug_putc(char c);
void debug_puts(const char * str);
void debug_printf(const char * fmt, ...) __attribute__((format(__printf__, 1, 2)));
void debug_hexdump(void * ptr, size_t bytes);

#endif        // ROSCO_M68K_SUPPORT_H
