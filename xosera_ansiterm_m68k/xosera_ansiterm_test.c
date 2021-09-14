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
static char ansiterm_waitchar()
{
    while (!xansiterm_checkchar())
        ;

    return xansiterm_readchar();
}

static void tputs(char * p)
{
    char c;
    while ((c = *p++) != '\0')
    {
        if (c == '\n')
        {
            xansiterm_putchar('\r');
        }
        xansiterm_putchar(c);
    }
}

#if !TINYECHO

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
    tputs(tprint_buff);
    va_end(args);
}

#if defined(_save_printf)        // retstore printf macro
#define printf _save_printf
#undef _save_printf
#endif

// attribute test
static int ansiterm_test_attrib()
{
    tprintf("\nAttribute test (space to pause, ^C to exit, ^A to reboot)\n\n");
    static uint8_t cbg_tbl[] = {40, 41, 42, 43, 44, 45, 46, 47, 100, 101, 102, 103, 104, 105, 106, 107, 49};
    static uint8_t cfg_tbl[] = {30, 31, 32, 33, 34, 35, 36, 37, 90, 91, 92, 93, 94, 95, 96, 97, 39};
    while (true)
    {
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
                    tprintf("\x1b[%d;%d;%dm ^[%d;%d;%dm AaBb123 \x1b[0m",
                            attr,
                            cbg_tbl[cbg],
                            cfg_tbl[cfg],
                            attr,
                            cbg_tbl[cbg],
                            cfg_tbl[cfg]);
                }
                if (xansiterm_checkchar())
                {
                    char c = xansiterm_readchar();
                    while (true)
                    {
                        if (c == 1)
                        {
                            return -1;
                        }
                        else if (c == 3)
                        {
                            return 0;
                        }
                        c = xansiterm_readchar();
                        if (c >= ' ')
                        {
                            break;
                        }
                    }
                }
                tprintf("\r\n");
            }
        }
    }
}

static int ansiterm_spamtest()
{
    char spam[97];
    for (uint8_t i = 0; i < 96; i++)
    {
        spam[i] = i + ' ';
    }
    spam[96] = 0;

    while (true)
    {
        tputs(spam);
        if (xansiterm_checkchar())
        {
            char c = xansiterm_readchar();
            while (true)
            {
                if (c == 1)
                {
                    return -1;
                }
                else if (c == 2 || c == 3)
                {
                    return 0;
                }
                c = xansiterm_readchar();
                if (c >= ' ')
                {
                    break;
                }
            }
        }
    }
}

static int ansiterm_echotest()
{
    tprintf("\nEcho test (^A to reboot, ^B for spam, ^C to exit)\n\n");
    while (true)
    {
        char c = ansiterm_waitchar();

        if (c == 1)        // ^A exit for kermit
        {
            return -1;
        }
        else if (c == 2)
        {
            if (ansiterm_spamtest() < 0)
                return -1;
        }
        else if (c == 3)        // ^B begin megatest again
        {
            return 0;
        }

        xansiterm_putchar(c);
    }
}

static void ansiterm_testmenu()
{
    while (true)
    {
        tprintf("\x9bm\n");
        tprintf("\n");
        tprintf("rosco_m68k ANSI Terminal Driver Test Menu\n");
        tprintf("\n");
        tprintf("  A - ANSI color attribute test.\n");
        tprintf("  B - Fast spam test\n");
        tprintf("  C - Echo test\n\n");
        tprintf(" ^A - Warm boot exit\n");
        tprintf(" ^C - Returns to this menu\n");
        tprintf("\n");
        tprintf("Selection:");

        int res = 1;
        do
        {
            char c = ansiterm_waitchar();
            switch (c)
            {
                case 1:        // ^A exit for kermit
                    return;
                case 'A':
                case 'a':
                    xansiterm_putchar(c);
                    xansiterm_putchar('\n');
                    res = ansiterm_test_attrib();
                    break;
                case 'B':
                case 'b':
                    xansiterm_putchar(c);
                    xansiterm_putchar('\n');
                    tprintf("\nSpam test (space to pause, ^C to exit, ^A to reboot)\n\n");
                    res = ansiterm_spamtest();
                    break;
                case 'C':
                case 'c':
                    xansiterm_putchar(c);
                    xansiterm_putchar('\n');
                    res = ansiterm_echotest();
                    break;
                default:
                    break;
            }

        } while (res > 0);
        if (res < 0)
        {
            return;
        }
    }
}

void xosera_ansiterm()
{
    LOG("\nxosera_ansiterm_test started.\n\n");
    xosera_init(1);
    xansiterm_init();

    ansiterm_testmenu();

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
        char c = ansiterm_waitchar();
        if (c == 1)        // ^A exit for kermit
        {
            break;
        }
        xansiterm_putchar(c);
    }
}
#endif