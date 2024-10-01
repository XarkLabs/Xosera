/*
 * Copyright (c) 2020 Ross Bamford
 */

#if !defined(DEBUG)
#define DEBUG 0
#endif

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <rosco_m68k/machine.h>
#include <rosco_m68k/xosera.h>

#include "rosco_m68k_support.h"

#if DEBUG
#include <debug.h>
#endif

#include "pt_mod.h"
#include "xosera_mod_play.h"

extern char _binary_xenon_mod_raw_start[];
extern char _binary_xenon_mod_raw_end[];

#if NTSC
#define TENTH_MS_PER_FRAME 166
#else
#define TENTH_MS_PER_FRAME 200
#endif

// #define VERBOSE             // Define to see "best effort" division printout

extern int play_mod(PtMod * mod);
extern int xosera_play(PtMod * mod);

#define LOAD_CHUNK (24 * 1024)
#if 0
uint8_t buffer[640 * 1024];
#else
uint8_t * buffer;
#endif

int load_mod(const char * filename, uint8_t * buf, int size)
{
    debug_printf("\nLoading %s into %p size %x\n", filename, buf, size);
    FILE * f = fopen(filename, "r");

    if (!f)
    {
        debug_printf("Unable to open MOD '%s'\n", filename);
        return 0;
    }
#if 0
    if (fseek(f, 0, SEEK_END))
    {
        fclose(f);
        debug_printf("Seek failed; bailing\n");
        return 0;
    }

    const long fsize = ftell(f);
    if (fsize == -1L)
    {
        fclose(f);
        debug_printf("ftell failed; bailing\n");
        return 0;
    }

    if (fseek(f, 0, SEEK_SET))
    {
        fclose(f);
        debug_printf("Second seek failed; bailing\n");
        return 0;
    }

    if (fsize > size)
    {
        fclose(f);
        debug_printf("File too big; bailing\n");
        return 0;
    }
    int size_remain = fsize;
#else
    const long fsize       = size;
    int        size_remain = size;
#endif

    while (size_remain > 0)
    {
        int partial_size = size_remain > LOAD_CHUNK ? LOAD_CHUNK : size_remain;
        int result       = fread(buf, partial_size, 1, f);
        if (result != partial_size)
        {
            fclose(f);
            debug_printf("\nRead failed; bailing\n");
            return 0;
        }
        size_remain -= result;
        buf += result;
        debug_printf(".");
    }

    fclose(f);
    debug_printf("done.\n");
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

#if 0

    FL_DIR dirstat;

    if (opendir("/", &dirstat))
    {
        struct fs_dir_ent dirent;

        while (num_mods < MAX_MODS && readdir(&dirstat, &dirent) == 0)
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

        closedir(&dirstat);
    }
#else
    strcpy(mod_files[0], "1990_mix.mod");
    strcpy(mod_files[1], "sd/a_fox_in_my_box.mod");
    strcpy(mod_files[2], "/sd/xenon2.mod");
    strcpy(mod_files[3], "xosera.mod");

    mod_size[0] = 315687;
    mod_size[1] = 7728;
    mod_size[2] = 365222;
    mod_size[3] = 115066;

    num_mods = 4;
#endif

    int num = 0;
    do
    {
        debug_printf("\n\nMOD files available:\n\n");

        for (int i = 0; i < num_mods; i++)
        {
            debug_printf("%c - [%3dK] %s\n", 'A' + i, ((mod_size[i] + 1023) / 1024), mod_files[i]);
        }

        debug_printf("\nSelect (A-%c):", 'A' + num_mods - 1);

        int key = mcInputchar();

        if (key == 27)
        {
            debug_printf("ESC\n\n");
            return NULL;
        }

        if (key >= 'a' && key <= 'z')
        {
            key -= ('a' - 'A');
        }

        num = key - 'A';

    } while (num < 0 || num >= num_mods);

    debug_printf("%c\n\n", 'A' + num);

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

int main()
{
    xv_prep();

    printf("\033c\033[?25l");        // XANSI reset, disable input cursor
    debug_printf("\033c");           // terminal reset
    debug_printf("rosco_pt_mod - xosera_init(XINIT_CONFIG_640x480) - ");
    xosera_init(XINIT_CONFIG_640x480);
    debug_printf("OK (%dx%d).\n", xosera_vid_width(), xosera_vid_height());

    init_viz();

    while (mcCheckInput())
    {
        mcInputchar();
    }

#if LOG
    // If we crashed last time, see if there's a log available...
    ptmodPrintLastlog();
#endif

    bool exit = false;

    while (!exit)
    {
#if 0
        const char * filename = get_file();

        if (filename == NULL)
        {
            exit = true;
            break;
        }
#endif
        init_viz();

#if DEBUG
        start_debugger();
#endif

#if 0
        debug_printf("Loading mod: \"%s\"", filename);
        if (load_mod(filename, buffer, sizeof(buffer)))
#else
        buffer = (uint8_t *)_binary_xenon_mod_raw_start;
#endif
        {

            PtMod * mod = (PtMod *)buffer;

            debug_printf("\nMOD is %-20.20s\n", mod->song_name);

#ifdef PRINT_INFO
            play_mod(mod);
#endif

#ifdef PLAY_SAMPLE
            while (mcCheckInput())
            {
                mcInputchar();
            }

            debug_printf("Starting playback; Hit 'ESC' to exit or any key for another song...\n");

            xreg_setw(AUD_CTRL, 0x0001);
            xm_setw(TIMER, TENTH_MS_PER_FRAME);

            ptmodPlay(mod, install_intr);

            while (!exit)
            {
                // Waiting...
                if (mcCheckInput())
                {
                    uint8_t c = mcInputchar();

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
                    debug_printf(
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

            debug_printf("\nPlayback stopped.\n");

            ptmodPrintlog();
        }
#if 0
        else
        {
            debug_printf("Can't load file...\n");
        }
#endif
#endif
    }
    xosera_init(XINIT_CONFIG_640x480);

#if LOG
    // Clear log so subsequent run doesn't think we crashed...
    ptmodClearLog();
#endif

    debug_printf("\nAll done, bye!\n");
}
