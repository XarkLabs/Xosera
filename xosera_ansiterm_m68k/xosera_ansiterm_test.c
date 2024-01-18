/*
 * vim: set et ts=4 sw=4
 *------------------------------------------------------------
 *                                  ___ ___ _
 *  ___ ___ ___ ___ ___       _____|  _| . | |_
 * |  _| . |_ -|  _| . |     |     | . | . | '_|
 * |_| |___|___|___|___|_____|_|_|_|___|___|_,_|
 *                     |_____|
 *  __ __
 * |  |  |___ ___ ___ ___ ___
 * |-   -| . |_ -| -_|  _| .'|
 * |__|__|___|___|___|_| |__,|
 *
 * Xark's Open Source Enhanced Retro Adapter
 *
 * - "Not as clumsy or random as a GPU, an embedded retro
 *    adapter for a more civilized age."
 *
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

#include "rosco_m68k_support.h"
#include "videoXoseraANSI/xosera_ansiterm_m68k.h"

#include "xosera_m68k_api.h"

#include "us-fishoe.h"

// keep test functions small
#pragma GCC push_options
#pragma GCC optimize("-Os")

#define INSTALL_XANSI 1        // 0 to disable XANSI RAM installation (to test firmware XANSI)
#define TINYECHO      0        // set to 1 for tiny echo only test

#if !TINYECHO
// attribute test
static int ansiterm_test_attrib()
{
    printf("\nAttribute test (space to pause, ^C to exit, ^A to reboot)\n\n");
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
                    printf("\x1b[%d;%d;%dm ^[%d;%d;%dm AaBb13 \x1b[0m",
                           attr,
                           cbg_tbl[cbg],
                           cfg_tbl[cfg],
                           attr,
                           cbg_tbl[cbg],
                           cfg_tbl[cfg]);
                }
                if (checkchar())
                {
                    char c = readchar();
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
                        c = readchar();
                        if (c >= ' ')
                        {
                            break;
                        }
                    }
                }
                printf("\r\n");
            }
        }
    }
}

static int ansiterm_spamtest()
{
    char   spam[259];
    char * p = spam;
    for (int i = 1; i < 256; i++)
    {
        if (i == 0x1b || i == 0x9b)
        {
            *p++ = '\x1b';
        }
        *p++ = i;
    }
    *p++ = '\0';

    print("\x1b[8m");

    while (true)
    {
        printchar('\0');
        print(spam);
        if (checkchar())
        {
            char c = readchar();
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
                c = readchar();
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
    printf("\nEcho test (^A to reboot, ^B for spam, ^C to exit)\n\n");
    while (true)
    {
        char c = readchar();

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

        printchar(c);
    }
}

static int ansiterm_arttest()
{
    print("\x1b[?3l");            // 80x30
    print("\x1b*");               // ANSI_PC_8x8 font
    print("\f\n\n\n\n\n");        // cls & vertical center
    print((char *)us_fishoe);

    char c = readchar();

    if (c == 1)        // ^A exit for kermit
    {
        return -1;
    }
    return 0;
}

char                reply_str[64];
static const char * wait_reply()
{
    size_t len = 0;
    xv_prep();
    memset(reply_str, 0, sizeof(reply_str));
    uint16_t start   = xm_getw(TIMER);
    uint16_t elapsed = 0;

    do
    {
        if (checkchar())
        {
            char cdata = readchar();
            // CAN/SUB
            if (cdata == 0x18 || cdata == 0x1a)
            {
                break;
            }

            reply_str[len++] = cdata;

            if ((len == 1 && cdata != '\x1b') || (len == 2 && cdata != '[') || (len > 2 && cdata >= 0x40))
            {
                break;
            }
            start = xm_getw(TIMER);
        }
        elapsed = xm_getw(TIMER) - start;
    } while (elapsed < 1000 && len < (sizeof(reply_str) - 1));        // 100.0 ms per char timeout

    return len ? reply_str : NULL;
}

static void ansiterm_testmenu()
{
    while (true)
    {
        printf("\x9bm\n");
        printf(
            "\x9b"
            "c");        // CSI c
        const char * r = wait_reply();
        printf("Terminal %s VT101 compatible. Reply %lu chars: %s%s\n",
               r ? "is" : "is NOT",
               strlen(r),
               r ? "<ESC>" : "",
               r ? r + 1 : "<none>");

        if (r)
        {
            printf("Terminal status: ");
            printf(
                "\x9b"
                "5n");        // DSR status
            r = wait_reply();
            if (r && strncmp(r, "\x1b[0n", 4) == 0)
            {
                printf("OK.");
            }
            else
            {
                printf("BAD!");
            }
            printf(" Reply %lu chars: %s%s\n", strlen(r), r ? "<ESC>" : "", r ? r + 1 : "<none>");

            printf(
                "\x9b"
                "68c");        // CSI 68 c
            r = wait_reply();
            if (r && strncmp(r, "\x1b[?68;", 6) == 0)
            {
                printf(
                    "Terminal gave XANSI reply %lu chars: %s%s\n", strlen(r), r ? "<ESC>" : "", r ? r + 1 : "<none>");
                printf("\x9bs");        // save cursor pos
                printf(
                    "\x9b"
                    "999;999H");        // go to edge of screen
                printf(
                    "\x9b"
                    "6n");        // cursor position report
                r = wait_reply();
                printf("\x9bu");        // restore cursor pos
                printf("Terminal CPR reply %lu chars: %s%s\n", strlen(r), r ? "<ESC>" : "", r ? r + 1 : "<none>");
            }
            else
            {
                printf("Terminal gave bad reply %lu chars: %s%s\n",
                       r ? strlen(r) : 0,
                       r ? "<ESC>" : "",
                       r ? r + 1 : "<none>");
            }
        }

        printf("\n");
        printf("rosco_m68k ANSI Terminal Driver Test Menu\n");
        printf("\n");
        printf("\n");
        printf("  A - ANSI color attribute test.\n");
        printf("  B - Fast spam test\n");
        printf("  C - Echo test\n");
        printf("  D - ANSI art test\n");
        printf("\n");
        printf(" ^A - Warm boot exit\n");
        printf(" ^C - Returns to this menu\n");
        printf("\n");
        printf("Selection:");

        int res = 1;
        do
        {
            char c = readchar();
            switch (c)
            {
                case 1:        // ^A exit for kermit
                    return;
                case 'A':
                case 'a':
                    printchar(c);
                    printchar('\n');
                    res = ansiterm_test_attrib();
                    break;
                case 'B':
                case 'b':
                    printchar(c);
                    printchar('\n');
                    printf("\nSpam test (space to pause, ^C to exit, ^A to reboot)\n\n");
                    res = ansiterm_spamtest();
                    break;
                case 'C':
                case 'c':
                    printchar(c);
                    printchar('\n');
                    res = ansiterm_echotest();
                    break;
                case 'D':
                case 'd':
                    printchar(c);
                    printchar('\n');
                    res = ansiterm_arttest();
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

void xosera_ansiterm_test()
{
    dprintf("Xosera_ansiterm_test started.\n");
#if INSTALL_XANSI
    xosera_xansi_detect(true);
    if (XANSI_CON_INIT())
    {
        printf("Xosera XANSI RAM console initialized.\n");
    }
    else
#endif
    {
        printf("Xosera XANSI RAM console NOT installed.\n");
    }

    printf("\n");

    ansiterm_testmenu();

    printf("\fExiting...\n");

    printf("\n\nxosera_ansiterm_test exiting.\n");

#if INSTALL_XANSI
    // force cold-boot if we messed with vectors
    __asm__ __volatile__(
        "   move.l  _FIRMWARE+4,%a0\n"
        "   reset\n"
        "   jmp     (%a0)\n");
#endif
}

#else

// lightweight echo only test
void xosera_ansiterm_test()
{
#if INSTALL_XANSI
    XANSI_CON_INIT();
#endif
    // PASSTHRU test:    print("\x9b" "8m");
    while (true)
    {
        char c = readchar();
        if (c == 1)        // ^A exit for kermit
        {
            break;
        }
        printchar(c);
    }
}
#endif

#pragma GCC pop_options        // end -Os
