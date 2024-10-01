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

#include <dirent.h>

#include <rosco_m68k/machine.h>
#include <rosco_m68k/xosera.h>

bool use_sd;        // true if SD card was detected

#define AUDIO_RESTART 0x8000        // restart PERIOD flag

#define SILENCE_VADDR XR_TILE_ADDR        // start of TILE (zero font word assumed)
#define SILENCE_TILE  0x8000              // TILE flag for silence
#define SILENCE_LEN   1                   // 1 word (two samples)

uint8_t num_audio_channels;
uint8_t audio_channel_mask;

static void audio_silence()
{
    xv_prep();
    // set all channels to "full volume" silence at very slow period
    for (int v = 0; v < 4 * 4; v += 4)
    {
        xreg_setw(AUD0_VOL + v, 0x8080);
        xreg_setw(AUD0_LENGTH + v, SILENCE_TILE | (SILENCE_LEN - 1));
        xreg_setw(AUD0_START + v, SILENCE_VADDR);
        xreg_setw(AUD0_PERIOD + v, AUDIO_RESTART | 0x7FFF);
    }
}

static int init_audio()
{
    xv_prep();
    xreg_setw(AUD_CTRL, 0x0000);        // disable audio

    xm_set_int_ack(INT_CTRL_CLEAR_ALL_F);
    // upload word of silence to TILE (probaby already zero, but...)
    xmem_setw_wait(SILENCE_VADDR, 0x0000);

    // play "really high pitch" silence to detect channels
    for (int v = 0; v < 4 * 4; v += 4)
    {
        xreg_setw(AUD0_VOL + v, 0);
        xreg_setw(AUD0_LENGTH + v, SILENCE_TILE | (SILENCE_LEN - 1));
        xreg_setw(AUD0_START + v, SILENCE_VADDR);
        xreg_setw(AUD0_PERIOD + v, 0);
    }

    num_audio_channels = 0;
    audio_channel_mask = 0;

    xreg_setw(AUD_CTRL, 0x0001);        // enable audio
    // check if audio fully disbled
    uint8_t aud_ena = xreg_getw(AUD_CTRL) & 1;
    if (!aud_ena)
    {
        printf("Xosera audio DMA support disabled.\n");
        return 0;
    }

    // channels should instantly trigger ready interrupt
    audio_channel_mask = xm_xm_get_int_status() & INT_CTRL_AUD_ALL_F;
    while (audio_channel_mask & (1 << num_audio_channels))
    {
        num_audio_channels++;
    }

    if (num_audio_channels == 0)
    {
        printf("Strange... Xosera has audio support, but no channels?\n");
    }

    audio_silence();

    return num_audio_channels;
}

void disable_audio()
{
    audio_silence();
    xv_prep();
    xreg_setw(AUD_CTRL, 0x0000);        // disable audio DMA (and silence will continue)
}

#define BUFFER_BYTES 0x4000
#define BUFFER_WORDS (BUFFER_BYTES / 2)
#define BUFFER       0xC000

uint32_t file_bytes;
bool     at_eof;
bool     graphics;

uint16_t cur_rate       = 2;
uint16_t sample_rates[] = {8000, 11025, 16000, 22050, 24000};

#define MAX_PCMS    26
#define MAX_NAMELEN 64
char pcm_files[MAX_PCMS][MAX_NAMELEN];
int  num_pcms;
int  pcm_size[MAX_PCMS];

uint16_t filebuffer[BUFFER_WORDS];


// NOTE: Name 8-bit signed headerless PCM files ending with "_<rate>.raw" (where <rate> is 8000 to 24000)
const char * get_file()
{
    num_pcms = 0;
    memset(pcm_files, 0, sizeof(pcm_files));

    opendir();

    DIR * dirfd;

    if (opendir("/", dirfd))
    {
        struct dirent * entry;

        while (num_pcms < MAX_PCMS && readdir(dirfd, &entry) == 0)
        {
            int len = strlen(entry->d_name);
            if (len >= 4)
            {
                const char * dig = entry->d_name + len - 5;
                while (*dig >= '0' && *dig <= '9')
                {
                    dig--;
                }
                int file_rate = atoi(dig + 1);

                const char * ext = entry->d_name + len - 4;
                if ((file_rate >= 8000 && file_rate <= 24000) && (strcmp(ext, ".raw") == 0 || strcmp(ext, ".RAW") == 0))
                {
                    strcpy(pcm_files[num_pcms], "/");
                    strcat(pcm_files[num_pcms], entry->d_name);
                    // TODO: stat
                    pcm_size[num_pcms] = entry.size;

                    num_pcms++;
                }
            }
        }

        closedir(&dirstat);
    }

    int num = 0;
    do
    {
        printf("\033cPCM files available:\n\n");

        for (int i = 0; i < num_pcms; i++)
        {
            printf("%c - [%6dK] %s\n", 'A' + i, ((pcm_size[i] + 1023) / 1024), pcm_files[i]);
        }

        printf("\nSelect [A-%c] or [+]/[-] to ajust rate %d:", 'A' + num_pcms - 1, sample_rates[cur_rate]);

        int key = mcInputchar();

        if (key == '-' || key == '+' || key == '=')
        {
            if (key == '-')
            {
                cur_rate -= 1;
            }
            else
            {
                cur_rate += 1;
            }

            cur_rate = cur_rate % NUM_ELEMENTS(sample_rates);
        }

        if (key == 27)
        {
            printf("ESC\n\n");
            return NULL;
        }

        if (key >= 'a' && key <= 'z')
        {
            key -= ('a' - 'A');
        }

        num = key - 'A';

    } while (num < 0 || num >= num_pcms);

    printf("%c\n\n", 'A' + num);

    return pcm_files[num];
}

// read PCM file into buffer
void read_buffer(void * file)
{
    uint8_t * bp = (uint8_t *)filebuffer;
    for (int r = 0; r < BUFFER_BYTES; r += 512)
    {
        int cnt;
        cnt = fl_fread(bp, 1, 512, file);
        if (cnt < 512)
        {
            if (cnt < 0)
            {
                cnt = 0;
            }

            memset(bp + cnt, 0, 512 - cnt);
            at_eof = true;
            cnt    = 512;
        }
        bp += cnt;
        file_bytes += cnt;
    }

    return;
}

// upload to current buffer
void upload_buffer(uint16_t buf_off)
{
    xv_prep();
    xm_set_vram_mask(0x0F);                    // no VRAM write masking
    xm_setw(WR_INCR, 0x0001);                  // increment of 1 word
    xm_setw(WR_ADDR, BUFFER + buf_off);        // upload address
    uint16_t * wp = &filebuffer[0];            // ptr to words to upload
    for (int i = 0; i < BUFFER_WORDS; i++)
    {
        uint16_t w = *wp++;
        xm_setw(DATA, w);
    }
}

// queue buffer for playback
void queue_buffer(uint16_t buf_off, uint16_t period)
{
    xv_prep();
    xreg_setw(AUD0_VOL, 0x8080);                       // full volume to L & R
    xreg_setw(AUD0_LENGTH, (BUFFER_WORDS - 1));        // words in buffer
    xreg_setw(AUD0_START, BUFFER + buf_off);           // address of current buffer
    xreg_setw(AUD0_PERIOD, period);                    // sample period

    xm_set_int_ack(INT_CTRL_AUD0_INTR_F);        // acknowledge & clear audio 0 ready interrupt
}

// audiostream test
int main()
{
    mcBusywait(1000 * 500);        // wait a bit for terminal window/serial
    while (mcCheckInput())         // clear any queued input
    {
        mcInputchar();
    }
    printf("Xosera_audiostream_m68k\n\n");

    if (SD_check_support())
    {
        if (SD_FAT_initialize())
        {
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

    bool quit = false;

    do
    {
        if (init_audio() < 1)
        {
            printf("Requires 1 audio channel, exiting.\n");
            return;
        }

        const char * filename = get_file();
        uint16_t     buf_off  = 0;
        at_eof                = 0;
        file_bytes            = 0;

        if (!filename)
        {
            break;
        }

        printf("Streaming test file: \"%s\"\n", filename);
        void * file = fl_fopen(filename, "r");
        if (!file)
        {
            printf("...Unable to open, exiting.\n");
            return;
        }

        xv_prep();

        uint16_t rate   = sample_rates[cur_rate];
        uint32_t clk_hz = xosera_sample_hz();
        uint16_t period = (clk_hz + (rate / 2)) / rate;

        printf("        Sample rate: %u (PERIOD %u @ %s MHz)\n",
               rate,
               period,
               xosera_sample_hz() > AUDIO_PERIOD_HZ_640 ? "33.75" : "25.125");

        printf("\nPlaying offset: %9u ", (unsigned int)file_bytes);


        bool next = false;

        // pre-read both buffers
        read_buffer(file);
        upload_buffer(0);
        queue_buffer(0, period);
        while ((xm_get_int_status() & INT_CTRL_AUD0_INTR_F) == 0)
        {
            if (mcCheckInput())
            {
                break;
            }
        }

        read_buffer(file);
        upload_buffer(BUFFER_WORDS);
        queue_buffer(BUFFER_WORDS, period);

        printf("\033[?25l");        // disable input cursor
        while (!next)
        {
            // mcPrint status
            if (!graphics)
            {
                printf("\rPlaying offset: %9u  VRAM: 0x%04x", (unsigned int)file_bytes, BUFFER + buf_off);
            }

            if (mcCheckInput())
            {
                char k = mcInputchar();
                if (k == '\x1b')
                {
                    quit = true;
                    break;
                }
                else if (k == 'g' || k == 'G')
                {
                    graphics = !graphics;

                    if (graphics)
                    {
                        xreg_setw(PA_DISP_ADDR, BUFFER);
                        xreg_setw(PA_GFX_CTRL, MAKE_GFX_CTRL(0xF0, GFX_VISIBLE, GFX_4_BPP, GFX_BITMAP, GFX_2X, GFX_2X));
                        xreg_setw(PA_LINE_LEN, 64);
                    }
                    else
                    {
                        xreg_setw(PA_DISP_ADDR, 0x0000);
                        xreg_setw(PA_GFX_CTRL,
                                  MAKE_GFX_CTRL(0x00, GFX_VISIBLE, GFX_1_BPP, GFX_TILEMAP, GFX_1X, GFX_1X));
                        xreg_setw(PA_LINE_LEN, xosera_vid_width() / 8);
                    }
                }
                else
                {
                    next = true;
                    break;
                }
            }

            // read PCM data
            read_buffer(file);

            // poll audio ready interrupt status until ready for next buffer
            while ((xm_get_int_status() & INT_CTRL_AUD0_INTR_F) == 0)
            {
                if (mcCheckInput())
                {
                    break;
                }
            }

            // upload PCM data
            upload_buffer(buf_off);

            // queue new buffer to play next
            queue_buffer(buf_off, period);

            // switch buffers
            buf_off ^= BUFFER_WORDS;

            // wait for buffers to empty
            if (at_eof)
            {
                while ((xm_get_int_status() & INT_CTRL_AUD0_INTR_F) == 0)
                {
                    if (mcCheckInput())
                    {
                        break;
                    }
                }
                queue_buffer(buf_off, period);
                while ((xm_get_int_status() & INT_CTRL_AUD0_INTR_F) == 0)
                {
                    if (mcCheckInput())
                    {
                        break;
                    }
                }
                next = true;
            }
        }

        disable_audio();

        fl_fclose(file);

    } while (!quit);

    xosera_xansi_restore();
}
