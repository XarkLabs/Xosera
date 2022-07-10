/*
 * vim: set et ts=4 sw=4
 *------------------------------------------------------------
 *                                  ___ ___ _
 *  ___ ___ ___ ___ ___       _____|  _| . | |_
 * |  _| . |_ -|  _| . |     |     | . | . | '_|
 * |_| |___|___|___|___|_____|_|_|_|___|___|_,_|
 *                     |_____|
 * ------------------------------------------------------------
 * Copyright (c) 2022 Xark
 * MIT License
 *
 * Mode test and demonstration for Xosera retro video card
 * ------------------------------------------------------------
 */

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <basicio.h>
#include <machine.h>
#include <sdfat.h>

//#define DELAY_TIME 15000        // slow human speed
//#define DELAY_TIME 5000        // human speed
//#define DELAY_TIME 1000        // impatient human speed
#define DELAY_TIME 500        // machine speed

#if !defined(NUM_ELEMENTS)
#define NUM_ELEMENTS(a) (sizeof(a) / sizeof(a[0]))
#endif

#define XV_PREP_REQUIRED        // require xv_prep() before xosera API functions (for efficiency)
#include "xosera_m68k_api.h"

bool use_sd;        // true if SD card was detected

#if !defined(checkchar)        // newer rosco_m68k library addition, this is in case not present
bool checkchar()
{
    int rc;
    __asm__ __volatile__(
        "move.l #6,%%d1\n"        // CHECKCHAR
        "trap   #14\n"
        "move.b %%d0,%[rc]\n"
        "ext.w  %[rc]\n"
        "ext.l  %[rc]\n"
        : [rc] "=d"(rc)
        :
        : "d0", "d1");
    return rc != 0;
}
#endif

//#define AUDIO_TILE    0x8000        // tilemem LENGTH flag
#define AUDIO_RESTART 0x8000        // restart PERIOD flag

#define SILENCE_VADDR 0xFFFF        // start of TILE
#define SILENCE_LEN   1             // 1 word (two samples)

uint8_t num_audio_channels;
uint8_t audio_channel_mask;

static int init_audio()
{
    xv_prep();
    xm_setw(INT_CTRL, INT_CTRL_CLEAR_ALL_F);
    // upload word of silence
    xm_setw(WR_ADDR, SILENCE_VADDR);
    xm_setw(DATA, 0x0000);

    // play "really high pitch" silence to detect channels
    for (int v = 0; v < 4 * 4; v += 4)
    {
        xreg_setw(AUD0_VOL + v, 0);
        xreg_setw(AUD0_PERIOD + v, 0);
        xreg_setw(AUD0_LENGTH + v, SILENCE_LEN - 1);
        xreg_setw(AUD0_START + v, SILENCE_VADDR);
    }

    num_audio_channels = 0;
    audio_channel_mask = 0;

    xreg_setw(AUD_CTRL, 0x0001);        // enable audio
    // check if audio fully disbled
    uint8_t aud_ena = xreg_getw(AUD_CTRL) & 1;
    if (!aud_ena)
    {
        printf("Xosera audio support disabled.\n");
        return 0;
    }

    // channels should instantly trigger ready interrupt
    audio_channel_mask = xm_getbl(INT_CTRL) & INT_CTRL_AUD_ALL_F;
    while (audio_channel_mask & (1 << num_audio_channels))
    {
        num_audio_channels++;
    }

    if (num_audio_channels == 0)
    {
        printf("Strange... Xosera has audio support, but no channels?\n");
    }

    printf("Xosera audio channels = %d\n", num_audio_channels);

    // set all channels to "full volume" silence at very slow period
    for (int v = 0; v < 4 * 4; v += 4)
    {
        xreg_setw(AUD0_VOL + v, 0x8080);
        xreg_setw(AUD0_PERIOD + v, AUDIO_RESTART | 1600);
        xreg_setw(AUD0_LENGTH + v, (SILENCE_LEN - 1));
        xreg_setw(AUD0_START + v, SILENCE_VADDR);
    }

    return num_audio_channels;
}

#define TEST_FILE    "/glorious_dawn_16k.raw"        // signed 8-bit mono PCM
#define SAMPLE_RATE  16000                           // sample rate for above
#define BUFFER_BYTES 0x1000
#define BUFFER_WORDS (BUFFER_BYTES / 2)
#define BUFFER       0x2000

uint16_t filebuffer[BUFFER_WORDS];

bool read_buffer(void * file)
{
    uint8_t * bp = (uint8_t *)filebuffer;
    for (int r = 0; r < BUFFER_BYTES; r += 512)
    {
        if (fl_fread(bp, 1, 512, file) <= 0)
        {
            return false;
        }
        bp += 512;
    }
    return true;
}

void upload_buffer(uint16_t buf_off)
{
    xv_prep();
    uint16_t * wp = &filebuffer[0];
    xm_setw(WR_INCR, 0x0001);
    xm_setw(WR_ADDR, BUFFER + buf_off);
    for (int i = 0; i < BUFFER_WORDS; i++)
    {
        uint16_t w = *wp++;
        xm_setw(DATA, w);
    }
}

void queue_buffer(uint16_t buf_off, uint16_t period)
{
    xv_prep();
    xreg_setw(AUD0_VOL, 0x8080);
    xreg_setw(AUD0_LENGTH, (BUFFER_WORDS - 1));
    xreg_setw(AUD0_START, BUFFER + buf_off);
    xreg_setw(AUD0_PERIOD, period);

    xm_setw(INT_CTRL, INT_CTRL_CLEAR_ALL_F);
}

void audiostream_test()
{
    cpu_delay(3000);

    printf("\033c\033[?25l");        // ANSI reset, disable input cursor

    printf("\nXosera_audiostream_m68k\n\n");

    if (SD_check_support())
    {
        printf("SD card supported: ");

        if (SD_FAT_initialize())
        {
            printf("SD card ready.\n");
            use_sd = true;
        }
        else
        {
            printf("no SD card present, exiting.\n");
            use_sd = false;
            return;
        }
    }
    else
    {
        printf("Requires SD card support, exiting.\n");
        return;
    }

    if (init_audio() < 2)
    {
        printf("Requires 2 audio channels, exiting.\n");
        return;
    }

    printf("Streaming test file: \"%s\"\n", TEST_FILE);
    void * file = fl_fopen(TEST_FILE, "r");
    if (!file)
    {
        printf("...Unable to open, exiting.\n");
        return;
    }

    xv_prep();

    uint16_t rate   = SAMPLE_RATE;
    uint32_t clk_hz = xreg_getw(VID_HSIZE) > 640 ? 33750000ULL : 25125000ULL;
    uint16_t period = (clk_hz + (rate / 2)) / rate;

    printf("        Sample rate: %u (PERIOD %u @ %s MHz)\n",
           rate,
           period,
           xreg_getw(VID_HSIZE) > 640 ? "33.75" : "25.125");

#if 0
    xreg_setw(PA_DISP_ADDR, BUFFER);
    xreg_setw(PA_LINE_LEN, 64);
    xreg_setw(PA_GFX_CTRL, 0x006F);
#endif
    xm_setw(SYS_CTRL, 0x000F);

    int bytes = 0;
    printf("\nPlaying offset: %9u ", bytes);

    uint16_t buf_off = 0;
    bool     exit    = false;

    read_buffer(file);
    upload_buffer(0);
    queue_buffer(0, period);
    read_buffer(file);
    upload_buffer(BUFFER_WORDS);
    queue_buffer(BUFFER_WORDS, period);

    while (!exit)
    {
        exit = !read_buffer(file);
        bytes += BUFFER_BYTES;

        if (exit || checkchar())
        {
            exit = true;
            break;
        }

        while ((xm_getw(INT_CTRL) & INT_CTRL_AUD0_INTR_F) == 0)
        {
            if (checkchar())
            {
                exit = true;
                break;
            }
        }
        upload_buffer(buf_off);
        queue_buffer(buf_off, period);

        printf("\rPlaying offset: %9u ", bytes);

        buf_off ^= BUFFER_WORDS;
    }

    // set all channels to silence at very slow period
    for (int v = 0; v < 4; v++)
    {
        uint16_t vo = 1 << v;
        xreg_setw(AUD0_VOL + vo, 0x0000);
        xreg_setw(AUD0_PERIOD + vo, 0x8000 | 0x7FFF);
        xreg_setw(AUD0_LENGTH + vo, (SILENCE_LEN - 1));
        xreg_setw(AUD0_START + vo, SILENCE_VADDR);
    }
    xreg_setw(AUD_CTRL, 0x0000);        // disable audio

    fl_fclose(file);

    printf("\n\nExiting.");
    printf("\033[?25h");
}
