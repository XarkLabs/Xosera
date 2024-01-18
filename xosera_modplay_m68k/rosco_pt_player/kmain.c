/*
 * Copyright (c) 2020 Ross Bamford
 */

#if !defined(DEBUG)
#define DEBUG   0
#endif

#include <basicio.h>
#include <limits.h>
#include <sdfat.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <xosera_m68k_api.h>

#if DEBUG
#include <debug.h>
#endif

#include "dprintf.h"
#include "pt_mod.h"
#include "xosera_mod_play.h"

#if NTSC
#define TENTH_MS_PER_FRAME 166
#else
#define TENTH_MS_PER_FRAME 200
#endif

// #define VERBOSE             // Define to see "best effort" division printout

extern int main(PtMod * mod);
extern int xosera_play(PtMod * mod);

#define LOAD_CHUNK (24 * 1024)
static uint8_t buffer[640 * 1024];

static int load_mod(const char * filename, uint8_t * buf, int size)
{
    FILE * f = fl_fopen(filename, "r");

    if (!f)
    {
        dprintf("Unable to open MOD '%s'\n", filename);
        return 0;
    }

    if (fl_fseek(f, 0, SEEK_END))
    {
        fl_fclose(f);
        dprintf("Seek failed; bailing\n");
        return 0;
    }

    const long fsize = fl_ftell(f);
    if (fsize == -1L)
    {
        fl_fclose(f);
        dprintf("ftell failed; bailing\n");
        return 0;
    }

    if (fl_fseek(f, 0, SEEK_SET))
    {
        fl_fclose(f);
        dprintf("Second seek failed; bailing\n");
        return 0;
    }

    if (fsize > size)
    {
        fl_fclose(f);
        dprintf("File too big; bailing\n");
        return 0;
    }

    int size_remain = fsize;
    while (size_remain > 0)
    {
        int partial_size = size_remain > LOAD_CHUNK ? LOAD_CHUNK : size_remain;
        int result       = fl_fread(buf, partial_size, 1, f);
        if (result != partial_size)
        {
            fl_fclose(f);
            dprintf("\nRead failed; bailing\n");
            return 0;
        }
        size_remain -= result;
        buf += result;
        dprintf(".");
    }

    fl_fclose(f);
    dprintf("done.\n");
    return fsize;
}

extern void install_intr();
extern void remove_intr();

#define MAX_MODS    26
#define MAX_NAMELEN 64
char mod_files[MAX_MODS][MAX_NAMELEN];
int  num_mods;
int  mod_size[MAX_MODS];

const char * get_file()
{
    num_mods = 0;
    memset(mod_files, 0, sizeof(mod_files));

    FL_DIR dirstat;

    if (fl_opendir("/", &dirstat))
    {
        struct fs_dir_ent dirent;

        while (num_mods < MAX_MODS && fl_readdir(&dirstat, &dirent) == 0)
        {
            if (!dirent.is_dir && dirent.filename[0] != '.')
            {
                const char * ext = strrchr(dirent.filename, '.');
                if (ext)
                {
                    if (strcmp(ext, ".mod") == 0 || strcmp(ext, ".MOD") == 0)
                    {
                        strcpy(mod_files[num_mods], "/");
                        strcat(mod_files[num_mods], dirent.filename);
                        mod_size[num_mods] = dirent.size;

                        num_mods++;
                    }
                }
            }
            memset(&dirent, 0, sizeof(dirent));
        }

        fl_closedir(&dirstat);
    }

    int num = 0;
    do
    {
        dprintf("\n\nMOD files available:\n\n");

        for (int i = 0; i < num_mods; i++)
        {
            dprintf("%c - [%3dK] %s\n", 'A' + i, ((mod_size[i] + 1023) / 1024), mod_files[i]);
        }

        dprintf("\nSelect (A-%c):", 'A' + num_mods - 1);

        int key = readchar();

        if (key == 27)
        {
            dprintf("ESC\n\n");
            return NULL;
        }

        if (key >= 'a' && key <= 'z')
        {
            key -= ('a' - 'A');
        }

        num = key - 'A';

    } while (num < 0 || num >= num_mods);

    dprintf("%c\n\n", 'A' + num);

    return mod_files[num];
}

void init_viz()
{
    xv_prep();

    // setup 4-bpp 16 x 16 tiled screen showing 4 sample buffers
    xreg_setw(PA_GFX_CTRL, 0x001E);             // colorbase = 0x00, tiled, 4-bpp, Hx4 Vx3
    xreg_setw(PA_HV_FSCALE, 0x0044);            // set 512x384 scaling
    xreg_setw(PA_TILE_CTRL, 0x0800 | 7);        // tiledata @ 0x800, 8 high
    xreg_setw(PA_DISP_ADDR, 0x0000);            // display VRAM @ 0x0000
    xreg_setw(PA_LINE_LEN, 0x0010);             // 16 chars per line

    // set colormap
    for (uint16_t i = i; i < 16; i++)
    {
        xmem_setw(XR_COLOR_A_ADDR + 0 + i, (i << 8) | (i << 4) | (i << 0));
        xmem_setw(XR_COLOR_A_ADDR + 16 + i, (i << 8) | (0 << 4) | (0 << 0));
        xmem_setw(XR_COLOR_A_ADDR + 32 + i, (0 << 8) | (i << 4) | (0 << 0));
        xmem_setw(XR_COLOR_A_ADDR + 48 + i, (i << 8) | (0 << 4) | (i << 0));
    }

    xm_setw(WR_INCR, 0x0001);
    int c = 0;

    for (int x = 0; x < 16; x += 8)
    {
        for (int y = 0; y < 16; y++)
        {
            xm_setw(WR_ADDR, (y * 16) + x);
            uint16_t color = ((x / 8) << 12) | ((y / 8) << 13);        // use colorbase for channel tint
            xm_setw(DATA, color | c);
            xm_setw(DATA, color | c);
            xm_setw(DATA, color | c);
            xm_setw(DATA, color | c);
            xm_setw(DATA, color | c);
            xm_setw(DATA, color | c);
            xm_setw(DATA, color | c);
            xm_setw(DATA, color | c);

            c += 1;
        }
    }

    xmem_setw_next_addr(XR_TILE_ADDR + 0x0800);
    for (int i = 0x0000; i < 0x1000; i++)
    {
        xmem_setw_next((i & 2) ? 0x0808 : 0x8080);
    }
}

void kmain()
{
    xv_prep();

    printf("\033c\033[?25l");        // XANSI reset, disable input cursor
    dprintf("\033c");                // terminal reset
    dprintf("rosco_pt_mod - xosera_init(2) - ");
    xosera_init(2);
    dprintf("OK (%dx%d).\n", xosera_vid_width(), xosera_vid_height());

    init_viz();

    while (checkchar())
    {
        readchar();
    }

#if LOG
    // If we crashed last time, see if there's a log available...
    ptmodPrintLastlog();
#endif

    bool exit = false;

    while (!exit)
    {
        if (!SD_FAT_initialize())
        {
            dprintf("no SD card, bailing\n");
            return;
        }
        const char * filename = get_file();

        if (filename == NULL)
        {
            exit = true;
            break;
        }

        init_viz();

#if DEBUG
        start_debugger();
#endif

        dprintf("Loading mod: \"%s\"", filename);
        if (load_mod(filename, buffer, sizeof(buffer)))
        {

            PtMod * mod = (PtMod *)buffer;

            dprintf("\nMOD is %-20.20s\n", mod->song_name);

#ifdef PRINT_INFO
            main(mod);
#endif

#ifdef PLAY_SAMPLE
            while (checkchar())
            {
                readchar();
            }

            dprintf("Starting playback; Hit 'ESC' to exit or any key for another song...\n");

            xreg_setw(AUD_CTRL, 0x0001);
            xm_setw(TIMER, TENTH_MS_PER_FRAME);

            ptmodPlay(mod, install_intr);

            while (!exit)
            {
                // Waiting...
                if (checkchar())
                {
                    uint8_t c = readchar();

                    if (c == 27)
                    {
                        exit = true;
                    }
                    break;
                }
#ifdef VERBOSE
                if (lastPatternPos != patternPos)
                {
                    lastPatternPos = patternPos;
                    dprintf(
                        "%02x[%02x]: SMPL: %02x; PD: %04d [%3s] (Xosera: %05d) CMD: %03x:%03x:%03x:%03x (SPD: %d / %d "
                        "ms/10)\n",
                        pattern,
                        patternPos,
                        sampleNumber,
                        period,
                        PtNoteName(period),
                        xoPeriod,
                        effect1,
                        effect2,
                        effect3,
                        effect4,
                        StepFrames,
                        tickTenMs);
                }
#endif
            }

            remove_intr();

            xreg_setw(AUD0_VOL, MAKE_AUD_VOL(0, 0));
            xreg_setw(AUD1_VOL, MAKE_AUD_VOL(0, 0));
            xreg_setw(AUD2_VOL, MAKE_AUD_VOL(0, 0));
            xreg_setw(AUD3_VOL, MAKE_AUD_VOL(0, 0));
            xreg_setw(AUD_CTRL, 0x0000);
            xm_setw(INT_CTRL, INT_CTRL_AUD_EN_ALL_F | INT_CTRL_CLEAR_ALL_F);

            dprintf("\nPlayback stopped.\n");

            ptmodPrintlog();

#endif
        }
        else
        {
            dprintf("Can't load file...\n");
        }
    }
    xosera_init(0);

#if LOG
    // Clear log so subsequent run doesn't think we crashed...
    ptmodClearLog();
#endif

    dprintf("\nAll done, bye!\n");
}
