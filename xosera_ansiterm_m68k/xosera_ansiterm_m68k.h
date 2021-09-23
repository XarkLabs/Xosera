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

#define TINYECHO 0        // lightweight echo only test
#define DEBUG    1        // set to 1 for debugging

#if !defined(_NOINLINE)
#define _NOINLINE __attribute__((noinline))
#endif

#if defined(DEBUG)

void dprintf(const char * fmt, ...) __attribute__((format(__printf__, 1, 2)));
#define DPRINTF(fmt, ...) dprintf(fmt, ##__VA_ARGS__)

#else
#define DPRINTF(fmt, ...) (void)0
#endif

#if DEBUG
#define LOG(msg)       dprintf(msg)
#define LOGF(fmt, ...) DPRINTF(fmt, ##__VA_ARGS__)
#else
#define LOG(msg)       (void)0
#define LOGF(fmt, ...) (void)0
#endif

// external terminal functions
void xansiterm_init();                 // initialize
void xansiterm_putchar(char c);        // output char
bool xansiterm_checkchar();            // check for input and blink cursor
char xansiterm_readchar();             // get input character
