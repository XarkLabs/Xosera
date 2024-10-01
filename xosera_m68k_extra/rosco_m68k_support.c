//
// rosco_m68k support routines
//
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rosco_m68k/machine.h>

#include "rosco_m68k_support.h"

void debug_putc(char c)
{
    static CharDevice uartA_device;
    static bool       got_device;
    if (!got_device)
    {
        if (mcGetDevice(0, &uartA_device))
        {
            got_device = true;
        }
    }
    mcSendDevice(c, &uartA_device);
}

void debug_puts(const char * str)
{
    register char c;
    while ((c = *str++) != '\0')
    {
        if (c == '\n')
        {
            debug_putc('\r');
        }
        debug_putc(c);
    }
}

char debug_msg_buffer[DEBUG_MSG_SIZE];
void debug_printf(const char * fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(debug_msg_buffer, sizeof(debug_msg_buffer) - 1, fmt, args);
    debug_puts(debug_msg_buffer);
    va_end(args);
}

void debug_hexdump(void * ptr, size_t bytes)
{
    uint8_t * p = (uint8_t *)ptr;
    for (size_t i = 0; i < bytes; i++)
    {
        if ((i & 0xf) == 0)
        {
            if (i)
            {
                debug_puts("    ");
                for (size_t j = i - 16; j < i; j++)
                {
                    int c = p[j];
                    debug_putc(c >= ' ' && c <= '~' ? c : '_');
                }
                debug_puts("\n");
            }
            debug_printf("%04x: ", i);
        }
        else
        {
            debug_puts(", ");
        }
        debug_printf("%02x", p[i]);
    }
    debug_puts("\n");
}
