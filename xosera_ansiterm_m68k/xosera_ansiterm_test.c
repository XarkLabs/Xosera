/*
 * vim: set et ts=4 sw=4
 *------------------------------------------------------------
 *                                  ___ ___ _
 *  ___ ___ ___ ___ ___       _____|  _| . | |_
 * |  _| . |_ -|  _| . |     |     | . | . | '_|
 * |_| |___|___|___|___|_____|_|_|_|___|___|_,_|
 *                     |_____|
 * ------------------------------------------------------------
 * Copyright (c) 2021 Xark
 * MIT License
 *
 * Proto-ANSI terminal emulation WIP
 * ------------------------------------------------------------
 */

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <basicio.h>
#include <machine.h>

#include "xosera_ansiterm_m68k.h"

#if defined(printf)        // this interferes with gcc format attributes
#undef printf
#endif

#define XV_PREP_REQUIRED        // require xv_prep()
#include "xosera_m68k_api.h"

#if !defined(_NOINLINE)
#define _NOINLINE __attribute__((noinline))
#endif

#if DEBUG
static void dputc(char c)
{
#ifndef __INTELLISENSE__
    __asm__ __volatile__(
        "move.w %[chr],%%d0\n"
        "move.l #2,%%d1\n"        // SENDCHAR
        "trap   #14\n"
        :
        : [chr] "d"(c)
        : "d0", "d1");
#endif
}

static void dprint(const char * str)
{
    register char c;
    while ((c = *str++) != '\0')
    {
        if (c == '\n')
        {
            dputc('\r');
        }
        dputc(c);
    }
}

void dprintf(const char * fmt, ...)
{
    static char dprint_buff[4096];
    va_list     args;
    va_start(args, fmt);
    vsnprintf(dprint_buff, sizeof(dprint_buff), fmt, args);
    dprint(dprint_buff);
    va_end(args);
}
#endif

// testing harness functions
#if defined(printf)        // printf macro interferes with gcc format attribute
#define _save_printf printf
#undef printf
#endif

static void tprintf(const char * fmt, ...) __attribute__((format(printf, 1, 2)));
static void tprintf(const char * fmt, ...)
{
    static char tprint_buff[4096];
    va_list     args;
    va_start(args, fmt);
    vsnprintf(tprint_buff, sizeof(tprint_buff), fmt, args);
    char * p = tprint_buff;
    char   c;
    while ((c = *p++) != '\0')
    {
        if (c == '\n')
        {
            xansiterm_putchar('\r');
        }
        xansiterm_putchar(c);
    }
    va_end(args);
}

#if defined(_save_printf)        // retstore printf macro
#define printf _save_printf
#undef _save_printf
#endif

#if !ECHOONLY
// attribute test
static int test_attrib()
{
    static uint8_t cbg_tbl[] = {40, 41, 42, 43, 44, 45, 46, 47, 100, 101, 102, 103, 104, 105, 106, 107, 49};
    static uint8_t cfg_tbl[] = {30, 31, 32, 33, 34, 35, 36, 37, 90, 91, 92, 93, 94, 95, 96, 97, 39};
    for (uint16_t cbg = 0; cbg < sizeof(cbg_tbl); cbg++)
    {
        for (uint16_t cfg = 0; cfg < sizeof(cfg_tbl); cfg++)
        {
            for (uint16_t attr = 0; attr < 8; attr++)
            {
                if (attr > 2 && attr < 7)
                {
                    continue;
                }
                tprintf("\x1b[%d;%d;%dm ^[%d;%d;%dm \x1b[0m",
                        attr,
                        cbg_tbl[cbg],
                        cfg_tbl[cfg],
                        attr,
                        cbg_tbl[cbg],
                        cfg_tbl[cfg]);

                if (checkchar())
                {
                    char c = readchar();
                    if (c == 1)
                    {
                        return -1;
                    }
                    if (c == 3)
                    {
                        return 0;
                    }

                    while (!checkchar())
                        ;

                    c = readchar();
                    if (c == 1)
                    {
                        return -1;
                    }
                    if (c == 3)
                    {
                        return 0;
                    }
                }
            }
            tprintf("\n");
        }
    }

    return 1;
}

void xosera_ansiterm()
{
    LOG("\nxosera_ansiterm_test started.\n\n");
    xosera_init(1);

    xansiterm_init();

    tprintf("\x1b)");        // alternate 8x8 font
    tprintf("Welcome to ANSI Terminal test\n\n");

    bool restart;
    do
    {
        restart = false;
        tprintf("\nPress a key to start mega-test\n");
        tprintf(" (any key pauses, ESC to skip)\n");

        while (!xansiterm_checkchar())
            ;

        char c = xansiterm_readchar();
        if (c == 1)        // ^A exit for kermit
        {
            break;
        }

        if (c != '\x1b')
        {
            int res;
            do
            {
                res = test_attrib();
            } while (res > 0);

            if (res < 0)        // exit for kermit
            {
                break;
            }
        }

        tprintf("\nEcho test, type ^A to exit...\n\n");

        while (true)
        {
            while (!xansiterm_checkchar())
                ;

            c = xansiterm_readchar();
            if (c == 1)        // ^A exit for kermit
            {
                break;
            }

            if (c == 2)        // ^B begin megatest again
            {
                restart = true;
                break;
            }
            xansiterm_putchar(c);
        }

    } while (restart);

    tprintf("\fExiting...\n");

    LOG("\n\nxosera_ansiterm_test exiting.\n");
}
#else
// lightweight echo only test
void xosera_ansiterm()
{
    xosera_init(1);
    xansiterm_init();
    while (true)
    {
        while (!xansiterm_checkchar())
            ;

        char c = xansiterm_readchar();
        if (c == 1)        // ^A exit for kermit
        {
            break;
        }
        xansiterm_putchar(c);
    }
}
#endif